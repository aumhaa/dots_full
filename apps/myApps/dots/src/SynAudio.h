//
//  SynAudio.h
//  dots
//
//  Synesthesia's audio uniforms (syn_BassLevel, syn_OnBeat, syn_BPM, ...),
//  worked out from dots's audio input. The analysis runs on the audio thread
//  with its own longer FFT, so bass gets more than a couple of bins; the shader
//  engine reads a copy each frame. Levels adapt to how loud the input is.
//

#pragma once

#include "ofMain.h"
#include "ofxFft.h"
#include <mutex>

class SynAudio {
public:
    SynAudio();
    ~SynAudio();

    // From the audio thread: mono samples at sampleRate
    void process(const float *samples, int count, float sampleRate);

    // Every uniform, in the order of names()
    static const vector<string> &names();
    vector<float> values() const;

private:
    enum Band { ALL, BASS, MID, MIDHIGH, HIGH, N_BANDS };
    void analyse(float dt, float sampleRate);

    ofxFft *fft;
    vector<float> window;   // the last fftSize samples

    struct BandState {
        float peakDb = -60, level = 0, fast = 0, slow = 0, hits = 0, presence = 0, time = 0;
    };
    BandState bands[N_BANDS];
    float curvedTime = 0;

    // Beats and tempo
    float onBeat = 0, toggleTarget = 0, toggle = 0, randomTarget = 0, random = 0, beatTime = 0;
    float sinceBeat = 10;
    deque<float> intervals;
    float bpm = 120, bpmConfidence = 0, phase = 0;
    float fadeInOut = 0, intensity = 0;

    mutable std::mutex mutex;
    vector<float> published;
};
