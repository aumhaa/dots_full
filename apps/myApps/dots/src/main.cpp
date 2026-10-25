#include "ofMain.h"
#include "GuiApp.h"
#include "ofAppGLFWWindow.h"
#include "ofApp.h"
#include "StateManager.h"
#include "MidiController.h"

//========================================================================
int main( ){
    ofGLFWWindowSettings settings;
    
    settings.setSize(1080, 720);
    // The viewer starts hidden; dots renders without it (show it from the control window or with v)
    settings.visible = false;
    settings.setPosition(ofVec2f(300,0));
    settings.resizable = true;
    settings.setGLVersion(3, 3);
    shared_ptr<ofAppBaseWindow> mainWindow = ofCreateWindow(settings);
    
    settings.setSize(650, 720);
    settings.visible = true;
    settings.setPosition(ofVec2f(0,0));
    settings.resizable = true;
    settings.shareContextWith = mainWindow;
    shared_ptr<ofAppBaseWindow> guiWindow = ofCreateWindow(settings);


    shared_ptr<StateManager> stateManager(new StateManager());
    stateManager->setup();

    shared_ptr<LightingLayer> lighting(new LightingLayer());
    lighting->setup();
    shared_ptr<ShaderEngine> shaderEngine(new ShaderEngine());   // set up by ofApp, in its GL context

    shared_ptr<GuiApp> guiApp(new GuiApp());
    guiApp->stateManager = stateManager;
    guiApp->lighting = lighting;
    guiApp->shaderEngine = shaderEngine;

    shared_ptr<ofApp> mainApp(new ofApp());
    mainApp->gui = guiApp;
    mainApp->stateManager = stateManager;
    mainApp->lighting = lighting;
    mainApp->shaderEngine = shaderEngine;

    shared_ptr<MidiController> midi(new MidiController());
    midi->setup();
    midi->stateManager = stateManager;
    midi->gui = guiApp;
    midi->lighting = lighting;
    midi->add_listeners();
    mainApp->midi = midi;

    ofRunApp(guiWindow, guiApp);
    ofRunApp(mainWindow, mainApp);
    ofRunMainLoop();
}
