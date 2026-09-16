@echo off

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [HotReload] vcvars64.bat failed
    exit /b 1
)

echo [HotReload] Compiling 9 files...
cl.exe /nologo /MD /O2 /Ob2 /DNDEBUG /D_ITERATOR_DEBUG_LEVEL=0 /Z7 /EHsc /std:c++latest /DNOMINMAX /c /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Framework\src" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\DirectX\d3dx12" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\DirectX\DirectXTex\include" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\Assimp\include" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\ImGui" /I"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\third_party\ImGui\backends" /Fo"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\\" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\Bullet.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\Explosion.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\FixedTarget.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\GameManager.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\GameUIManager.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\Player.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\Test.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\TestBehavior.cpp" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\Game\GameCode\TitleManager.cpp" > "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\hotreload_compile.log" 2>&1
if errorlevel 1 (
    echo [HotReload] Compile FAILED - see hotreload_compile.log
    exit /b 1
)

echo [HotReload] Linking...
link.exe /nologo /DLL /OPT:NOREF /DEBUG:NONE /OUT:"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\bin\Release\GameCode.staged.dll" /IMPLIB:"C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\lib\Release\GameCode.staged.lib" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\lib\Release\101Framework.lib" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\Bullet.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\Explosion.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\FixedTarget.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\GameManager.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\GameUIManager.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\Player.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\Test.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\TestBehavior.obj" "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\build\GameCode_hotreload\Release\obj\TitleManager.obj" >> "C:\Users\kamka\デスクトップ\101Engine-developing\101Engine\hotreload_compile.log" 2>&1
if errorlevel 1 (
    echo [HotReload] Link FAILED - see hotreload_compile.log
    exit /b 1
)

echo [HotReload] Build succeeded.
