/*
 * Copyright (c) 2013 Dan Wilcox <danomatika@gmail.com>
 *
 * BSD Simplified License.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 *
 * See https://github.com/danomatika/ofxMidi for documentation
 *
 */
#include "ofxRtMidiOut.h"

ofPtr<RtMidiOut> ofxRtMidiOut::s_midiOut;

// -----------------------------------------------------------------------------
ofxRtMidiOut::ofxRtMidiOut(const string name) :
	ofxBaseMidiOut(name), midiOut(RtMidi::UNSPECIFIED, name) {
}

// -----------------------------------------------------------------------------
ofxRtMidiOut::~ofxRtMidiOut() {
	closePort();
}

// -----------------------------------------------------------------------------
void ofxRtMidiOut::listPorts() {
	if(s_midiOut == NULL) {
		s_midiOut = ofPtr<RtMidiOut>(new RtMidiOut(RtMidi::UNSPECIFIED, "ofxMidi Client"));
	}
	ofLogNotice("ofxMidiOut") << s_midiOut->getPortCount() << " ports available";
	for(unsigned int i = 0; i < s_midiOut->getPortCount(); ++i){
		ofLogNotice("ofxMidiOut") <<  i << ": " << s_midiOut->getPortName(i);
	}
}

// -----------------------------------------------------------------------------
vector<string>& ofxRtMidiOut::getPortList() {
	if(s_midiOut == NULL) {
		s_midiOut = ofPtr<RtMidiOut>(new RtMidiOut(RtMidi::UNSPECIFIED, "ofxMidi Client"));
	}
	portList.clear();
	for(unsigned int i = 0; i < s_midiOut->getPortCount(); ++i) {
		portList.push_back(s_midiOut->getPortName(i));
	}
	return portList;
}

// -----------------------------------------------------------------------------
int ofxRtMidiOut::getNumPorts() {
	if(s_midiOut == NULL) {
		s_midiOut = ofPtr<RtMidiOut>(new RtMidiOut(RtMidi::UNSPECIFIED, "ofxMidi Client"));
	}
	return s_midiOut->getPortCount();
}

// -----------------------------------------------------------------------------
string ofxRtMidiOut::getPortName(unsigned int portNumber) {
	if(s_midiOut == NULL) {
		s_midiOut = ofPtr<RtMidiOut>(new RtMidiOut(RtMidi::UNSPECIFIED, "ofxMidi Client"));
	}
	// handle rtmidi exceptions
	try {
		return s_midiOut->getPortName(portNumber);
	}
	catch(RtMidiError& err) {
		ofLogError("ofxMidiOut") << "couldn't get name for port " << portNumber << ": " << err.what();
	}
	return "";
}

// -----------------------------------------------------------------------------
bool ofxRtMidiOut::openPort(unsigned int portNumber) {	
	// handle rtmidi exceptions
	try {
		closePort();
		midiOut.openPort(portNumber, "ofxMidi Output "+ofToString(portNumber));
	}
	catch(RtMidiError& err) {
		ofLogError("ofxMidiOut") << "couldn't open port " << portNumber << ": " << err.what();
		return false;
	}
	portNum = portNumber;
	portName = midiOut.getPortName(portNumber);
	bOpen = true;
	ofLogVerbose("ofxMidiOut") << "opened port " << portNum << " " <<  portName;
	return true;
}

// -----------------------------------------------------------------------------
bool ofxRtMidiOut::openPort(string deviceName) {
	
	// iterate through MIDI ports, find requested device
	int port = -1;
	for(unsigned int i = 0; i < midiOut.getPortCount(); ++i) {
		string name = midiOut.getPortName(i);
		if(name == deviceName) {
			port = i;
			break;
		}
	}
	
	// bail if not found
	if(port == -1) {
		ofLogError("ofxMidiOut") << "port \"" << deviceName << "\" is not available";
		return false;
	} 
	
	return openPort(port);
}

// -----------------------------------------------------------------------------
bool ofxRtMidiOut::openVirtualPort(string portName) {
	// handle rtmidi exceptions
	try {
		closePort();
		midiOut.openVirtualPort(portName);
	}
	catch(RtMidiError& err) {
		ofLogError("ofxMidiOut") << "couldn't open virtual port \"" << portName << "\": " << err.what();
		return false;
	}
	
	this->portName = portName;
	bOpen = true;
	bVirtual = true;
	ofLogVerbose("ofxMidiOut") << "opened virtual port " << portName;
	return true;
}

// -----------------------------------------------------------------------------
void ofxRtMidiOut::closePort() {
	if(bVirtual && bOpen) {
		ofLogVerbose("ofxMidiOut") << "closing virtual port " << portName;
	}
	else if(bOpen && portNum > -1) {
		ofLogVerbose("ofxMidiOut") << "closing port " << portNum << " " << portName;
	}
	midiOut.closePort();
	portNum = -1;
	portName = "";
	bOpen = false;
	bMsgInProgress = false;
	bVirtual = false;
}

// PRIVATE
// -----------------------------------------------------------------------------
void ofxRtMidiOut::sendMessage() {
	// handle rtmidi exceptions
	try {
		midiOut.sendMessage(&message);
	}
	catch(RtMidiError& err) {
		ofLogError("ofxMidiOut") << "couldn't send message: " << err.what();
	}
	bMsgInProgress = false;
}
