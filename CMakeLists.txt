cmake_minimum_required(VERSION 3.22)

project(BeatDrop VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(BEATDROP_BUILD_TESTS "Build the BeatDrop unit tests" ON)
option(BEATDROP_COPY_AFTER_BUILD "Install the plugin into ~/Library/Audio/Plug-Ins after building" OFF)

add_subdirectory(JUCE)

# ---------------------------------------------------------------------------
# Sources shared by the plugin and the test runner
# ---------------------------------------------------------------------------
set(BEATDROP_CORE_SOURCES
    Source/Core/MasterRecorder.cpp
    Source/Core/Metadata.cpp
    Source/Core/Settings.cpp
    Source/Core/TokenStore.cpp
    Source/YouTube/Http.cpp
    Source/YouTube/OAuthClient.cpp
    Source/YouTube/YouTubeClient.cpp
    Source/YouTube/UploadJob.cpp
    Source/Video/VideoFrame.cpp
)

if(APPLE)
    list(APPEND BEATDROP_CORE_SOURCES
        Source/Video/VideoEncoder_mac.mm
        Source/Core/Keychain_mac.mm)
else()
    list(APPEND BEATDROP_CORE_SOURCES
        Source/Video/VideoEncoder_unsupported.cpp
        Source/Core/Keychain_unsupported.cpp)
endif()

set(BEATDROP_JUCE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_cryptography
    juce::juce_gui_basics)

set(BEATDROP_COMPILE_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    BEATDROP_VERSION="${PROJECT_VERSION}")

set(BEATDROP_APPLE_FRAMEWORKS
    "-framework AVFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Security")

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(BeatDrop
    COMPANY_NAME "RecklessBoise"
    COMPANY_WEBSITE "https://github.com/RecklessBoise/BeatDrop"
    BUNDLE_ID com.recklessboise.beatdrop
    PLUGIN_MANUFACTURER_CODE Rckl
    PLUGIN_CODE Bdrp
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "BeatDrop"
    DESCRIPTION "Upload type beats to YouTube straight from your master bus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${BEATDROP_COPY_AFTER_BUILD})

target_sources(BeatDrop PRIVATE
    ${BEATDROP_CORE_SOURCES}
    Source/Core/AppServices.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/UI/BeatDropLookAndFeel.cpp
    Source/UI/CapturePanel.cpp
    Source/UI/ImageDropZone.cpp
    Source/UI/MetadataPanel.cpp
    Source/UI/SettingsPanel.cpp)

target_include_directories(BeatDrop PRIVATE Source)
target_compile_definitions(BeatDrop PUBLIC ${BEATDROP_COMPILE_DEFINITIONS})

target_link_libraries(BeatDrop
    PRIVATE
        ${BEATDROP_JUCE_MODULES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(APPLE)
    target_link_libraries(BeatDrop PRIVATE ${BEATDROP_APPLE_FRAMEWORKS})
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BEATDROP_BUILD_TESTS)
    juce_add_console_app(BeatDropTests PRODUCT_NAME "BeatDropTests")

    target_sources(BeatDropTests PRIVATE
        ${BEATDROP_CORE_SOURCES}
        Tests/TestMain.cpp
        Tests/MetadataTests.cpp
        Tests/OAuthTests.cpp
        Tests/RecorderTests.cpp
        Tests/VideoTests.cpp
        Tests/YouTubeRequestTests.cpp)

    target_include_directories(BeatDropTests PRIVATE Source)
    target_compile_definitions(BeatDropTests PRIVATE ${BEATDROP_COMPILE_DEFINITIONS} JUCE_UNIT_TESTS=1)

    target_link_libraries(BeatDropTests
        PRIVATE
            ${BEATDROP_JUCE_MODULES}
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    if(APPLE)
        target_link_libraries(BeatDropTests PRIVATE ${BEATDROP_APPLE_FRAMEWORKS})
    endif()

    enable_testing()
    add_test(NAME BeatDropTests COMMAND BeatDropTests)
endif()
