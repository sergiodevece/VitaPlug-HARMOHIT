#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::AudioParameterFloatAttributes percentAttributes()
{
    return juce::AudioParameterFloatAttributes().withLabel("%");
}
juce::AudioParameterFloatAttributes dbAttributes()
{
    return juce::AudioParameterFloatAttributes().withLabel("dB");
}
bool containsParameterId(const juce::ValueTree& state, const juce::String& id)
{
    for (const auto& child : state)
        if (child.getProperty("id").toString() == id) return true;
    return false;
}
#if defined(VITAPLUG_HARMOHIT_MASTER)
constexpr auto productVariant = vitaplug::harmohit::dsp::ProductVariant::master;
#else
constexpr auto productVariant = vitaplug::harmohit::dsp::ProductVariant::mix;
#endif
}

HarmoHitAudioProcessor::HarmoHitAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()), core(productVariant)
{
    harmonics = parameters.getRawParameterValue("harmonics");
    exciter = parameters.getRawParameterValue("exciter");
    drive = parameters.getRawParameterValue("drive");
    exciterMode = parameters.getRawParameterValue("exciterMode");
    outputTrim = parameters.getRawParameterValue("outputTrim");
}

const juce::String HarmoHitAudioProcessor::getName() const
{
#if defined(VITAPLUG_HARMOHIT_MASTER)
    return "VitaPlug HARMOHIT MASTER";
#else
    return "VitaPlug HARMOHIT MIX";
#endif
}

juce::AudioProcessorEditor* HarmoHitAudioProcessor::createEditor()
{
    return new HarmoHitAudioProcessorEditor(*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout HarmoHitAudioProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "harmonics", 1 }, "Harmonics", Range { 0.0f, 100.0f, 0.1f }, 0.0f, percentAttributes()));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "exciter", 1 }, "Exciter", Range { 0.0f, 100.0f, 0.1f }, 0.0f, percentAttributes()));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "exciterMode", 1 }, "Exciter Mode", juce::StringArray { "3 kHz", "8 kHz", "PUSH ALL" }, 0));
    // exciterMode is retained exactly for old sessions and host automation.
    // This parameter remembers the selected band while PUSH ALL (mode 2) is
    // active, so returning to OFF restores 3 kHz or 8 kHz deterministically.
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "exciterRange", 1 }, "Exciter Range", juce::StringArray { "3 kHz", "8 kHz" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "drive", 1 }, "Drive", Range { 0.0f, 100.0f, 0.1f }, 0.0f, percentAttributes()));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "outputTrim", 1 }, "Output Trim", Range { -24.0f, 12.0f, 0.1f }, 0.0f, dbAttributes()));
    return layout;
}

void HarmoHitAudioProcessor::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = std::max(1.0, sampleRate);
    inputVuMeanSquare = outputVuMeanSquare = 0.0f;
    inputVuDbfs.store(-72.0f, std::memory_order_relaxed);
    outputVuDbfs.store(-72.0f, std::memory_order_relaxed);
    core.prepare(sampleRate, maximumBlockSize, getTotalNumOutputChannels());
    setLatencySamples(core.getLatencySamples());
}
void HarmoHitAudioProcessor::releaseResources() { core.reset(); }
bool HarmoHitAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}
void HarmoHitAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateVuMeter(buffer, true);
    std::array<float*, 2> channels {};
    const int active = std::min(2, buffer.getNumChannels());
    for (int channel = 0; channel < active; ++channel) channels[static_cast<size_t>(channel)] = buffer.getWritePointer(channel);
    const auto mode = std::clamp(static_cast<int>(exciterMode->load()), 0, 2);
    core.process(channels.data(), active, buffer.getNumSamples(), { harmonics->load() / 100.0f, exciter->load() / 100.0f,
        drive->load() / 100.0f, outputTrim->load(), static_cast<vitaplug::harmohit::dsp::ExciterMode>(mode) });
    updateVuMeter(buffer, false);
    vuUpdateCounter.fetch_add(1, std::memory_order_relaxed);
}
void HarmoHitAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Keep bypass in the same oversampling/delay pipeline as active processing.
    // Hosts that latency-compensate from getLatencySamples() and hosts that
    // switch this callback directly both retain temporal coherence.
    updateVuMeter(buffer, true);
    std::array<float*, 2> channels {};
    const int active = std::min(2, buffer.getNumChannels());
    for (int channel = 0; channel < active; ++channel) channels[static_cast<size_t>(channel)] = buffer.getWritePointer(channel);
    core.process(channels.data(), active, buffer.getNumSamples(), {});
    updateVuMeter(buffer, false);
    vuUpdateCounter.fetch_add(1, std::memory_order_relaxed);
}

void HarmoHitAudioProcessor::updateVuMeter(const juce::AudioBuffer<float>& buffer, bool isInput) noexcept
{
    const int channels = std::min(2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0) return;

    // Power is averaged across channels, never summed: a stereo programme
    // retains its RMS calibration even when the channels are anti-phase.
    double energy = 0.0;
    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* data = buffer.getReadPointer(channel);
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto value = data[sample];
            if (std::isfinite(value)) energy += static_cast<double>(value) * value;
        }
    }
    const auto meanSquare = static_cast<float>(energy / static_cast<double>(channels * samples));
    // 300 ms exponential VU integration. This tracks power in the audio
    // thread, while the editor applies a separate visual-only needle smoothing.
    const auto coefficient = std::exp(static_cast<float>(-samples / (currentSampleRate * 0.300)));
    auto& integratedSquare = isInput ? inputVuMeanSquare : outputVuMeanSquare;
    integratedSquare = coefficient * integratedSquare + (1.0f - coefficient) * meanSquare;
    const auto dbfs = 10.0f * std::log10(std::max(integratedSquare, 1.0e-12f));
    (isInput ? inputVuDbfs : outputVuDbfs).store(juce::jlimit(-72.0f, 24.0f, dbfs), std::memory_order_relaxed);
}
void HarmoHitAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, destination);
}
void HarmoHitAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
        {
            const auto restoredState = juce::ValueTree::fromXml(*xml);
            const auto hasExciterRange = containsParameterId(restoredState, "exciterRange");
            parameters.replaceState(restoredState);
            // Old sessions held only exciterMode. Modes 0/1 map exactly to the
            // new remembered range; legacy PUSH ALL retains its historical 3k
            // fallback because no hidden range existed in those sessions.
            if (!hasExciterRange)
            {
                const auto legacyMode = static_cast<int>(exciterMode->load());
                if (auto* range = parameters.getParameter("exciterRange"))
                    range->setValueNotifyingHost(legacyMode == 1 ? 1.0f : 0.0f);
            }
        }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HarmoHitAudioProcessor(); }
