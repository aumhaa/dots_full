#include "AudioDeviceRate.h"

#include <CoreAudio/CoreAudio.h>
#include <vector>

static std::string stringProperty(AudioObjectID device, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress address = {selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    CFStringRef value = NULL;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(device, &address, 0, NULL, &size, &value) != noErr || !value) {
        return "";
    }
    char buffer[256];
    bool ok = CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(value);
    return ok ? buffer : "";
}

double currentSampleRate(const std::string &rtAudioName) {
    AudioObjectPropertyAddress devicesAddress = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesAddress, 0, NULL, &size) != noErr) {
        return 0;
    }
    std::vector<AudioObjectID> devices(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesAddress, 0, NULL, &size, devices.data()) != noErr) {
        return 0;
    }

    for (AudioObjectID device : devices) {
        // RtAudio names devices "Manufacturer: Name"
        std::string name = stringProperty(device, kAudioObjectPropertyManufacturer) + ": " +
                           stringProperty(device, kAudioObjectPropertyName);
        if (name != rtAudioName) continue;

        AudioObjectPropertyAddress rateAddress = {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        Float64 rate = 0;
        size = sizeof(rate);
        if (AudioObjectGetPropertyData(device, &rateAddress, 0, NULL, &size, &rate) == noErr) {
            return rate;
        }
    }
    return 0;
}
