// ManagedSystemBridge.h
// Lightweight helper to load managed exports and invoke managed system entrypoints

#pragma once

#include <cstdint>

namespace GrapeEngine::Scripting::NativeBridge
{
    // Load the managed host/library that contains the exported functions.
    // Returns true on success.
    bool LoadManagedLibrary(const char* dllPath);

    // Unload the managed library if loaded.
    void UnloadManagedLibrary();

    // Create an instance of a managed system by CLR type full name.
    // Returns assigned system handle (0 on failure).
    uint64_t CreateManagedSystemInstance(const char* clrTypeFullNameUtf8);

    // Invoke lifecycle methods on the managed system by handle.
    void InvokeManagedSystemOnCreate(uint64_t systemHandle, void* worldPtr);
    void InvokeManagedSystemOnUpdate(uint64_t systemHandle, void* worldPtr);

    // Invoke update that returns a native job handle (intptr_t). Returns 0 when none.
    intptr_t InvokeManagedSystemOnUpdateJob(uint64_t systemHandle, void* worldPtr, intptr_t dependsOn);

    void InvokeManagedSystemOnDestroy(uint64_t systemHandle, void* worldPtr);
}
