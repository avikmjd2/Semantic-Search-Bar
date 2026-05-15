cmake_minimum_required(VERSION 3.10)
project(MySearchApp)

# Force C++ 17 Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Gather all source files
set(SOURCES
    src/main.cpp
    src/engine/searcher.cpp
    src/engine/semantic_engine.cpp
    src/os/indexer.cpp
)

# Create the executable
add_executable(MySearchApp ${SOURCES})

# Point to OpenVINO headers and libraries
target_include_directories(MySearchApp PRIVATE include)
target_link_directories(MySearchApp PRIVATE lib/intel64/Release)
target_link_libraries(MySearchApp PRIVATE openvino)

# --- WINDOWS SPECIFIC COMPILER FLAGS ---
if(MSVC)
    # /O2: Maximum Speed Optimization
    target_compile_options(MySearchApp PRIVATE /O2)
    
    # /MT: Static Linking (Embeds the C++ runtime directly into the .exe)
    set_property(TARGET MySearchApp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:DebugDLL>")
    
    # NOTE: When you write your UI and change main() to WinMain(), 
    # uncomment the line below to hide the black terminal window.
    # set_target_properties(MySearchApp PROPERTIES WIN32_EXECUTABLE TRUE)
endif()