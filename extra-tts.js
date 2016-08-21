(function () {
    var acapela = require('acapela');
    var extract = require('extract-zip');
    var request = require('request');
    var rimraf = require('rimraf');
    var fs = require('fs');

    var tts = {
        exec: function () {
        },
        enabled: false
    };

    if (acapela) {
        tts.enabled = true;
        tts.acapela = acapela;
        var speaker = {
            listen: function (callback) {
                speaker.listeners = speaker.listeners || [];
                speaker.listeners.push(callback);
                if (!speaker.pinging) {
                    speaker.pinging = true;
                    setTimeout(speaker.ping, 100);
                }
            },
            ping: function () {
                // There is a callback on the native speak method, but I'm not smart
                // enough to figure out how to trigger a javascript callback from
                // a c callback, so I poll instead. Don't judge.
                var res = acapela.isSpeaking();
                if (res) {
                    setTimeout(speaker.ping, 50);
                } else {
                    speaker.pinging = false;
                    while (speaker.listeners && speaker.listeners.length > 0) {
                        var cb = speaker.listeners.shift();
                        cb();
                    }
                }
            }
        };
        var downloader = {
            download_file_to_location(url, file_path, progress) {
                var size = 0;
                request({
                    uri: url
                }, function (err, res, body) {
                  if(err) {
                    console.log("error downloading " + url);
                    console.log(err);
                    progress(null, null, "error downloading " + url);
                  } else {
                    console.log("got a response");
                    progress(null, null, "got a response");
                  }
                }).on('data', function (data) {
                    size = size + data.length;
                    progress(size, false, null);
                }).pipe(fs.createWriteStream(file_path)).on('close', function() {
                    progress(0, true, null);
                });                  
            },
            download_file: function(url, expected_size, percent_pre, percent_amount, done) {
                downloader.download_file_to_location(url, downloader.tmp_file, function (size, complete, error) {
                  if(error) {
                    console.log(error);
                  } else if(complete) {
                    done();
                  } else {
                    downloader.watcher({
                      percent: percent_pre + Math.min(1.0, size / expected_size) * percent_amount,
                      done: false
                    });
                  }
                });
            },
            assert_directory: function(dir, done) {
                fs.stat(dir, function(err, stats) {
                    if(err && err.errno === 34) {
                        fs.mkdir(dir, function() {
                            done();
                        });                  
                    } else {
                        done();
                    }
                });
            },
            unzip_file_to_location: function(file_path, dir, progress) {
                var entries = 0;
                downloader.assert_directory(dir, function() {
                    extract(downloader.tmp_file, {
                        dir: dir, onEntry: function () {
                            entries++;
                            progress(entries, false, null);
                        }
                    }, function (err) {
                        fs.unlink(downloader.tmp_file);
                        progress(0, true, null);
                    });
                });
            },
            unzip_file: function(language_dir, n_entries, percent_pre, percent_amount, done) {
                downloader.unzip_file_to_location(downloader.tmp_file, './data/' + language_dir, function(entries, complete, error) {
                  if(error) {
                    console.log(error);
                  } else if(complete) {
                    done();
                  } else {
                    downloader.watcher({
                      percent: percent_pre + (Math.min(1.0, (entries / n_entries)) * percent_amount),
                      done: false
                    });
                  }
                });
            },
            download_voice: function (opts) {
                var dir_id = opts.voice_id.replace(/^acap:/, '');
                var language_dir = opts.language_dir;
                downloader.tmp_file = 'tmp_download' + Math.round(Math.random() * 99999) + '.zip';
                downloader.watcher = downloader.watcher || function() { };
                
                fs.unlink(downloader.tmp_file, function () {
                    var download_language = function () {
                        console.log('downloading language...');
                        downloader.download_file(opts.language_url, (5 * 1024 * 1024), 0, 0.10, unzip_language);
                    };
                    var unzip_language = function() {
                        console.log('unzipping language...');
                        downloader.unzip_file(language_dir, 10, 0.10, 0.05, download_voice);
                    };
                    var download_voice = function() {
                        console.log('downloading voice...');
                        downloader.download_file(opts.voice_url, (50 * 1024 * 1024), 0.15, 0.65, unzip_voice);
                    };
                    var unzip_voice = function() {
                        console.log('unzipping voice...');
                        downloader.unzip_file(language_dir, 20, 0.85, 0.15, done);
                    };
                    var done = function() {
                        downloader.watcher({
                            percent: 1.0,
                            done: true
                        });
                    };

                    download_language();
                });
            },
            delete_voice: function(opts) {
                var dir_id = opts.voice_id.replace(/^acap:/, '');
                var found_dir = null;
                fs.readdir('./data/' + opts.language_dir, function(list) {
                    for(var idx = 0; idx < list.length; idx++) {
                        var fn = list[idx];
                        var re = new RegExp(dir_id + "[^A-Za-z]", 'i');
                        if(fn.match(re)) {
                            found_dir = fn;
                        }
                    }
                });
                if(fn) {
                    rimraf('./data/' + opts.language_dir + '/' + found_dir, function () {
                        if (opts.success) {
                            opts.success();
                        }
                    });
                } else {
                    if(opts.success) {
                        opts.success({message: "no matching directory found"});
                    }
                }
            },
            watch: function (callback) {
                downloader.watcher = callback;
            }
        };
        tts.exec = function (method, opts) {
            opts = opts || {};
            if (method == 'speakText') {
                speaker.listen(opts.success);
                if (!tts.voice_id_map) {
                    tts.getAvailableVoices();
                }
                if (tts.voice_id_map && tts.voice_id_map[opts.voice_id]) {
                    opts.voice_key = tts.voice_id_map[opts.voice_id];
                    acapela.openVoice(opts.voice_key);
                }
                if (opts.volume) {
                    opts.volume = Math.min(Math.max(0, opts.volume * 100), 150);
                }
                if (opts.rate) {
                    opts.rate = Math.min(Math.max(30, opts.rate * 100), 300);
                }
                if (opts.pitch) {
                    opts.pitch = Math.min(Math.max(50, opts.pitch * 100), 150);
                }
                opts.success = function () { };
            }
            var res = acapela[method](opts);
            if (res && res.ready !== false) {
                if (opts.success) {
                    opts.success(res);
                }
            } else {
                if (opts.error) {
                    opts.error({ error: "negative response to " + method });
                }
            }
        }
        var keys = ['init', 'status', 'getAvailableVoices', 'downloadVoice', 'deleteVoice', 'speakText', 'stopSpeakingText'];
        for (var idx = 0; idx < keys.length; idx++) {
            (function (method) {
                tts[method] = function (opts) {
                    tts.exec(method, opts);
                }
            })(keys[idx]);
        }
        tts.getAvailableVoices = function (opts) {
            opts = opts || {};
            var raw_list = acapela.getAvailableVoices();
            var new_list = [];
            tts.voice_id_map = {};
            for (var idx = 0; idx < raw_list.length; idx++) {
                var voice = raw_list[idx];
                voice.raw_voice_id = voice.voice_id;
                voice.voice_id = 'acap:' + voice.raw_voice_id.split(/[^A-Za-z-]/)[0];
                tts.voice_id_map[voice.voice_id] = voice.raw_voice_id;
                new_list.push(voice);
            }
            if (opts.success) {
                opts.success(new_list);
            }
        };
        tts.downloadVoice = function (opts) {
            downloader.watch(opts.progress || opts.success);
            downloader.download_voice(opts || {});
        };
        tts.deleteVoice = function (opts) {
            downloader.delete_voice(opts || {});
        };
        tts.download_file = function(url, file_path, progress) {
            downloader.download_file_to_location(url, file_path, progress);
        };
        tts.unzip_file = function(file_path, dir_path, progress) {
            downloader.unzip_file_to_location(file_path, dir_path, progress);
        };
    }
    
    module.exports = tts;
})();