#include "Debugger.h"
#include <GLFW/glfw3.h>
#include <iostream>

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)
*/

void Debugger::Initialize() {

}


void Debugger::Update() {


	// Update window title with current application state
	std::ostringstream oss;
	// Show FPS information in title bar
	oss << " | FPS: " << /*FPS VALUE*/;

		// Set the window title with formatted string
		glfwSetWindowTitle(/*WINDOW PTR*/, oss.str().c_str());
}


void Assert(bool condition, const std::string message, const char* file, int line) {

}

std::string Debugger::Name() const { return "Debugger System"; }