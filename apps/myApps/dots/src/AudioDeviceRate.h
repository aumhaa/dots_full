#pragma once

#include <string>

// The sample rate a CoreAudio device is currently running at, found by the
// "Manufacturer: Name" that RtAudio lists it under. 0 if it isn't found.
double currentSampleRate(const std::string &rtAudioName);
