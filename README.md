# LiveLily
![LiveLily logo](images/livelily_logo_scaled.png?raw=true)
   
version-beta0.2

LiveLily is a live sequencing and live scoring system through live coding with a [Lilypond]-like language. The system includes a text editor, a parser, a sequencer, and an interactive/animated score. The commands are inspired by the Lilypond language, with certain commands that accommodate fast typing and fast coding sequences being added.

LiveLily does not produce any sound. It sends OSC or MIDI messages to control other software and hardware. It can be combined with the LiveLily-Score, to display parts of the full score separately, that can be sight-read by instrumentalists. The possible setups that LiveLily supports are pure electronic, electroacoustic, or acoustic only.

LiveLily is written entirely in [openFrameworks], using the ofxOsc and ofxMidi addons. You will need to use the OF 0.12.0 or higher.
The directories livelily/ (which is the main program) and livelily-part/ must be placed in the OF_ROOT/apps/myapps/ directory, so they can compile properly.
The addons/ofxMidi repository should be added to OF_ROOT/addons. To include it, clone with:
`git clone --recursive https://github.com/alexdrymonitis/LiveLily.git`

This software is under heavy development, so stay tuned as things change, new features are added and bugs are constantly fixed.
   
![LiveLily screenshot](images/LiveLily_screenshot.png?raw=true)

List of videos with LiveLily:
- https://vimeo.com/1041385184?fl=ip&fe=ec
- https://vimeo.com/1130135700?fl=ip&fe=ec
- https://vimeo.com/1040471260?fl=ip&fe=ec (although the code projection is not clear)

If you use this software in an academic context, please cite it as follows:
```
 @article{nime2023_37,
 abstract = {LiveLily is an open-source system for live sequencing and live scoring through live coding in a subset of the Lilypond language. It is written in openFrameworks and consists of four distinct parts, the text editor, the language parser, the sequencer, and the music score. It supports the MIDI and OSC protocols to communicate the sequencer with other software or hardware, as LiveLily does not produce any sound. It can be combined with audio synthesis software that supports OSC, like Pure Data, SuperCollider, and others, or hardware synthesizers that support MIDI. This way, the users can create their sounds in another, audio-complete framework or device, and use LiveLily to control their music.
LiveLily can also be used as a live scoring system to write music scores for acoustic instruments live. This feature can be combined with its live sequencing capabilities, so acoustic instruments can be combined with live electronics. Both live scoring and live sequencing in LiveLily provide expressiveness to a great extent, as many musical gestures can be included either in the score or the sequencer. Such gestures include dynamics, articulation, and arbitrary text that can be interpreted in any desired way, much like the way Western-music notation scores are written.},
 address = {Mexico City, Mexico},
 articleno = {37},
 author = {Alexandros Drymonitis},
 booktitle = {Proceedings of the International Conference on New Interfaces for Musical Expression},
 editor = {Miguel Ortiz and Adnan Marquez-Borbon},
 issn = {2220-4806},
 month = {May},
 numpages = {6},
 pages = {256--261},
 title = {LiveLily: An Expressive Live Sequencing and Live Scoring System Through Live Coding With the Lilypond Language},
 track = {Papers},
 url = {http://nime.org/proceedings/2023/nime2023_37.pdf},
 year = {2023}
}
```

  
TODO:
- fix positioning of stems and beams on certain rhythms
- write the documentation of the program

[openFrameworks]: https://openframeworks.cc/
[Lilypond]: https://lilypond.org/
