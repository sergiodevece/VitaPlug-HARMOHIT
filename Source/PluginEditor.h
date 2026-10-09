#pragma once

#include "PluginProcessor.h"

class HarmoEngineeringMeter final : public juce::Component, private juce::Timer
{
public:
    HarmoEngineeringMeter(juce::String name, std::atomic<float>& source);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::String name;
    std::atomic<float>& source;
    float displayedDbfs = -72.0f;
};

class HarmoHitAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HarmoHitAudioProcessorEditor(HarmoHitAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    HarmoHitAudioProcessor& audioProcessor;
    juce::Label harmonicsLabel { {}, "HARMONICS" };
    juce::Label exciterLabel { {}, "EXCITER" };
    juce::Label driveLabel { {}, "DRIVE" };
    juce::Label trimLabel { {}, "OUTPUT TRIM" };
    juce::Slider harmonics, exciter, drive, outputTrim;
    juce::Label rangeLabel { {}, "RANGE" };
    juce::ComboBox range;
    juce::ToggleButton pushAll { "PUSH ALL" }, bypass { "BYPASS" };
    HarmoEngineeringMeter inputMeter { "INPUT", audioProcessor.getInputVuSource() };
    HarmoEngineeringMeter outputMeter { "OUTPUT", audioProcessor.getOutputVuSource() };
    SliderAttachment harmonicsAttachment { audioProcessor.parameters, "harmonics", harmonics };
    SliderAttachment exciterAttachment { audioProcessor.parameters, "exciter", exciter };
    SliderAttachment driveAttachment { audioProcessor.parameters, "drive", drive };
    SliderAttachment outputTrimAttachment { audioProcessor.parameters, "outputTrim", outputTrim };
    ComboBoxAttachment rangeAttachment { audioProcessor.parameters, "exciterRange", range };
    ButtonAttachment pushAllAttachment { audioProcessor.parameters, "pushAll", pushAll };
    ButtonAttachment bypassAttachment { audioProcessor.parameters, "bypass", bypass };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmoHitAudioProcessorEditor)
};
