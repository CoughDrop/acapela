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

## License
MIT License

## TODO
- add examples (in the mean time, you can see how we're using it in
the TTS section here, https://github.com/CoughDrop/coughdrop/blob/master/app/frontend/app/utils/capabilities.js)
- specs (stop judging me, I'm not a native app developer)
