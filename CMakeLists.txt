cmake_minimum_required(VERSION 3.20)
project(ScreenRecorder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enforce Unicode and Windows 10+ target
add_definitions(-DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -DNOMINMAX -DWIN32_LEAN_AND_MEAN)

if(MSVC)
    add_compile_options(
        /permissive-
        /W4
        /WX-
        /utf-8
        /EHsc
        /MP
        /wd4324
    )
    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Oi /Ot /GL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core subsystems sources
set(CORE_SOURCES
    src/core/Engine.cpp
    src/core/StateMachine.cpp
    src/core/EventBus.cpp
    src/gpu/D3D11Device.cpp
    src/gpu/GpuAdapter.cpp
    src/gpu/Compositor.cpp
    src/capture/SourceManager.cpp
    src/capture/WgcEngine.cpp
    src/capture/DxgiDuplicationEngine.cpp
    src/encode/MftVideoEncoder.cpp
    src/encode/HardwareDetector.cpp
    src/audio/WasapiAudioEngine.cpp
    src/audio/AudioMixer.cpp
    src/audio/MftAudioEncoder.cpp
    src/mux/MkvMuxer.cpp
    src/mux/Mp4Remuxer.cpp
    src/replay/CircularBuffer.cpp
    src/overlay/RecordingHud.cpp
    src/overlay/Telemetry.cpp
    src/config/Settings.cpp
    src/config/PresetManager.cpp
    src/config/PathTemplates.cpp
    src/ipc/NamedPipeServer.cpp
    src/ui/MainWindow.cpp
    src/ui/Direct2DRenderer.cpp
    src/ui/TrayIcon.cpp
)

set(CORE_HEADERS
    include/core/Types.h
    include/core/Engine.h
    include/core/StateMachine.h
    include/core/EventBus.h
    include/core/LockFreeQueue.h
    include/gpu/D3D11Device.h
    include/gpu/GpuAdapter.h
    include/gpu/Compositor.h
    include/capture/ICaptureEngine.h
    include/capture/SourceManager.h
    include/capture/WgcEngine.h
    include/capture/DxgiDuplicationEngine.h
    include/encode/IVideoEncoder.h
    include/encode/EncoderTypes.h
    include/encode/MftVideoEncoder.h
    include/encode/HardwareDetector.h
    include/audio/IAudioEngine.h
    include/audio/WasapiAudioEngine.h
    include/audio/AudioMixer.h
    include/audio/MftAudioEncoder.h
    include/mux/MuxTypes.h
    include/mux/MkvMuxer.h
    include/mux/Mp4Remuxer.h
    include/replay/CircularBuffer.h
    include/overlay/RecordingHud.h
    include/overlay/Telemetry.h
    include/config/Settings.h
    include/config/PresetManager.h
    include/config/PathTemplates.h
    include/ipc/NamedPipeServer.h
    include/ui/MainWindow.h
    include/ui/Direct2DRenderer.h
    include/ui/TrayIcon.h
    include/ui/UITheme.h
)

# Main desktop executable target
add_executable(ScreenRecorder WIN32
    src/main.cpp
    src/resource.rc
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_link_libraries(ScreenRecorder PRIVATE
    d3d11.lib
    dxgi.lib
    d2d1.lib
    dwrite.lib
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    wmcodecdspuuid.lib
    windowsapp.lib
    dwmapi.lib
    uxtheme.lib
    ole32.lib
    oleaut32.lib
    avrt.lib
    version.lib
)

set_target_properties(ScreenRecorder PROPERTIES
    OUTPUT_NAME "ScreenRecorder"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Automated unit & logic tests target
add_executable(ScreenRecorderTests
    tests/TestMain.cpp
    src/config/Settings.cpp
    src/config/PresetManager.cpp
    src/config/PathTemplates.cpp
    src/replay/CircularBuffer.cpp
    src/core/StateMachine.cpp
    src/core/EventBus.cpp
    src/mux/MkvMuxer.cpp
    src/mux/Mp4Remuxer.cpp
    src/encode/HardwareDetector.cpp
    src/audio/AudioMixer.cpp
    src/capture/SourceManager.cpp
    src/ipc/NamedPipeServer.cpp
)

target_link_libraries(ScreenRecorderTests PRIVATE
    ole32.lib
    oleaut32.lib
    mfplat.lib
    mfreadwrite.lib
    mfuuid.lib
    wmcodecdspuuid.lib
    d3d11.lib
    dxgi.lib
    dwmapi.lib
    windowsapp.lib
)

set_target_properties(ScreenRecorderTests PROPERTIES
    OUTPUT_NAME "ScreenRecorderTests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

enable_testing()
add_test(NAME CoreLogicTests COMMAND ScreenRecorderTests)
