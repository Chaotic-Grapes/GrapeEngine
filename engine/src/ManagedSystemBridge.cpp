// ManagedSystemBridge.cpp
// Loads managed DLL and invokes exports provided by the managed scripting layer.

#include "ManagedSystemBridge.h"

#include <windows.h>
#include <string>
#include <iostream>

namespace GrapeEngine::Scripting::NativeBridge
{
    static HMODULE s_managedModule = nullptr;

    using fnCreateInstance_t = uint64_t(__cdecl*)(const char*);
    using fnInvokeOnCreate_t = void(__cdecl*)(uint64_t, void*);
    using fnInvokeOnUpdate_t = void(__cdecl*)(uint64_t, void*);
    using fnInvokeOnUpdateJob_t = intptr_t(__cdecl*)(uint64_t, void*, intptr_t);
    using fnInvokeOnDestroy_t = void(__cdecl*)(uint64_t, void*);

    static fnCreateInstance_t s_createInstance = nullptr;
    static fnInvokeOnCreate_t s_onCreate = nullptr;
    static fnInvokeOnUpdate_t s_onUpdate = nullptr;
    static fnInvokeOnUpdateJob_t s_onUpdateJob = nullptr;
    static fnInvokeOnDestroy_t s_onDestroy = nullptr;

    bool LoadManagedLibrary(const char* dllPath)
    {
        if (s_managedModule)
            return true; // already loaded

        s_managedModule = LoadLibraryA(dllPath);
        if (!s_managedModule)
        {
            std::cerr << "LoadLibraryA failed for " << dllPath << " (" << GetLastError() << ")\n";
            return false;
        }

        s_createInstance = (fnCreateInstance_t)GetProcAddress(s_managedModule, "ManagedSystem_CreateInstanceFromTypeName");
        s_onCreate = (fnInvokeOnCreate_t)GetProcAddress(s_managedModule, "ManagedSystem_InvokeOnCreate");
        s_onUpdate = (fnInvokeOnUpdate_t)GetProcAddress(s_managedModule, "ManagedSystem_InvokeOnUpdate");
        s_onUpdateJob = (fnInvokeOnUpdateJob_t)GetProcAddress(s_managedModule, "ManagedSystem_InvokeOnUpdateJob");
        s_onDestroy = (fnInvokeOnDestroy_t)GetProcAddress(s_managedModule, "ManagedSystem_InvokeOnDestroy");

        // It's okay if some exports are missing (backwards compatibility)
        if (!s_createInstance) std::cerr << "Warning: Managed create-instance export not found.\n";
        if (!s_onCreate) std::cerr << "Warning: Managed OnCreate export not found.\n";
        if (!s_onUpdate) std::cerr << "Warning: Managed OnUpdate export not found.\n";
        if (!s_onUpdateJob) std::cerr << "Warning: Managed OnUpdateJob export not found.\n";
        if (!s_onDestroy) std::cerr << "Warning: Managed OnDestroy export not found.\n";

        return true;
    }

    void UnloadManagedLibrary()
    {
        if (s_managedModule)
        {
            FreeLibrary(s_managedModule);
            s_managedModule = nullptr;
            s_createInstance = nullptr;
            s_onCreate = nullptr;
            s_onUpdate = nullptr;
            s_onUpdateJob = nullptr;
            s_onDestroy = nullptr;
        }
    }

    uint64_t CreateManagedSystemInstance(const char* clrTypeFullNameUtf8)
    {
        if (!s_createInstance)
            return 0;
        return s_createInstance(clrTypeFullNameUtf8);
    }

    void InvokeManagedSystemOnCreate(uint64_t systemHandle, void* worldPtr)
    {
        if (!s_onCreate)
            return;
        s_onCreate(systemHandle, worldPtr);
    }

    void InvokeManagedSystemOnUpdate(uint64_t systemHandle, void* worldPtr)
    {
        if (!s_onUpdate)
            return;
        s_onUpdate(systemHandle, worldPtr);
    }

    intptr_t InvokeManagedSystemOnUpdateJob(uint64_t systemHandle, void* worldPtr, intptr_t dependsOn)
    {
        if (!s_onUpdateJob)
            return 0;
        return s_onUpdateJob(systemHandle, worldPtr, dependsOn);
    }

    void InvokeManagedSystemOnDestroy(uint64_t systemHandle, void* worldPtr)
    {
        if (!s_onDestroy)
            return;
        s_onDestroy(systemHandle, worldPtr);
    }
}
