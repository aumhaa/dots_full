//
//  FlameRenderer.cpp
//  dots
//

#include "FlameRenderer.h"
extern "C" {
#include "variations.h"   // xform_precalc
}

// The genome texture: a row per xform (the final xform after the standard ones), these columns
static const int MAX_XFORMS = 32, MAX_VARIATIONS = 12;
static const int COL_AFFINE = 0,   // c00 c01 c10 c11
                 COL_OFFSET = 1,   // c20 c21 colour colour-speed
                 COL_POST = 2,     // post 00 01 10 11
                 COL_POST_OFFSET = 3,   // post 20 21, opacity, has post
                 COL_INFO = 4,     // cumulative probability, variation count, pre-blur
                 COL_VARIATIONS = 5,    // two a variation: (index, weight, p1, p2), (p3, p4, p5, p6)
                 COLUMNS = COL_VARIATIONS + 2 * MAX_VARIATIONS;

// The variations the shader has: the ones the genebank uses
static const int supported[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 44, 46, 48, 49, 50,
                                51, 52, 53};

static bool isSupported(int variation) {
    return std::find(std::begin(supported), std::end(supported), variation) != std::end(supported);
}

static const char *iterateVertex = R"(#version 330
in vec4 position;          // x, y, colour index, the point's random seed (a 24-bit integer)
out vec4 state;            // the point after this pass's step, captured for the next pass
out vec4 plotColor;

uniform sampler2D xforms;
uniform sampler2D palette;
uniform int numXforms;     // the standard xforms, rows 0..numXforms-1
uniform int finalXform;    // the final xform's row, or -1
uniform uint salt;         // differs every pass
uniform vec2 center;
uniform vec2 rotation;     // the camera's, as cos and sin
uniform vec2 scale;        // flame units to clip space
uniform float reseedChance;
uniform float paletteShift;

const float PI = 3.14159265358979;
const float EPS = 1e-10;

uint rng;
float rnd() {
    rng = rng * 747796405u + 2891336453u;
    uint word = ((rng >> ((rng >> 28u) + 4u)) ^ rng) * 277803737u;
    return float((word >> 22u) ^ word) * (1.0 / 4294967296.0);
}

vec4 cell(int row, int col) { return texelFetch(xforms, ivec2(col, row), 0); }

// One xform on the point (x, y, colour), as flam3's apply_xform
vec3 applyXform(int row, vec3 p, out float opacity) {
    vec4 affine = cell(row, 0), offset = cell(row, 1), post = cell(row, 2), postOffset = cell(row, 3), info = cell(row, 4);
    float colour = mix(p.z, offset.z, offset.w);
    opacity = postOffset.z;
    float tx = affine.x * p.x + affine.z * p.y + offset.x;
    float ty = affine.y * p.x + affine.w * p.y + offset.y;
    if (info.z != 0.0) {   // pre_blur
        float g = info.z * (rnd() + rnd() + rnd() + rnd() - 2.0);
        float a = rnd() * 2.0 * PI;
        tx += g * cos(a);
        ty += g * sin(a);
    }
    float sumsq = tx * tx + ty * ty;
    float r = sqrt(sumsq);
    float atanxy = atan(tx, ty), atanyx = atan(ty, tx);
    float sina = tx / r, cosa = ty / r;
    vec2 q = vec2(0.0);
    int count = int(info.y);
    for (int v = 0; v < count; ++v) {
        vec4 a = cell(row, 5 + 2 * v), b = cell(row, 6 + 2 * v);
        int index = int(a.x);
        float w = a.y, p1 = a.z, p2 = a.w, p3 = b.x, p4 = b.y, p5 = b.z, p6 = b.w;
        switch (index) {
        case 1: q += w * vec2(sin(tx), sin(ty)); break;
        case 2: q += w / (sumsq + EPS) * vec2(tx, ty); break;
        case 3: { float s = sin(sumsq), c = cos(sumsq); q += w * vec2(s * tx - c * ty, c * tx + s * ty); } break;
        case 4: q += w / (r + EPS) * vec2((tx - ty) * (tx + ty), 2.0 * tx * ty); break;
        case 5: q += w * vec2(atanxy / PI, r - 1.0); break;
        case 6: q += w * r * vec2(sin(atanxy + r), cos(atanxy - r)); break;
        case 7: { float a2 = r * atanxy; q += w * r * vec2(sin(a2), -cos(a2)); } break;
        case 8: { float a2 = atanxy / PI, r2 = PI * r; q += w * a2 * vec2(sin(r2), cos(r2)); } break;
        case 9: { float r2 = r + EPS; q += w / r2 * vec2(cosa + sin(r2), sina - cos(r2)); } break;
        case 10: q += w * vec2(sina / (r + EPS), cosa * (r + EPS)); break;
        case 11: q += w * vec2(sina * cos(r), cosa * sin(r)); break;
        case 12: { float n0 = sin(atanxy + r), n1 = cos(atanxy - r);
                   float m0 = n0 * n0 * n0 * r, m1 = n1 * n1 * n1 * r; q += w * vec2(m0 + m1, m0 - m1); } break;
        case 13: { float a2 = 0.5 * atanxy + (rnd() < 0.5 ? PI : 0.0); q += w * sqrt(r) * vec2(cos(a2), sin(a2)); } break;
        case 14: q += w * vec2(tx < 0.0 ? tx * 2.0 : tx, ty < 0.0 ? ty * 0.5 : ty); break;
        case 15: q += w * vec2(tx + affine.z * sin(ty * p1), ty + affine.w * sin(tx * p2)); break;
        case 16: q += 2.0 * w / (r + 1.0) * vec2(ty, tx); break;
        case 17: q += w * vec2(tx + offset.x * sin(tan(3.0 * ty)), ty + offset.y * sin(tan(3.0 * tx))); break;
        case 18: { float dx = w * exp(tx - 1.0), dy = PI * ty; q += dx * vec2(cos(dy), sin(dy)); } break;
        case 19: q += w * pow(r, sina) * vec2(cosa, sina); break;
        case 20: { float a2 = tx * PI; q += w * vec2(cos(a2) * cosh(ty), -sin(a2) * sinh(ty)); } break;
        case 21: { float dx = offset.x * offset.x + EPS;
                   float r2 = w * (mod(r + dx, 2.0 * dx) - dx + r * (1.0 - dx)); q += r2 * vec2(cosa, sina); } break;
        case 22: { float dx = PI * (offset.x * offset.x + EPS), dx2 = 0.5 * dx;
                   float a2 = atanxy + (mod(atanxy + offset.y, dx) > dx2 ? -dx2 : dx2); q += w * r * vec2(cos(a2), sin(a2)); } break;
        case 23: { float r2 = r * (p1 + (p2 - p1) * (0.5 + 0.5 * sin(p3 * atanxy))); q += w * r2 * vec2(sina, cosa); } break;
        case 24: q += w * vec2(sin(p1 * ty) - cos(p2 * tx), sin(p3 * tx) - cos(p4 * ty)); break;
        case 25: { float dx = PI * (p1 * p1 + EPS), dx2 = 0.5 * dx;
                   float t = atanxy + p2 - dx * float(int((atanxy + p2) / dx));
                   float a2 = t > dx2 ? atanxy - dx2 : atanxy + dx2; q += w * r * vec2(sin(a2), cos(a2)); } break;
        case 26: { float dx = p1 * p1 + EPS;
                   float r2 = r - 2.0 * dx * float(int((r + dx) / (2.0 * dx))) + r * (1.0 - dx); q += w * r2 * vec2(sina, cosa); } break;
        case 27: q += 2.0 * w / (r + 1.0) * vec2(tx, ty); break;
        case 28: q += w / (0.25 * sumsq + 1.0) * vec2(tx, ty); break;
        case 29: q += w * vec2(sin(tx), ty); break;
        case 30: { float t = 1.0 / (p1 - ty * p2); q += w * vec2(p1 * tx * t, p3 * ty * t); } break;
        case 31: { float a2 = rnd() * 2.0 * PI, r2 = w * rnd(); q += vec2(tx * r2 * cos(a2), ty * r2 * sin(a2)); } break;
        case 32: { float k = floor(p2 * rnd());
                   float a2 = (atanyx + 2.0 * PI * k) / p1; q += w * pow(sumsq, p3) * vec2(cos(a2), sin(a2)); } break;
        case 33: { int k = int(p2 * rnd());
                   float a2 = ((k & 1) == 0 ? 2.0 * PI * float(k) + atanyx : 2.0 * PI * float(k) - atanyx) / p1;
                   q += w * pow(sumsq, p3) * vec2(cos(a2), sin(a2)); } break;
        case 34: { float a2 = rnd() * 2.0 * PI, r2 = w * rnd(); q += r2 * vec2(cos(a2), sin(a2)); } break;
        case 35: { float a2 = rnd() * 2.0 * PI, r2 = w * (rnd() + rnd() + rnd() + rnd() - 2.0); q += r2 * vec2(cos(a2), sin(a2)); } break;
        case 36: { float g = w * (rnd() + rnd() + rnd() + rnd() - 2.0);
                   float a2 = atanyx + p1 * g, rz = p2 * g - 1.0; q += r * vec2(cos(a2), sin(a2)) + rz * vec2(tx, ty); } break;
        case 37: { float slice = floor(rnd() * p1 + 0.5);
                   float a2 = p2 + 2.0 * PI * (slice + rnd() * p3) / p1; q += w * rnd() * vec2(cos(a2), sin(a2)); } break;
        case 38: { float rf = pow(sumsq, p2 / 2.0), b2 = 2.0 * PI / p1;
                   float phi = atanyx - b2 * floor(atanyx / b2); if (phi > b2 / 2.0) phi -= b2;
                   float amp = (p4 * (1.0 / (cos(phi) + EPS) - 1.0) + p3) / (rf + EPS); q += w * amp * vec2(tx, ty); } break;
        case 39: { float re = 1.0 + p1 * tx + p2 * (tx * tx - ty * ty), im = p1 * ty + 2.0 * p2 * tx * ty;
                   float r2 = w / (re * re + im * im); q += r2 * vec2(tx * re + ty * im, ty * re - tx * im); } break;
        case 40: q += w * vec2(p1 == 0.0 ? tx : (2.0 * floor(tx / p1) + 1.0) * p1 - tx,
                               p2 == 0.0 ? ty : (2.0 * floor(ty / p2) + 1.0) * p2 - ty); break;
        case 44: { float a2 = w * rnd() * PI, r2 = w / (sumsq + EPS), tr = w * tan(a2) * r2; q += tr * vec2(cos(tx), sin(ty)); } break;
        case 46: { float cr = cos(w * r), icr = 1.0 / cr; q += vec2(w * tx, w * (cr < 0.0 ? icr + 1.0 : icr - 1.0)); } break;
        case 48: { float s = tx * tx - ty * ty; q += w * sqrt(1.0 / (s * s + EPS)) * vec2(tx, ty); } break;
        case 49: { float t = p1 * (tx + ty), r2 = w * atanxy / PI; q += r2 * vec2(sin(t) + p3, cos(t) + p2); } break;
        case 50: { float theta = p3 * atanyx + PI / 4.0;
                   float t1 = pow(abs(cos(theta)), p5), t2 = pow(abs(sin(theta)), p6);
                   float r2 = w * ((p1 * rnd() + (1.0 - p1) * r) - p2) * pow(t1 + t2, p4) / r; q += r2 * vec2(tx, ty); } break;
        case 51: q += w * (rnd() - p2) * cos(p1 * atanyx) / r * vec2(tx, ty); break;
        case 52: q += w * (rnd() - p2) * p1 / (1.0 + p1 * tx / r) / r * vec2(tx, ty); break;
        case 53: q += vec2(p1 * w * sin(r) * sin(r) * rnd(), p2 * w * cos(r) * rnd()); break;
        default: q += w * vec2(tx, ty); break;   // linear, and the ones not ported
        }
    }
    if (postOffset.w > 0.0) {
        q = vec2(post.x * q.x + post.z * q.y + postOffset.x, post.y * q.x + post.w * q.y + postOffset.y);
    }
    return vec3(q, colour);
}

bool bad(vec3 p) {
    return any(isnan(p)) || any(isinf(p)) || abs(p.x) > 1e10 || abs(p.y) > 1e10;
}

void main() {
    rng = uint(position.w) ^ (salt * 2654435761u) ^ (uint(gl_VertexID) * 2246822519u);
    rnd();
    vec3 p = position.xyz;
    // A point that ran off, or one chosen at random to keep the flock spread, starts again and
    // runs a few steps before it's plotted, as flam3's fuse
    int steps = 1;
    if (bad(p) || rnd() < reseedChance) {
        p = vec3(rnd() * 2.0 - 1.0, rnd() * 2.0 - 1.0, rnd());
        steps = 20;
    }
    float opacity = 1.0;
    for (int s = 0; s < steps; ++s) {
        float pick = rnd();
        int row = 0;
        while (row < numXforms - 1 && cell(row, 4).x < pick) ++row;
        p = applyXform(row, p, opacity);
    }
    bool lost = bad(p);
    state = vec4(lost ? vec3(1e30) : p, float(rng & 0xFFFFFFu));   // a lost point starts again next pass

    vec3 plotted = p;
    if (finalXform >= 0) {
        float finalOpacity = cell(finalXform, 3).z;
        float ignored;
        if (finalOpacity >= 1.0 || rnd() < finalOpacity) plotted = applyXform(finalXform, p, ignored);
    }
    vec2 d = plotted.xy - center;
    d = vec2(d.x * rotation.x - d.y * rotation.y, d.x * rotation.y + d.y * rotation.x);
    gl_Position = (lost || bad(plotted) || opacity <= 0.0) ? vec4(2.0, 2.0, 2.0, 1.0) : vec4(d * scale * vec2(1.0, -1.0), 0.0, 1.0);
    gl_PointSize = 1.0;
    vec3 colour = texelFetch(palette, ivec2(clamp(int(fract(plotted.z + paletteShift) * 256.0), 0, 255), 0), 0).rgb;
    plotColor = vec4(colour * opacity, opacity);
}
)";

static const char *iterateFragment = R"(#version 330
in vec4 plotColor;
out vec4 fragColor;
void main() {
    fragColor = plotColor;
}
)";

static const char *quadVertex = R"(#version 330
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 uv;
void main() {
    uv = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

// flam3's tone mapping (rect.c), in units where a sample adds 1 to the count: the density goes
// through k1 log(1 + count k2); alpha is that to the 1/gamma (linear near 0); vibrancy mixes the
// mean colour at that alpha with each channel's own gamma curve; the background shows through 1 - alpha
static const char *toneMapFragment = R"(#version 330
in vec2 uv;
out vec4 fragColor;
uniform sampler2D histogram;
uniform float k1, k2, gamma, gammaThreshold, vibrancy;
uniform vec3 background;

float calcAlpha(float density) {
    float g = 1.0 / gamma;
    if (density < gammaThreshold && gammaThreshold > 0.0) {
        float frac = density / gammaThreshold;
        return (1.0 - frac) * density * (pow(gammaThreshold, g) / gammaThreshold) + frac * pow(density, g);
    }
    return pow(density, g);
}

void main() {
    vec4 acc = texture(histogram, uv);
    if (acc.a <= 0.0) {
        fragColor = vec4(background, 1.0);
        return;
    }
    float density = k1 * log(1.0 + acc.a * k2);
    float alpha = clamp(calcAlpha(density), 0.0, 1.0);
    vec3 mean = acc.rgb / acc.a;
    vec3 scaled = acc.rgb * density / acc.a;
    vec3 colour = vibrancy * alpha * mean + (1.0 - vibrancy) * pow(max(scaled, 0.0), vec3(1.0 / gamma));
    fragColor = vec4(clamp(colour + (1.0 - alpha) * background, 0.0, 1.0), 1.0);
}
)";

void FlameRenderer::setup(int width, int height) {
    ofShader::TransformFeedbackSettings settings;
    settings.shaderSources[GL_VERTEX_SHADER] = iterateVertex;
    settings.shaderSources[GL_FRAGMENT_SHADER] = iterateFragment;
    settings.varyingsToCapture = {"state"};
    settings.bufferMode = GL_INTERLEAVED_ATTRIBS;
    if (!iterate.setup(settings)) ofLogError("FlameRenderer") << "the iterating shader didn't build";

    toneMap.setupShaderFromSource(GL_VERTEX_SHADER, quadVertex);
    toneMap.setupShaderFromSource(GL_FRAGMENT_SHADER, toneMapFragment);
    toneMap.bindDefaults();
    toneMap.linkProgram();

    xforms.allocate(COLUMNS, MAX_XFORMS + 1, GL_RGBA32F, false);
    xforms.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    palette.allocate(256, 1, GL_RGBA32F, false);
    palette.setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
    xformData.assign(COLUMNS * (MAX_XFORMS + 1) * 4, 0);
    resize(width, height);
}

void FlameRenderer::resize(int width, int height) {
    if (output.isAllocated() && output.getWidth() == width && output.getHeight() == height) return;
    ofFbo::Settings settings;
    settings.width = width;
    settings.height = height;
    settings.internalformat = GL_RGBA32F;
    settings.textureTarget = GL_TEXTURE_2D;
    histogram.allocate(settings);
    settings.internalformat = GL_RGBA;
    output.allocate(settings);
    quad.clear();
    quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
    quad.addVertex(glm::vec3(0, 0, 0)); quad.addTexCoord(glm::vec2(0, 0));
    quad.addVertex(glm::vec3(width, 0, 0)); quad.addTexCoord(glm::vec2(1, 0));
    quad.addVertex(glm::vec3(width, height, 0)); quad.addTexCoord(glm::vec2(1, 1));
    quad.addVertex(glm::vec3(0, height, 0)); quad.addTexCoord(glm::vec2(0, 1));
    clear();
}

void FlameRenderer::clear() {
    histogram.begin();
    ofClear(0, 0, 0, 0);
    histogram.end();
}

void FlameRenderer::allocatePoints(int count) {
    // Every point starts out lost, so its first pass seeds it and runs the fuse
    vector<glm::vec4> start(count);
    for (int i = 0; i < count; ++i) start[i] = glm::vec4(1e30f, 1e30f, 0, std::floor(ofRandom(1 << 24)));
    for (int i = 0; i < 2; ++i) {
        points[i].allocate(sizeof(glm::vec4) * count, start.data(), GL_DYNAMIC_COPY);
        pointsVbo[i].setVertexBuffer(points[i], 4, sizeof(glm::vec4), 0);
    }
    pointCount = count;
}

int FlameRenderer::unsupportedVariations(const flam3_genome &genome) {
    int missing = 0;
    for (int i = 0; i < genome.num_xforms; ++i) {
        for (int v = 0; v < flam3_nvariations; ++v) {
            if (genome.xform[i].var[v] != 0 && v != VAR_PRE_BLUR && !isSupported(v)) ++missing;
        }
    }
    return missing;
}

// The genome into the xform and palette textures
void FlameRenderer::upload(flam3_genome &genome) {
    std::fill(xformData.begin(), xformData.end(), 0.0f);
    finalRow = -1;
    numStandard = 0;
    double total = 0;
    for (int i = 0; i < genome.num_xforms; ++i) {
        if (i != genome.final_xform_index) total += genome.xform[i].density;
    }
    double cumulative = 0;
    for (int i = 0; i < genome.num_xforms; ++i) {
        flam3_xform &xf = genome.xform[i];
        bool isFinal = i == genome.final_xform_index;
        if (!isFinal && numStandard >= MAX_XFORMS) continue;
        int row = isFinal ? MAX_XFORMS : numStandard++;
        if (isFinal) finalRow = row;
        xform_precalc(&genome, i);
        auto put = [&](int col, float a, float b, float c, float d) {
            float *cellData = &xformData[(row * COLUMNS + col) * 4];
            cellData[0] = a; cellData[1] = b; cellData[2] = c; cellData[3] = d;
        };
        put(COL_AFFINE, xf.c[0][0], xf.c[0][1], xf.c[1][0], xf.c[1][1]);
        put(COL_OFFSET, xf.c[2][0], xf.c[2][1], xf.color, xf.color_speed);
        put(COL_POST, xf.post[0][0], xf.post[0][1], xf.post[1][0], xf.post[1][1]);
        put(COL_POST_OFFSET, xf.post[2][0], xf.post[2][1], isFinal ? xf.opacity : xf.vis_adjusted, xf.has_post ? 1 : 0);
        if (!isFinal) cumulative += total > 0 ? xf.density / total : 0;
        int count = 0;
        for (int v = 0; v < flam3_nvariations && count < MAX_VARIATIONS; ++v) {
            double w = xf.var[v];
            if (w == 0 || v == VAR_PRE_BLUR) continue;
            float p[6] = {0, 0, 0, 0, 0, 0};
            switch (v) {
                case VAR_WAVES: p[0] = xf.waves_dx2; p[1] = xf.waves_dy2; break;
                case VAR_BLOB: p[0] = xf.blob_low; p[1] = xf.blob_high; p[2] = xf.blob_waves; break;
                case VAR_PDJ: p[0] = xf.pdj_a; p[1] = xf.pdj_b; p[2] = xf.pdj_c; p[3] = xf.pdj_d; break;
                case VAR_FAN2: p[0] = xf.fan2_x; p[1] = xf.fan2_y; break;
                case VAR_RINGS2: p[0] = xf.rings2_val; break;
                case VAR_PERSPECTIVE: p[0] = xf.perspective_dist; p[1] = xf.persp_vsin; p[2] = xf.persp_vfcos; break;
                case VAR_JULIAN: p[0] = xf.julian_power; p[1] = xf.julian_rN; p[2] = xf.julian_cn; break;
                case VAR_JULIASCOPE: p[0] = xf.juliascope_power; p[1] = xf.juliascope_rN; p[2] = xf.juliascope_cn; break;
                case VAR_RADIAL_BLUR: p[0] = xf.radialBlur_spinvar; p[1] = xf.radialBlur_zoomvar; break;
                case VAR_PIE: p[0] = xf.pie_slices; p[1] = xf.pie_rotation; p[2] = xf.pie_thickness; break;
                case VAR_NGON: p[0] = xf.ngon_sides; p[1] = xf.ngon_power; p[2] = xf.ngon_circle; p[3] = xf.ngon_corners; break;
                case VAR_CURL: p[0] = xf.curl_c1; p[1] = xf.curl_c2; break;
                case VAR_RECTANGLES: p[0] = xf.rectangles_x; p[1] = xf.rectangles_y; break;
                case VAR_DISC2: p[0] = xf.disc2_timespi; p[1] = xf.disc2_sinadd; p[2] = xf.disc2_cosadd; break;
                case VAR_SUPER_SHAPE:
                    p[0] = xf.super_shape_rnd; p[1] = xf.super_shape_holes; p[2] = xf.super_shape_pm_4;
                    p[3] = xf.super_shape_pneg1_n1; p[4] = xf.super_shape_n2; p[5] = xf.super_shape_n3;
                    break;
                case VAR_FLOWER: p[0] = xf.flower_petals; p[1] = xf.flower_holes; break;
                case VAR_CONIC: p[0] = xf.conic_eccentricity; p[1] = xf.conic_holes; break;
                case VAR_PARABOLA: p[0] = xf.parabola_height; p[1] = xf.parabola_width; break;
            }
            put(COL_VARIATIONS + 2 * count, isSupported(v) ? v : 0, w, p[0], p[1]);
            put(COL_VARIATIONS + 2 * count + 1, p[2], p[3], p[4], p[5]);
            ++count;
        }
        put(COL_INFO, isFinal ? 1 : cumulative, count, xf.has_preblur, 0);
    }
    xforms.loadData(xformData.data(), COLUMNS, MAX_XFORMS + 1, GL_RGBA);

    vector<float> colours(256 * 4);
    for (int i = 0; i < 256; ++i) {
        for (int c = 0; c < 4; ++c) colours[i * 4 + c] = genome.palette[i].color[c];
    }
    palette.loadData(colours.data(), 256, 1, GL_RGBA);
}

void FlameRenderer::render(flam3_genome &genome, const Settings &settings) {
    if (!iterate.isLoaded()) return;
    int wanted = std::max(1000, settings.points);
    if (wanted != pointCount) allocatePoints(wanted);
    upload(genome);

    float width = histogram.getWidth(), height = histogram.getHeight();
    // The genome's pixels per unit are for its own size: fit its height to ours
    double ppu = genome.pixels_per_unit * pow(2.0, genome.zoom) * height / std::max(1, genome.height);
    double angle = genome.rotate * PI / 180.0;

    histogram.begin();
    ofPushStyle();
    glEnable(GL_BLEND);
    // Fade what's there
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    ofSetColor(ofFloatColor(settings.fade, settings.fade, settings.fade, settings.fade));
    ofDrawRectangle(0, 0, width, height);

    // The passes: each moves every point a step and adds where it lands
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    iterate.begin();
    iterate.setUniformTexture("xforms", xforms, 0);
    iterate.setUniformTexture("palette", palette, 1);
    iterate.setUniform1i("numXforms", numStandard);
    iterate.setUniform1i("finalXform", finalRow);
    iterate.setUniform2f("center", genome.center[0], genome.center[1]);
    iterate.setUniform2f("rotation", cos(angle), sin(angle));
    iterate.setUniform2f("scale", 2 * ppu / width, 2 * ppu / height);
    iterate.setUniform1f("reseedChance", 0.001f);
    iterate.setUniform1f("paletteShift", settings.paletteShift);
    // Feedback by hand: openFrameworks's helpers turn rasterizing off (and unbind the shader),
    // and here every step is drawn as well as kept
    GLint saltLocation = glGetUniformLocation(iterate.getProgram(), "salt");
    for (int pass = 0; pass < settings.passes; ++pass) {
        glUniform1ui(saltLocation, ++frame);
        int next = 1 - current;
        points[next].bindBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
        glBeginTransformFeedback(GL_POINTS);
        pointsVbo[current].draw(GL_POINTS, 0, pointCount);
        glEndTransformFeedback();
        points[next].unbindBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
        current = next;
    }
    iterate.end();
    glDisable(GL_PROGRAM_POINT_SIZE);
    ofPopStyle();
    histogram.end();

    // Tone map: k2 makes the density independent of how many samples there are (flam3's sample
    // density), from the samples the histogram holds once its fade settles
    double samples = (double)pointCount * settings.passes / std::max(0.01, 1.0 - settings.fade);
    double contrast = genome.contrast > 0 ? genome.contrast : 1;
    output.begin();
    ofClear(0, 255);
    toneMap.begin();
    toneMap.setUniformTexture("histogram", histogram.getTexture(), 0);
    toneMap.setUniform1f("k1", contrast * genome.brightness * settings.exposure * 268.0 / 256.0);
    toneMap.setUniform1f("k2", ppu * ppu / (contrast * samples));
    toneMap.setUniform1f("gamma", std::max(0.1, genome.gamma * settings.gammaScale));
    toneMap.setUniform1f("gammaThreshold", genome.gam_lin_thresh);
    toneMap.setUniform1f("vibrancy", genome.vibrancy);
    toneMap.setUniform3f("background", genome.background[0], genome.background[1], genome.background[2]);
    quad.draw();
    toneMap.end();
    output.end();
}

