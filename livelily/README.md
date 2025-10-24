# LiveLily
![LiveLily logo](../images/livelily_logo_scaled.png?raw=true)

version beta-0.2
This is the main LiveLily program. It contains the editor, the parser, the sequencer, and the interactive/animated full score. This program can be used combined with audio-generating software that support either the OSC or the MIDI protocol. It can also be used combined with the LiveLily-Part program, that displays one part of the full score, to be sight-read by an instrumentalist, in case of live scoring an acoustic instrument or ensemble.

These two scenarios can be user either separately, or combined, to control an electroacoustic live coding session.

Change log:
- Syntax highlighting
- Pane spitting
- Editor modes (Normal, Insert, Visual, Command) like in Vim
- Error, warning, note message handling
- Nested Commands
- New Commands
-- \bars
-- Use of the wildcard
-- \function
-- \list
- Change in the language design, now commands are split between first and second level
- Python integration
- Reading MusicXML files
- Connecting to serial devices
- Enhanced OSC features
