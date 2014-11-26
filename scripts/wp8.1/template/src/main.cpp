#include "ofApp.h"

//--------------------------------------------------------------
#if defined(TARGET_WINRT)
void ofmain(){
#else
int main(){
#endif
    ofSetupOpenGL(1024, 768, OF_WINDOW);			// <-------- setup the GL context
    ofRunApp(new ofApp()); // start the app

#if defined(TARGET_WINRT)
	return;
#else
	return 0;
#endif
}
