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
        /**
         * @brief Set program name used in crash output.
         * @param newName Program label shown in popups and logs.
         */
        static void SetProgramName(const string& newName) { name = newName; }

        /**
         * @brief Enable or disable minidump generation.
         * @param newState True to create dumps on crash.
         */
        static void SetDumpCreateState(bool newState) { createDump = newState; }

        /**
         * @brief Register shutdown callback invoked after crash handling.
         * @param callback Callback executed before process termination.
         */
        static inline void SetShutdownCallback(function<void()> callback)
        {
            ShutdownCallback = std::move(callback);
        }

        /**
         * @brief Initialize global crash handling hooks.
         */
        static void Initialize();

    private:
        static inline bool createDump{ false };
        static inline string name{ "Game" };
        static inline function<void()> ShutdownCallback;

        /**
         * @brief Invoke registered shutdown callback if one exists.
         */
        static inline void Shutdown() { if (ShutdownCallback) ShutdownCallback(); }

#ifdef _WIN32
        /**
         * @brief Windows unhandled-exception callback entry point.
         * @param info Exception details provided by Windows.
         * @return Exception handler disposition for the OS.
         */
        static LONG WINAPI HandleCrash(EXCEPTION_POINTERS* info);
#endif

        /**
         * @brief Write crash log text to disk.
         * @param message Crash message content.
         * @param outputDir Output directory for generated files.
         * @param timeStamp Timestamp suffix used in filenames.
         */
        static void WriteLog(const string& message, const string& outputDir, const string& timeStamp);

#ifdef _WIN32
        /**
         * @brief Create a Windows minidump file for current exception.
         * @param info Exception details provided by Windows.
         * @param outputDir Output directory for generated files.
         * @param timeStamp Timestamp suffix used in filenames.
         */
        static void WriteMiniDump(EXCEPTION_POINTERS* info, const string& outputDir, const string& timeStamp);

        /**
         * @brief Append symbolic call stack information into stream.
         * @param oss Output stream receiving formatted call stack.
         * @param context CPU context captured at crash time.
         */
        static void AppendCallStackToStream(ostringstream& oss, CONTEXT* context);
#endif

        /**
         * @brief Display user-facing crash popup with summary message.
         * @param message Error message to display.
         */
        static void CreateErrorPopup(const string& message);

        /**
         * @brief Build timestamp string for crash artifact naming.
         * @return Current timestamp string.
         */
        static string GetCurrentTimeStamp();

        /**
         * @brief Resolve executable directory path.
         * @return Executable directory path string.
         */
        static string GetExePath();
    };
}
