/* Start Header *****************************************************************/
/*!
\file   CrashDumping.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   7th November 2025
\brief
Declares crash handling utilities for Grape_Engine, including initialization,
Windows SEH handling, optional minidump creation, and crash log/popup reporting.
*/
/* End Header *******************************************************************/

#pragma once
#include <string>
#include <sstream>

#include <functional>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Grape_Engine {
    using std::string;
    using std::ostringstream;
    using std::function;

    class CrashDumping
    {
    public:
        // Set the program name shown in crash popup/log.
        static void SetProgramName(const string& newName) { name = newName; }

        // Toggle dump file creation (Windows only).
        static void SetDumpCreateState(bool newState) { createDump = newState; }

        // Set a shutdown callback invoked after crash handling.
        static inline void SetShutdownCallback(function<void()> callback)
        {
            ShutdownCallback = std::move(callback);
        }

        // Initialize the crash handler.
        static void Initialize();

    private:
        static inline bool createDump{ false };
        static inline string name{ "Game" };
        static inline function<void()> ShutdownCallback;

        // Invoke the shutdown callback if set.
        static inline void Shutdown() { if (ShutdownCallback) ShutdownCallback(); }

#ifdef _WIN32
        // Windows SEH crash handler entry point.
        static LONG WINAPI HandleCrash(EXCEPTION_POINTERS* info);
#endif

        // Write crash information to a log file.
        static void WriteLog(const string& message, const string& exePath, const string& timeStamp);

#ifdef _WIN32
        // Create a minidump file capturing crash details.
        static void WriteMiniDump(EXCEPTION_POINTERS* info, const string& exePath, const string& timeStamp);
        // Append call stack information to the output stream.
        static void AppendCallStackToStream(ostringstream& oss, CONTEXT* context);
#endif

        // Show a modal error popup with the given message.
        static void CreateErrorPopup(const string& message);
        // Get the current timestamp as a string.
        static string GetCurrentTimeStamp();
        // Get the executable path.
        static string GetExePath();
    };
}