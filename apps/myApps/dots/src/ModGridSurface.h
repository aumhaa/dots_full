//
//  ModGridSurface.h
//  dots
//
//  Drives the modGrid iPad app directly, in place of Live's ModGrid control
//  surface script: it opens the modGrid Bridge's Control port pair for the
//  iPad ("modGrid - <iPad> - Control" / "... In", or the Simulator's "modGrid
//  Sim" pair) and speaks the same protocol (docs in the modGrid repo:
//  modgrid-script-structure.md and protocol-mpe-v1.md).
//
//    right strip 33-37         the modes: flames, milk, shader, lights, sets. A
//                              mode puts its parameters on the dials and its
//                              list on the grid (flames scenes, MilkDrop presets,
//                              shaders, set entries); in lights mode the grid
//                              places the selected light, keys 108-111 pick it
//    16 dials (CC 0-15)        a page of the mode's parameters (ControlPages),
//                              with their names and values on the iPad
//    arrows 70/73, 71/72       up/down the dial pages, left/right the grid's;
//                              dim when they can't go further. 107 shows the
//                              dial page, 115 the grid's
//    left column               mixer dials (15), crossfader (CC 17), engine A/B
//                              (11, 32), lights A-D on/off (16-19), flames,
//                              MilkDrop and shader on/off (12-14)
//    buttons 92-99             previous/next track, scene, MilkDrop, shader
//    right strip 40/41         previous/next in the set
//
//  Main controls are on channel 15 and the grid on 13 and 14 (0-based). Colours
//  are modGrid palette numbers, sent as the velocity of a note back on the
//  control's own note. Live's ModGrid script must not be bound to the same port.
//

#pragma once

#include "ofMain.h"
#include "ofxMidi.h"
#include "ControlPages.h"
#include <mutex>

class ofApp;

class ModGridSurface : public ofxMidiListener {
public:
    ~ModGridSurface();
    void setup(ofApp *app);

    // Main thread, once a frame: connects or disconnects as asked, acts on what
    // the iPad sent, and sends what changed
    void update(bool enabled);

    bool isConnected() const { return connected; }
    const string &getPortName() const { return portName; }

    void newMidiMessage(ofxMidiMessage &message) override;

private:
    enum Browse { BROWSE_SETS, BROWSE_SHADERS, BROWSE_MILKDROP, BROWSE_SCENES, N_BROWSE };

    void connect();
    void disconnect();
    void handle(const ofxMidiMessage &message);
    void press(int note);
    void pressPad(int row, int column);
    void refresh();                       // forget what was sent, so everything goes again
    void sendFeedback();
    void sendCaps();

    // The browse list: how many items, which is current, and loading one
    int browseCount() const;
    int browsePageCount() const;
    int browseCurrent() const;
    string browseName(int item) const;
    void browseLoad(int item);
    void scanMilkdrop();
    void selectMode(int which);
    // The dial pages on show: their group, its pages, which of them, and that page's index in pages
    string dialGroup() const;
    vector<int> groupPages() const;
    int groupPosition() const;
    int currentPage() const;

    // Sending, each only when it changed since it was last sent
    void sendColor(int channel, int note, int color);
    void sendDial(int cc, int value);
    void sendText(int type, int channel, int id, const string &text);

    ofApp *app = nullptr;
    ControlPages pages;
    ofxMidiIn in;
    ofxMidiOut out;
    bool connected = false;
    string portName;
    float lastScan = -10, lastSend = 0;

    std::mutex mutex;
    vector<ofxMidiMessage> received;   // from the MIDI thread, for update()

    enum { N_MODES = 5 };
    int mode = 0, browse = BROWSE_SCENES;
    bool mixerShown = false;           // the mixer's dials, over the mode's
    int browsePage[N_BROWSE] = {0, 0, 0, 0};
    std::map<string, int> pageInGroup;   // group -> the page it's on
    vector<string> milkdropPresets;    // relative to the presets folder

    std::map<int, int> sentColors;     // (channel << 8 | note) -> palette index
    std::map<int, int> sentDials;      // cc -> value
    std::map<int, string> sentTexts;   // (type << 16 | channel << 8 | id) -> text
    int msgid = 0;
};
