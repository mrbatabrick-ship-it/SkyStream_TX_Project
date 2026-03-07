cmake_minimum_required(VERSION 3.15)
project(SkyStream_TX_Project VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(juce)

# Tambahkan WIN32 agar tidak error WinMain di Windows
juce_add_gui_app(SkyStream_TX
    PRODUCT_NAME "SkyStream TX"
    VERSION 1.0.0
    BUNDLE_ID "com.skystream.tx"
    IS_CONTENT_BROWSER_APP TRUE
    WIN32 
)

target_sources(SkyStream_TX PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
)

# Tambahkan definisi ini untuk stabilitas entry point
if(MSVC)
    target_compile_definitions(SkyStream_TX PRIVATE JUCE_WORLD_IS_GUI=1)
endif()

target_link_libraries(SkyStream_TX PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

juce_generate_juce_header(SkyStream_TX)