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

### Engines and the mixer

dots has four engines: the flames, MilkDrop, the shader engine (ISF and
Synesthesia) and Electric Sheep. The Mixer panel puts one on A and one on B
(0 flames, 1 MilkDrop, 2 shader, 3 sheep; CC 27 and CC 28) with a crossfade
between them (CC 21). The two are added, each on a curve set by "xfade centre
level": the level each side has with the crossfade in the middle. 0.5 is a
plain linear mix, which dims in the middle; the default 0.75 keeps the middle
about as bright as the ends (bright areas of both can clip to white). An engine only works while
it's running and something shows it (the mixer, the lights' feed B, or for
the shader its own Syphon feed); otherwise it holds its last frame.

### Electric Sheep

The sheep engine renders Electric Sheep live on the GPU from their genomes:
the 1,708 sheep in `bin/data/genebank2.flam3` (the flock's genebank, loaded
for the flames too). Each turns as a sheep loop does, flam3 rotating its
xforms a full circle every loop. `src/FlameRenderer.cpp` runs flam3's chaos
game on a flock of points in a vertex shader (transform feedback keeps each
point between passes), adds every step to a floating-point histogram that fades
a little each frame, and tone-maps it as flam3 does (log density, gamma,
vibrancy). The variations the genebank uses are ported from
`src/flam3/variations.c`; any other is drawn as linear, and the panel's status
says how many a sheep has.

As the screensaver does, it moves from sheep to sheep by morphing: flam3
interpolates the two genomes, both still turning, over the morph time (0 cuts).
A sheep chosen by hand morphs in; with auto advance on, the next comes after so
many loops, in order or shuffled. The music pushes it: the overall level speeds
the turning, bass twists the xforms further and pulses the zoom, the level
brightens it, and the high end shifts the colours along the palette, each with
its own amount (0 turns it off).

The Sheep panel (and sheep mode's dials on the modGrid): the sheep, loop
seconds, morph seconds, loops a sheep, auto advance, shuffle, the flock
(points, in thousands), passes (steps a frame), trail (how much of the picture
a frame keeps), exposure, gamma, and the five audio amounts.

`DOTS_TEST_SHEEP=<folder>` renders a spread of sheep (`DOTS_TEST_SHEEP_COUNT`,
24 by default) into the folder as PNGs, logs each one's time a frame and
unsupported variations, and quits. `DOTS_TEST_SHEEP_MORPH=<folder>` saves eight
steps of a morph (`DOTS_TEST_SHEEP_FROM` to `DOTS_TEST_SHEEP_TO`).

### Flames block locks

The Locks panel (under Tracks and Scenes) has a check for each block of the
flames' scene: audio analysis, drawing, speed, dots, lines, audio effects
(CC 72-77). A locked block keeps its values through wandering, scene and track
changes, fades and flames presets; it can still be changed by hand, and its
values aren't written into the scenes, which keep their own.

### MilkDrop (projectM)

dots runs MilkDrop presets through libprojectM. The
library is built into `libs/projectM` and the presets and textures are fetched
into `bin/data/projectM` (not in git) by:

    sh scripts/build_projectm.sh

(needs CMake: `brew install cmake`). projectM is pinned to a master commit,
because drawing into our own framebuffer (`projectm_opengl_render_frame_fbo`)
arrives in projectM 4.2, which isn't released yet.

MilkDrop hears the same audio as the flames. The control
window's MilkDrop panel has running, seconds per preset, lock and shuffle, and
previous/next preset (also `[` and `]`, and notes 5 and 6 on channel 1).

### Shaders (ISF)

The shader engine runs ISF generators from `/Library/Graphics/ISF`,
`~/Library/Graphics/ISF` and `bin/data/shaders`: single-pass shaders that make
their own picture. By default only the ones that animate by themselves are
offered; turn off "animated only" in the Shader panel to add the static
patterns. Filters, transitions and multi-pass shaders aren't run yet.

Shaders with audio inputs (`audio`, `audioFFT`) get dots's audio input, the
same signal the flames hear, and count as animated.

The Shader panel has running (CC 29) and previous/next shader (`<` and `>`,
notes 7 and 8); the shader's own inputs get a panel of their own, rebuilt for
each shader. A shader reloads when its file changes, so it can be edited live.
Its output is also published on its own Syphon feed, "dots shader".

### Synesthesia scenes

The shader engine also runs Synesthesia scenes (`.synScene` folders), read
in place from Synesthesia's own scene folder (its "primary scene folder"
preference), the scenes that come with Synesthesia in
`/Applications/Synesthesia.app`, and `bin/data/synesthesia`. They're listed
with the ISF shaders (hover one in the library to see its preview); "kinds:
all/ISF/syn" in the Shader panel narrows the list that previous/next steps
through.

A scene's passes run with their own buffers, which keep their last frame, so
feedback scenes work; its controls become the shader inputs panel (smooth,
speed, bang and counter controls behave as in Synesthesia), and its images
are loaded. dots supplies Synesthesia's built-in functions itself
(`src/SynLibrary.h`), and works out the audio uniforms (`syn_BassLevel`,
`syn_OnBeat`, `syn_BPM` and the rest) from its own audio input
(`src/SynAudio.cpp`).

A scene's `script.js` runs each frame before its passes, in JavaScriptCore
(part of macOS): it sees the scene's inputs, can change its controls, and
what it sets becomes uniforms (`src/SynScript.cpp`). Synesthesia's meta
controls (`meta/brightness` and so on) aren't in dots, so scripts setting them
do nothing; what scripts print goes to dots's log.

**Media** (Synesthesia's `syn_UserImage`): a scene can be given the flames,
MilkDrop, a Syphon feed, or an image or video file. Pick one in the library's
Files tab under "Media for Synesthesia scenes" (it lists Synesthesia's media
folders, from its own settings, and `bin/data/media`), or the kind with
"media (0-4)" in the Shader panel: none, flames, MilkDrop, Syphon, file. The
choice is saved in shader presets, and an engine used as media keeps running.
As in Synesthesia, media only shows in scenes written to read it (about a
third: the library marks them "[Synesthesia, media]", and the Shader panel
says "unused:" when the scene running ignores it); Raw Video shows it as it is.

"alpha out" in the Shader panel is Synesthesia's Alpha Channel: the shader's
own alpha goes out with its picture (on the "dots shader" feed, say), where
otherwise it's opaque. Many scenes write alpha 0 where they have nothing to
show, so they come out transparent there.

Not yet: the song-change transitions (each picks a value when the scene
loads).

To check every scene, run dots from Terminal with `DOTS_TEST_SCENES=1`
(`=all` adds the ISF shaders): it compiles and runs each for 30 frames with a
test signal, prints a line for each, and quits. `DOTS_TEST_SHOTS=<folder>`
also saves the last frame of each and contact sheets of them all.

    DOTS_TEST_SCENES=1 bin/dots.app/Contents/MacOS/dots | grep scene-test

`DOTS_TEST_ONLY=<text>` tests only the scenes whose names contain it, and
`DOTS_TEST_MEDIA=<file>` (or `syphon:<app>/<server>`) gives them media. With a
video or Syphon feed the frames it saves can come out wrong, as macOS has to
be let run during the test; trust the scene lines, and check the pictures in
dots itself. `DOTS_TEST_AUDIO_DEVICE=<name>` opens that audio input as dots
would for 15 s and quits, for checking what an input does to other gear.

### The library: presets and files

The "dots library" window (ImGui; "show library (l)" in the Meta panel, its
close button only hides it) has two tabs.

**Presets**: presets for each module (flames, MilkDrop, shader, lights) and
global ones (the mixer plus every module; lights only with "save with
lights", since they're usually per venue), saved as JSON in
`bin/data/presets/<module>/<name>.json`. Double-click to recall; Save new,
Overwrite and Delete (which moves the file to a `deleted/` folder beside it).

**Files**: flames tracks and their scenes, every MilkDrop preset (with
search), every shader (static ones marked), the light snapshots, and buttons
that open each folder in Finder.

### Sets

A set is an ordered list of global presets, each with its own fade or the
default preset fade, in `bin/data/sets/<name>.json`. The library's Sets tab
opens, makes and deletes sets, adds global presets (or saves what's running
as a new one and adds it), reorders and removes entries, and steps through:
previous/next, or double-click an entry. Changes save at once. Over MIDI,
notes 9 and 10 on channel 1 step to the previous/next entry; the `-` and `=`
keys do the same.

### The control window's arrangement

The panels sit on a grid of panel-wide columns by default (the preview and the
audio box span the first two). Drag them anywhere, fold what you don't need,
size and place the window: dots keeps it all in `bin/data/layout.json` (saved a
moment after a change and on quitting, kept out of git) and comes back that
way. Delete the file to go back to the default.

### Pausing the engines

"flames running" (Meta panel, CC 25), MilkDrop's "running" (CC 26) and the
shader's (CC 29) stop an engine's processing; it holds its last frame. Engines
nothing shows pause by themselves (see the mixer), and the control window says
"flames paused" / "shader paused". "max fps" (Meta panel, 30 by default)
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
  premultiplied): the flames, MilkDrop or the shader masked by the light
  (whichever it shows keeps rendering for it), or the light alone. Fully
  transparent where no light falls.
- **dots shader**: the shader engine's own output, with the shader's alpha
  when "alpha out" is on.

The viewer window starts hidden and its close button only hides it; dots
keeps rendering for the preview and Syphon. Show it with "show viewer (v)" in
the Meta panel or the `v` key. Closing the control window quits dots. The
output resolution follows the viewer window's size (1080x720 by default).

### modGrid (the iPad app), without Live

With "modGrid" on in the Meta panel, dots drives the modGrid iPad app itself,
in place of Live's ModGrid script (`src/ModGridSurface.cpp`). It opens the
modGrid Bridge's Control ports for the iPad (`modGrid - <iPad> - Control` and
`... In`; the Simulator's `modGrid Sim` pair if no iPad is there), so **Live's
ModGrid script must not be bound to the same port** while dots has it. The
Bridge app has to be running; dots looks for the port every few seconds.
`DOTS_MODGRID_PORT=<text>` picks a port by name until one is chosen in the
library's MIDI tab.

- **Modes** (the right strip: flames, milk, shader, sheep, lights, then sets
  above Modul8). A mode puts its parameters on the 16 dials (names and values
  on the iPad) and its list on the grid: flames scenes, MilkDrop presets,
  shaders, sheep, set entries, 64 to a page, each pad with a short name, the current item
  white; press a pad to load it. In **lights** mode the grid is the picture:
  press a pad to move the selected light there; the four buttons above the
  grid's left half pick which light, and each light shows as its letter.
- **Switches**: a dial page's on/off settings and triggers (the flames locks
  and wandering, MilkDrop's lock, shuffle and hard cuts, a shader's toggles and
  bangs) sit on the row of buttons under the dials, lit while on, instead of
  on dials; a shader takes as many pages as its dials or switches need.
- **Arrows** (right strip): up/down step through the mode's dial pages,
  left/right page the grid; each dims when it can't go further. The button
  over the last key shows the dial page ("Flames 1/2"), the last key the grid's
  page ("milk 34/154").
- **Left column**: mixer (top button: the mixer's dials over the mode's,
  pressed again to go back), the crossfader (top dial), engine A and B (each
  press steps it), flames, MilkDrop, shader and sheep on/off, then lights A-C
  on/off, lit while on; light D's switch is in lights mode, next to its key.
- **Buttons above the dials**: track, scene, MilkDrop and shader previous/next.
  In **sets** mode the first two keys step through the set, dim at either end.
- **Modul8** (the right column's lower button) is a sixth mode: the 16 dials
  and the two rows of buttons above the grid go to Modul8, while the left
  column, the modes and the grid (kept from the mode before) stay with dots.
  dots passes them on its own virtual `dots modGrid` ports, as a Midi Fighter
  Twister's knobs (CC 0-15, channel 1) and buttons (CC 0-15, channel 2), and
  shows what comes back: dial values, and button colours as Twister colour
  values. Modul8 runs `modul8/modGrid.m8m` (copy it into
  `/Applications/modul8v3/Modules`; pick `dots modGrid` in its list). Each
  button sits under its dial. Dials: crossfade, alpha 1, alpha 2, -,
  transition, saturation, lightness, contrast / speed, alpha 3, alpha 4, -,
  scale, red, green, blue. Buttons: bank A or B (layers 1-4 or 6-9), layer 1,
  layer 2, -, invert, saturation, lightness and contrast on / -, layer 3,
  layer 4, -, flash, red, green and blue additive. Modul8 can't send the
  names, so dots holds them: `bin/data/modul8.json` (`{"dials": [16 names],
  "buttons": [16 names]}`) replaces the built-in ones when the module's layout
  changes; a name like `A|B` shows the form the button's state (CC on
  channel 3 from the module) picks.

On connecting, and whenever the app asks for everything, dots puts the app in
its half view (the 8x8 grid this layout is for).

The library's **MIDI** tab picks the iPad's port (or the first modGrid port, an
iPad before the Simulator) and the Modul8 link (dots's own ports, or a pair
such as an IAC bus); the choices are kept in `bin/data/midi.json`.

The MilkDrop page has projectM's own settings too (also in the MilkDrop panel):
transition time, beat sensitivity, hard cuts on a loud beat with their
sensitivity and minimum interval, the warp mesh size and aspect correction.

The pages come from `src/ControlPages.cpp`, which is meant to grow into the
parameter layer every UI shares.

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
