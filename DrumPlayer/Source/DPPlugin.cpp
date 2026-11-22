// Drum Player: key-triggered drum pattern player and editor (MIDI out to any drum plugin)
#include "DPEditor.h"

juce::AudioProcessorEditor* dp::DrumPlayerProcessor::createEditor() { return new DrumPlayerEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new dp::DrumPlayerProcessor(); }
