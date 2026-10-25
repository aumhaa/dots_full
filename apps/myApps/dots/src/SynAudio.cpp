//
//  SynAudio.cpp
//  dots
//

#include "SynAudio.h"

static const int fftSize = 2048;

// Band edges in Hz: whole spectrum, bass, mid, mid-high, high
static const float bandLow[] = {30, 30, 250, 1000, 4000};
static const float bandHigh[] = {16000, 250, 1000, 4000, 16000};

const vector<string> &SynAudio::names() {
    static const vector<string> all = {
        "syn_Level", "syn_BassLevel", "syn_MidLevel", "syn_MidHighLevel", "syn_HighLevel",
        "syn_Hits", "syn_BassHits", "syn_MidHits", "syn_MidHighHits", "syn_HighHits",
        "syn_Time", "syn_BassTime", "syn_MidTime", "syn_MidHighTime", "syn_HighTime",
        "syn_Presence", "syn_BassPresence", "syn_MidPresence", "syn_MidHighPresence", "syn_HighPresence",
        "syn_CurvedTime", "syn_BassExpoCurve",
        "syn_OnBeat", "syn_ToggleOnBeat", "syn_RandomOnBeat", "syn_BeatTime",
        "syn_BPM", "syn_BPMConfidence", "syn_BPMTwitcher",
        "syn_BPMSin", "syn_BPMSin2", "syn_BPMSin4", "syn_BPMTri", "syn_BPMTri2", "syn_BPMTri4",
        "syn_FadeInOut", "syn_Intensity",
    };
    return all;
}

SynAudio::SynAudio() {
    fft = ofxFft::create(fftSize, OF_FFT_WINDOW_HAMMING);
    window.assign(fftSize, 0);
    published.assign(names().size(), 0);
}

SynAudio::~SynAudio() {
    delete fft;
}

vector<float> SynAudio::values() const {
    std::lock_guard<std::mutex> lock(mutex);
    return published;
}

void SynAudio::process(const float *samples, int count, float sampleRate) {
    if (count <= 0 || sampleRate <= 0) return;
    // Slide the new samples into the window
    count = std::min(count, fftSize);
    std::move(window.begin() + count, window.end(), window.begin());
    std::copy(samples, samples + count, window.end() - count);
    analyse(count / sampleRate, sampleRate);
}

// Exponential approach to a target with time constant tau seconds
static float approach(float value, float target, float dt, float tau) {
    return value + (target - value) * (1 - std::exp(-dt / std::max(tau, 1e-4f)));
}

static float triangle(float phase) {
    return std::abs(2 * (phase - std::floor(phase)) - 1);
}

void SynAudio::analyse(float dt, float sampleRate) {
    // Signal level, to tell silence from quiet
    double sumSquares = 0;
    for (int i = fftSize - (int)(dt * sampleRate); i < fftSize; ++i) {
        if (i >= 0) sumSquares += window[i] * window[i];
    }
    float rms = std::sqrt(sumSquares / std::max(1.0f, dt * sampleRate));
    bool silent = !std::isfinite(rms) || rms < 0.0005f;   // about -66 dBFS

    fft->setSignal(window.data());
    const float *amplitude = fft->getAmplitude();
    int bins = fft->getBinSize();
    float binHz = sampleRate / fftSize;

    for (int b = 0; b < N_BANDS; ++b) {
        BandState &band = bands[b];
        int lo = std::max(1, (int)(bandLow[b] / binHz)), hi = std::min(bins - 1, (int)(bandHigh[b] / binHz));
        double power = 0;
        for (int i = lo; i <= hi; ++i) power += amplitude[i] * amplitude[i];
        float db = 10 * std::log10(power + 1e-12);
        if (!std::isfinite(db)) db = -120;

        // Level against the loudest this band has been lately (falls 3 dB a second)
        band.peakDb = std::max(db, band.peakDb - 3 * dt);
        float raw = silent ? 0 : ofClamp((db - (band.peakDb - 30)) / 30, 0, 1);
        raw = raw * raw;

        band.level = approach(band.level, raw, dt, raw > band.level ? 0.03f : 0.25f);
        band.fast = approach(band.fast, raw, dt, 0.015f);
        band.slow = approach(band.slow, raw, dt, 0.3f);
        float hit = ofClamp((band.fast - band.slow) * 3, 0, 1);
        band.hits = std::max(hit, band.hits * std::exp(-dt / 0.1f));
        band.presence = approach(band.presence, ofClamp(band.level * 1.4f, 0, 1), dt, 1.5f);
        band.time += dt * 2 * band.level;
    }
    curvedTime += dt * 4 * std::pow(bands[ALL].level, 3.0f);

    // Beats: bass onsets, at most about 4 a second
    sinceBeat += dt;
    float bassOnset = ofClamp((bands[BASS].fast - bands[BASS].slow) * 3, 0, 1);
    if (!silent && bassOnset > 0.35f && sinceBeat > 0.25f) {
        if (sinceBeat < 1.5f) {
            intervals.push_back(sinceBeat);
            if (intervals.size() > 12) intervals.pop_front();
        }
        sinceBeat = 0;
        onBeat = 1;
        toggleTarget = 1 - toggleTarget;
        randomTarget = ofRandom(1);
        beatTime += 1;

        // Pull the tempo clock toward the beat when it's close
        float offset = std::round(phase) - phase;
        if (std::abs(offset) < 0.25f) phase += offset * 0.3f;
    }
    onBeat *= std::exp(-dt / 0.12f);
    toggle = approach(toggle, toggleTarget, dt, 0.08f);
    random = approach(random, randomTarget, dt, 0.08f);

    // Tempo from the median beat interval, folded into 75-165 BPM
    if (intervals.size() >= 4) {
        vector<float> sorted(intervals.begin(), intervals.end());
        std::sort(sorted.begin(), sorted.end());
        float median = sorted[sorted.size() / 2];
        float estimate = 60 / median;
        while (estimate < 75) estimate *= 2;
        while (estimate > 165) estimate /= 2;
        bpm = approach(bpm, estimate, dt, 2);
        float spread = 0;
        for (float interval : sorted) spread += std::abs(interval - median);
        spread /= sorted.size();
        bpmConfidence = approach(bpmConfidence, ofClamp(1 - 4 * spread / median, 0, 1), dt, 2);
    }
    phase += dt * bpm / 60;

    fadeInOut = approach(fadeInOut, bands[ALL].level > 0.05f ? 1 : 0, dt, 4);
    intensity = approach(intensity, ofClamp(bands[ALL].presence * 1.2f, 0, 1), dt, 10);

    float fraction = phase - std::floor(phase);
    float twitcher = std::floor(phase) + (1 - std::exp(-8 * fraction)) / (1 - std::exp(-8.0f));
    float bass = bands[BASS].level;

    vector<float> v;
    v.reserve(names().size());
    for (int b = 0; b < N_BANDS; ++b) v.push_back(bands[b].level);
    for (int b = 0; b < N_BANDS; ++b) v.push_back(bands[b].hits);
    for (int b = 0; b < N_BANDS; ++b) v.push_back(bands[b].time);
    for (int b = 0; b < N_BANDS; ++b) v.push_back(bands[b].presence);
    v.push_back(curvedTime);
    v.push_back((std::exp(3 * bass) - 1) / (std::exp(3.0f) - 1));
    v.push_back(onBeat);
    v.push_back(toggle);
    v.push_back(random);
    v.push_back(beatTime);
    v.push_back(bpm);
    v.push_back(bpmConfidence);
    v.push_back(twitcher);
    v.push_back(0.5f + 0.5f * std::cos(TWO_PI * phase));
    v.push_back(0.5f + 0.5f * std::cos(TWO_PI * phase / 2));
    v.push_back(0.5f + 0.5f * std::cos(TWO_PI * phase / 4));
    v.push_back(triangle(phase));
    v.push_back(triangle(phase / 2));
    v.push_back(triangle(phase / 4));
    v.push_back(fadeInOut);
    v.push_back(intensity);
    for (float &value : v) {
        if (!std::isfinite(value)) value = 0;
    }

    std::lock_guard<std::mutex> lock(mutex);
    published.swap(v);
}
