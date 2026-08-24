@echo off

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [HotReload] vcvars64.bat failed
    exit /b 1
)

echo [HotReload] Compiling 2 files...
cl.exe /nologo /MDd /Z7 /EHsc /std:c++latest /DNOMINMAX /c /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Framework\src" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\DirectX\d3dx12" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\DirectX\DirectXTex\include" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\Assimp\include" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\ImGui" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\ImGui\backends" /Fo"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\obj\\" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\Test.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\TestBehavior.cpp" > "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\hotreload_compile.log" 2>&1
if errorlevel 1 (
    echo [HotReload] Compile FAILED - see hotreload_compile.log
    exit /b 1
)

echo [HotReload] Linking...
link.exe /nologo /DLL /OPT:NOREF /DEBUG:NONE /OUT:"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\bin\Debug\GameCode.staged.dll" /IMPLIB:"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\lib\Debug\GameCode.staged.lib" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\lib\Debug\101Framework.lib" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\obj\Test.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\obj\TestBehavior.obj" >> "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\hotreload_compile.log" 2>&1
if errorlevel 1 (
    echo [HotReload] Link FAILED - see hotreload_compile.log
    exit /b 1
)

echo [HotReload] Build succeeded.
