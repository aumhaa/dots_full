//
//  SheepEngine.cpp
//  dots
//

#include "SheepEngine.h"
extern "C" {
#include "variations.h"   // prepare_precalc_flags
}

SheepEngine::SheepEngine() {
    memset(&frame, 0, sizeof(flam3_genome));
    parameters.setName("Sheep");
    parameters.add(sheep.set("sheep", 0, 0, 0));
    parameters.add(loopSeconds.set("loop seconds", 12, 2, 60));
    parameters.add(pointsK.set("points (k)", 300, 50, 4000));
    parameters.add(passes.set("passes", 6, 1, 32));
    parameters.add(trail.set("trail", 0.85, 0, 0.98));
    parameters.add(exposure.set("exposure", 1, 0.1, 8));
    parameters.add(gammaScale.set("gamma", 1, 0.25, 3));
}

SheepEngine::~SheepEngine() {
    clear_cp(&frame, 0);
}

void SheepEngine::setup(int width, int height, flam3_genome *bank, int bankCount) {
    genebank = bank;
    count = bankCount;
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
    if (unsupported > 0) status += ", " + ofToString(unsupported) + " variations drawn as linear";
    return status;
}

void SheepEngine::testShots(const string &folder, int shots) {
    ofDirectory::createDirectory(folder, false, true);
    ofPixels pixels;
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
}

void SheepEngine::render() {
    if (count == 0) return;
    int index = ofClamp(sheep.get(), 0, count - 1);
    if (index != shownSheep) {
        shownSheep = index;
        phase = 0;
        renderer.clear();
        unsupported = FlameRenderer::unsupportedVariations(genebank[index]);
    }
    phase = std::fmod(phase + ofGetLastFrameTime() / std::max(0.1f, loopSeconds.get()), 1.0f);

    // A sheep loop: the xforms turned a full circle over the loop
    flam3_copy(&frame, &genebank[index]);
    flam3_rotate(&frame, 360.0 * phase, flam3_inttype_log);
    if (prepare_precalc_flags(&frame) != 0) return;

    FlameRenderer::Settings settings;
    settings.points = pointsK * 1000;
    settings.passes = passes;
    settings.fade = trail;
    settings.exposure = exposure;
    settings.gammaScale = gammaScale;
    renderer.render(frame, settings);
}
