#include "DSP/HarmoCore.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
bool allFinite(const float* data, int size)
{
    for (int i = 0; i < size; ++i) if (!std::isfinite(data[i])) return false;
    return true;
}
std::vector<float> tone(int size, float hz, double sr)
{
    std::vector<float> result(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) result[static_cast<size_t>(i)] = 0.25f * std::sin(float(2.0 * 3.141592653589793 * hz * i / sr));
    return result;
}
bool processCore(int channels, double rate, int block, vitaplug::harmohit::dsp::HarmoSettings settings)
{
    vitaplug::harmohit::dsp::HarmoCore core(vitaplug::harmohit::dsp::ProductVariant::mix);
    core.prepare(rate, block, channels);
    auto left = tone(block, 1000.0f, rate); auto right = tone(block, 6000.0f, rate);
    float* data[] { left.data(), right.data() };
    core.process(data, channels, block, settings);
    return allFinite(left.data(), block) && (channels == 1 || allFinite(right.data(), block));
}
float maxDifference(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    float maximum = 0.0f;
    for (int i = 0; i < a.getNumSamples(); ++i) maximum = std::max(maximum, std::abs(a.getSample(0, i) - b.getSample(0, i)));
    return maximum;
}
float maxAdjacentDifference(const float* data, int size)
{
    float maximum = 0.0f;
    for (int i = 1; i < size; ++i) maximum = std::max(maximum, std::abs(data[i] - data[i - 1]));
    return maximum;
}
}

int main()
{
    using namespace vitaplug::harmohit::dsp;
    constexpr int block = 257;

    HarmoCore core(ProductVariant::mix); core.prepare(48000.0, block, 1);
    if (core.getOversamplingFactor() != 4 || core.getLatencySamples() <= 0)
    { std::cerr << "4x oversampling or latency reporting is inactive\n"; return 1; }

    // Zero settings are transparent apart from the explicitly reported filter delay.
    std::vector<float> impulse(1024, 0.0f); impulse[0] = 1.0f; float* mono[] { impulse.data() };
    core.prepare(48000.0, static_cast<int>(impulse.size()), 1);
    core.process(mono, 1, static_cast<int>(impulse.size()), {});
    int peakIndex = 0;
    for (int i = 1; i < static_cast<int>(impulse.size()); ++i)
        if (std::abs(impulse[static_cast<size_t>(i)]) > std::abs(impulse[static_cast<size_t>(peakIndex)])) peakIndex = i;
    if (std::abs(peakIndex - core.getLatencySamples()) > 1 || !allFinite(impulse.data(), static_cast<int>(impulse.size())))
    { std::cerr << "neutral oversampling path is not latency coherent\n"; return 1; }

    for (auto mode : { ExciterMode::presence3k, ExciterMode::brilliance8k, ExciterMode::pushAll })
        if (!processCore(2, 48000.0, block, { 1.0f, 1.0f, 1.0f, 0.0f, mode }))
        { std::cerr << "non-finite exciter mode\n"; return 1; }
    if (!processCore(1, 44100.0, 31, { 0.8f, 0.7f, 0.9f, 0.0f, ExciterMode::pushAll })
        || !processCore(2, 96000.0, 1024, { 0.8f, 0.7f, 0.9f, 0.0f, ExciterMode::pushAll }))
    { std::cerr << "format/sample-rate/block-size failure\n"; return 1; }

    HarmoCore finiteCore(ProductVariant::master); finiteCore.prepare(48000.0, block, 1);
    auto invalid = tone(block, 1000.0f, 48000.0); invalid[4] = std::numeric_limits<float>::quiet_NaN(); invalid[9] = std::numeric_limits<float>::infinity();
    float* invalidData[] { invalid.data() }; finiteCore.process(invalidData, 1, block, { 1.0f, 1.0f, 1.0f, 0.0f, ExciterMode::pushAll });
    if (!allFinite(invalid.data(), block)) { std::cerr << "non-finite containment failed\n"; return 1; }

    // Active zero and bypass use exactly the same oversampling/delay route.
    HarmoHitAudioProcessor active, bypassed;
    active.prepareToPlay(48000.0, 128); bypassed.prepareToPlay(48000.0, 128);
    if (active.getLatencySamples() <= 0 || active.getLatencySamples() != bypassed.getLatencySamples())
    { std::cerr << "processor latency reporting failed\n"; return 1; }
    juce::MidiBuffer midi;
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        auto source = tone(128, 1000.0f, 48000.0);
        juce::AudioBuffer<float> a(1, 128), b(1, 128);
        a.copyFrom(0, 0, source.data(), 128); b.copyFrom(0, 0, source.data(), 128);
        active.processBlock(a, midi); bypassed.processBlockBypassed(b, midi);
        if (maxDifference(a, b) > 1.0e-6f) { std::cerr << "bypass is not time aligned\n"; return 1; }
    }

    juce::AudioBuffer<float> automation(1, 128); auto automationSource = tone(128, 1000.0f, 48000.0); automation.copyFrom(0, 0, automationSource.data(), 128);
    active.parameters.getParameter("harmonics")->setValueNotifyingHost(1.0f);
    active.processBlock(automation, midi);
    if (!allFinite(automation.getReadPointer(0), 128)
        || maxAdjacentDifference(automation.getReadPointer(0), 128) > 0.25f)
    { std::cerr << "automation produced a discontinuity or non-finite output\n"; return 1; }
    juce::MemoryBlock state; active.getStateInformation(state);
    HarmoHitAudioProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    if (std::abs(restored.parameters.getRawParameterValue("harmonics")->load() - 100.0f) > 0.01f)
    { std::cerr << "parameter state serialisation failed\n"; return 1; }

    // The engineering editor exposes direct APVTS attachments with visible
    // numeric slider text and no editor-owned DSP mode state.
    juce::ScopedJuceInitialiser_GUI gui;
    HarmoHitAudioProcessor controlProcessor;
    controlProcessor.parameters.getParameter("harmonics")->setValueNotifyingHost(0.25f);
    HarmoHitAudioProcessorEditor controlEditor(controlProcessor);
    auto* harmonicControl = dynamic_cast<juce::Slider*>(controlEditor.findChildWithID("harmonics"));
    auto* exciterControl = dynamic_cast<juce::Slider*>(controlEditor.findChildWithID("exciter"));
    auto* driveControl = dynamic_cast<juce::Slider*>(controlEditor.findChildWithID("drive"));
    auto* rangeControl = dynamic_cast<juce::ComboBox*>(controlEditor.findChildWithID("exciterRange"));
    auto* pushAllControl = dynamic_cast<juce::ToggleButton*>(controlEditor.findChildWithID("pushAll"));
    auto* bypassControl = dynamic_cast<juce::ToggleButton*>(controlEditor.findChildWithID("bypass"));
    auto* trimControl = dynamic_cast<juce::Slider*>(controlEditor.findChildWithID("outputTrim"));
    if (controlEditor.getWidth() != 520 || controlEditor.getHeight() != 300 || harmonicControl == nullptr || exciterControl == nullptr
        || driveControl == nullptr || rangeControl == nullptr || pushAllControl == nullptr || bypassControl == nullptr || trimControl == nullptr
        || harmonicControl->getSliderStyle() != juce::Slider::LinearHorizontal || harmonicControl->getMinimum() != 0.0 || harmonicControl->getMaximum() != 100.0)
    { std::cerr << "engineering editor configuration failed\n"; return 1; }
    if (std::abs(harmonicControl->getValue() - 25.0) > 0.11 || std::abs(exciterControl->getValue()) > 0.01 || std::abs(driveControl->getValue()) > 0.01)
    { std::cerr << "host-to-slider attachment failed\n"; return 1; }
    harmonicControl->setValue(0.0, juce::sendNotificationSync);
    if (std::abs(controlProcessor.parameters.getRawParameterValue("harmonics")->load()) > 0.01f)
    { std::cerr << "main knob minimum failed\n"; return 1; }
    harmonicControl->setValue(100.0, juce::sendNotificationSync);
    if (std::abs(controlProcessor.parameters.getRawParameterValue("harmonics")->load() - 100.0f) > 0.11f)
    { std::cerr << "main knob maximum failed\n"; return 1; }
    harmonicControl->setValue(25.0, juce::sendNotificationSync);
    exciterControl->setValue(72.0, juce::sendNotificationSync);
    if (std::abs(controlProcessor.parameters.getRawParameterValue("exciter")->load() - 72.0f) > 0.11f
        || std::abs(controlProcessor.parameters.getRawParameterValue("harmonics")->load() - 25.0f) > 0.11f
        || std::abs(controlProcessor.parameters.getRawParameterValue("drive")->load()) > 0.01f)
    { std::cerr << "control attachment changed the wrong parameter\n"; return 1; }
    controlProcessor.parameters.getParameter("drive")->setValueNotifyingHost(1.0f);
    juce::MemoryBlock controlState; controlProcessor.getStateInformation(controlState);
    HarmoHitAudioProcessor restoredControls;
    restoredControls.setStateInformation(controlState.getData(), static_cast<int>(controlState.getSize()));
    if (std::abs(restoredControls.parameters.getRawParameterValue("harmonics")->load() - 25.0f) > 0.11f
        || std::abs(restoredControls.parameters.getRawParameterValue("exciter")->load() - 72.0f) > 0.11f
        || std::abs(restoredControls.parameters.getRawParameterValue("drive")->load() - 100.0f) > 0.11f)
    { std::cerr << "main knob preset restoration failed\n"; return 1; }

    // RANGE and PUSH ALL are direct processor parameters; no editor callback
    // decides the selected DSP mode.
    const auto rangeValue = [&controlProcessor] { return controlProcessor.parameters.getRawParameterValue("exciterRange")->load(); };
    const auto pushValue = [&controlProcessor] { return controlProcessor.parameters.getRawParameterValue("pushAll")->load(); };
    if (std::abs(rangeValue()) > 0.01f || std::abs(pushValue()) > 0.01f || pushAllControl->getToggleState())
    { std::cerr << "3 kHz + OFF mapping failed\n"; return 1; }
    rangeControl->setSelectedId(2, juce::sendNotificationSync);
    if (std::abs(rangeValue() - 1.0f) > 0.01f || pushAllControl->getToggleState()) { std::cerr << "RANGE attachment failed\n"; return 1; }
    if (!pushAllControl->getClickingTogglesState() || !bypassControl->getClickingTogglesState())
    { std::cerr << "toggle controls are not mouse-operable\n"; return 1; }
    controlProcessor.parameters.getParameter("pushAll")->setValueNotifyingHost(1.0f);
    controlProcessor.parameters.getParameter("bypass")->setValueNotifyingHost(1.0f);
    if (std::abs(pushValue() - 1.0f) > 0.01f || controlProcessor.parameters.getRawParameterValue("bypass")->load() < 0.5f)
    { std::cerr << "PUSH ALL/BYPASS host automation failed\n"; return 1; }
    trimControl->setValue(12.0, juce::sendNotificationSync);
    if (std::abs(controlProcessor.parameters.getRawParameterValue("outputTrim")->load() - 12.0f) > 0.11f)
    { std::cerr << "Output Trim attachment failed\n"; return 1; }

    // Simulate a saved session from before RANGE/PUSH ALL were independent.
    HarmoHitAudioProcessor legacySource;
    legacySource.parameters.getParameter("exciterMode")->setValueNotifyingHost(0.5f); // choice index 1: 8 kHz
    auto legacyState = legacySource.parameters.copyState();
    for (int index = legacyState.getNumChildren(); --index >= 0;)
        if (legacyState.getChild(index).getProperty("id").toString() == "exciterRange"
            || legacyState.getChild(index).getProperty("id").toString() == "pushAll") legacyState.removeChild(index, nullptr);
    juce::MemoryBlock legacyData;
    if (auto legacyXml = legacyState.createXml()) HarmoHitAudioProcessor::copyXmlToBinary(*legacyXml, legacyData);
    HarmoHitAudioProcessor migrated;
    migrated.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
    if (std::abs(migrated.parameters.getRawParameterValue("exciterMode")->load() - 1.0f) > 0.01f
        || std::abs(migrated.parameters.getRawParameterValue("exciterRange")->load() - 1.0f) > 0.01f
        || migrated.parameters.getRawParameterValue("pushAll")->load() > 0.01f)
    { std::cerr << "legacy exciter state migration failed\n"; return 1; }

    // No editor is created here: host automation of RANGE and PUSH ALL must
    // select different processor modes on its own.
    const auto renderClosedEditorMode = [&midi] (float range, float push)
    {
        HarmoHitAudioProcessor processor;
        processor.prepareToPlay(48000.0, 128);
        processor.parameters.getParameter("exciter")->setValueNotifyingHost(1.0f);
        processor.parameters.getParameter("exciterRange")->setValueNotifyingHost(range);
        processor.parameters.getParameter("pushAll")->setValueNotifyingHost(push);
        juce::AudioBuffer<float> rendered(1, 128);
        for (int iteration = 0; iteration < 20; ++iteration)
        {
            const auto source = tone(128, 3000.0f, 48000.0);
            rendered.copyFrom(0, 0, source.data(), 128);
            processor.processBlock(rendered, midi);
        }
        return rendered;
    };
    const auto presenceClosed = renderClosedEditorMode(0.0f, 0.0f);
    const auto brillianceClosed = renderClosedEditorMode(1.0f, 0.0f);
    const auto pushAllClosed = renderClosedEditorMode(1.0f, 1.0f);
    if (maxDifference(presenceClosed, brillianceClosed) < 1.0e-4f
        || maxDifference(pushAllClosed, brillianceClosed) < 1.0e-4f)
    { std::cerr << "closed-editor RANGE/PUSH ALL automation did not reach DSP\n"; return 1; }

    // VU taps are fixed at the processor input and post-Trim output. A 300 ms
    // run allows the documented 300 ms power integrator to settle.
    HarmoHitAudioProcessor metered;
    metered.prepareToPlay(48000.0, 128);
    if (!std::isfinite(metered.getInputVuDbfs()) || !std::isfinite(metered.getOutputVuDbfs())
        || std::abs(metered.getInputVuDbfs() + 72.0f) > 1.0e-6f || std::abs(metered.getOutputVuDbfs() + 72.0f) > 1.0e-6f)
    { std::cerr << "VU silence reset failed\n"; return 1; }
    auto renderMeterBlocks = [&metered, &midi] (float amplitude, int count)
    {
        for (int iteration = 0; iteration < count; ++iteration)
        {
            juce::AudioBuffer<float> meterBuffer(1, 128);
            auto signal = tone(128, 1000.0f, 48000.0);
            for (int sample = 0; sample < 128; ++sample) meterBuffer.setSample(0, sample, signal[static_cast<size_t>(sample)] * amplitude);
            metered.processBlock(meterBuffer, midi);
            if (!allFinite(meterBuffer.getReadPointer(0), 128)) return false;
        }
        return true;
    };
    if (!renderMeterBlocks(0.0f, 16) || metered.getInputVuDbfs() > -70.0f || metered.getOutputVuDbfs() > -70.0f)
    { std::cerr << "VU silence behaviour failed\n"; return 1; }
    if (!renderMeterBlocks(1.0f, 320)) { std::cerr << "VU neutral render failed\n"; return 1; }
    const auto inputBeforeTrim = metered.getInputVuDbfs();
    const auto outputBeforeTrim = metered.getOutputVuDbfs();
    if (!std::isfinite(inputBeforeTrim) || !std::isfinite(outputBeforeTrim)
        || inputBeforeTrim < -18.0f || inputBeforeTrim > -13.0f || std::abs(outputBeforeTrim - inputBeforeTrim) > 1.0f)
    { std::cerr << "VU input/output neutral tap positions failed\n"; return 1; }
    metered.parameters.getParameter("outputTrim")->setValueNotifyingHost(1.0f);
    if (!renderMeterBlocks(1.0f, 320)) { std::cerr << "VU trim render failed\n"; return 1; }
    if (std::abs(metered.getInputVuDbfs() - inputBeforeTrim) > 0.5f
        || metered.getOutputVuDbfs() - metered.getInputVuDbfs() < 10.0f)
    { std::cerr << "VU output is not post-Trim or input is post-DSP\n"; return 1; }

    std::cout << "DSP tests passed: 4x, " << core.getLatencySamples() << "-sample latency, transparent delayed neutral path, mono/stereo, finite output, sample-rate/block-size changes, automation, state, exciter modes, bypass alignment and pre/post VU taps.\n";
}
