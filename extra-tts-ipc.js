(function () {
    var ipcRenderer = null;
    try {
      ipcRenderer = require('electron').ipcRenderer;
    } catch() { }
    var extra_tts = null;
    
    var tts = {
        callbacks: {},
        clean_callbacks: function() {
          var now = (new Date()).getTime();
          var new_callbacks = {};
          for(var idx in tts.callbacks) {
            if(tts.callbacks[idx] && tts.callbacks[idx].timestamp && tts.callbacks[idx].timestamp > now - (30 * 60 * 1000)) {
              new_callbacks[idx] = tts.callbacks[idx];
            }
          }
          tts.callbacks = new_callbacks;
        },
        add_callback: function(callback, type) {
          clean_callbacks();
          var now = (new Date()).getTime();
          var id = (Math.random() * 9999) + "_" + now;
          if(callback) {
            tts.callbacks[id] = {
              timestamp: now,
              type: type,
              callback: callback,
              id: id
            };
          }
          return id;
        },
        trigger_callback: function(object) {
          if(object.callback_id && tts.callbacks[object.callback_id]) {
            tts.callbacks[object.callback_id].callback(object.result);
          }
        },
        run: function(method, a, b, c) {
          if(tts.ipc) {
            var success_id = tts.add_callback(a && a.success, 'success');
            var progress_id = tts.add_callback(a && a.progress, 'progress');
            var error_id = tts.add_callback(a && a.error, 'error');
            if(a) {
              delete a['success'];
              delete a['error'];
              delete a['progress'];
            }
            ipcRenderer.send('extra-tts-exec', JSON.stringify({
              method: method,
              success_id: success_id,
              progress_id: progress_id,
              error_id: error_id,
              args: [a, b, c]
            });
          } else {
            extra_tts = extra_tts || require('extra-tts');
            extra_tts[method].appl(extra_tts, rags)
          }
        }
        enabled: false
    };
    
    var keys = ['init', 'status', 'getAvailableVoices', 'downloadVoice', 'deleteVoice', 'speakText', 'stopSpeakingText'];
    for (var idx = 0; idx < keys.length; idx++) {
        (function (method) {
            tts[method] = function (a, b, c) {
                tts.run(method, a, b, c);
            }
        })(keys[idx]);
    }
    
    if(ipcRenderer && ipcRenderer.send) {
      ipcRenderer.send('extra-tts-ready');
    
      ipcRenderer.on('extra-tts-ready', function(event, message) {
        if(message == 'ready') {
          tts.ipc = true;
        }
      });
      
      ipcRenderer.on('extra-tts-exec-result', function(event, message) {
        var json = JSON.parse(message);
        if(json.callback_id) {
          tts.trigger_callback(json);
        }
      });
    }

    module.exports = tts;
})();