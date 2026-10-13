#include "PluginEditor.h"

namespace
{
void configurePercentSlider(juce::Slider& slider, const juce::String& id)
{
    slider.setComponentID(id);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
    slider.setRange(0.0, 100.0, 0.1);
    slider.setTextValueSuffix(" %");
}
}

HarmoEngineeringMeter::HarmoEngineeringMeter(juce::String newName, std::atomic<float>& newSource)
    : name(std::move(newName)), source(newSource)
{
    startTimerHz(30);
}

void HarmoEngineeringMeter::timerCallback()
{
    const auto target = source.load(std::memory_order_relaxed);
    displayedDbfs += (target - displayedDbfs) * (target > displayedDbfs ? 0.35f : 0.12f);
    repaint();
}

void HarmoEngineeringMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto proportion = juce::jmap(juce::jlimit(-72.0f, 0.0f, displayedDbfs), -72.0f, 0.0f, 0.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.82f));
    g.setFont(13.0f);
    g.drawText(name + "  " + juce::String(displayedDbfs, 1) + " dBFS", bounds.removeFromTop(18.0f).toNearestInt(), juce::Justification::centredLeft);
    const auto bar = bounds.reduced(0.0f, 5.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(bar, 3.0f);
    g.setColour(proportion > 0.88f ? juce::Colours::orange : juce::Colours::limegreen);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * proportion), 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawRoundedRectangle(bar, 3.0f, 1.0f);
}

HarmoHitAudioProcessorEditor::HarmoHitAudioProcessorEditor(HarmoHitAudioProcessor& p, bool useVintage)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
#if defined(VITAPLUG_HARMOHIT_MIX)
    if (useVintage)
    {
        vintageSkin = std::make_unique<harmohit::ui::HarmoHitSkin>(p.parameters,
            harmohit::ui::ParameterIds{}, [&p] { return p.getInputVuDbfs() + 18.0f; },
            [&p] { return p.getOutputVuDbfs() + 18.0f; });
        addAndMakeVisible(*vintageSkin);
        setSize(760, 380);
        setResizable(false, false);
        return;
    }
#else
    juce::ignoreUnused(useVintage);
#endif
    setSize(520, 300);
    setResizable(false, false);
    for (auto* label : { &harmonicsLabel, &exciterLabel, &driveLabel, &trimLabel, &rangeLabel })
    {
        label->setJustificationType(juce::Justification::centredLeft);
        label->setFont(juce::FontOptions(13.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    configurePercentSlider(harmonics, "harmonics");
    configurePercentSlider(exciter, "exciter");
    configurePercentSlider(drive, "drive");
    outputTrim.setComponentID("outputTrim");
    outputTrim.setSliderStyle(juce::Slider::LinearHorizontal);
    outputTrim.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 22);
    outputTrim.setRange(-24.0, 12.0, 0.1);
    outputTrim.setTextValueSuffix(" dB");
    for (auto* slider : { &harmonics, &exciter, &drive, &outputTrim }) addAndMakeVisible(*slider);
    range.setComponentID("exciterRange");
    range.addItem("3 kHz", 1);
    range.addItem("8 kHz", 2);
    pushAll.setComponentID("pushAll");
    bypass.setComponentID("bypass");
    pushAll.setClickingTogglesState(true);
    bypass.setClickingTogglesState(true);
    addAndMakeVisible(range);
    addAndMakeVisible(pushAll);
    addAndMakeVisible(bypass);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
}

void HarmoHitAudioProcessorEditor::resized()
{
    if (vintageSkin) { vintageSkin->setBounds(getLocalBounds()); return; }
    auto area = getLocalBounds().reduced(16);
    auto controls = area.removeFromTop(148);
    const auto placeSlider = [&controls] (juce::Label& label, juce::Slider& slider)
    {
        auto row = controls.removeFromTop(36);
        label.setBounds(row.removeFromLeft(112));
        slider.setBounds(row);
    };
    placeSlider(harmonicsLabel, harmonics);
    placeSlider(exciterLabel, exciter);
    placeSlider(driveLabel, drive);
    placeSlider(trimLabel, outputTrim);
    auto switches = area.removeFromTop(38);
    rangeLabel.setBounds(switches.removeFromLeft(58));
    range.setBounds(switches.removeFromLeft(104).reduced(0, 5));
    pushAll.setBounds(switches.removeFromLeft(112));
    bypass.setBounds(switches.removeFromLeft(100));
    area.removeFromTop(5);
    auto meters = area.removeFromTop(42);
    inputMeter.setBounds(meters.removeFromLeft(meters.getWidth() / 2).reduced(0, 2));
    outputMeter.setBounds(meters.reduced(0, 2));
}

void HarmoHitAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (vintageSkin) return;
    g.fillAll(juce::Colour(0xff20242a));
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(16.0f, 194.0f, static_cast<float>(getWidth() - 16), 194.0f, 1.0f);
}
