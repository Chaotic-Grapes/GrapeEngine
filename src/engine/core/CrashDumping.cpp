/* Start Header *****************************************************************/
/*!
\file   CrashDumping.cpp
\author Samantha Leong (100%)
\par    s.leong@digipen.edu
\date   7th November 2025
\brief
Provides robust crash handling that captures exception details, composes a
human‑readable report, optionally writes a Windows minidump, and notifies the
user via a popup.

Responsibilities:
- Initialize platform crash handling at startup (SEH filter on Windows).
- Detect unhandled exceptions, derive human‑friendly reason, and capture call stack.
- Persist a timestamped crash report (.txt) and optional minidump (.dmp) in the
  executable directory.

Usage:
- Call `CrashDumping::Initialize()` during application startup.
- Configure program name and dump creation: `SetProgramName("GrapeEngine");`
  `SetDumpCreateState(true);`
- On crash, files will be named `<timestamp>.txt` and `<timestamp>.dmp` beside the exe.

Platform Notes:
- Windows: Uses `SetUnhandledExceptionFilter`, `DbgHelp` (`MiniDumpWriteDump`, `StackWalk64`),
  and `Shlwapi` for path manipulation.
- Linux: Placeholder for signal‑based handling if parity is desired.

Dependencies:
- C++ standard library (`chrono`, `filesystem`, `sstream`, etc.).
- Windows: `DbgHelp.lib`, `Shlwapi.lib`.

Performance:
- No runtime overhead in normal operation; work only occurs on crash paths.
*/
/* End Header *******************************************************************/


#include "core/CrashDumping.h"
#include "core/Logger.h"

#include <iostream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#include <dbghelp.h>
#include <shlwapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "Shlwapi.lib")
#else
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#endif

// logging shortcuts mapped to your Logger system
//#define LOG_DEBUG(msg)   Logger::GetInstance().Log("DEBUG", msg)
//#define LOG_SUCCESS(msg) Logger::GetInstance().Log("SUCCESS", msg)
//#define LOG_ERROR(msg)   Logger::GetInstance().Log("ERROR", msg)

using std::string;
using std::ostringstream;
using std::ofstream;
using std::setw;
using std::setfill;
using std::replace;
using std::time_t;
using std::tm;
using std::filesystem::path;

namespace Grape_Engine
{
    // Initialize global crash handler and platform-specific setup.
    void CrashDumping::Initialize()
    {
#ifdef _WIN32
        // Reserve emergency stack space (for stack overflow handling).
        ULONG stackSize = 32768; // 32KB
        SetThreadStackGuarantee(&stackSize);
        // Register SEH unhandled exception filter.
        SetUnhandledExceptionFilter(HandleCrash);
#else
        // TODO: Linux handler (signal-based) if you want parity.
#endif
        // Inform via logger that crash handling is initialized.
       // Logger::Get().Log(LogLevel::INFO, "CrashHandler initialized.");
        LOG_INFO("[CrashHandler] Initialized.");
    }

#ifdef _WIN32
    // Windows SEH callback for unhandled exceptions; composes report and handles output.
    LONG WINAPI CrashDumping::HandleCrash(EXCEPTION_POINTERS* info)
    {
        // Capture exception code and address.
        DWORD code = info->ExceptionRecord->ExceptionCode;

        ostringstream oss;
        oss << "Crash detected!\n\n";
        oss << "Exception code: 0x" << std::hex << code << "\n";
        oss << "Address: 0x" << std::hex
            << (uintptr_t)info->ExceptionRecord->ExceptionAddress << "\n\n";

        // Map common exception codes to human-readable reasons.
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION:
        {
            const ULONG_PTR* accessType = info->ExceptionRecord->ExceptionInformation;
            const char* accessStr = "unknown";

            switch (accessType[0])
            {
            case 0: accessStr = "read from"; break;
            case 1: accessStr = "write to"; break;
            case 8: accessStr = "execute"; break;
            }

            oss << "Reason: Access violation - attempted to "
                << accessStr << " invalid memory at address 0x"
                << std::hex << accessType[1];

            if (accessType[0] == 8)
                oss << " (possible code execution or exploit attempt)";
            oss << "\n";
            break;
        }
        case EXCEPTION_STACK_OVERFLOW:
            oss << "Reason: Stack overflow (likely infinite recursion)\n";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            oss << "Reason: Integer divide by zero\n";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            oss << "Reason: Illegal CPU instruction executed\n";
            break;
        case EXCEPTION_BREAKPOINT:
            oss << "Reason: Breakpoint hit (INT 3 instruction)\n";
            break;
        case EXCEPTION_GUARD_PAGE:
            oss << "Reason: Guard page accessed (stack/memory protection violation)\n";
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            oss << "Reason: Privileged instruction executed in user mode\n";
            break;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            oss << "Reason: Non-continuable exception triggered\n";
            break;
        case EXCEPTION_IN_PAGE_ERROR:
            oss << "Reason: Memory access failed (I/O or paging error)\n";
            break;
        default:
            oss << "Reason: Unknown exception (code: 0x" << std::hex << code << ")\n";
            break;
        }

        // Append stack trace to the report.
        AppendCallStackToStream(oss, info->ContextRecord);

        // Resolve paths and timestamp used for file naming.
        string exePath = GetExePath();
        string timeStamp = GetCurrentTimeStamp();

        if (createDump)
        {
            // Write minidump file to executable directory.
            WriteMiniDump(info, exePath, timeStamp);
            oss << "A dump file '" << timeStamp
                << ".dmp' was created at exe root folder.\n";
        }
        else
        {
          //  Logger::Get().Log(LogLevel::DEBUG, "Dump file creation disabled by user.");
           // LOG_INFO("[CrashHandler] Dump creation disabled.");
        }

        // Persist the crash report and notify user.
        WriteLog(oss.str(), exePath, timeStamp);
        CreateErrorPopup(oss.str());

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif // _WIN32
    // Write crash details into a timestamped .txt file at exe directory.
    void CrashDumping::WriteLog(
        const string& message,
        const string& exePath,
        const string& timeStamp)
    {
        // Compose crash log file path.
        string filePath = timeStamp + ".txt";
        string fullPath = (path(exePath) / filePath).string();
        ofstream logFile(fullPath);

        if (!logFile.is_open())
        {
          //  Logger::Get().Log(LogLevel::ERR, "Failed to open crash log file: " + fullPath);
            LOG_ERROR("[CrashHandler] Failed to open crash log file: " << fullPath);
            return;
        }

        // Write report and close file.
        logFile << message;
        logFile.close();

       // Logger::Get().Log(LogLevel::INFO, "Crash log written to " + fullPath);
        LOG_INFO("[CrashHandler] Crash log written to " << fullPath); // write for mini dump

    }

#ifdef _WIN32
    // Create a minidump (.dmp) file containing crash details using DbgHelp.
    void CrashDumping::WriteMiniDump(
        EXCEPTION_POINTERS* info,
        const string& exePath,
        const string& timeStamp)
    {
        // Dump filename (timestamp-based).
        string filePath = timeStamp + ".dmp";

        // Convert exe path to wide-char and build full dump path.
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, nullptr, 0);
        std::wstring widePath(sizeNeeded - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, &widePath[0], sizeNeeded);

        // build full path
        widePath += L"\\" + std::wstring(filePath.begin(), filePath.end());

        // Create the .dmp file.
        HANDLE hFile = CreateFileW(
            widePath.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (hFile != INVALID_HANDLE_VALUE)
        {
            // Populate dump info and write minidump.
            MINIDUMP_EXCEPTION_INFORMATION dumpInfo{};
            dumpInfo.ThreadId = GetThreadId(GetCurrentThread());
            dumpInfo.ExceptionPointers = info;
            dumpInfo.ClientPointers = FALSE;

            constexpr MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
                MiniDumpWithIndirectlyReferencedMemory |
                MiniDumpScanMemory |
                MiniDumpWithThreadInfo |
                MiniDumpWithUnloadedModules);

            MiniDumpWriteDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                hFile,
                dumpType,
                &dumpInfo,
                nullptr,
                nullptr);

            // Close handle and report success.
            CloseHandle(hFile);
           // Logger::Get().Log(LogLevel::INFO, "Minidump file created.");
            std::cout << "[CrashHandler] Minidump file created.\n"; // need the full file path
        }
        else
        {
           // Logger::Get().Log(LogLevel::ERR, "Failed to create minidump file.");
            std::cout << "[CrashHandler] Failed to create minidump file.\n"; // need the full file path
        }
    }
#endif // _WIN32
    // Return formatted timestamp string for crash logs (e.g., "crash_02_14_15_23").
    string CrashDumping::GetCurrentTimeStamp()
    {
        // Get local time now.
        time_t now = time(nullptr);
        tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        // Format day_hour_min_sec into crash_ prefix.
        ostringstream oss;
        oss << setfill('0')
            << localTime.tm_mday << "_"
            << setw(2) << localTime.tm_hour << "_"
            << setw(2) << localTime.tm_min << "_"
            << setw(2) << localTime.tm_sec;

        string timeStamp = "crash_" + oss.str();

        // Replace any colon with dash (defensive; not expected in this format).
        replace(timeStamp.begin(), timeStamp.end(), ':', '-');
        return timeStamp;
    }
    // Retrieve the directory path of the executable (platform-specific).
    string CrashDumping::GetExePath()
    {
#ifdef _WIN32
        // Query executable path and strip filename.
        WCHAR exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        PathRemoveFileSpecW(exePath);

        // Convert wide-char path to UTF-8 string.
        int sizeNeeded = WideCharToMultiByte(
            CP_UTF8,
            0,
            exePath,
            -1,
            nullptr,
            0,
            nullptr,
            nullptr);

        string exePathStr(sizeNeeded - 1, 0);
        WideCharToMultiByte(
            CP_UTF8,
            0,
            exePath,
            -1,
            &exePathStr[0],
            sizeNeeded,
            nullptr,
            nullptr);

        return exePathStr;
#else
        // Read /proc/self/exe symlink and return directory name.
        char exePath[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
        if (count == -1) return "";

        exePath[count] = '\0';
        return std::string(dirname(exePath));
#endif
    }

#ifdef _WIN32
    // Append a stack trace to the crash report stream using DbgHelp.
    void CrashDumping::AppendCallStackToStream(ostringstream& oss, CONTEXT* context)
    {
        // Initialize symbol handler for the current process.
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();

        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, nullptr, TRUE);

        STACKFRAME64 stack{};
#if defined(_M_X64)
        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
        stack.AddrPC.Offset = context->Rip;
        stack.AddrFrame.Offset = context->Rbp;
        stack.AddrStack.Offset = context->Rsp;
#else
        DWORD machineType = IMAGE_FILE_MACHINE_I386;
        stack.AddrPC.Offset = context->Eip;
        stack.AddrFrame.Offset = context->Ebp;
        stack.AddrStack.Offset = context->Esp;
#endif

        stack.AddrPC.Mode = AddrModeFlat;
        stack.AddrFrame.Mode = AddrModeFlat;
        stack.AddrStack.Mode = AddrModeFlat;

        oss << "\n========================================\n";
        oss << "Call stack:\n";

        // Walk up to 10 frames, resolving symbols and line info.
        for (int i = 0; i < 10; ++i)
        {
            if (!StackWalk64(
                machineType,
                process,
                thread,
                &stack,
                context,
                nullptr,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr))
                break;

            DWORD64 addr = stack.AddrPC.Offset;
            if (addr == 0) break;

            char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;

            oss << "  " << i << ": ";
            DWORD64 displacement = 0;
            if (SymFromAddr(process, addr, &displacement, symbol))
            {
                oss << symbol->Name;
            }
            else
            {
                oss << "(symbol not found)";
            }

            IMAGEHLP_LINE64 lineInfo{};
            DWORD lineDisplacement = 0;
            lineInfo.SizeOfStruct = sizeof(lineInfo);

            if (SymGetLineFromAddr64(process, addr, &lineDisplacement, &lineInfo))
            {
                oss << " (line " << lineInfo.LineNumber << ")";
            }

            oss << " [0x" << std::hex << addr << "]\n";
        }

        SymCleanup(process);
        oss << "========================================\n\n";
    }
#endif // _WIN32
    // Create a popup to notify the user of the crash and then shutdown.
    void CrashDumping::CreateErrorPopup(const string& message)
    {
        // Compose popup title and log to console.
        string title = name + " has shut down";

       // Logger::Get().Log(LogLevel::ERR, "Program crashed:\n" + message);
        std::cout << "[CrashHandler] Program crashed:\n" << message << "\n";

#ifdef _WIN32
        // Windows native popup.
        MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
        Shutdown();
#elif __linux__
        // Linux popup via zenity.
        string command = "zenity --error --text=\"" + message + "\" --title=\"" + title + "\"";
        system(command.c_str());
        Shutdown();
#endif
    }
}