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

### MilkDrop (projectM)

dots can mix MilkDrop presets over the flames, through libprojectM. The
library is built into `libs/projectM` and the presets and textures are fetched
into `bin/data/projectM` (not in git) by:

    sh scripts/build_projectm.sh

(needs CMake: `brew install cmake`). projectM is pinned to a master commit,
because drawing into our own framebuffer (`projectm_opengl_render_frame_fbo`)
arrives in projectM 4.2, which isn't released yet.

MilkDrop hears the same audio as the flames (not in MP3 mode) and only renders
while its mix is above 0. The control window's MilkDrop panel has the mix,
seconds per preset, lock and shuffle, and previous/next preset (also `[` and
`]`). Over MIDI: CC 21 on channel 1 is the mix (sent back on DotsOutput like
the other parameters), notes 5 and 6 on channel 1 are previous/next preset.

### Pausing the engines

"flames running" (Meta panel, CC 25) and MilkDrop's "running" (CC 26) stop an
engine's processing; it holds its last frame. The flames also pause by
themselves while MilkDrop's mix is at 100%, unless feed B is showing them, and
the control window says "flames paused". "max fps" (Meta panel, 30 by default)
caps the frame rate, so a paused engine frees the CPU rather than speeding the
loop up: the flames are about 85% of a core, MilkDrop about 5%.

### Spotlights

Up to four spotlights sit over the final picture (`src/LightingLayer`). Each
has on, mode (add: lays coloured light on top; reveal: lights the picture
under it while everything outside the reveal beams drops to the ambient
level), intensity, hue, saturation, x, y, size, focus (edge hardness) and
frost (diffusion). They're set in the control window's Lights panel, or by
dragging on the preview: a click near a light grabs it, anywhere else moves
the selected light there.

The lighting is its own layer, apart from tracks and scenes, saved in
numbered snapshots (`bin/data/lights/snapshot-N.xml`) with the panel's slot,
Save and Load. It isn't loaded at startup: load the venue's snapshot.

Over MIDI (channel 1): CC 22 ambient, CC 23 lights on feed A, CC 24 feed B's
source, and CC 32-71 the lights, ten each from 32, 42, 52 and 62 in the order
above.

### Syphon feeds and the viewer

- **dots**: the final picture, with or without the lights (the Lights panel's
  "lights on feed A").
- **dots B**: a top layer with alpha (the amount of light, colour not
  premultiplied): the flames masked by the light, MilkDrop masked by the light
  (MilkDrop keeps rendering for it even at mix 0), or the light alone. Fully
  transparent where no light falls.

The viewer window starts hidden and its close button only hides it; dots
keeps rendering for the preview and Syphon. Show it with "show viewer (v)" in
the Meta panel or the `v` key. Closing the control window quits dots. The
output resolution follows the viewer window's size (1080x720 by default).

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
