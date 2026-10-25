//
//  SynLibrary.h
//  dots
//
//  dots's own versions of the functions Synesthesia gives every scene, with
//  the same names and signatures (scenes that define their own helpers rely
//  on these exact ones, and only these, being there). Written from the SSF
//  documentation; the noise is the common value noise those signatures
//  (_mod289, _perm) come from. Comes after the uniforms.
//

#pragma once

static const char *synLibrary = R"(
vec2 _xy = gl_FragCoord.xy;
vec2 _uv = _xy / RENDERSIZE.xy;
vec2 _uvc = (_xy - 0.5 * RENDERSIZE.xy) / RENDERSIZE.y;
const float PI = 3.14159265359;

// Meta controls Synesthesia applies after the scene; here they stay neutral
const float _Invert_Media = 0.0;
const float _Media_Contrast = 1.0;
const float __Media_Contrast = 1.0;

// Math
float _scale(float var, float min, float max) { return var * (max - min) + min; }
float _smin(float a, float b, float k) { return -log(exp(-k * a) + exp(-k * b)) / k; }
float _rand(float seed) { return fract(sin(seed) * 43758.5453123); }
float _rand(vec2 seed) { return fract(sin(dot(seed, vec2(12.9898, 4.1414))) * 43758.5453); }
float _pulse(float variable, float center, float width) {
    return smoothstep(center - width, center, variable) - smoothstep(center, center + width, variable);
}
float _sqPulse(float variable, float center, float width) {
    return step(center - width, variable) - step(center + width, variable);
}
float _triWave(float var, float period) {
    float m = mod(var, period);
    return (min(m, period - m) - period * 0.5) / period;
}
float _pixelate(float x, float amount) { return floor(x * amount) / amount; }
vec2 _pixelate(vec2 x, float amount) { return floor(x * amount) / amount; }
vec3 _pixelate(vec3 x, float amount) { return floor(x * amount) / amount; }

// Noise, 0 to 1
float _mod289(float x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 _mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 _perm(vec4 x) { return _mod289(((x * 34.0) + 1.0) * x); }
float _noise(float seed) {
    float i = floor(seed), f = fract(seed);
    return mix(_rand(i), _rand(i + 1.0), f * f * (3.0 - 2.0 * f));
}
float _noise(vec2 seed) {
    vec2 i = floor(seed), f = fract(seed);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(_rand(i), _rand(i + vec2(1.0, 0.0)), u.x),
               mix(_rand(i + vec2(0.0, 1.0)), _rand(i + vec2(1.0, 1.0)), u.x), u.y);
}
float _noise(vec3 p) {
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);
    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = _perm(b.xyxy);
    vec4 k2 = _perm(k1.xyxy + b.zzww);
    vec4 c = k2 + a.zzzz;
    vec4 k3 = _perm(c);
    vec4 k4 = _perm(c + 1.0);
    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));
    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);
    return o4.y * d.y + o4.x * (1.0 - d.y);
}
#define _NUM_OCTAVES 5
float _fbm(float position) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < _NUM_OCTAVES; ++i) { v += a * _noise(position); position = position * 2.0 + 100.0; a *= 0.5; }
    return v;
}
float _fbm(vec2 position) {
    float v = 0.0, a = 0.5;
    mat2 turn = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
    for (int i = 0; i < _NUM_OCTAVES; ++i) { v += a * _noise(position); position = turn * position * 2.0 + vec2(100.0); a *= 0.5; }
    return v;
}
float _fbm(vec3 position) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < _NUM_OCTAVES; ++i) { v += a * _noise(position); position = position * 2.0 + vec3(100.0); a *= 0.5; }
    return v;
}
float _statelessContinuousChaotic(float t) {
    float total = 0.0;
    float primes[10] = float[](2.0, 3.0, 5.0, 7.0, 11.0, 13.0, 17.0, 19.0, 23.0, 29.0);
    for (int i = 0; i < 10; ++i) total += float(i) * sin(primes[i] * t);
    return sin(total);
}

// Coordinates
void _uv2uvc(inout vec2 posIn) { posIn = posIn * 2.0 - 1.0; }
void _uvc2uv(inout vec2 posIn) { posIn = posIn * 0.5 + 0.5; }
vec2 _rotate(vec2 v, float t) { return vec2(cos(t) * v.x + sin(t) * v.y, -sin(t) * v.x + cos(t) * v.y); }
vec2 _toPolar(vec2 v) { return vec2(length(v), atan(v.y / v.x)); }
vec2 _toPolarTrue(vec2 uv) {
    float theta = atan(uv.y, uv.x) / (2.0 * PI);
    return vec2(length(uv), theta < 0.0 ? theta + 1.0 : theta);
}
vec2 _toRect(vec2 v) { return vec2(v.x * cos(v.y), v.x * sin(v.y)); }

// Colour
vec3 _rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 _hsv2rgb(vec3 c) {
    vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}
vec3 _normalizeRGB(float r, float g, float b) { return clamp(vec3(r, g, b), 0.0, 255.0) / 255.0; }
vec3 _palette(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d) { return a + b * cos(6.28318 * (c * t + d)); }
vec4 _brightness(vec4 colIn, float brightnessVal) { colIn.rgb = clamp(colIn.rgb + brightnessVal, 0.0, 1.0); return colIn; }
vec4 _gamma(vec4 colIn, float gammaVal) { colIn.rgb = pow(colIn.rgb, vec3(1.0 / gammaVal)); return colIn; }
float _contrastMath(float brightnessIn, float contrastVal) {
    if (contrastVal > 1.0) contrastVal = pow(contrastVal, 3.0);
    contrastVal = max(contrastVal, 0.001);
    return clamp((brightnessIn - 0.5) * contrastVal + 0.5, 0.0, 1.0);
}
vec4 _contrast(vec4 colIn, float contrastVal) {
    vec3 hsv = _rgb2hsv(clamp(colIn.rgb, 0.0, 1.0));
    hsv.z = _contrastMath(hsv.z, contrastVal);
    return vec4(_hsv2rgb(hsv), colIn.a);
}
vec4 _hueAndSaturation(vec4 colIn, float hueVal, float saturationVal) {
    vec3 hsv = _rgb2hsv(clamp(colIn.rgb, 0.0, 1.0));
    hsv.x += hueVal;
    hsv.y = clamp(hsv.y + saturationVal, 0.0, 1.0);
    return vec4(_hsv2rgb(hsv), colIn.a);
}
vec4 _hueSaturationContrast(vec4 colIn, float hueVal, float saturationVal, float contrastVal) {
    vec3 hsv = _rgb2hsv(clamp(colIn.rgb, 0.0, 1.0));
    hsv.x += hueVal;
    hsv.y = clamp(hsv.y + saturationVal, 0.0, 1.0);
    hsv.z = _contrastMath(hsv.z, contrastVal);
    return vec4(_hsv2rgb(hsv), colIn.a);
}
vec4 _invertImage(vec4 colIn) { return mix(colIn, 1.0 - colIn, _Invert_Media); }
vec4 _imageToMask(vec4 colIn) { return vec4(step(0.5, dot(colIn.rgb, vec3(0.299, 0.587, 0.114)))); }

// Media. With none loaded, syn_UserImage is a single pixel and _exists() is false.
bool _exists(sampler2D tex) { ivec2 size = textureSize(tex, 0); return size.x > 1 || size.y > 1; }
bool _isMediaActive() { return syn_MediaType > 0.5; }
vec2 _invertYAxisVideo(vec2 posIn) { return syn_MediaType > 1.5 ? vec2(posIn.x, 1.0 - posIn.y) : posIn; }
// Fills the picture with the image, keeping its proportions
vec2 _correctImageCoords(vec2 imageSize, vec2 uv) {
    float screen = RENDERSIZE.x / RENDERSIZE.y, image = imageSize.x / max(imageSize.y, 1.0);
    vec2 c = uv - 0.5;
    if (screen > image) c.y *= image / screen; else c.x *= screen / image;
    return c + 0.5;
}
vec2 _correctImageCoords(vec2 imageSize) { return _correctImageCoords(imageSize, _uv); }
vec2 _correctUserImageCoords(vec2 uv) { return _correctImageCoords(vec2(textureSize(syn_UserImage, 0)), uv); }
vec4 _textureImage(sampler2D imageName, vec2 uv, float isUserImage) {
    return texture(imageName, isUserImage > 0.5 ? _invertYAxisVideo(uv) : uv);
}
vec4 _textureImage(sampler2D imageName, vec2 uv) { return _textureImage(imageName, uv, 0.0); }
vec4 _textureImageAsMask(sampler2D imageName, vec2 uv, float isUserImage) { return _imageToMask(_textureImage(imageName, uv, isUserImage)); }
vec4 _textureImageAsMask(sampler2D imageName, vec2 uv) { return _textureImageAsMask(imageName, uv, 0.0); }
vec4 _loadImage(sampler2D imageName, vec2 offset, float isUserImage) {
    return _textureImage(imageName, _correctImageCoords(vec2(textureSize(imageName, 0))) + offset, isUserImage);
}
vec4 _loadImage(sampler2D imageName, vec2 offset) { return _loadImage(imageName, offset, 0.0); }
vec4 _loadImage(sampler2D imageName) { return _loadImage(imageName, vec2(0.0), 0.0); }
vec4 _loadImageAsMask(sampler2D imageName, vec2 offset, float isUserImage) { return _imageToMask(_loadImage(imageName, offset, isUserImage)); }
vec4 _loadImageAsMask(sampler2D imageName, vec2 offset) { return _loadImageAsMask(imageName, offset, 0.0); }
vec4 _loadImageAsMask(sampler2D imageName) { return _loadImageAsMask(imageName, vec2(0.0), 0.0); }
vec4 _loadUserImage(vec2 offset) { return _loadImage(syn_UserImage, offset, 1.0); }
vec4 _loadUserImage() { return _loadUserImage(vec2(0.0)); }
vec4 _loadUserImageAsMask(vec2 offset) { return _imageToMask(_loadUserImage(offset)); }
vec4 _loadUserImageAsMask() { return _loadUserImageAsMask(vec2(0.0)); }
vec4 _textureUserImage(vec2 uv) { return _textureImage(syn_UserImage, uv, 1.0); }
vec4 _textureUserImageAsMask(vec2 uv) { return _imageToMask(_textureUserImage(uv)); }
)";
