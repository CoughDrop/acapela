# Acapela
Node module for supporting Acapela voices in an Electron app. Outside of Electron
your mileage may vary. Windows only.

## Requirements
Built on top of Acapela's speech engine. You'll
need to raeach out to Acapela to get the needed libraries and license
files. `AcaTTS.dll`, `AcaTTS.64.dll` and `AcaTTS.ini` are all required
to be in the main directory of your app (not the library's directory,
the Electron app's root folder). The app also expects to have 
the required `/bin` and `/data` directores in the same directory.

To compile the app you'll first need to get license files from Acapela
and put those in the library's `/include` directory. Rename the .h file
to `license.h` and then you can run `node-gyp rebuild` or however you
build modules.

## Usage

`npm install https://www.github.com/coughdrop/acapela.git`

The easiest way to use the library is to require `extra-tts.js` in the 
acapela module. If you require it in the app process then you can do things
like the following:

```
var extra_tts = require('acapela/extra-tts.js');

extra_tts.getAvailableVoices({success: function(list) {
  console.log(list);
}});

extra_tts.speakText({
  voice_id: "<voice id from the list>",
  text: "hello my friends, I am speaking to you",
  success: function() {
    console.log("done speaking!");
  }
});

extra_tts.stopSpeakingText();
```

### Chrome Memory Issue

Chrome apparently doesn't like downloading and unzipping really large files in a 
browser process, and on older computers also seems to struggle with the acapela
libraries, it'll sometimes result in the entire window turning black. You
can get around this in electron by requiring `extra-tts.js` on the main process,
and `extra-tts-ipc.js` on the rendering process. You'll also need a little extra
code on the main process, something like:

```
ipcMain.on('extra-tts-exec', function(event, message) {
  var sender = event.sender;
  var opts = JSON.parse(str);
  var args = opts.args;
  opts[0].success = function(res) {
    sender.send('extra-tts-exec-result', JSON.stringify({
      success: true,
      callback_id: opts.success_id,
      result: res
    });
  };
  opts[0].progress = function(res) {
    sender.send('extra-tts-exec-result', JSON.stringify({
      progress: true,
      callback_id: opts.progress_id,
      result: res
    });
  };
  opts[0].error = function(err) {
    sender.send('extra-tts-exec-result', JSON.stringify({
      error: true,
      callback_id: opts.error_id,
      result: err
    });
  };
  extra_tts[opts.method].apply(extra_tts, args);
});

ipcMain.on('extra-tts-ready', function() {
  event.sender.send('extra-tts-ready', 'ready');
});
```

## License
MIT License

## TODO
- add examples (in the mean time, you can see how we're using it in
the TTS section here, https://github.com/CoughDrop/coughdrop/blob/master/app/frontend/app/utils/capabilities.js)
- specs (stop judging me, I'm not a native app developer)
