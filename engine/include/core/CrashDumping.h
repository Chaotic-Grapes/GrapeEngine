/* Start Header *****************************************************************/
/*!
\file   CrashDumping.h
\author Samantha Leong (100%)
\par    s.leong@digipen.edu

\brief
Declaration of the CrashDumping class for handling application crashes,
including generating dump files and logging error information. Provides mechanisms
to set program name, toggle dump creation, and register shutdown callbacks.
*/
/* End Header *******************************************************************/

#pragma once
#include <string>
#include <sstream>

#include <functional>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Engine {
    using std::string;
    using std::ostringstream;
    using std::function;

    class CrashDumping
    {
    public:
        /// <summary>
        /// Assign a name that will be displayed in the crash popup/log.
        /// </summary>
        static void SetProgramName(const string& newName) { name = newName; }

        /// <summary>
        /// Toggle dump file creation (Windows only).
        /// </summary>
        static void SetDumpCreateState(bool newState) { createDump = newState; }

        /// <summary>
        /// Assign a shutdown function to be called after crash handling.
        /// </summary>
        static inline void SetShutdownCallback(function<void()> callback)
        {
            ShutdownCallback = std::move(callback);
        }

        /// <summary>
        /// Initialize the crash handler.
        /// </summary>
        static void Initialize();

    private:
        static inline bool createDump{ false };
        static inline string name{ "Game" };
        static inline function<void()> ShutdownCallback;

        // Handle shutdown callback.
        static inline void Shutdown() { if (ShutdownCallback) ShutdownCallback(); }

#ifdef _WIN32
        static LONG WINAPI HandleCrash(EXCEPTION_POINTERS* info);
#endif

        static void WriteLog(const string& message, const string& outputDir, const string& timeStamp);

#ifdef _WIN32
        static void WriteMiniDump(EXCEPTION_POINTERS* info, const string& outputDir, const string& timeStamp);
        static void AppendCallStackToStream(ostringstream& oss, CONTEXT* context);
#endif

        static void CreateErrorPopup(const string& message);
        static string GetCurrentTimeStamp();
        static string GetExePath();
    };
}
