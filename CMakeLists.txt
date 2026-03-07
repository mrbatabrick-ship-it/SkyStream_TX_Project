cmake_minimum_required(VERSION 3.15)

project(SkyStream_TX VERSION 2.2.7)

# 1. Mencari library JUCE
find_package(JUCE CONFIG REQUIRED)

# 2. Membuat Aplikasi
juce_add_gui_app(SkyStream_TX
    COMPANY_NAME "SkyStream"
    PRODUCT_NAME "SkyStream_TX"
)

# 3. Menghubungkan file kode di folder Source
target_sources(SkyStream_TX PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
)

# 4. Menentukan modul JUCE yang dipakai
target_link_libraries(SkyStream_TX PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_events
    juce::juce_core
)

# 5. Setting standar C++
target_compile_features(SkyStream_TX PRIVATE cxx_std_17)