//
//  DotsSections.h
//  dots
//
//  Each section of dots's parameters has a colour, so a section's knobs on
//  the Midi Fighter Twister light up the same colour as the section in the
//  control window. The colour is a Twister LED value (1-126, round the hue
//  wheel starting from blue); dots sends it on MIDI channel 2.
//

#pragma once

#include "ofMain.h"

struct DotsSection {
    string group;      // ofParameterGroup name, as shown in the control window
    int twisterColor;  // Twister LED value, 1-126
};

static const vector<DotsSection> dotsSections = {
    {"Wander",         110},  // purple
    {"Audio Analysis",  25},  // cyan
    {"Drawing",         63},  // yellow
    {"Speed",           80},  // red
    {"Dots",            45},  // green
    {"Lines",            1},  // blue
    {"Audio Effects",   95},  // pink
    {"MilkDrop",        72},  // orange
    {"Lights",         120},  // violet
};

inline int twisterColorForSection(const string &group) {
    for (const DotsSection &section : dotsSections) {
        if (section.group == group) return section.twisterColor;
    }
    return 0;
}

// The Twister's hue wheel runs from blue at 0, through green, yellow and red,
// to purple near 127.
inline ofColor guiColorForTwister(int twisterColor, float brightness = 200) {
    float hueDegrees = fmod(240.0f - twisterColor * 360.0f / 127.0f + 360.0f, 360.0f);
    return ofColor::fromHsb(hueDegrees / 360.0f * 255.0f, 190, brightness);
}
