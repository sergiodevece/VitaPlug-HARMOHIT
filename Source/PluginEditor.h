#pragma once

#include "PluginProcessor.h"

class HarmoVuMeter final : public juce::Component, private juce::Timer
{
public:
    HarmoVuMeter(juce::String caption, std::atomic<float>& sourceDbfs, std::atomic<uint64_t>& sourceCounter);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::String caption;
    std::atomic<float>& sourceDbfs;
    std::atomic<uint64_t>& sourceCounter;
    uint64_t lastCounter = 0;
    float displayedDbfs = -72.0f;
};

class HarmoMainKnob final : public juce::Slider
{
public:
    explicit HarmoMainKnob(juce::String caption);
    void paint(juce::Graphics&) override;
    void valueChanged() override;

private:
    juce::String caption;
};

class HarmoRangeSwitch final : public juce::Button
{
public:
    HarmoRangeSwitch();
    void paintButton(juce::Graphics&, bool, bool) override;
};

class HarmoPushAllButton final : public juce::Button
{
public:
    HarmoPushAllButton();
    void paintButton(juce::Graphics&, bool, bool) override;
};

class HarmoTrimKnob final : public juce::Slider
{
public:
    HarmoTrimKnob();
    void paint(juce::Graphics&) override;
    void valueChanged() override;
};

// The front-panel composition remains static apart from the two VU components.
// Control widgets and parameter attachments follow in later UI phases.
class HarmoHitAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HarmoHitAudioProcessorEditor(HarmoHitAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    HarmoHitAudioProcessor& audioProcessor;
    juce::Image panelArtwork;
    HarmoVuMeter inputVu { "INPUT", audioProcessor.getInputVuSource(), audioProcessor.getVuUpdateCounter() };
    HarmoVuMeter outputVu { "OUTPUT", audioProcessor.getOutputVuSource(), audioProcessor.getVuUpdateCounter() };
    HarmoMainKnob harmonicsKnob { "HARMONICS" };
    HarmoMainKnob exciterKnob { "EXCITER" };
    HarmoMainKnob driveKnob { "DRIVE" };
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    Attachment harmonicsAttachment { audioProcessor.parameters, "harmonics", harmonicsKnob };
    Attachment exciterAttachment { audioProcessor.parameters, "exciter", exciterKnob };
    Attachment driveAttachment { audioProcessor.parameters, "drive", driveKnob };
    HarmoRangeSwitch rangeSwitch;
    HarmoPushAllButton pushAllButton;
    HarmoTrimKnob outputTrimKnob;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    ButtonAttachment rangeAttachment { audioProcessor.parameters, "exciterRange", rangeSwitch };
    Attachment outputTrimAttachment { audioProcessor.parameters, "outputTrim", outputTrimKnob };
    std::unique_ptr<juce::ParameterAttachment> pushAllAttachment;
    std::unique_ptr<juce::ParameterAttachment> rangeModeAttachment;
    bool syncingLegacyExciterMode = false;
    void syncLegacyExciterMode(float);
    void setLegacyExciterMode(int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmoHitAudioProcessorEditor)
};
