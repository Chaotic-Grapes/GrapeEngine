#ifndef PROFILER_H
#define PROFILER_H
#include "ISystem.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <map>
#include <fstream> 
#include <chrono> // for wall-clock time and timelapse

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)*/


class Profiler : public Engine::ISystem {
public:
    /// Call this function before any other methods of the Logger class
    void Initialize() override;

    /// This will update all time variables that need modifications
    /// To be called ONLY by the Engine's Update() loop
    void Update() override;

    /// Debug name
    std::string Name() const override;

};

#endif