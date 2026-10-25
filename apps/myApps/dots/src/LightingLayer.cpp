//
//  LightingLayer.cpp
//  dots
//

#include "LightingLayer.h"

static const char *vertexShader = R"(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 texCoordVarying;
void main() {
    texCoordVarying = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

// Each beam is a disc of radius `size` (picture height = 1). Focus sets how
// wide the soft edge is; frost widens it further and adds a diffuse glow.
// Reveal beams light the picture (tinted by their colour) and everything
// outside them drops to the ambient level; add beams lay light on top.
static const char *fragmentShader = R"(#version 150
uniform sampler2DRect src_tex_unit0;
uniform vec2 resolution;
uniform float ambient;
uniform float outputMode;   // 0 lit picture, 1 picture masked by the light, 2 the light alone
uniform float lightOn[4];
uniform float lightMode[4];
uniform float lightIntensity[4];
uniform vec3 lightColor[4];
uniform vec2 lightPos[4];
uniform float lightSize[4];
uniform float lightFocus[4];
uniform float lightFrost[4];
in vec2 texCoordVarying;
out vec4 outputColor;

void main() {
    vec3 base = texture(src_tex_unit0, texCoordVarying).rgb;
    vec3 added = vec3(0.0);
    vec3 revealed = vec3(0.0);
    bool anyReveal = false;

    for (int i = 0; i < 4; i++) {
        if (lightOn[i] < 0.5) continue;
        float r = length(texCoordVarying - lightPos[i] * resolution) / max(lightSize[i] * resolution.y, 1.0);
        float soft = mix(0.6, 0.02, lightFocus[i]) + 0.5 * lightFrost[i];
        float beam = 1.0 - smoothstep(1.0 - soft, 1.0 + soft, r);
        float glow = lightFrost[i] * 0.4 * exp(-r * r * 0.6);
        vec3 light = lightColor[i] * lightIntensity[i] * (beam * (1.0 - 0.3 * lightFrost[i]) + glow);
        if (lightMode[i] > 0.5) {
            revealed += light;
            anyReveal = true;
        } else {
            added += light;
        }
    }

    // Feed B is a top layer: alpha is the amount of light, transparent where
    // there is none, with the colour stored un-premultiplied
    if (outputMode > 0.5) {
        vec3 light = added + revealed;
        float amount = clamp(max(light.r, max(light.g, light.b)), 0.0, 1.0);
        vec3 shown = outputMode > 1.5 ? light : base * light;
        outputColor = vec4(clamp(shown / max(amount, 0.001), 0.0, 1.0), amount);
        return;
    }
    vec3 lit = anyReveal ? base * (vec3(ambient) + revealed) : base;
    outputColor = vec4(lit + added, 1.0);
}
)";

void LightingLayer::setup() {
    parameters.setName("Lights");
    parameters.add(ambient.set("ambient", 0.15, 0, 1));
    parameters.add(selected.set("selected", 1, 1, LIGHTING_N_LIGHTS));
    parameters.add(snapshot.set("snapshot", 1, 1, 8));
    parameters.add(feedAIncludesLights.set("lights on feed A", true));
    parameters.add(feedBSource.set("feed B (flames/milk/light/shader)", LIGHTING_FEED_B_LIGHT, LIGHTING_FEED_B_FLAMES, LIGHTING_FEED_B_SHADER));

    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        Light &light = lights[i];
        light.group.setName("Light " + ofToString(i + 1));
        light.group.add(light.on.set("on", false));
        light.group.add(light.mode.set("mode (add/reveal)", LIGHTING_MODE_ADD, LIGHTING_MODE_ADD, LIGHTING_MODE_REVEAL));
        light.group.add(light.intensity.set("intensity", 0.8, 0, 2));
        light.group.add(light.hue.set("hue", 0.1 + 0.25 * i, 0, 1));
        light.group.add(light.saturation.set("saturation", 0.2, 0, 1));
        light.group.add(light.x.set("x", 0.2 + 0.2 * i, 0, 1));
        light.group.add(light.y.set("y", 0.5, 0, 1));
        light.group.add(light.size.set("size", 0.2, 0.01, 1));
        light.group.add(light.focus.set("focus", 0.7, 0, 1));
        light.group.add(light.frost.set("frost", 0.2, 0, 1));
        parameters.add(light.group);
    }
}

bool LightingLayer::anyOn() {
    for (Light &light : lights) {
        if (light.on) return true;
    }
    return false;
}

// Shader output modes
#define OUTPUT_LIT 0
#define OUTPUT_MASKED 1
#define OUTPUT_LIGHT 2

static void allocateLike(ofFbo &fbo, ofFbo &source) {
    if (!fbo.isAllocated() || fbo.getWidth() != source.getWidth() || fbo.getHeight() != source.getHeight()) {
        fbo.allocate(source.getWidth(), source.getHeight(), GL_RGBA);
        fbo.begin();
        ofClear(0, 0);
        fbo.end();
    }
}

ofFbo &LightingLayer::apply(ofFbo &source) {
    if (!feedAIncludesLights || !anyOn()) return source;
    allocateLike(result, source);
    render(source, result, OUTPUT_LIT);
    return result;
}

ofFbo &LightingLayer::renderFeedB(ofFbo &flames, ofFbo &milkdrop, ofFbo &shaderOut) {
    allocateLike(feedB, flames);
    if (!anyOn()) {
        // Nothing lit: black, cleared once
        if (feedBWasLit) {
            feedB.begin();
            ofClear(0, 0);
            feedB.end();
            feedBWasLit = false;
        }
        return feedB;
    }
    feedBWasLit = true;
    if (feedBSource == LIGHTING_FEED_B_LIGHT) {
        render(flames, feedB, OUTPUT_LIGHT);
    } else {
        ofFbo &source = feedBSource == LIGHTING_FEED_B_MILKDROP ? milkdrop
                      : feedBSource == LIGHTING_FEED_B_SHADER ? shaderOut : flames;
        render(source, feedB, OUTPUT_MASKED);
    }
    return feedB;
}

bool LightingLayer::feedBUsesMilkDrop() {
    return feedBSource == LIGHTING_FEED_B_MILKDROP && anyOn();
}

bool LightingLayer::feedBUsesFlames() {
    return feedBSource == LIGHTING_FEED_B_FLAMES && anyOn();
}

bool LightingLayer::feedBUsesShader() {
    return feedBSource == LIGHTING_FEED_B_SHADER && anyOn();
}

void LightingLayer::render(ofFbo &source, ofFbo &target, int outputMode) {
    // Compiled on first use, in the main window's GL context
    if (!shader.isLoaded()) {
        shader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
        shader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentShader);
        shader.bindDefaults();
        shader.linkProgram();
    }

    float on[4], mode[4], intensity[4], color[12], pos[8], size[4], focus[4], frost[4];
    for (int i = 0; i < LIGHTING_N_LIGHTS; ++i) {
        Light &light = lights[i];
        ofFloatColor c = ofFloatColor::fromHsb(light.hue, light.saturation, 1);
        on[i] = light.on ? 1 : 0;
        mode[i] = light.mode;
        intensity[i] = light.intensity;
        color[3*i] = c.r; color[3*i+1] = c.g; color[3*i+2] = c.b;
        pos[2*i] = light.x; pos[2*i+1] = light.y;
        size[i] = light.size;
        focus[i] = light.focus;
        frost[i] = light.frost;
    }

    // The shader writes the final colour and alpha itself, so no blending
    target.begin();
    ofClear(0, outputMode == OUTPUT_LIT ? 255 : 0);
    ofPushStyle();
    ofDisableAlphaBlending();
    ofSetColor(255);
    shader.begin();
    shader.setUniform2f("resolution", source.getWidth(), source.getHeight());
    shader.setUniform1f("ambient", ambient);
    shader.setUniform1f("outputMode", outputMode);
    shader.setUniform1fv("lightOn", on, 4);
    shader.setUniform1fv("lightMode", mode, 4);
    shader.setUniform1fv("lightIntensity", intensity, 4);
    shader.setUniform3fv("lightColor", color, 4);
    shader.setUniform2fv("lightPos", pos, 4);
    shader.setUniform1fv("lightSize", size, 4);
    shader.setUniform1fv("lightFocus", focus, 4);
    shader.setUniform1fv("lightFrost", frost, 4);
    source.draw(0, 0, target.getWidth(), target.getHeight());
    shader.end();
    ofPopStyle();
    target.end();
}

static string snapshotPath(int slot) {
    return ofToDataPath("lights/snapshot-" + ofToString(slot) + ".xml", true);
}

void LightingLayer::saveSnapshot(int slot) {
    ofDirectory::createDirectory(ofToDataPath("lights", true), false, true);
    ofxXmlSettings settings;
    settings.addTag("lighting");
    settings.pushTag("lighting");
    ofSerialize(settings, ambient);
    ofSerialize(settings, feedAIncludesLights);
    ofSerialize(settings, feedBSource);
    for (Light &light : lights) {
        ofSerialize(settings, light.group);
    }
    settings.popTag();
    settings.save(snapshotPath(slot));
    ofLogNotice("LightingLayer") << "Saved lighting snapshot " << slot;
}

void LightingLayer::loadSnapshot(int slot) {
    ofxXmlSettings settings;
    if (!settings.load(snapshotPath(slot))) {
        ofLogWarning("LightingLayer") << "No lighting snapshot " << slot;
        return;
    }
    settings.pushTag("lighting");
    ofDeserialize(settings, ambient);
    ofDeserialize(settings, feedAIncludesLights);
    ofDeserialize(settings, feedBSource);
    for (Light &light : lights) {
        ofDeserialize(settings, light.group);
    }
    settings.popTag();
    ofLogNotice("LightingLayer") << "Loaded lighting snapshot " << slot;
}
