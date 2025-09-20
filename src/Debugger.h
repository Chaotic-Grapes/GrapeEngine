#ifndef DEBUGGER_H
#define DEBUGGER_H
#include "ISystem.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>

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

    // This implements the Singleton pattern, ensuring only one instance of Logger exists.
    static Debugger& Get() {
        static Debugger instance;
        return instance;
    }

    // Disable copy ctor and copy assignment operator just in case they get created
    Debugger(const Debugger&) = delete;
    Debugger& operator=(const Debugger&) = delete;

    // @brief Checks a condition and logs an error if it's a false.
    // If assertion fails, it logs an error message using Logger and terminates the program.
    // @param condition is checked
    // @param message log if assertion fails
    // @param file where the assertion occured
    // @param line the line number
    void Assert(bool condition, const std::string message, const char* file, int line);
   
    // @brief Prints the value of a variable to the debugger output.
    // Template function to handle different data types
    // @param The name of the variable 
    // @param The value of the variable
    template <typename T>
    void Inspect(const std::string& name, const T& value);

    // @brief Pauses execution and waits for a keypress
    void Break();

private:
    // ctor
    Debugger() = default;

    // Helper function to convert a container to a string for inspection.



};
#endif // DEBUGGER_H