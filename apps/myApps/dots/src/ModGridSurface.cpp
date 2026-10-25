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
    // Right column: the modes, through the set, and the arrows (up/down the dial pages, left/right the grid's)
    {33, "flames", "Drawing"},    {34, "milk", "MilkDrop"},     {35, "shader", "Shader"},    {36, "lights", "Lights"},
    {37, "sets", "Sets"},
    {40, "set prev", "Sets"},     {41, "set next", "Sets"},
    {70, "^", "Mixer"},           {73, "v", "Mixer"},           {71, "<", "Mixer"},          {72, ">", "Mixer"},
};
static const int MIXER_NOTE = 15, FIRST_MODE_NOTE = 33, UP_NOTE = 70, LEFT_NOTE = 71, RIGHT_NOTE = 72, DOWN_NOTE = 73;
// Left column: the engine on each side of the mixer (a press steps it), and the crossfader dial
static const int ENGINE_A_NOTE = 11, ENGINE_B_NOTE = 32, CROSSFADE_CC = 17, SPARE_CC = 16;
// Above the grid: the dial page (over the last key) and the grid's page (the last key); in lights
// mode the first four keys pick the light the grid moves
static const int DIAL_PAGE_NOTE = 107, GRID_PAGE_NOTE = 115, FIRST_LIGHT_KEY = 108;
// Shown blank (Live's labels would otherwise stay on them); the light keys too, outside lights mode
static const int BLANK_NOTES[] = {100, 101, 102, 103, 104, 105, 106, 112, 113, 114};
static const char *engineNames[] = {"flames", "MilkDrop", "shader"};
static const char *engineSections[] = {"Drawing", "MilkDrop", "Shader"};
// Each mode: its dial pages, what its grid browses (-1: the grid places the selected light), its colour
struct SurfaceMode { const char *group, *name, *section; int browse; };
static const SurfaceMode modes[] = {
    {"flames", "scene", "Drawing", 3},   {"milk", "milk", "MilkDrop", 2},  {"shader", "shader", "Shader", 1},
    {"lights", "light", "Lights", -1},   {"sets", "sets", "Sets", 0},
};
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
        if (message.control < 16) {
            const vector<ControlEntry> &entries = pages.get()[currentPage()].entries;
            if (message.control < (int)entries.size()) entries[message.control].setNormalized(message.value / 127.0f);
        } else if (message.control == CROSSFADE_CC) {
            app->gui->crossfade = message.value / 127.0f;
        }
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

// The dial pages on show: the mixer's while it's up, else the mode's
string ModGridSurface::dialGroup() const {
    return mixerShown ? "mixer" : modes[mode].group;
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
    bool lightsGrid = modes[mode].browse < 0;
    if (note >= FIRST_MODE_NOTE && note < FIRST_MODE_NOTE + N_MODES) {
        selectMode(note - FIRST_MODE_NOTE);
        return;
    }
    if (lightsGrid && note >= FIRST_LIGHT_KEY && note < FIRST_LIGHT_KEY + LIGHTING_N_LIGHTS) {
        lighting.selected = note - FIRST_LIGHT_KEY + 1;
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
        case 40: app->presets->previousInSet(); break;
        case 41: app->presets->nextInSet(); break;
        case 12: gui.flamesRunning = !gui.flamesRunning; break;
        case 13: gui.milkdropRunning = !gui.milkdropRunning; break;
        case 14: gui.shaderRunning = !gui.shaderRunning; break;
        case 16: case 17: case 18: case 19: {
            ofParameter<bool> &on = lighting.lights[note - 16].on;
            on = !on;
            break;
        }
        case ENGINE_A_NOTE: gui.engineA = (gui.engineA + 1) % 3; break;
        case ENGINE_B_NOTE: gui.engineB = (gui.engineB + 1) % 3; break;
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
    if (modes[mode].browse < 0) {
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
    const ControlPage &current = all[currentPage()];

    // The dials: this page's parameters
    for (int cc = 0; cc < 16; ++cc) {
        bool used = cc < (int)current.entries.size();
        const ControlEntry *entry = used ? &current.entries[cc] : nullptr;
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

    // The buttons: switches lit while on, the mode lit, the arrows and set buttons dim when they can't move
    LightingLayer &lighting = *app->lighting;
    PresetManager &presets = *app->presets;
    bool lightsGrid = modes[mode].browse < 0;
    int count = lightsGrid ? 0 : browseCount(), currentItem = lightsGrid ? -1 : browseCurrent();
    int pageCount = lightsGrid ? 1 : browsePageCount(), position = groupPosition(), positions = groupPages().size();
    for (const SurfaceButton &button : buttons) {
        bool lit = true;
        const char *section = button.section;
        switch (button.note) {
            case 12: lit = gui.flamesRunning; break;
            case 13: lit = gui.milkdropRunning; break;
            case 14: lit = gui.shaderRunning; break;
            case 16: case 17: case 18: case 19: lit = lighting.lights[button.note - 16].on; break;
            case MIXER_NOTE: lit = mixerShown; break;
            case 33: case 34: case 35: case 36: case 37: lit = mode == button.note - FIRST_MODE_NOTE; break;
            case 40: lit = presets.setPosition > 0 && !presets.setEntries.empty(); break;
            case 41: lit = presets.setPosition < (int)presets.setEntries.size() - 1; break;
            case UP_NOTE: lit = position > 0; section = current.section.c_str(); break;
            case DOWN_NOTE: lit = position < positions - 1; section = current.section.c_str(); break;
            case LEFT_NOTE: lit = !lightsGrid && browsePage[browse] > 0; section = modes[mode].section; break;
            case RIGHT_NOTE: lit = !lightsGrid && browsePage[browse] < pageCount - 1; section = modes[mode].section; break;
        }
        sendColor(MAIN_CHANNEL, button.note, sectionColor(section, lit));
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, button.note, button.label);
    }
    int engines[] = {gui.engineA, gui.engineB}, engineNotes[] = {ENGINE_A_NOTE, ENGINE_B_NOTE};
    for (int side = 0; side < 2; ++side) {
        int engine = ofClamp(engines[side], 0, 2);
        sendColor(MAIN_CHANNEL, engineNotes[side], sectionColor(engineSections[engine], true));
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, engineNotes[side], string(side ? "B " : "A ") + engineNames[engine]);
    }
    for (int note : BLANK_NOTES) {
        sendColor(MAIN_CHANNEL, note, COLOR_OFF);
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, note, "");
    }
    for (int key = 0; key < 8; ++key) {   // the keys, which this layout doesn't show
        sendColor(MAIN_CHANNEL, key, COLOR_OFF);
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, key, "");
    }

    // Above the grid: the light keys in lights mode, then the dial page and the grid's page
    int selected = ofClamp(lighting.selected - 1, 0, LIGHTING_N_LIGHTS - 1);
    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        sendColor(MAIN_CHANNEL, FIRST_LIGHT_KEY + i, lightsGrid ? sectionColor("Lights", i == selected) : COLOR_OFF);
        sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, FIRST_LIGHT_KEY + i, lightsGrid ? "move " + string(1, 'A' + i) : "");
    }
    sendColor(MAIN_CHANNEL, DIAL_PAGE_NOTE, sectionColor(current.section, true));
    sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, DIAL_PAGE_NOTE,
             all[groupPages().front()].name + " " + ofToString(position + 1) + "/" + ofToString(positions));
    sendColor(MAIN_CHANNEL, GRID_PAGE_NOTE, sectionColor(modes[mode].section, true));
    sendText(SYSEX_BUTTON_TEXT, MAIN_CHANNEL - 1, GRID_PAGE_NOTE,
             lightsGrid ? "light " + string(1, 'A' + selected)
                        : string(modes[mode].name) + " " + ofToString(browsePage[browse] + 1) + "/" + ofToString(pageCount));

    // The grid: in lights mode, where the lights are (the selected one white, the others lit while on);
    // otherwise this page of the browse list, each pad with its item's name, the current item white
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
                    color = item == currentItem ? COLOR_WHITE : sectionColor(modes[mode].section, false);
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
