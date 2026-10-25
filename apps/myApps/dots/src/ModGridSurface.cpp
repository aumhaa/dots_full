//
//  ModGridSurface.cpp
//  dots
//

#include "ModGridSurface.h"
#include "ofApp.h"

// ofxMidi channels are 1-based: main controls on 16, the grid on 14 (pads 0-127) and 15 (128-255)
static const int MAIN_CHANNEL = 16, PAD_CHANNEL_LOW = 14, PAD_CHANNEL_HIGH = 15;
static const vector<unsigned char> sysexHeader = {0xF0, 0x00, 0x01, 0x61, 0x5D, 0x5D};
static const int SYSEX_BUTTON_TEXT = 0, SYSEX_DIAL_NAME = 1, SYSEX_DIAL_VALUE = 2;
static const int SYSEX_REQUEST_UPDATE = 0x09, SYSEX_CAPS = 0x45, SYSEX_STATE_REQUEST = 0x46;
// From the script, 0x09 with a value sets the app's view; 1 is the half view (the 8x8 grid)
static const int SYSEX_SET_VIEW = 0x09, VIEW_HALF = 1;

// modGrid palette numbers (its docs/palette.json) for each section: bright, then dim
struct SectionColor { const char *section; int bright, dim; };
static const SectionColor sectionColors[] = {
    {"Wander", 87, 90},         {"Audio Analysis", 72, 80}, {"Drawing", 71, 85},   {"Speed", 74, 84},
    {"Dots", 75, 81},           {"Lines", 76, 79},          {"Audio Effects", 13, 99},
    {"MilkDrop", 16, 83},       {"Lights", 26, 118},        {"Shader", 86, 82},    {"Mixer", 14, 96},
    {"Sets", 94, 89},           {"Scenes", 71, 85},         {"Modul8", 22, 114},        {"Sheep", 5, 98},
};
static const int COLOR_OFF = 0, COLOR_WHITE = 14;

static int sectionColor(const string &section, bool bright) {
    for (const SectionColor &c : sectionColors) {
        if (section == c.section) return bright ? c.bright : c.dim;
    }
    return bright ? COLOR_WHITE : 96;
}

// The buttons with a fixed job: label and colour, lit (or dim) as sendFeedback() decides
struct SurfaceButton { int note; const char *label, *section; };
static const SurfaceButton buttons[] = {
    // Above the dials
    {92, "track <", "Drawing"},   {93, "track >", "Drawing"},   {94, "scene <", "Drawing"},  {95, "scene >", "Drawing"},
    {96, "milk <", "MilkDrop"},   {97, "milk >", "MilkDrop"},   {98, "shader <", "Shader"},  {99, "shader >", "Shader"},
    // Left column: the mixer's dials, the lights and the engines on or off
    {15, "mixer", "Mixer"},
    {16, "light A", "Lights"},    {17, "light B", "Lights"},    {18, "light C", "Lights"},   {19, "light D", "Lights"},
    {12, "flames", "Drawing"},    {13, "MilkDrop", "MilkDrop"}, {14, "shader", "Shader"},
    // Right column (its mode buttons are the modes' own): the arrows, up/down the dial pages, left/right the grid's
    {70, "^", "Mixer"},           {73, "v", "Mixer"},           {71, "<", "Mixer"},          {72, ">", "Mixer"},
};
static const int MIXER_NOTE = 15;
static const int UP_NOTE = 70, LEFT_NOTE = 71, RIGHT_NOTE = 72, DOWN_NOTE = 73;
// Left column: the engine on each side of the mixer (a press steps it), and the crossfader dial
static const int ENGINE_A_NOTE = 11, ENGINE_B_NOTE = 32, CROSSFADE_CC = 17, SPARE_CC = 16;
// The two rows above the grid (100-115): Modul8's 16 buttons in its mode. Otherwise the dial page's
// switches (100-106), lit while on, the dial page (over the last key) and the grid's page (the last
// key); in lights mode the first four keys pick the light the grid moves, in sets mode the first two
// step through the set; the rest blank
static const int FIRST_ROW_NOTE = 100, DIAL_PAGE_NOTE = 107, GRID_PAGE_NOTE = 115, FIRST_KEY = 108;
static const char *engineNames[] = {"flames", "MilkDrop", "shader", "sheep"};
static const char *engineSections[] = {"Drawing", "MilkDrop", "Shader", "Sheep"};
// Each mode: its button and label, its dial pages, what its grid browses (-1: the grid places the
// selected light) and its name there, and its colour. Modul8's has no dial pages or list: its dials
// and buttons go to Modul8, and the grid keeps the mode before it
struct SurfaceMode { int note; const char *label, *group, *name, *section; int browse; };
static const SurfaceMode modes[] = {
    {33, "flames", "flames", "scene", "Drawing", 3},     {34, "milk", "milk", "milk", "MilkDrop", 2},
    {35, "shader", "shader", "shader", "Shader", 1},     {36, "lights", "lights", "light", "Lights", -1},
    {40, "sets", "sets", "sets", "Sets", 0},             {41, "modul8", "modul8", "modul8", "Modul8", -1},
    {37, "sheep", "sheep", "sheep", "Sheep", 4},
};
static const int N_MODES = sizeof(modes) / sizeof(modes[0]);
static const int MODE_LIGHTS = 3, MODE_SETS = 4, MODE_MODUL8 = 5;

// The link to Modul8 (its modGrid module): dials as CC 0-15 on channel 1 and buttons as CC 0-15 on
// channel 2 (127 pressed, 0 released), as a Midi Fighter Twister sends them. Modul8 answers with dial
// values on channel 1, button colours on channel 2 (Twister colour values, 0 off) and, for a button
// named two ways ("A|B"), its state on channel 3; CC 127 on channel 1 asks it for everything.
static const string LINK_VIRTUAL_NAME = "dots modGrid";
static const int LINK_DIAL_CHANNEL = 1, LINK_BUTTON_CHANNEL = 2, LINK_STATE_CHANNEL = 3, LINK_REFRESH_CC = 127;
// A Twister colour value 1-127 is a hue, blue at 0 round through green, yellow and red to blue at 127;
// these are the brightest modGrid palette colours nearest each 15 degrees of hue
static const int hueColors[24] = {127, 16, 16, 94, 88, 5, 5, 6, 6, 6, 7, 7, 78, 72, 76, 77, 77, 87, 87, 73, 86, 27, 27, 74};
static int twisterColor(int value) {
    if (value <= 0) return COLOR_OFF;
    float hue = std::fmod(240 - value * 360.0f / 127 + 720, 360);
    return hueColors[(int)std::round(hue / 15) % 24];
}
// What the modGrid module does with each dial and button (each button under its dial), for their names
// on the iPad; bin/data/modul8.json ({"dials": [16 names], "buttons": [16 names]}) replaces them when the
// module changes. A name with two forms ("A|B") shows the first while the button's state is off.
static const char *modul8DialNames[16] = {
    "crossfade", "alpha 1", "alpha 2", "", "transition", "saturation", "lightness", "contrast",
    "speed", "alpha 3", "alpha 4", "", "scale", "red", "green", "blue"};
static const char *modul8ButtonNames[16] = {
    "A|B", "layer 1", "layer 2", "", "invert", "sat on", "light on", "contr on",
    "", "layer 3", "layer 4", "", "flash", "red+", "green+", "blue+"};
// A pad shows its item's name, wrapped onto lines; this many letters at most
static const int PAD_TEXT_LENGTH = 32;

// An item's name short enough for a pad: no folders or extension, tighter joins, and a long one
// kept at both ends, since neighbours in a sorted list share a beginning and differ at the end
static string padName(string name) {
    name = ofFilePath::getFileName(name);
    std::size_t dot = name.rfind('.');
    if (dot != string::npos && dot > 0 && name.size() - dot <= 5) name = name.substr(0, dot);
    ofStringReplace(name, "_", " ");
    ofStringReplace(name, " - ", "-");
    ofStringReplace(name, " + ", "+");
    while (name.find("  ") != string::npos) ofStringReplace(name, "  ", " ");
    std::size_t first = name.find_first_not_of(" $-"), last = name.find_last_not_of(' ');
    name = first == string::npos ? "" : name.substr(first, last - first + 1);
    if ((int)name.size() > PAD_TEXT_LENGTH) {
        int head = (PAD_TEXT_LENGTH - 2) / 2;
        name = name.substr(0, head) + ".." + name.substr(name.size() - (PAD_TEXT_LENGTH - 2 - head));
    }
    return name;
}

ModGridSurface::~ModGridSurface() {
    disconnect();
}

void ModGridSurface::setup(ofApp *dots) {
    app = dots;
    msgid = ofRandom(128);   // a fresh start never reuses the last session's ids
    pages.setup(app);
    link.owner = this;

    // The ports chosen in the library; DOTS_MODGRID_PORT=text picks the iPad's until one is chosen
    // (a missing file loads as null)
    ofJson ports = ofFile::doesFileExist(PORTS_FILE) ? ofLoadJson(PORTS_FILE) : ofJson::object();
    const char *wanted = getenv("DOTS_MODGRID_PORT");
    surfaceChoice = ports.is_object() ? ports.value("modgrid", wanted ? string(wanted) : "") : "";
    linkChoice = ports.is_object() ? ports.value("modul8", "") : "";

    ofJson names = ofFile::doesFileExist("modul8.json") ? ofLoadJson("modul8.json") : ofJson::object();
    for (int i = 0; i < 16; ++i) {
        modul8Dials[i].name = modul8DialNames[i];
        modul8Buttons[i].name = modul8ButtonNames[i];
        for (auto [key, controls] : {std::pair<const char *, Modul8Control *>{"dials", modul8Dials}, {"buttons", modul8Buttons}}) {
            if (names.is_object() && names.contains(key) && i < (int)names[key].size() && names[key][i].is_string()) {
                controls[i].name = names[key][i].get<string>();
            }
        }
    }
}

void ModGridSurface::newMidiMessage(ofxMidiMessage &message) {
    std::lock_guard<std::mutex> lock(mutex);
    received.push_back(message);
}

void ModGridSurface::LinkListener::newMidiMessage(ofxMidiMessage &message) {
    std::lock_guard<std::mutex> lock(owner->mutex);
    owner->receivedFromLink.push_back(message);
}

// The out port that goes with an in port: the modGrid Bridge's "<name> In", else one of the same name
string ModGridSurface::pairedOut(const string &inName) const {
    for (const string &o : portsOut) if (o == inName + " In") return o;
    for (const string &o : portsOut) if (o == inName) return o;
    return "";
}

void ModGridSurface::setSurfacePort(const string &name) {
    surfaceChoice = name;
    disconnect();
    lastScan = -10;
    savePorts();
}

void ModGridSurface::setLinkPort(const string &name) {
    linkChoice = name;
    disconnectLink();
    lastScan = -10;
    savePorts();
}

void ModGridSurface::savePorts() {
    ofSavePrettyJson(PORTS_FILE, ofJson{{"modgrid", surfaceChoice}, {"modul8", linkChoice}});
}

// The Control pair: the one chosen, else the Bridge's "modGrid - <iPad> - Control" (a real iPad
// first), else the Simulator's "modGrid Sim"
void ModGridSurface::connect() {
    const vector<string> &ins = portsIn;
    int best = -1, bestScore = 0;
    for (int i = 0; i < (int)ins.size(); ++i) {
        const string &name = ins[i];
        int score = 0;
        if (!surfaceChoice.empty()) {
            score = name == surfaceChoice ? 3 : name.find(surfaceChoice) != string::npos ? 1 : 0;   // a typed part of the name
        } else if (name.find("modGrid") != string::npos && name.find("MPE") == string::npos) {
            score = name.find(" - Control") != string::npos ? 2 : name.find("modGrid Sim") != string::npos ? 1 : 0;
        }
        if (score > bestScore) {
            best = i;
            bestScore = score;
        }
    }
    if (best < 0) return;
    string outName = pairedOut(ins[best]);
    if (outName.empty()) return;
    in.ignoreTypes(false, true, true);   // SysEx in: the app's requests
    if (!in.openPort(ins[best]) || !out.openPort(outName)) {
        disconnect();
        return;
    }
    in.addListener(this);
    connected = true;
    portName = ins[best];
    ofLogNotice("ModGridSurface") << "Connected to " << portName;
    refresh();
    sendCaps();   // one unsolicited CAPS, as a script does on start, so the app asks for everything
}

// The link to Modul8: dots's own virtual "dots modGrid" pair, or a pair chosen in the library (an IAC bus)
void ModGridSurface::connectLink() {
    bool opened = linkChoice.empty()
        ? linkIn.openVirtualPort(LINK_VIRTUAL_NAME) && linkOut.openVirtualPort(LINK_VIRTUAL_NAME)
        : !pairedOut(linkChoice).empty() && linkIn.openPort(linkChoice) && linkOut.openPort(pairedOut(linkChoice));
    if (!opened) {
        disconnectLink();
        return;
    }
    linkIn.addListener(&link);
    linked = true;
    ofLogNotice("ModGridSurface") << "Modul8 link on " << getLinkName();
}

void ModGridSurface::disconnectLink() {
    if (linkIn.isOpen()) {
        linkIn.removeListener(&link);
        linkIn.closePort();
    }
    if (linkOut.isOpen()) linkOut.closePort();
    linked = false;
}

string ModGridSurface::getLinkName() const {
    return linkChoice.empty() ? LINK_VIRTUAL_NAME + " (dots's own)" : linkChoice;
}

void ModGridSurface::disconnect() {
    if (in.isOpen()) {
        in.removeListener(this);
        in.closePort();
    }
    if (out.isOpen()) out.closePort();
    if (connected) ofLogNotice("ModGridSurface") << "Disconnected from " << portName;
    connected = false;
    portName.clear();
}

void ModGridSurface::update(bool enabled) {
    // Look at the ports every few seconds: for the library, and to notice the iPad's going (the Bridge
    // restarting, the iPad leaving) or coming
    float now = ofGetElapsedTimef();
    bool scan = now - lastScan > 3;
    if (scan) {
        lastScan = now;
        portsIn = in.getInPortList();
        portsOut = out.getOutPortList();
    }
    if (!enabled) {
        if (connected) disconnect();
        if (linked) disconnectLink();
        return;
    }
    if (scan) {
        if (!connected) {
            connect();
        } else if (std::find(portsIn.begin(), portsIn.end(), portName) == portsIn.end()) {
            disconnect();
        }
        if (!linked) connectLink();
    }

    vector<ofxMidiMessage> messages, fromLink;
    {
        std::lock_guard<std::mutex> lock(mutex);
        messages.swap(received);
        fromLink.swap(receivedFromLink);
    }
    for (const ofxMidiMessage &message : fromLink) handleLink(message);
    if (!connected) return;
    for (const ofxMidiMessage &message : messages) handle(message);

    pages.update();   // the shader's pages follow the shader; groupPosition() keeps within them
    if (now - lastSend >= 1 / 30.0f) {
        lastSend = now;
        sendFeedback();
    }
}

void ModGridSurface::handle(const ofxMidiMessage &message) {
    if (message.status == MIDI_SYSEX) {
        const vector<unsigned char> &bytes = message.bytes;
        if (bytes.size() < 8 || !std::equal(sysexHeader.begin(), sysexHeader.end(), bytes.begin())) return;
        int type = bytes[6];
        if (type == SYSEX_REQUEST_UPDATE || type == SYSEX_STATE_REQUEST) {
            sendCaps();
            refresh();
        } else if (type == SYSEX_CAPS) {
            refresh();
        }
        return;
    }
    if (message.status == MIDI_CONTROL_CHANGE && message.channel == MAIN_CHANNEL) {
        if (message.control < 16 && mode == MODE_MODUL8) {
            modul8Dials[message.control].value = message.value;
            if (linked) linkOut.sendControlChange(LINK_DIAL_CHANNEL, message.control, message.value);
        } else if (message.control < 16) {
            const vector<ControlEntry> &entries = pages.get()[currentPage()].entries;
            if (message.control < (int)entries.size()) entries[message.control].setNormalized(message.value / 127.0f);
        } else if (message.control == CROSSFADE_CC) {
            app->gui->crossfade = message.value / 127.0f;
        }
        return;
    }
    // Modul8's buttons go to it pressed and released (its toggles act on the release)
    bool noteOn = message.status == MIDI_NOTE_ON && message.velocity > 0;
    bool noteOff = message.status == MIDI_NOTE_OFF || (message.status == MIDI_NOTE_ON && message.velocity == 0);
    if ((noteOn || noteOff) && message.channel == MAIN_CHANNEL && mode == MODE_MODUL8 &&
        message.pitch >= FIRST_ROW_NOTE && message.pitch < FIRST_ROW_NOTE + 16) {
        if (linked) linkOut.sendControlChange(LINK_BUTTON_CHANNEL, message.pitch - FIRST_ROW_NOTE, noteOn ? 127 : 0);
        return;
    }
    if (noteOn) {
        if (message.channel == MAIN_CHANNEL) {
            press(message.pitch);
        } else if (message.channel == PAD_CHANNEL_LOW || message.channel == PAD_CHANNEL_HIGH) {
            int pad = message.pitch + (message.channel == PAD_CHANNEL_HIGH ? 128 : 0);
            pressPad(pad / 16, pad % 16);
        }
    }
}

// Modul8 answering: dial values on channel 1, button colours on channel 2
void ModGridSurface::handleLink(const ofxMidiMessage &message) {
    if (message.status != MIDI_CONTROL_CHANGE || message.control >= 16) return;
    if (message.channel == LINK_DIAL_CHANNEL) modul8Dials[message.control].value = message.value;
    if (message.channel == LINK_BUTTON_CHANNEL) modul8Buttons[message.control].value = message.value;
    if (message.channel == LINK_STATE_CHANNEL) modul8Buttons[message.control].state = message.value;
}

// The dial pages on show: the mixer's while it's up, else the mode's
string ModGridSurface::dialGroup() const {
    return mixerShown ? "mixer" : mode == MODE_MODUL8 ? "" : modes[mode].group;
}

vector<int> ModGridSurface::groupPages() const {
    vector<int> found;
    const vector<ControlPage> &all = pages.get();
    for (int i = 0; i < (int)all.size(); ++i) {
        if (all[i].group == dialGroup()) found.push_back(i);
    }
    return found;
}

int ModGridSurface::groupPosition() const {
    auto at = pageInGroup.find(dialGroup());
    int count = groupPages().size();
    return at == pageInGroup.end() ? 0 : ofClamp(at->second, 0, std::max(count - 1, 0));
}

int ModGridSurface::currentPage() const {
    vector<int> found = groupPages();
    return found.empty() ? 0 : found[groupPosition()];
}

int ModGridSurface::browsePageCount() const {
    return std::max(1, (browseCount() + 63) / 64);
}

void ModGridSurface::selectMode(int which) {
    mode = which;
    mixerShown = false;
    if (mode == MODE_MODUL8) {
        // Modul8 sends everything again, for the dials and buttons it now has
        if (linked) linkOut.sendControlChange(LINK_DIAL_CHANNEL, LINK_REFRESH_CC, 127);
        return;
    }
    gridMode = mode;
    if (modes[mode].browse < 0) return;
    browse = modes[mode].browse;
    if (browse == BROWSE_MILKDROP && milkdropPresets.empty()) scanMilkdrop();
    // Open on the page with the current item
    int current = browseCurrent();
    if (current >= 0) browsePage[browse] = current / 64;
}

void ModGridSurface::press(int note) {
    GuiApp &gui = *app->gui;
    LightingLayer &lighting = *app->lighting;
    bool lightsGrid = modes[gridMode].browse < 0;
    for (int m = 0; m < N_MODES; ++m) {
        if (note != modes[m].note) continue;
        selectMode(m);
        return;
    }
    // A switch on the dial page: a bool flips, a trigger fires
    int switchIndex = note - FIRST_ROW_NOTE;
    if (mode != MODE_MODUL8 && switchIndex >= 0 && switchIndex < ControlPage::SWITCHES_PER_PAGE) {
        const vector<ControlEntry> &switches = pages.get()[currentPage()].switches;
        if (switchIndex < (int)switches.size()) switches[switchIndex].setNormalized(switches[switchIndex].normalized() >= 0.5f ? 0 : 1);
        return;
    }
    if (mode == MODE_LIGHTS && note >= FIRST_KEY && note < FIRST_KEY + LIGHTING_N_LIGHTS) {
        lighting.selected = note - FIRST_KEY + 1;
        return;
    }
    if (mode == MODE_SETS && (note == FIRST_KEY || note == FIRST_KEY + 1)) {
        if (note == FIRST_KEY) app->presets->previousInSet();
        else app->presets->nextInSet();
        return;
    }
    switch (note) {
        case 92: app->stateManager->noteToSceneParams(0); break;   // previous track
        case 93: app->stateManager->noteToSceneParams(1); break;   // next track
        case 94: app->stateManager->noteToSceneParams(2); break;   // previous scene
        case 95: app->stateManager->noteToSceneParams(3); break;   // next scene
        case 96: app->projectM.previousPreset(); break;
        case 97: app->projectM.nextPreset(); break;
        case 98: app->shaderEngine->previous(); break;
        case 99: app->shaderEngine->next(); break;
        case 12: gui.flamesRunning = !gui.flamesRunning; break;
        case 13: gui.milkdropRunning = !gui.milkdropRunning; break;
        case 14: gui.shaderRunning = !gui.shaderRunning; break;
        case 16: case 17: case 18: case 19: {
            ofParameter<bool> &on = lighting.lights[note - 16].on;
            on = !on;
            break;
        }
        case ENGINE_A_NOTE: gui.engineA = (gui.engineA + 1) % N_ENGINES; break;
        case ENGINE_B_NOTE: gui.engineB = (gui.engineB + 1) % N_ENGINES; break;
        case MIXER_NOTE: mixerShown = !mixerShown; break;
        case UP_NOTE: pageInGroup[dialGroup()] = std::max(groupPosition() - 1, 0); break;
        case DOWN_NOTE: pageInGroup[dialGroup()] = std::min(groupPosition() + 1, (int)groupPages().size() - 1); break;
        case LEFT_NOTE: if (!lightsGrid) browsePage[browse] = std::max(browsePage[browse] - 1, 0); break;
        case RIGHT_NOTE: if (!lightsGrid) browsePage[browse] = std::min(browsePage[browse] + 1, browsePageCount() - 1); break;
    }
}

void ModGridSurface::pressPad(int row, int column) {
    if (row >= 8 || column >= 8) return;
    // Lights mode: the pad is where the selected light goes, the grid spanning the picture
    if (modes[gridMode].browse < 0) {
        LightingLayer &lighting = *app->lighting;
        Light &light = lighting.lights[(int)ofClamp(lighting.selected - 1, 0, LIGHTING_N_LIGHTS - 1)];
        light.x = ofLerp(light.x.getMin(), light.x.getMax(), (column + 0.5f) / 8);
        light.y = ofLerp(light.y.getMin(), light.y.getMax(), (row + 0.5f) / 8);
        return;
    }
    int item = browsePage[browse] * 64 + row * 8 + column;
    if (item < browseCount()) browseLoad(item);
}

int ModGridSurface::browseCount() const {
    switch (browse) {
        case BROWSE_SETS: return app->presets->setEntries.size();
        case BROWSE_SHADERS: return app->shaderEngine->getCount();
        case BROWSE_MILKDROP: return milkdropPresets.size();
        case BROWSE_SHEEP: return app->sheepEngine->getCount();
        case BROWSE_SCENES: {
            StateManager &sm = *app->stateManager;
            return sm.trackIdx >= 0 && sm.trackIdx < (int)sm.tracks.size() ? sm.tracks[sm.trackIdx].scenes.size() : 0;
        }
    }
    return 0;
}

int ModGridSurface::browseCurrent() const {
    switch (browse) {
        case BROWSE_SETS: return app->presets->setPosition;
        case BROWSE_SHADERS: return app->shaderEngine->getIndex();
        case BROWSE_MILKDROP: {
            string current = ofFilePath::makeRelative(app->projectM.getPresetsFolder(), app->projectM.getPresetPath());
            auto found = std::find(milkdropPresets.begin(), milkdropPresets.end(), current);
            return found == milkdropPresets.end() ? -1 : found - milkdropPresets.begin();
        }
        case BROWSE_SCENES: return app->stateManager->sceneIdx;
        case BROWSE_SHEEP: return app->sheepEngine->getIndex();
    }
    return -1;
}

string ModGridSurface::browseName(int item) const {
    if (item < 0 || item >= browseCount()) return "";
    switch (browse) {
        case BROWSE_SETS: return app->presets->setEntries[item].preset;
        case BROWSE_SHADERS: return app->shaderEngine->getName(item);
        case BROWSE_MILKDROP: return ofFilePath::getBaseName(milkdropPresets[item]);
        case BROWSE_SCENES: return "scene " + ofToString(item);
        case BROWSE_SHEEP: return app->sheepEngine->getName(item);
    }
    return "";
}

void ModGridSurface::browseLoad(int item) {
    switch (browse) {
        case BROWSE_SETS: app->presets->goTo(item); break;
        case BROWSE_SHADERS: app->shaderEngine->select(item); break;
        case BROWSE_MILKDROP:
            app->projectM.loadPresetFile(ofFilePath::join(app->projectM.getPresetsFolder(), milkdropPresets[item]), true);
            break;
        case BROWSE_SCENES: app->stateManager->selectScene(item); break;
        case BROWSE_SHEEP: app->sheepEngine->select(item); break;
    }
}

void ModGridSurface::scanMilkdrop() {
    string folder = app->projectM.getPresetsFolder();
    std::error_code ignored;
    for (auto &entry : std::filesystem::recursive_directory_iterator(folder, ignored)) {
        if (entry.path().extension() == ".milk") milkdropPresets.push_back(ofFilePath::makeRelative(folder, entry.path().string()));
    }
    std::sort(milkdropPresets.begin(), milkdropPresets.end());
}

void ModGridSurface::refresh() {
    sentColors.clear();
    sentDials.clear();
    sentTexts.clear();
    // The half view: the 8x8 grid this layout is for
    vector<unsigned char> bytes = sysexHeader;
    bytes.insert(bytes.end(), {(unsigned char)SYSEX_SET_VIEW, (unsigned char)VIEW_HALF, 0xF7});
    out.sendMidiBytes(bytes);
}

void ModGridSurface::sendFeedback() {
    const vector<ControlPage> &all = pages.get();
    bool modul8 = mode == MODE_MODUL8 && !mixerShown;
    vector<int> shownPages = groupPages();
    int position = groupPosition(), positions = shownPages.size();
    const ControlPage *current = shownPages.empty() ? nullptr : &all[shownPages[position]];

    // The dials: this page's parameters, or Modul8's
    for (int cc = 0; cc < 16; ++cc) {
        if (modul8) {
            const Modul8Control &dial = modul8Dials[cc];
            sendDial(cc, dial.value);
            sendText(SYSEX_DIAL_NAME, MAIN_CHANNEL - 1, cc, dial.name);
            sendText(SYSEX_DIAL_VALUE, MAIN_CHANNEL - 1, cc, dial.name.empty() ? "" : ofToString(dial.value / 127.0f, 2));
            continue;
        }
        bool used = current && cc < (int)current->entries.size();
        const ControlEntry *entry = used ? &current->entries[cc] : nullptr;
        sendDial(cc, used ? (int)std::round(entry->normalized() * 127) : 0);
        sendText(SYSEX_DIAL_NAME, MAIN_CHANNEL - 1, cc, used ? entry->label : "");
        sendText(SYSEX_DIAL_VALUE, MAIN_CHANNEL - 1, cc, used ? entry->valueText() : "");
    }

    // The left column's dials: the crossfader, and one spare
    GuiApp &gui = *app->gui;
    sendDial(CROSSFADE_CC, (int)std::round(gui.crossfade * 127));
    sendText(SYSEX_DIAL_NAME, MAIN_CHANNEL - 1, CROSSFADE_CC, "crossfade");
    sendText(SYSEX_DIAL_VALUE, MAIN_CHANNEL - 1, CROSSFADE_CC, ofToString(gui.crossfade.get(), 2));
    sendDial(SPARE_CC, 0);
    sendText(SYSEX_DIAL_NAME, MAIN_CHANNEL - 1, SPARE_CC, "");
    sendText(SYSEX_DIAL_VALUE, MAIN_CHANNEL - 1, SPARE_CC, "");

    // The buttons: switches lit while on, the mode lit, the arrows dim when they can't move
    LightingLayer &lighting = *app->lighting;
    PresetManager &presets = *app->presets;
    const SurfaceMode &grid = modes[gridMode];
    bool lightsGrid = grid.browse < 0;
    int count = lightsGrid ? 0 : browseCount(), currentItem = lightsGrid ? -1 : browseCurrent();
    int pageCount = lightsGrid ? 1 : browsePageCount();
    const char *dialSection = current ? current->section.c_str() : modes[mode].section;
    for (const SurfaceButton &button : buttons) {
        bool lit = true;
        const char *section = button.section;
        switch (button.note) {
            case 12: lit = gui.flamesRunning; break;
            case 13: lit = gui.milkdropRunning; break;
            case 14: lit = gui.shaderRunning; break;
            case 16: case 17: case 18: case 19: lit = lighting.lights[button.note - 16].on; break;
            case MIXER_NOTE: lit = mixerShown; break;
            case UP_NOTE: lit = position > 0; section = dialSection; break;
            case DOWN_NOTE: lit = position < positions - 1; section = dialSection; break;
            case LEFT_NOTE: lit = !lightsGrid && browsePage[browse] > 0; section = grid.section; break;
            case RIGHT_NOTE: lit = !lightsGrid && browsePage[browse] < pageCount - 1; section = grid.section; break;
        }
        sendColor(MAIN_CHANNEL, button.note, sectionColor(section, lit));
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, button.note, button.label);
    }
    int engines[] = {gui.engineA, gui.engineB}, engineNotes[] = {ENGINE_A_NOTE, ENGINE_B_NOTE};
    for (int side = 0; side < 2; ++side) {
        int engine = ofClamp(engines[side], 0, N_ENGINES - 1);
        sendColor(MAIN_CHANNEL, engineNotes[side], sectionColor(engineSections[engine], true));
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, engineNotes[side], string(side ? "B " : "A ") + engineNames[engine]);
    }
    for (int m = 0; m < N_MODES; ++m) {
        sendColor(MAIN_CHANNEL, modes[m].note, sectionColor(modes[m].section, mode == m));
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, modes[m].note, modes[m].label);
    }
    for (int key = 0; key < 8; ++key) {   // the keys, which this layout doesn't show
        sendColor(MAIN_CHANNEL, key, COLOR_OFF);
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, key, "");
    }

    // The two rows above the grid: Modul8's buttons in its mode; otherwise the mode's keys, then the
    // dial page and the grid's page
    int selected = ofClamp(lighting.selected - 1, 0, LIGHTING_N_LIGHTS - 1);
    for (int note = FIRST_ROW_NOTE; note < FIRST_ROW_NOTE + 16; ++note) {
        int color = COLOR_OFF, key = note - FIRST_KEY;
        string text;
        if (mode == MODE_MODUL8) {
            const Modul8Control &button = modul8Buttons[note - FIRST_ROW_NOTE];
            color = button.name.empty() && !button.value ? COLOR_OFF : button.value ? twisterColor(button.value) : sectionColor("Modul8", false);
            std::size_t bar = button.name.find('|');
            text = bar == string::npos ? button.name : button.state ? button.name.substr(bar + 1) : button.name.substr(0, bar);
        } else if (current && note - FIRST_ROW_NOTE < (int)current->switches.size()) {
            const ControlEntry &entry = current->switches[note - FIRST_ROW_NOTE];
            color = sectionColor(entry.section, entry.normalized() >= 0.5f);
            text = entry.label;
        } else if (note == DIAL_PAGE_NOTE && current) {
            color = sectionColor(current->section, true);
            text = all[shownPages.front()].name + " " + ofToString(position + 1) + "/" + ofToString(positions);
        } else if (note == GRID_PAGE_NOTE) {
            color = sectionColor(grid.section, true);
            text = lightsGrid ? "light " + string(1, 'A' + selected)
                              : string(grid.name) + " " + ofToString(browsePage[browse] + 1) + "/" + ofToString(pageCount);
        } else if (mode == MODE_LIGHTS && key >= 0 && key < LIGHTING_N_LIGHTS) {
            color = sectionColor("Lights", key == selected);
            text = "move " + string(1, 'A' + key);
        } else if (mode == MODE_SETS && (key == 0 || key == 1)) {
            bool canMove = key == 0 ? presets.setPosition > 0 && !presets.setEntries.empty()
                                    : presets.setPosition < (int)presets.setEntries.size() - 1;
            color = sectionColor("Sets", canMove);
            text = key == 0 ? "set prev" : "set next";
        }
        sendColor(MAIN_CHANNEL, note, color);
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, note, text);
    }

    // The grid: in lights mode, where the lights are (the selected one white, the others lit while on);
    // otherwise this page of the browse list, each pad with its item's name, the current item white.
    // Modul8's mode keeps the grid of the mode before it.
    int lightPads[LIGHTING_N_LIGHTS];
    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        Light &light = lighting.lights[i];
        int column = ofClamp((int)(ofMap(light.x, light.x.getMin(), light.x.getMax(), 0, 1) * 8), 0, 7);
        int row = ofClamp((int)(ofMap(light.y, light.y.getMin(), light.y.getMax(), 0, 1) * 8), 0, 7);
        lightPads[i] = row * 8 + column;
    }
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            int note = row * 16 + column, color = COLOR_OFF;
            string text;
            if (lightsGrid) {
                // On a pad two lights share, the selected one shows, else the first
                int shown = lightPads[selected] == row * 8 + column ? selected : -1;
                for (int i = 0; i < LIGHTING_N_LIGHTS && shown < 0; ++i) {
                    if (lightPads[i] == row * 8 + column) shown = i;
                }
                if (shown >= 0) {
                    color = shown == selected ? COLOR_WHITE : sectionColor("Lights", lighting.lights[shown].on);
                    text = string(1, 'A' + shown);
                }
            } else {
                int item = browsePage[browse] * 64 + row * 8 + column;
                if (item < count) {
                    color = item == currentItem ? COLOR_WHITE : sectionColor(grid.section, false);
                    text = padName(browseName(item));
                }
            }
            sendColor(PAD_CHANNEL_LOW, note, color);
            sendText(SYSEX_BUTTON_TEXT, PAD_CHANNEL_LOW - 1, note, text);
        }
    }
}

void ModGridSurface::sendColor(int channel, int note, int color) {
    int key = channel << 8 | note;
    auto sent = sentColors.find(key);
    if (sent != sentColors.end() && sent->second == color) return;
    sentColors[key] = color;
    out.sendNoteOn(channel, note, color);
}

void ModGridSurface::sendDial(int cc, int value) {
    auto sent = sentDials.find(cc);
    if (sent != sentDials.end() && sent->second == value) return;
    sentDials[cc] = value;
    out.sendControlChange(MAIN_CHANNEL, cc, value);
}

// Text for a control's display: type 0 a button's (12 letters, a pad's PAD_TEXT_LENGTH), 1 a dial's name and 2 its value (20 each)
void ModGridSurface::sendText(int type, int channel, int id, const string &text) {
    int key = type << 16 | channel << 8 | id;
    auto sent = sentTexts.find(key);
    if (sent != sentTexts.end() && sent->second == text) return;
    sentTexts[key] = text;
    vector<unsigned char> bytes = sysexHeader;
    bytes.push_back(type);
    bytes.push_back(channel);
    bytes.push_back(id);
    std::size_t limit = type != SYSEX_BUTTON_TEXT ? 20 : channel == PAD_CHANNEL_LOW - 1 ? PAD_TEXT_LENGTH : 12;
    for (std::size_t i = 0; i < text.size() && i < limit; ++i) {
        unsigned char c = text[i];
        bytes.push_back(c >= 0x20 && c < 0x7F ? c : '?');
    }
    if (text.empty()) bytes.push_back(' ');   // the app trims it; an empty frame would be too short
    bytes.push_back(0xF7);
    out.sendMidiBytes(bytes);
}

// CAPS (protocol-mpe-v1 4.5): a script, version 1, no layout features, chunks up to 128
void ModGridSurface::sendCaps() {
    vector<unsigned char> body = {SYSEX_CAPS, 1, (unsigned char)(msgid++ & 0x7F), 0, 1,
                                  0, 1, 0, 0, 0, 0, 128 >> 7, 128 & 0x7F};
    int sum = 0;
    for (unsigned char b : body) sum += b;
    vector<unsigned char> bytes = sysexHeader;
    bytes.insert(bytes.end(), body.begin(), body.end());
    bytes.push_back((128 - sum % 128) % 128);
    bytes.push_back(0xF7);
    out.sendMidiBytes(bytes);
}
