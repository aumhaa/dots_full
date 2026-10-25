//
//  MediaInput.h
//  dots
//
//  What Synesthesia scenes get as media (syn_UserImage): nothing, the flames,
//  MilkDrop, a Syphon feed, or an image or video file. Each frame the source
//  is drawn into one GL_TEXTURE_2D texture at its own size. Videos and live
//  sources are drawn top row first, as Synesthesia's videos come (scenes flip
//  them with _invertYAxisVideo); images are drawn the other way up, as
//  Synesthesia's images come.
//

#pragma once

#include "ofMain.h"
#include "ofxSyphon.h"
#include <functional>

// What a shader gets: the texture (null for none) and Synesthesia's syn_MediaType
struct ShaderMedia {
    const ofTexture *texture = nullptr;
    int type = 0;   // 0 none, 1 image, 2 video, 3 live (Synesthesia's webcam)
};

class MediaInput {
public:
    enum Kind { NONE, FLAMES, MILKDROP, SYPHON, FILE, N_KINDS };

    void setup();

    // The kind of source; a Syphon feed or file stays chosen while another kind is in use
    void select(int kind);
    int getKind() const { return kind; }
    // An image or video; false if it can't be opened
    bool loadFile(const string &path);
    void useSyphon(const string &serverName, const string &appName);

    // Draws the source into the media texture (the flames and MilkDrop pictures as they are now)
    void update(ofFbo &flames, ofFbo &milkdrop);
    const ShaderMedia &get() const { return media; }

    bool usesFlames() const { return kind == FLAMES; }
    bool usesMilkDrop() const { return kind == MILKDROP; }
    string describe() const;
    const string &getFile() const { return file; }
    const string &getSyphonServer() const { return syphonServer; }
    const string &getSyphonApp() const { return syphonApp; }

    // The Syphon feeds there are now, and the image and video files in Synesthesia's
    // media folders (its media.json) and bin/data/media
    const vector<ofxSyphonServerDescription> &getSyphonServers() const { return directory.getServerList(); }
    static vector<string> listFiles();
    static bool isVideo(const string &path);

private:
    // Into the media texture at width x height: drawSource draws the source at (0, 0, w, h)
    void draw(float width, float height, bool flip, int type, std::function<void(float, float, float, float)> drawSource);

    int kind = NONE;
    ShaderMedia media;
    ofFbo fbo;

    string file;
    bool fileIsVideo = false, fileOk = false;
    ofVideoPlayer video;
    ofImage image;

    string syphonServer, syphonApp;
    ofxSyphonClient syphon;
    ofxSyphonServerDirectory directory;
};
