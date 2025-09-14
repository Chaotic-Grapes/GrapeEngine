#ifndef DEBUGGER_H
#define DEBUGGER_H
#include "ISystem.h"
#include <iostream>
#include <string>

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)*/


class Debugger : public Engine::ISystem {
public:
    /// Call this function before any other methods of the Logger class
    void Initialize() override;

    /// This will update all time variables that need modifications
    /// To be called ONLY by the Engine's Update() loop
    void Update() override;

    /// Debug name
    std::string Name() const override;


}
#endif // DEBUGGER_H