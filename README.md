# LiveLily
![LiveLily logo](images/livelily_logo_scaled.png?raw=true)
   
version-beta0.1

LiveLily is a live sequencing and live scoring system through live coding with a [Lilypond]-like language. The system includes a text editor, a parser, a sequencer, and an interactive/animated score. The commands are inspired by the Lilypond language, with certain commands that accommodate fast typing and fast coding sequences being added.

LiveLily does not produce any sound. It sends OSC or MIDI messages to control other software and hardware. It can be combined with the LiveLily-Score, to display parts of the full score separately, that can be sight-read by instrumentalists. The possible setups that LiveLily supports are pure electronic, electroacoustic, or acoustic only.

LiveLily is written entirely in [openFrameworks], using the ofxOsc and ofxMidi addons. You will need to use the OF 0.12.0 or higher.
The directories livelily/ (which is the main program) and livelily-score-part/ must be placed in the OF_ROOT/apps/myapps/ directory, so they can compile properly.
   
![LiveLily screenshow](images/livelily_screenshot.png?raw=true)
  
[Demo video] (including earlier syntax with some quotation symbols that are now removed):

  
TODO:
- fix positioning of stems and beams on certain rhythms
- write the documentation of the program
- work on the livelily-score app to display the pulsating rectangle and to display more than one bar

[openFrameworks]: https://openframeworks.cc/
[Demo video]: https://vimeo.com/781559305
[Lilypond]: https://lilypond.org/
