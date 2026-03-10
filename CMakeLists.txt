cmake_minimum_required(VERSION 3.15)
project(SkyStream_TX_Project VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.0
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(SkyStream_TX
    PRODUCT_NAME "SkyStream TX"
)

# --- BAGIAN PENTING: Mencegah Error 127 tanpa merusak kode ---
# Kita tetap biarkan header ada, tapi matikan auto-generation yang butuh juceaide
juce_set_juce_header_generation(SkyStream_TX OFF) 

target_sources(SkyStream_TX PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
)

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