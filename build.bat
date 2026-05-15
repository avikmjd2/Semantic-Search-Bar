@echo off
echo --- Building NPU-Accelerated Hybrid Search ---

:: Ensure we are using the 64-bit MSVC toolchain (OpenVINO libs are x64-only)
:: Detect Visual Studio installation and activate x64 environment
where cl >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [Error] cl.exe not found. Run this from a Developer Command Prompt.
    exit /b 1
)
:: Check if we're already in x64 mode by looking at the cl.exe banner
cl 2>&1 | findstr /C:"x64" >nul
if %ERRORLEVEL% NEQ 0 (
    echo [Setup] Switching to x64 toolchain...
    if exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    ) else if exist "%VS180COMNTOOLS%..\..\VC\Auxiliary\Build\vcvarsall.bat" (
        call "%VS180COMNTOOLS%..\..\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
    ) else (
        echo [Error] Cannot find vcvarsall.bat. Please open an "x64 Native Tools Command Prompt" instead.
        exit /b 1
    )
)

:: THE COMPILER COMMAND (Using elegant relative paths)
cl /EHsc /std:c++17 /O2 /arch:AVX2 /MD ^
   /I"include" ^
   src\main.cpp src\engine\searcher.cpp src\engine\semantic_engine.cpp src\os\indexer.cpp ^
   /Fe:MyHybridSearchApp.exe ^
   /link /LIBPATH:"lib\intel64\Release" openvino.lib

:: POST-BUILD AUTOMATION
if %ERRORLEVEL% EQU 0 (
    echo.
    echo [Success] Compilation Finished!
    
    echo [Setup] Copying required runtime DLLs to the executable directory...
    copy "bin\intel64\Release\*.dll" . >nul
    copy "bin\tbb12.dll" . >nul
    
    echo.
    echo System is ready. Type MyHybridSearchApp.exe to start the engine!
) else (
    echo.
    echo [Fatal] Build Failed. Check the compiler errors above.
)