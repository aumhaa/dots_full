### Building (openFrameworks 0.12.1, Apple Silicon)

The last openFrameworks 0.9.3 (Intel) version is tagged `of-0.9.3-final`.

1. Unpack `of_v0.12.1_osx_release` to `~/openFrameworks/of_v0.12.1_osx_release`.
2. Open `dots.xcodeproj` and build the `dots Release` scheme, or:
   `xcodebuild -project dots.xcodeproj -scheme "dots Release" -configuration Release`
   The app is `bin/dots.app`.

The addons live in `addons/` (ofxMidi 1.3.0, ofxSyphon master 2025-03-29,
ofxFft master using KISS FFT); ofxGui and ofxXmlSettings come with openFrameworks.

If you regenerate the project with the project generator, run
`python3 scripts/postgen_xcodeproj.py dots.xcodeproj/project.pbxproj` afterwards:
it takes flam3's command-line tools out of the build, links libxml2, and fixes
the Syphon framework path.

### MIDI and the Twister

dots opens two virtual MIDI ports. `DotsInput` takes CC 0-20 on channel 1
(the scene parameters, see `StateManager::controllerToSceneParams`), notes 0-4
on channel 1 (prev/next track, prev/next scene, wander), and CC 127 on
channel 1, which asks dots to send everything. `DotsOutput` sends each
parameter back as CC 0-20 on channel 1 when its MIDI value changes (at most
20 times a second), and on request each parameter's section colour as CC 0-20
on channel 2, a Midi Fighter Twister LED value (`src/DotsSections.h`).

`modul8/Twister.m8m` is the Modul8 module that drives a Midi Fighter Twister,
a copy of the one installed in `/Applications/modul8v3/Modules`. Its side
button switches the Twister between Modul8 and dots. In dots mode the knobs
follow dots's values and section colours, knob buttons 1-4 step tracks and
scenes, knob button 16 flips to the second page (motionId and the effect
sizes), and the other buttons do nothing. Modul8 only picks up a changed
module file after a restart. Nothing else should be driving the Twister at the
same time, for example a Live control surface on the same device.

### Running multiple screens

- Go to system preferences >> mission control >> and set "Displays have separate Spaces" to false. Log out and Log in again, and it will work. ([source](https://forum.openframeworks.cc/t/dual-monitor-full-screen/13654/3))

### Audio Loopback

1. Install SoundFlower: https://github.com/mattingalls/Soundflower/releases/tag/2.0b2
2. Follow the instructions in the above link on how to create a "multi-output" device
3. Select that device in Audio MIDI Setup for Sound Output
4. Run dots, and drag the soundStreamDevice slider until SoundFlower (2ch) is selected
