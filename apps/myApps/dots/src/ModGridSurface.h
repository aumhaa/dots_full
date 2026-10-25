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
//    16 dials (CC 0-15)        a page of dots's parameters (ControlPages), with
//                              their names and values shown on the iPad
//    8 keys (notes 0-7)        pick the page
//    buttons 92-99, 100-107    previous/next and on/off switches
//    buttons 108-115           what the grid browses, and its page
//    the 8x8 grid              browse: set entries, shaders, MilkDrop presets,
//                              flames scenes; the right strip shows names
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
    int browseCurrent() const;
    string browseName(int item) const;
    void browseLoad(int item);
    void scanMilkdrop();

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

    int page = 0, browse = BROWSE_SHADERS;
    int browsePage[N_BROWSE] = {0, 0, 0, 0};
    vector<string> milkdropPresets;    // relative to the presets folder

    std::map<int, int> sentColors;     // (channel << 8 | note) -> palette index
    std::map<int, int> sentDials;      // cc -> value
    std::map<int, string> sentTexts;   // (type << 16 | channel << 8 | id) -> text
    int msgid = 0;
};
