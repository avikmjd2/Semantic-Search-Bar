CXX = cl
CXXFLAGS = /EHsc /std:c++17 /O2 /arch:AVX2 /MD /D NOMINMAX /I"include" /I"webview" /I"WebView2\include"

# Linker flags and libraries
LDFLAGS = /link /LIBPATH:"lib\intel64\Release" /LIBPATH:"WebView2\x64"
LIBS = openvino.lib WebView2Loader.dll.lib advapi32.lib ole32.lib shell32.lib shlwapi.lib user32.lib version.lib

# Target executable
TARGET = MyHybridSearchApp.exe

# Source files
SRCS = src\main.cpp src\engine\searcher.cpp src\engine\semantic_engine.cpp src\os\indexer.cpp

all: $(TARGET) copy_dlls

# Build target
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) /Fe:$(TARGET) $(LDFLAGS) $(LIBS)

# Copy required runtime DLLs
copy_dlls:
	@echo Copying required runtime DLLs to the executable directory...
	@copy /Y "bin\intel64\Release\*.dll" . >nul 2>&1 || exit 0
	@copy /Y "bin\tbb12.dll" . >nul 2>&1 || exit 0
	@copy /Y "WebView2\x64\WebView2Loader.dll" . >nul 2>&1 || exit 0
	@echo System is ready. Type $(TARGET) to start the engine!

# Clean up build artifacts
clean:
	@del /Q *.obj $(TARGET) >nul 2>&1 || exit 0
	@echo Cleaned up build artifacts.
