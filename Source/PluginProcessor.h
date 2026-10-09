#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/HarmoCore.h"

class HarmoHitAudioProcessor final : public juce::AudioProcessor
{
public:
    HarmoHitAudioProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;
    float getInputVuDbfs() const noexcept { return inputVuDbfs.load(std::memory_order_relaxed); }
    float getOutputVuDbfs() const noexcept { return outputVuDbfs.load(std::memory_order_relaxed); }
    std::atomic<float>& getInputVuSource() noexcept { return inputVuDbfs; }
    std::atomic<float>& getOutputVuSource() noexcept { return outputVuDbfs; }
    std::atomic<uint64_t>& getVuUpdateCounter() noexcept { return vuUpdateCounter; }
private:
    vitaplug::harmohit::dsp::HarmoCore core;
    std::atomic<float> *harmonics = nullptr, *exciter = nullptr, *drive = nullptr, *exciterMode = nullptr, *outputTrim = nullptr;
    std::atomic<float> inputVuDbfs { -72.0f };
    std::atomic<float> outputVuDbfs { -72.0f };
    std::atomic<uint64_t> vuUpdateCounter { 0 };
    float inputVuMeanSquare = 0.0f;
    float outputVuMeanSquare = 0.0f;
    double currentSampleRate = 44100.0;
    void updateVuMeter(const juce::AudioBuffer<float>&, bool isInput) noexcept;
};
