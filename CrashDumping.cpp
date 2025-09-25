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
#define LOG_DEBUG(msg)   Logger::GetInstance().Log("DEBUG", msg)
#define LOG_SUCCESS(msg) Logger::GetInstance().Log("SUCCESS", msg)
#define LOG_ERROR(msg)   Logger::GetInstance().Log("ERROR", msg)

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

        LOG_SUCCESS("CrashHandler initialized.");
    }

#ifdef _WIN32
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
            LOG_DEBUG("Dump file creation disabled by user.");
        }

        WriteLog(oss.str(), exePath, timeStamp);
        CreateErrorPopup(oss.str());

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif // _WIN32

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
            LOG_ERROR("Failed to open crash log file: " + fullPath);
            return;
        }

        logFile << message;
        logFile.close();

        LOG_SUCCESS("Crash log written to " + fullPath);
    }

#ifdef _WIN32
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
            LOG_SUCCESS("Minidump file created.");
        }
        else
        {
            LOG_ERROR("Failed to create minidump file.");
        }
    }
#endif // _WIN32

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

    void CrashDumping::CreateErrorPopup(const string& message)
    {
        string title = name + " has shut down";

        LOG_ERROR("Program crashed:\n" + message);

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