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

// modGrid palette numbers (its docs/palette.json) for each section: bright, then dim
struct SectionColor { const char *section; int bright, dim; };
static const SectionColor sectionColors[] = {
    {"Wander", 87, 90},         {"Audio Analysis", 72, 80}, {"Drawing", 71, 85},   {"Speed", 74, 84},
    {"Dots", 75, 81},           {"Lines", 76, 79},          {"Audio Effects", 13, 99},
    {"MilkDrop", 16, 83},       {"Lights", 26, 118},        {"Shader", 86, 82},    {"Mixer", 14, 96},
    {"Sets", 94, 89},           {"Scenes", 71, 85},
};
static const int COLOR_OFF = 0, COLOR_WHITE = 14;

static int sectionColor(const string &section, bool bright) {
    for (const SectionColor &c : sectionColors) {
        if (section == c.section) return bright ? c.bright : c.dim;
    }
    return bright ? COLOR_WHITE : 96;
}

// The buttons round the dials: what each does, its label, its colour, and when it's lit
struct SurfaceButton { int note; const char *label, *section; };
static const SurfaceButton buttons[] = {
    {92, "track <", "Drawing"},   {93, "track >", "Drawing"},   {94, "scene <", "Drawing"},  {95, "scene >", "Drawing"},
    {96, "milk <", "MilkDrop"},   {97, "milk >", "MilkDrop"},   {98, "shader <", "Shader"},  {99, "shader >", "Shader"},
    {100, "set <", "Sets"},       {101, "set >", "Sets"},       {102, "wander", "Wander"},   {103, "flames", "Drawing"},
    {104, "MilkDrop", "MilkDrop"}, {105, "shader", "Shader"},   {106, "lights A", "Lights"}, {107, "alpha", "Shader"},
    {108, "sets", "Sets"},        {109, "shaders", "Shader"},   {110, "MilkDrop", "MilkDrop"}, {111, "scenes", "Scenes"},
    {112, "page <", "Mixer"},     {113, "page >", "Mixer"},     {114, "engine A", "Mixer"},  {115, "engine B", "Mixer"},
};
static const char *browseNames[] = {"sets", "shaders", "MilkDrop", "scenes"};
static const char *browseSections[] = {"Sets", "Shader", "MilkDrop", "Scenes"};
// The right strip's buttons carry the browse mode and the current item's name
static const int INFO_NOTES[] = {33, 34, 35, 36, 37};

ModGridSurface::~ModGridSurface() {
    disconnect();
}

void ModGridSurface::setup(ofApp *dots) {
    app = dots;
    msgid = ofRandom(128);   // a fresh start never reuses the last session's ids
    pages.setup(app);
}

void ModGridSurface::newMidiMessage(ofxMidiMessage &message) {
    std::lock_guard<std::mutex> lock(mutex);
    received.push_back(message);
}

// The Control pair: the Bridge's "modGrid - <iPad> - Control" (a real iPad first), else the Simulator's
// "modGrid Sim". DOTS_MODGRID_PORT=text picks the first whose name contains it.
void ModGridSurface::connect() {
    vector<string> ins = in.getInPortList(), outs = out.getOutPortList();
    const char *wanted = getenv("DOTS_MODGRID_PORT");
    int best = -1, bestScore = 0;
    for (int i = 0; i < (int)ins.size(); ++i) {
        const string &name = ins[i];
        if (wanted ? name.find(wanted) == string::npos : name.find("modGrid") == string::npos) continue;
        if (name.find("MPE") != string::npos) continue;
        int score = name.find(" - Control") != string::npos ? 2 : name.find("modGrid Sim") != string::npos ? 1 : 0;
        if (wanted) score = std::max(score, 1);
        if (score > bestScore) {
            best = i;
            bestScore = score;
        }
    }
    if (best < 0) return;
    string base = ins[best].substr(ins[best].find(wanted ? wanted : "modGrid"));
    for (int o = 0; o < (int)outs.size(); ++o) {
        if (outs[o].find(base + " In") == string::npos) continue;
        in.ignoreTypes(false, true, true);   // SysEx in: the app's requests
        if (!in.openPort(best) || !out.openPort(o)) {
            disconnect();
            return;
        }
        in.addListener(this);
        connected = true;
        portName = ins[best];
        ofLogNotice("ModGridSurface") << "Connected to " << portName;
        refresh();
        sendCaps();   // one unsolicited CAPS, as a script does on start, so the app asks for everything
        return;
    }
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
    if (!enabled) {
        if (connected) disconnect();
        return;
    }
    // Look for the port every few seconds, and notice it going (the Bridge restarting, the iPad leaving)
    float now = ofGetElapsedTimef();
    if (now - lastScan > 3) {
        lastScan = now;
        if (!connected) {
            connect();
        } else {
            vector<string> ins = in.getInPortList();
            if (std::find(ins.begin(), ins.end(), portName) == ins.end()) disconnect();
        }
    }
    if (!connected) return;

    vector<ofxMidiMessage> messages;
    {
        std::lock_guard<std::mutex> lock(mutex);
        messages.swap(received);
    }
    for (const ofxMidiMessage &message : messages) handle(message);

    if (pages.update()) {
        page = std::min(page, (int)pages.get().size() - 1);
    }
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
    if (message.status == MIDI_CONTROL_CHANGE && message.channel == MAIN_CHANNEL && message.control < 16) {
        const vector<ControlEntry> &entries = pages.get()[page].entries;
        if (message.control < (int)entries.size()) entries[message.control].setNormalized(message.value / 127.0f);
        return;
    }
    if (message.status == MIDI_NOTE_ON && message.velocity > 0) {
        if (message.channel == MAIN_CHANNEL) {
            press(message.pitch);
        } else if (message.channel == PAD_CHANNEL_LOW || message.channel == PAD_CHANNEL_HIGH) {
            int pad = message.pitch + (message.channel == PAD_CHANNEL_HIGH ? 128 : 0);
            pressPad(pad / 16, pad % 16);
        }
    }
}

void ModGridSurface::press(int note) {
    GuiApp &gui = *app->gui;
    int pageCount = std::max(1, (browseCount() + 63) / 64);
    if (note >= 0 && note < 8) {
        if (note < (int)pages.get().size()) page = note;
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
        case 100: app->presets->previousInSet(); break;
        case 101: app->presets->nextInSet(); break;
        case 102: gui.wandering = !gui.wandering; break;
        case 103: gui.flamesRunning = !gui.flamesRunning; break;
        case 104: gui.milkdropRunning = !gui.milkdropRunning; break;
        case 105: gui.shaderRunning = !gui.shaderRunning; break;
        case 106: app->lighting->feedAIncludesLights = !app->lighting->feedAIncludesLights; break;
        case 107: gui.shaderAlpha = !gui.shaderAlpha; break;
        case 108: case 109: case 110: case 111: {
            browse = note - 108;
            if (browse == BROWSE_MILKDROP && milkdropPresets.empty()) scanMilkdrop();
            // Open on the page with the current item
            int current = browseCurrent();
            if (current >= 0) browsePage[browse] = current / 64;
            break;
        }
        case 112: browsePage[browse] = (browsePage[browse] + pageCount - 1) % pageCount; break;
        case 113: browsePage[browse] = (browsePage[browse] + 1) % pageCount; break;
        case 114: gui.engineA = (gui.engineA + 1) % 3; break;
        case 115: gui.engineB = (gui.engineB + 1) % 3; break;
    }
}

void ModGridSurface::pressPad(int row, int column) {
    if (row >= 8 || column >= 8) return;
    int item = browsePage[browse] * 64 + row * 8 + column;
    if (item < browseCount()) browseLoad(item);
}

int ModGridSurface::browseCount() const {
    switch (browse) {
        case BROWSE_SETS: return app->presets->setEntries.size();
        case BROWSE_SHADERS: return app->shaderEngine->getCount();
        case BROWSE_MILKDROP: return milkdropPresets.size();
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
}

void ModGridSurface::sendFeedback() {
    const vector<ControlPage> &all = pages.get();
    const ControlPage &current = all[page];

    // The dials: this page's parameters
    for (int cc = 0; cc < 16; ++cc) {
        bool used = cc < (int)current.entries.size();
        const ControlEntry *entry = used ? &current.entries[cc] : nullptr;
        sendDial(cc, used ? (int)std::round(entry->normalized() * 127) : 0);
        sendText(SYSEX_DIAL_NAME, MAIN_CHANNEL - 1, cc, used ? entry->label : "");
        sendText(SYSEX_DIAL_VALUE, MAIN_CHANNEL - 1, cc, used ? entry->valueText() : "");
    }

    // The keys: the pages, this one lit
    for (int key = 0; key < 8; ++key) {
        bool used = key < (int)all.size();
        sendColor(MAIN_CHANNEL, key, used ? sectionColor(all[key].section, key == page) : COLOR_OFF);
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, key, used ? all[key].name : "");
    }

    // The buttons: switches lit while on, the browse mode lit, the rest dim
    GuiApp &gui = *app->gui;
    for (const SurfaceButton &button : buttons) {
        bool lit = false;
        switch (button.note) {
            case 102: lit = gui.wandering; break;
            case 103: lit = gui.flamesRunning; break;
            case 104: lit = gui.milkdropRunning; break;
            case 105: lit = gui.shaderRunning; break;
            case 106: lit = app->lighting->feedAIncludesLights; break;
            case 107: lit = gui.shaderAlpha; break;
            case 108: case 109: case 110: case 111: lit = browse == button.note - 108; break;
        }
        sendColor(MAIN_CHANNEL, button.note, sectionColor(button.section, lit));
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, button.note, button.label);
    }

    // The grid: this page of the browse list, the current item white
    int count = browseCount(), currentItem = browseCurrent();
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            int item = browsePage[browse] * 64 + row * 8 + column;
            int color = item >= count ? COLOR_OFF : item == currentItem ? COLOR_WHITE : sectionColor(browseSections[browse], false);
            sendColor(PAD_CHANNEL_LOW, row * 16 + column, color);
        }
    }

    // The right strip: the browse mode and page, then the current item's name in 12-letter pieces
    int pageCount = std::max(1, (count + 63) / 64);
    sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, INFO_NOTES[0],
             string(browseNames[browse]) + " " + ofToString(browsePage[browse] + 1) + "/" + ofToString(pageCount));
    sendColor(MAIN_CHANNEL, INFO_NOTES[0], sectionColor(browseSections[browse], true));
    string name = browseName(currentItem);
    for (int i = 1; i < 5; ++i) {
        std::size_t from = (i - 1) * 12;
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, INFO_NOTES[i], from < name.size() ? name.substr(from, 12) : "");
        sendColor(MAIN_CHANNEL, INFO_NOTES[i], from < name.size() ? sectionColor(browseSections[browse], false) : COLOR_OFF);
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

// Text for a control's display: type 0 a button's (12 letters), 1 a dial's name and 2 its value (20 each)
void ModGridSurface::sendText(int type, int channel, int id, const string &text) {
    int key = type << 16 | channel << 8 | id;
    auto sent = sentTexts.find(key);
    if (sent != sentTexts.end() && sent->second == text) return;
    sentTexts[key] = text;
    vector<unsigned char> bytes = sysexHeader;
    bytes.push_back(type);
    bytes.push_back(channel);
    bytes.push_back(id);
    std::size_t limit = type == SYSEX_BUTTON_TEXT ? 12 : 20;
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
