/**
 * @Name: Samantha Leong, 2403088
 * @email: s.leong@digipen.edu
 * @file CrashDumping.cpp
 * @brief Implements the crash handling and logging system for Grape_Engine.
 *
 * This file sets up a global crash handler that catches unhandled exceptions
 * (on Windows via Structured Exception Handling). When a crash occurs,
 * the system:
 *   - Collects crash information such as exception code, address, and reason.
 *   - Appends a call stack trace for easier debugging.
 *   - Writes all details into a timestamped `.txt` crash log file.
 *   - Optionally creates a `.dmp` minidump file for deeper analysis in debuggers.
 *   - Displays a popup message to notify the user of the crash.
 *
 * Platform-specific handling:
 *   - Windows: Uses `SetUnhandledExceptionFilter`, DbgHelp APIs, and minidump writing.
 *   - Linux: (TODO) placeholder for signal-based crash handling.
 *
 * Dependencies:
 *   - Logger system (optional, currently commented out).
 *   - Windows DbgHelp / Shlwapi libraries for stack walking and dump creation.
 *
 * Usage:
 *   Call `CrashDumping::Initialize()` once at program startup to enable crash logging.
 */


#include "CrashDumping.h"
#include "systems/Logger.h"


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
    /**
    * @brief Initializes the crash handler system.
    *
    * On Windows, reserves emergency stack space and sets a global
    * unhandled exception filter that redirects crashes to HandleCrash().
    * On Linux, a TODO placeholder exists for a signal-based handler.
    */
    void CrashDumping::Initialize()
    {
#ifdef _WIN32
        // reserve emergency stack space (for stack overflow handling)
        ULONG stackSize = 32768; // 32KB
        SetThreadStackGuarantee(&stackSize);

        SetUnhandledExceptionFilter(HandleCrash);
#else
        // TODO: Linux handler (signal-based) if you want parity
#endif

       // Logger::Get().Log(LogLevel::INFO, "CrashHandler initialized.");
        LOG_INFO("[CrashHandler] Initialized.");
    }

#ifdef _WIN32
    /**
    * @brief Windows callback for unhandled exceptions.
    *
    * Gathers crash details (exception code, address, reason),
    * appends a call stack trace, and handles logging.
    * Optionally writes a minidump file, then saves details
    * to a timestamped .txt log and shows a popup to the user.
    *
    * @param info Pointer to the exception context.
    * @return LONG Exception handler return code (EXCEPTION_EXECUTE_HANDLER).
    */
    LONG WINAPI CrashDumping::HandleCrash(EXCEPTION_POINTERS* info)
    {
        DWORD code = info->ExceptionRecord->ExceptionCode;

        ostringstream oss;
        oss << "Crash detected!\n\n";
        oss << "Exception code: 0x" << std::hex << code << "\n";
        oss << "Address: 0x" << std::hex
            << (uintptr_t)info->ExceptionRecord->ExceptionAddress << "\n\n";

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

        AppendCallStackToStream(oss, info->ContextRecord);

        string exePath = GetExePath();
        string timeStamp = GetCurrentTimeStamp();

        if (createDump)
        {
            WriteMiniDump(info, exePath, timeStamp);
            oss << "A dump file '" << timeStamp
                << ".dmp' was created at exe root folder.\n";
        }
        else
        {
          //  Logger::Get().Log(LogLevel::DEBUG, "Dump file creation disabled by user.");
           // LOG_INFO("[CrashHandler] Dump creation disabled.");
        }

        WriteLog(oss.str(), exePath, timeStamp);
        CreateErrorPopup(oss.str());

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif // _WIN32
    /**
    * @brief Writes the crash details into a timestamped .txt file.
    *
    * @param message   Crash report contents to write.
    * @param exePath   Path of the executable (used to build log path).
    * @param timeStamp Time label used for unique file naming.
    */
    void CrashDumping::WriteLog(
        const string& message,
        const string& exePath,
        const string& timeStamp)
    {
        string filePath = timeStamp + ".txt";
        string fullPath = (path(exePath) / filePath).string();
        ofstream logFile(fullPath);

        if (!logFile.is_open())
        {
          //  Logger::Get().Log(LogLevel::ERR, "Failed to open crash log file: " + fullPath);
            LOG_ERROR("[CrashHandler] Failed to open crash log file: " << fullPath);
            return;
        }

        logFile << message;
        logFile.close();

       // Logger::Get().Log(LogLevel::INFO, "Crash log written to " + fullPath);
        LOG_INFO("[CrashHandler] Crash log written to " << fullPath); // write for mini dump

    }

#ifdef _WIN32
    /**
    * @brief Creates a minidump (.dmp) file containing crash details.
    *
    * Uses Windows DbgHelp API to write a minidump, which can be loaded
    * into Visual Studio or WinDbg for deep debugging.
    *
    * @param info      Exception pointers captured from the crash.
    * @param exePath   Path of the executable.
    * @param timeStamp Time label used for file naming.
    */
    void CrashDumping::WriteMiniDump(
        EXCEPTION_POINTERS* info,
        const string& exePath,
        const string& timeStamp)
    {
        string filePath = timeStamp + ".dmp";

        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, nullptr, 0);
        std::wstring widePath(sizeNeeded - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, exePath.c_str(), -1, &widePath[0], sizeNeeded);

        // build full path
        widePath += L"\\" + std::wstring(filePath.begin(), filePath.end());

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

    /**
    * @brief Returns a formatted timestamp string for crash logs.
    *
    * Example: "crash_02_14_15_23"
    *
    * @return std::string with the current date/time.
    */
    string CrashDumping::GetCurrentTimeStamp()
    {
        time_t now = time(nullptr);
        tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        ostringstream oss;
        oss << setfill('0')
            << localTime.tm_mday << "_"
            << setw(2) << localTime.tm_hour << "_"
            << setw(2) << localTime.tm_min << "_"
            << setw(2) << localTime.tm_sec;

        string timeStamp = "crash_" + oss.str();

        replace(timeStamp.begin(), timeStamp.end(), ':', '-');
        return timeStamp;
    }
    /**
    * @brief Retrieves the directory path of the executable.
    *
    * Platform-specific:
    *   - Windows: Uses GetModuleFileNameW and PathRemoveFileSpecW.
    *   - Linux: Reads /proc/self/exe and strips file name.
    *
    * @return Executable directory path as a string.
    */
    string CrashDumping::GetExePath()
    {
#ifdef _WIN32
        WCHAR exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        PathRemoveFileSpecW(exePath);

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
        char exePath[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", exePath, PATH_MAX);
        if (count == -1) return "";

        exePath[count] = '\0';
        return std::string(dirname(exePath));
#endif
    }

#ifdef _WIN32
    /**
    * @brief Appends a stack trace to the crash report stream.
    *
    * Walks up to 10 stack frames using DbgHelp StackWalk64 API,
    * resolves symbols and line numbers where possible.
    *
    * @param oss     Output stream to append stack trace info.
    * @param context Processor context captured at crash time.
    */
    void CrashDumping::AppendCallStackToStream(ostringstream& oss, CONTEXT* context)
    {
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
    /**
    * @brief Creates a popup to notify the user of the crash.
    *
    * On Windows: Uses MessageBoxA.
    * On Linux: Uses zenity to display an error dialog.
    * Also prints the crash message to console.
    *
    * @param message Crash details to display in popup.
    */
    void CrashDumping::CreateErrorPopup(const string& message)
    {
        string title = name + " has shut down";

       // Logger::Get().Log(LogLevel::ERR, "Program crashed:\n" + message);
        std::cout << "[CrashHandler] Program crashed:\n" << message << "\n";

#ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
        Shutdown();
#elif __linux__
        string command = "zenity --error --text=\"" + message + "\" --title=\"" + title + "\"";
        system(command.c_str());
        Shutdown();
#endif
    }
}