# Build Validation Checklist

## Post-Refactor Validation Steps

After completing Step 4 (Restructure Project Layout), verify that the build system works correctly.

### ✅ CMake Configuration Tests

```bash
# Test 1: Editor-only build
cmake -B build-editor -DBUILD_EDITOR=ON -DBUILD_GAME=OFF
cmake --build build-editor --config Debug

# Test 2: Runtime-only build
cmake -B build-runtime -DBUILD_EDITOR=OFF -DBUILD_GAME=ON
cmake --build build-runtime --config Debug

# Test 3: Both targets
cmake -B build-both -DBUILD_EDITOR=ON -DBUILD_GAME=ON
cmake --build build-both --config Debug

# Test 4: Shared library
cmake -B build-shared -DBUILD_ENGINE_SHARED=ON
cmake --build build-shared --config Debug

# Test 5: Static library
cmake -B build-static -DBUILD_ENGINE_SHARED=OFF
cmake --build build-static --config Debug
```

### ✅ Expected Outputs

**Editor Build** (BUILD_EDITOR=ON):
```
build/Debug/
├── GrapeEngine.exe              # Editor executable
├── GrapeEngineNative.dll        # Engine library (if shared)
├── imgui.lib                    # ImGui library
└── GrapeEngine.ScriptAPI.dll    # C# API
```

**Runtime Build** (BUILD_GAME=ON):
```
build/Debug/
├── <ProjectName>Game.exe        # Auto-named game executable
├── GrapeEngine.exe              # Alias for P/Invoke compatibility
├── GrapeEngineNative.dll        # Engine library (if shared)
└── assets/                      # Game assets (copied)
```

### ✅ Dependency Validation

**Engine Library (GrapeEngineLib)**:
- [ ] Compiles without ImGui
- [ ] Compiles without editor headers
- [ ] Links: glfw, glad, glm, freetype, fmod, nethost
- [ ] NO dependencies on `editor/` folder

**Editor Executable (GrapeEditor)**:
- [ ] Links GrapeEngineLib
- [ ] Links imgui
- [ ] Includes editor headers
- [ ] Has USE_IMGUI defined
- [ ] Can run independently

**Runtime Executable (GrapeRuntime)**:
- [ ] Links GrapeEngineLib ONLY
- [ ] NO ImGui dependencies
- [ ] NO editor dependencies
- [ ] NO USE_IMGUI defined
- [ ] Auto-names based on project folder

### ✅ Clean Build Test

```bash
# Remove all build artifacts
rm -rf build
rm -rf .vs
rm -rf out

# Fresh configure
cmake -B build

# Build from scratch
cmake --build build --config Debug

# Verify no errors
```

### ✅ Code Validation

**Check for Editor Code in Engine**:
```bash
# Should return NO matches
grep -r "ImGui::" engine/src/
grep -r "#include.*imgui" engine/include/

# Should return NO matches (EditorCamera moved to editor/)
grep -r "EditorCamera" engine/src/
```

**Check for Engine Independence**:
```bash
# Engine should not reference editor/
grep -r "#include.*editor/" engine/

# Should return empty
```

### ✅ Linking Test

**Static Library Mode**:
- [ ] Editor links successfully
- [ ] Runtime links successfully
- [ ] Both can run simultaneously
- [ ] Script API exports from executables

**Shared Library Mode**:
- [ ] GrapeEngineNative.dll created
- [ ] Editor runs with DLL
- [ ] Runtime runs with DLL
- [ ] Script API exports from DLL

### ✅ Functional Tests

**Editor**:
- [ ] Launches without errors
- [ ] ImGui panels render
- [ ] Viewport displays
- [ ] Scene hierarchy works
- [ ] Inspector shows components
- [ ] Asset browser loads
- [ ] C# scripts compile

**Runtime**:
- [ ] Launches without console (Release)
- [ ] Loads scene correctly
- [ ] C# scripts execute
- [ ] Physics runs
- [ ] Rendering works
- [ ] Input responds
- [ ] Audio plays

### ✅ Cross-Platform Build (If Applicable)

**Windows**:
- [ ] MSVC compiler
- [ ] Visual Studio generator
- [ ] MinGW (optional)

**Linux** (future):
- [ ] GCC compiler
- [ ] Makefile generator

**macOS** (future):
- [ ] Clang compiler
- [ ] Xcode generator

### ✅ CMake Target Dependencies

Verify correct build order:
```
1. GrapeEngineLib (engine library)
   ↓
2. GrapeEditor (if BUILD_EDITOR=ON)
   GrapeRuntime (if BUILD_GAME=ON)
   ↓
3. BuildScriptAPI (C# API)
   ↓
4. Build_<ProjectName>Scripts (game scripts)
```

### ✅ Install Test (Optional)

```bash
cmake --install build --prefix install-test
```

Should create:
```
install-test/
├── bin/
│   └── GrapeEngineNative.dll
├── lib/
│   └── GrapeEngineLib.lib
└── include/
    └── [engine headers]
```

## Known Issues & Solutions

### Issue: "Cannot find imgui target"
**Solution**: Ensure ImportDependencies.cmake properly imports imgui BEFORE add_subdirectory(editor)

### Issue: "Undefined reference to ScriptAPI_*"
**Solution**: 
- Shared mode: Check GRAPEENGINE_EXPORTS is defined
- Static mode: Check /EXPORT flags in editor/runtime CMakeLists.txt

### Issue: "GrapeEditor.exe not found"
**Solution**: Build creates `GrapeEngine.exe` (OUTPUT_NAME property in editor/CMakeLists.txt)

### Issue: "EditorCamera missing"
**Solution**: EditorCamera is now in `editor/src/EditorCamera.cpp`, not engine

## Success Criteria

All tests pass when:
- ✅ Engine builds without editor dependencies
- ✅ Editor builds and runs with ImGui
- ✅ Runtime builds and runs without ImGui
- ✅ Both can run simultaneously
- ✅ C# scripting works in both
- ✅ No circular dependencies
- ✅ Clean rebuild from scratch succeeds

## Next Steps After Validation

Once all checks pass:
1. Commit the refactored structure
2. Update team documentation
3. Proceed to Step 5: Build System Testing
4. Begin C# Interop work (EngineInterop project)
