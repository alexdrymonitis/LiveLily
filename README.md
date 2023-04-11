# LiveLily
![LiveLily logo](images/livelily_logo_scaled.png?raw=true)
  
  
LiveLily is a live sequencing and live scoring system through live coding with a Lilypond-like language. The system includes a text editor, a parser, a sequencer, and an interactive/animated score. The commands are inspired by the Lilypond language, with certain commands that accommodate fast typing and fast coding sequences being added.

LiveLily does not produce any sound. It sends messages either with OSC or MIDI, to control other software and hardware. It can be combined with the LiveLily-Score, to display parts of the full score separately, that can be sight-read by instrumentalists. The possible setups that LiveLily supports are pure electronic, electroacoustic, or acoustic only.

LiveLily is entirely written in [openFrameworks], using the ofxOsc and ofxMidi addons. You will need to use the nightly builds to compile this because of [this issue], and as a neat-pick, because of [this].

The directories livelily/ and livelily-score/ must be placed in the OF_ROOT/apps/myapps/ directory, so they can compile properly.
  
  
![LiveLily screenshow](images/livelily_screenshot.png?raw=true)

[openFrameworks]: https://openframeworks.cc/
[this issue]: https://forum.openframeworks.cc/t/are-monospace-fonts-really-monospace/40358
[this]: https://forum.openframeworks.cc/t/how-to-set-a-custom-icon-for-an-app/41613
