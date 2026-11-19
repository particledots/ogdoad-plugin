# The Ogdoad plugin wrapper: what Logic sees (AU / VST3, a MIDI effect) and the panel.
# The LFOs and the CC stream live in ../engine.

# The Audio Unit format exists on macOS only; elsewhere (the Linux checking computer) only VST3 is built.
if(APPLE)
    set(OGDOAD_FORMATS AU VST3)
else()
    set(OGDOAD_FORMATS VST3)
endif()

# A MIDI effect (AU type aumi), set up like Dotriaconta-tone and Fractal-Garden: no audio buses, MIDI in and MIDI out.
# These codes and the bundle id were chosen once and never change (saved projects find the plugin by them).
juce_add_plugin(Ogdoad
    COMPANY_NAME                "particledots"
    BUNDLE_ID                   com.particledots.ogdoad
    PLUGIN_MANUFACTURER_CODE    Pdot
    PLUGIN_CODE                 Ogd1
    FORMATS                     ${OGDOAD_FORMATS}
    PRODUCT_NAME                "Ogdoad"
    PLUGIN_NAME                 "Ogdoad"
    DESCRIPTION                 "Eight drifting LFOs as MIDI CC"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx
    AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor
)

set(OGDOAD_PLUGIN_SOURCES
    Source/Parameters.h
    Source/Parameters.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_sources(Ogdoad PRIVATE ${OGDOAD_PLUGIN_SOURCES})

target_compile_definitions(Ogdoad PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Ogdoad
    PRIVATE
        ogdoad_engine
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Optional, off by default: a small program that draws the window to a picture file and checks the controls, the saved
# state and the MIDI going in and out, without Logic. Build with -DOGDOAD_BUILD_PREVIEW=ON.
option(OGDOAD_BUILD_PREVIEW "Build the offline window picture and plugin checks" OFF)
if(OGDOAD_BUILD_PREVIEW)
    juce_add_console_app(OgdoadPreview PRODUCT_NAME "OgdoadPreview")
    target_sources(OgdoadPreview PRIVATE preview/preview.cpp ${OGDOAD_PLUGIN_SOURCES})
    target_include_directories(OgdoadPreview PRIVATE Source)
    target_compile_definitions(OgdoadPreview PRIVATE
        JucePlugin_Name="Ogdoad"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(OgdoadPreview
        PRIVATE ogdoad_engine juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
