//
//  SheepEngine.cpp
//  dots
//

#include "SheepEngine.h"
#include "SynAudio.h"
extern "C" {
#include "variations.h"   // prepare_precalc_flags
}

// The audio values the sheep follow, found by name in SynAudio's list
static int audioIndex(const char *name) {
    const vector<string> &names = SynAudio::names();
    return std::find(names.begin(), names.end(), name) - names.begin();
}

SheepEngine::SheepEngine() {
    memset(&frame, 0, sizeof(flam3_genome));
    memset(pair, 0, sizeof(pair));
    parameters.setName("Sheep");
    parameters.add(sheep.set("sheep", 0, 0, 0));
    parameters.add(loopSeconds.set("loop seconds", 12, 2, 60));
    parameters.add(morphSeconds.set("morph seconds", 8, 0, 30));
    parameters.add(autoAdvance.set("auto advance", true));
    parameters.add(loopsPerSheep.set("loops a sheep", 2, 1, 16));
    parameters.add(shuffle.set("shuffle", true));
    parameters.add(pointsK.set("points (k)", 300, 50, 4000));
    parameters.add(passes.set("passes", 6, 1, 32));
    parameters.add(trail.set("trail", 0.85, 0, 0.98));
    parameters.add(exposure.set("exposure", 1, 0.1, 8));
    parameters.add(gammaScale.set("gamma", 1, 0.25, 3));
    audioParameters.setName("Sheep audio");
    audioParameters.add(audioSpeed.set("audio speed", 0.5, 0, 4));
    audioParameters.add(audioTwist.set("audio twist", 0.25, 0, 1));
    audioParameters.add(audioZoom.set("audio zoom", 0.15, 0, 1));
    audioParameters.add(audioBright.set("audio bright", 0.3, 0, 1));
    audioParameters.add(audioColour.set("audio colour", 0.2, 0, 1));
    parameters.add(audioParameters);
}

SheepEngine::~SheepEngine() {
    clear_cp(&frame, 0);
    clear_cp(&pair[0], 0);
    clear_cp(&pair[1], 0);
}

void SheepEngine::setup(int width, int height, flam3_genome *bank, int bankCount, const SynAudio *synAudio) {
    genebank = bank;
    count = bankCount;
    audio = synAudio;
    sheep.setMax(std::max(0, count - 1));
    renderer.setup(width, height);
}

void SheepEngine::resize(int width, int height) {
    renderer.resize(width, height);
}

string SheepEngine::getName(int index) const {
    if (index < 0 || index >= count) return "";
    string name = genebank[index].flame_name;
    return (name.empty() ? "sheep" : name) + " " + ofToString(index);
}

void SheepEngine::select(int index) {
    if (count > 0) sheep = (index % count + count) % count;
}

string SheepEngine::getStatus() const {
    if (count == 0) return "no genebank";
    string status = ofToString(sheep + 1) + "/" + ofToString(count) + " " + getName(sheep);
    if (morphFrom >= 0) status += " (morphing)";
    if (unsupported > 0) status += ", " + ofToString(unsupported) + " variations drawn as linear";
    return status;
}

// The next sheep, by auto advance: the following one, or any
void SheepEngine::advance() {
    select(shuffle ? (int)ofRandom(count) : sheep + 1);
}

// This frame's genome: the sheep (or the two of a morph, blended) turned to the angle
void SheepEngine::build(float angle, float morph) {
    if (morphFrom < 0) {
        flam3_copy(&frame, &genebank[shownSheep]);
        flam3_rotate(&frame, angle, flam3_inttype_log);
        return;
    }
    flam3_copy(&pair[0], &genebank[morphFrom]);
    flam3_copy(&pair[1], &genebank[shownSheep]);
    for (int i = 0; i < 2; ++i) {
        // flam3's smooth interpolation needs four genomes; with two it reads before the array
        pair[i].interpolation = flam3_interpolation_linear;
        pair[i].time = i;
        flam3_rotate(&pair[i], angle, flam3_inttype_log);
    }
    float eased = morph * morph * (3 - 2 * morph);
    flam3_interpolate(pair, 2, eased, 0, &frame);
}

void SheepEngine::render() {
    if (count == 0) return;
    float dt = ofGetLastFrameTime();
    int index = ofClamp(sheep.get(), 0, count - 1);
    if (shownSheep < 0) {
        shownSheep = index;
        unsupported = FlameRenderer::unsupportedVariations(genebank[index]);
    }
    // A new sheep morphs in (or cuts, with no morph time)
    if (index != shownSheep) {
        if (morphSeconds > 0) {
            morphFrom = shownSheep;
            morphTime = 0;
        } else {
            renderer.clear();
            morphFrom = -1;
        }
        shownSheep = index;
        loopsDone = 0;
        unsupported = FlameRenderer::unsupportedVariations(genebank[index]);
    }

    // The music: level (overall, bass, high) and bass hits
    float level = 0, bass = 0, high = 0, bassHits = 0;
    if (audio) {
        static const int LEVEL = audioIndex("syn_Level"), BASS = audioIndex("syn_BassLevel");
        static const int HIGH = audioIndex("syn_HighLevel"), BASS_HITS = audioIndex("syn_BassHits");
        vector<float> values = audio->values();
        auto value = [&values](int i) { return i < (int)values.size() ? ofClamp(values[i], 0, 1) : 0.0f; };
        level = value(LEVEL);
        bass = value(BASS);
        high = value(HIGH);
        bassHits = value(BASS_HITS);
    }

    float speed = 1 + audioSpeed * level * 2;
    phase += dt * speed / std::max(0.1f, loopSeconds.get());
    if (phase >= 1) {
        phase -= std::floor(phase);
        if (morphFrom < 0 && autoAdvance && ++loopsDone >= loopsPerSheep) advance();
    }
    float morph = 1;
    if (morphFrom >= 0) {
        morphTime += dt;
        morph = ofClamp(morphTime / std::max(0.01f, morphSeconds.get()), 0, 1);
    }

    build(360.0f * phase + 45.0f * audioTwist * bass, morph);
    if (morphFrom >= 0 && morph >= 1) morphFrom = -1;
    frame.zoom += audioZoom * bassHits;
    if (prepare_precalc_flags(&frame) != 0) return;

    FlameRenderer::Settings settings;
    settings.points = pointsK * 1000;
    settings.passes = passes;
    settings.fade = trail;
    settings.exposure = exposure * (1 + 2 * audioBright * level);
    settings.gammaScale = gammaScale;
    settings.paletteShift = audioColour * high;
    renderer.render(frame, settings);
}

void SheepEngine::testShots(const string &folder, int shots) {
    ofDirectory::createDirectory(folder, false, true);
    ofPixels pixels;
    float morph = morphSeconds;
    morphSeconds = 0;   // each shot cuts to its sheep
    for (int n = 0; n < shots && count > 0; ++n) {
        int index = (int)((long)n * count / shots);
        select(index);
        uint64_t start = ofGetElapsedTimeMicros();
        const int frames = 30;
        for (int f = 0; f < frames; ++f) render();
        glFinish();
        double ms = (ofGetElapsedTimeMicros() - start) / 1000.0 / frames;
        getFbo().readToPixels(pixels);
        string name = ofToString(index, 4, '0') + (unsupported ? "_u" + ofToString(unsupported) : "") + ".png";
        ofSaveImage(pixels, ofFilePath::join(folder, name));
        ofLogNotice("SheepEngine") << "sheep " << index << ": " << ofToString(ms, 1) << " ms a frame, "
                                   << unsupported << " unsupported variations";
    }
    morphSeconds = morph;
}

void SheepEngine::testMorph(const string &folder, int from, int to, int frames) {
    ofDirectory::createDirectory(folder, false, true);
    ofPixels pixels;
    shownSheep = to;
    sheep = to;
    for (int f = 0; f < frames; ++f) {
        morphFrom = from;
        float t = frames > 1 ? (float)f / (frames - 1) : 1;
        renderer.clear();
        for (int settle = 0; settle < 15; ++settle) {
            build(0, t);
            if (prepare_precalc_flags(&frame) != 0) break;
            FlameRenderer::Settings settings;
            settings.points = pointsK * 1000;
            settings.passes = passes;
            settings.fade = trail;
            settings.exposure = exposure;
            settings.gammaScale = gammaScale;
            renderer.render(frame, settings);
        }
        getFbo().readToPixels(pixels);
        ofSaveImage(pixels, ofFilePath::join(folder, ofToString(f, 2, '0') + ".png"));
    }
    morphFrom = -1;
    ofLogNotice("SheepEngine") << "morph " << from << " to " << to << ": " << frames << " frames";
}
