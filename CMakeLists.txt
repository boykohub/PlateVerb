cmake_minimum_required(VERSION 3.22)
project(PlateVerb VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PlateVerb
  COMPANY_NAME "Boyko"
  PLUGIN_MANUFACTURER_CODE Boyk
  PLUGIN_CODE Plt1
  FORMATS VST3
  PRODUCT_NAME "PlateVerb")

target_sources(PlateVerb PRIVATE Source/PluginProcessor.cpp)
target_compile_features(PlateVerb PRIVATE cxx_std_17)
target_compile_definitions(PlateVerb PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(PlateVerb
  PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------- ChannelStrip (second plugin)
juce_add_plugin(ChannelStrip
  COMPANY_NAME "Boyko"
  PLUGIN_MANUFACTURER_CODE Boyk
  PLUGIN_CODE Chs1
  FORMATS VST3
  PRODUCT_NAME "ChannelStrip")

target_sources(ChannelStrip PRIVATE ChannelStrip/Source/CSPlugin.cpp)
target_include_directories(ChannelStrip PRIVATE Source ChannelStrip/Source)
target_compile_features(ChannelStrip PRIVATE cxx_std_17)
target_compile_definitions(ChannelStrip PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_PLUGINHOST_VST3=1)
target_link_libraries(ChannelStrip
  PRIVATE juce::juce_audio_processors juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------- N1081 (third plugin: Neve 1081 circuit model)
juce_add_plugin(N1081
  COMPANY_NAME "Boyko"
  PLUGIN_MANUFACTURER_CODE Boyk
  PLUGIN_CODE N181
  FORMATS VST3
  PRODUCT_NAME "N1081")

target_sources(N1081 PRIVATE N1081/Source/N1081Plugin.cpp)
target_include_directories(N1081 PRIVATE N1081/Source)
target_compile_features(N1081 PRIVATE cxx_std_17)
target_compile_definitions(N1081 PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(N1081
  PRIVATE juce::juce_audio_processors juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
