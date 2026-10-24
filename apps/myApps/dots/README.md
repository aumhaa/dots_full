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

### Running multiple screens

- Go to system preferences >> mission control >> and set "Displays have separate Spaces" to false. Log out and Log in again, and it will work. ([source](https://forum.openframeworks.cc/t/dual-monitor-full-screen/13654/3))

### Audio Loopback

1. Install SoundFlower: https://github.com/mattingalls/Soundflower/releases/tag/2.0b2
2. Follow the instructions in the above link on how to create a "multi-output" device
3. Select that device in Audio MIDI Setup for Sound Output
4. Run dots, and drag the soundStreamDevice slider until SoundFlower (2ch) is selected
