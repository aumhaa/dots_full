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
//    right strip 33-37, 40     the modes: flames, milk, shader, sheep, lights,
//                              sets. A mode puts its parameters on the dials and
//                              its list on the grid (flames scenes, MilkDrop
//                              presets, shaders, sheep, set entries); in lights
//                              mode the grid places the selected light, keys
//                              108-111 pick it
//    16 dials (CC 0-15)        a page of the mode's parameters (ControlPages),
//                              with their names and values on the iPad
//    arrows 70/73, 71/72       up/down the dial pages, left/right the grid's;
//                              dim when they can't go further. 107 shows the
//                              dial page, 115 the grid's
//    left column               mixer dials (15), crossfader (CC 17), engine A/B
//                              (11, 32), flames, MilkDrop, shader and sheep
//                              on/off (16-19), lights A-C on/off (12-14)
//    buttons 92-99             previous/next track, scene, MilkDrop, shader
//    right strip 41            Modul8's mode: the dials and the two rows of
//                              buttons above the grid go to Modul8's modGrid
//                              module (modul8/modGrid.m8m) over the "dots
//                              modGrid" ports, as a Twister's knobs and buttons;
//                              Modul8's values and colours come back, the names
//                              from bin/data/modul8.json or the built-in ones
//
//  The ports chosen in the library's MIDI tab are kept in bin/data/midi.json.
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

    // The iPad's Control port ("" for the first modGrid one, a real iPad before the Simulator) and the
    // link to Modul8 ("" for dots's own virtual "dots modGrid" pair, else a pair such as an IAC bus);
    // each is the in port's name, its out port found by name
    const string &getSurfaceChoice() const { return surfaceChoice; }
    const string &getLinkChoice() const { return linkChoice; }
    void setSurfacePort(const string &name);
    void setLinkPort(const string &name);
    bool isLinked() const { return linked; }
    string getLinkName() const;
    // The system's ports, looked at every few seconds
    const vector<string> &inPorts() const { return portsIn; }
    string pairedOut(const string &inName) const;   // "<name> In" (the modGrid Bridge's), else the same name

    void newMidiMessage(ofxMidiMessage &message) override;

private:
    enum Browse { BROWSE_SETS, BROWSE_SHADERS, BROWSE_MILKDROP, BROWSE_SCENES, BROWSE_SHEEP, N_BROWSE };

    void connect();
    void disconnect();
    void connectLink();
    void disconnectLink();
    void savePorts();
    void handle(const ofxMidiMessage &message);
    void handleLink(const ofxMidiMessage &message);
    void press(int note);
    void pressPad(int row, int column);
    void refresh();                       // set the half view, and forget what was sent so everything goes again
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
    vector<ofxMidiMessage> received, receivedFromLink;   // from the MIDI threads, for update()

    // The link to Modul8, and what it last said of each dial and button
    struct LinkListener : public ofxMidiListener {
        ModGridSurface *owner = nullptr;
        void newMidiMessage(ofxMidiMessage &message) override;
    } link;
    ofxMidiIn linkIn;
    ofxMidiOut linkOut;
    bool linked = false;
    string surfaceChoice, linkChoice;
    vector<string> portsIn, portsOut;
    const string PORTS_FILE = "midi.json";
    struct Modul8Control { string name; int value = 0, state = 0; };   // a dial's 0-127, a button's Twister colour and state
    Modul8Control modul8Dials[16], modul8Buttons[16];

    int mode = 0, gridMode = 0, browse = BROWSE_SCENES;   // gridMode: the mode whose list the grid shows
    bool mixerShown = false;           // the mixer's dials, over the mode's
    int browsePage[N_BROWSE] = {};
    std::map<string, int> pageInGroup;   // group -> the page it's on
    vector<string> milkdropPresets;    // relative to the presets folder

    std::map<int, int> sentColors;     // (channel << 8 | note) -> palette index
    std::map<int, int> sentDials;      // cc -> value
    std::map<int, string> sentTexts;   // (type << 16 | channel << 8 | id) -> text
    int msgid = 0;
};
