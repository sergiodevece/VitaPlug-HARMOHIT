#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
namespace HarmoHitMixLayout
{
constexpr float width = 1180.0f;
constexpr float height = 590.0f;
constexpr int artworkWidthPixels = 1774;
constexpr int artworkHeightPixels = 887;
constexpr float artworkWidth = static_cast<float>(artworkWidthPixels);
// 1180 / 1774 == 590 / 887, so the source artwork maps with one exact,
// uniform scale factor and never needs cropping or aspect-ratio correction.
static_assert(1180 * artworkHeightPixels == 590 * artworkWidthPixels);
constexpr float artworkScale = width / artworkWidth;
constexpr float sourceX(float x) { return x * artworkScale; }
constexpr float sourceY(float y) { return y * artworkScale; }

const juce::Rectangle<float> inputVu { sourceX(124.0f), sourceY(304.0f), sourceX(328.0f), sourceY(202.0f) };
const juce::Rectangle<float> outputVu { sourceX(1313.0f), sourceY(509.0f), sourceX(328.0f), sourceY(202.0f) };
constexpr std::array<juce::Point<float>, 3> knobCentres {{{ sourceX(600.0f), sourceY(478.0f) },
                                                             { sourceX(887.0f), sourceY(478.0f) },
                                                             { sourceX(1174.0f), sourceY(478.0f) }}};
constexpr juce::Point<float> rangeCentre { sourceX(199.0f), sourceY(639.0f) };
constexpr juce::Point<float> pushAllCentre { sourceX(354.0f), sourceY(639.0f) };
constexpr juce::Point<float> trimCentre { sourceX(1488.0f), sourceY(416.0f) };
}

const auto ivory = juce::Colour(0xffe9d4ae);
const auto brass = juce::Colour(0xffa57533);

juce::Point<float> polar(juce::Point<float> centre, float radius, float radians)
{
    return { centre.x + radius * std::cos(radians), centre.y + radius * std::sin(radians) };
}

void drawVuNeedle(juce::Graphics& g, juce::Rectangle<float> bounds, float dbfs)
{
    // The face, frame, scale and printed label are now part of panelArtwork.
    // These measurements map the moving needle to the pivot in that artwork.
    const auto pivot = juce::Point<float>(bounds.getCentreX(), bounds.getY() + bounds.getHeight() - 20.0f);
    constexpr float first = juce::MathConstants<float>::pi * 0.84f;
    constexpr float last = juce::MathConstants<float>::pi * 0.16f;
    // 0 VU = -18 dBFS RMS. The face retains -20..+3 as its printed region,
    // while the needle has a useful -30..+6 VU movement range.
    const auto vu = juce::jlimit(-30.0f, 6.0f, dbfs + 18.0f);
    const auto needleAngle = first + (last - first) * ((vu + 30.0f) / 36.0f);
    const auto needle = polar(pivot, 76.0f, needleAngle);
    g.setColour(juce::Colour(0xff17110c));
    g.drawLine({ pivot, needle }, 2.1f);
    g.fillEllipse(pivot.x - 6.0f, pivot.y - 6.0f, 12.0f, 12.0f);
}

void drawMainKnob(juce::Graphics& g, juce::Rectangle<float> bounds, float proportion)
{
    constexpr float radius = 54.0f;
    const auto centre = juce::Point<float>(bounds.getCentreX(), bounds.getY() + 92.0f);
    juce::ColourGradient metal(juce::Colour(0xff5d4632), centre.x - 40.0f, centre.y - 48.0f, juce::Colour(0xff171411), centre.x + 50.0f, centre.y + 58.0f, false);
    g.setGradientFill(metal);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(brass.withAlpha(0.65f));
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.2f);
    const auto indicatorAngle = juce::degreesToRadians(135.0f + 270.0f * proportion);
    g.setColour(juce::Colour(0xffecede4));
    g.drawLine({ polar(centre, 25.0f, indicatorAngle), polar(centre, 48.0f, indicatorAngle) }, 3.0f);
}

void drawTrimKnob(juce::Graphics& g, juce::Rectangle<float> bounds, float proportion)
{
    const auto centre = juce::Point<float>(bounds.getCentreX(), bounds.getY() + 79.0f);
    g.setColour(juce::Colour(0xff141312)); g.fillEllipse(centre.x - 29.0f, centre.y - 29.0f, 58.0f, 58.0f);
    g.setColour(brass.withAlpha(0.65f)); g.drawEllipse(centre.x - 29.0f, centre.y - 29.0f, 58.0f, 58.0f, 1.8f);
    const auto angle = juce::degreesToRadians(135.0f + 270.0f * proportion);
    g.setColour(ivory); g.drawLine({ polar(centre, 12.0f, angle), polar(centre, 27.0f, angle) }, 2.0f);
}

juce::Image loadPanelArtwork()
{
    juce::MemoryInputStream stream(BinaryData::HarmoHitMixPanelArtworkRuntime_jpg,
                                   static_cast<size_t>(BinaryData::HarmoHitMixPanelArtworkRuntime_jpgSize), false);
    juce::JPEGImageFormat jpeg;
    return jpeg.decodeImage(stream);
}
}

HarmoVuMeter::HarmoVuMeter(juce::String newCaption, std::atomic<float>& newSourceDbfs,
                           std::atomic<uint64_t>& newSourceCounter)
    : caption(std::move(newCaption)), sourceDbfs(newSourceDbfs), sourceCounter(newSourceCounter)
{
    // This timer exists only while the editor and its meter components exist.
    startTimerHz(30);
}

void HarmoVuMeter::timerCallback()
{
    const auto counter = sourceCounter.load(std::memory_order_relaxed);
    const auto target = counter != lastCounter ? sourceDbfs.load(std::memory_order_relaxed) : -72.0f;
    lastCounter = counter;
    const auto response = target > displayedDbfs ? 0.38f : 0.12f;
    const auto next = displayedDbfs + (target - displayedDbfs) * response;
    if (std::abs(next - displayedDbfs) > 0.002f)
    {
        displayedDbfs = next;
        repaint();
    }
}

void HarmoVuMeter::paint(juce::Graphics& g)
{
    drawVuNeedle(g, getLocalBounds().toFloat(), displayedDbfs);
}

HarmoMainKnob::HarmoMainKnob(juce::String newCaption) : caption(std::move(newCaption))
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 100.0, 0.1);
    setRotaryParameters(juce::degreesToRadians(135.0f), juce::degreesToRadians(405.0f), true);
    setDoubleClickReturnValue(true, 0.0);
    setVelocityBasedMode(true);
    setVelocityModeParameters(0.75, 1, 0.0, true);
    setMouseDragSensitivity(220);
    setWantsKeyboardFocus(true);
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
}

void HarmoMainKnob::paint(juce::Graphics& g)
{
    drawMainKnob(g, getLocalBounds().toFloat(), static_cast<float>(valueToProportionOfLength(getValue())));
}

void HarmoMainKnob::valueChanged()
{
    repaint();
}

HarmoRangeSwitch::HarmoRangeSwitch() : juce::Button("Exciter Range")
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

void HarmoRangeSwitch::paintButton(juce::Graphics& g, bool, bool)
{
    const auto centreX = getLocalBounds().getCentreX();
    constexpr float pivotY = 35.0f;
    const auto leverY = getToggleState() ? 49.0f : 21.0f;
    g.setColour(juce::Colour(0xffbeb8aa)); g.fillEllipse(static_cast<float>(centreX - 8), pivotY - 8.0f, 16.0f, 16.0f);
    g.setColour(juce::Colour(0xff292723)); g.fillEllipse(static_cast<float>(centreX - 5), pivotY - 5.0f, 10.0f, 10.0f);
    g.setColour(ivory); g.drawLine(static_cast<float>(centreX), pivotY, static_cast<float>(centreX), leverY, 2.2f);
    g.setColour(juce::Colour(0xffb8b1a4)); g.fillEllipse(static_cast<float>(centreX - 4), leverY - 4.0f, 8.0f, 8.0f);
}

HarmoPushAllButton::HarmoPushAllButton() : juce::Button("Push All")
{
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

void HarmoPushAllButton::paintButton(juce::Graphics& g, bool, bool isDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(11.0f);
    g.setColour(juce::Colours::black.withAlpha(0.75f)); g.fillEllipse(bounds);
    g.setColour(brass.withAlpha(getToggleState() ? 1.0f : 0.65f)); g.drawEllipse(bounds, getToggleState() ? 2.2f : 1.4f);
    g.setColour(getToggleState() ? juce::Colour(0xffffc03a) : juce::Colour(0xff60594e));
    g.fillEllipse(bounds.reduced(isDown ? 12.0f : 11.0f));
}

HarmoTrimKnob::HarmoTrimKnob()
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(-24.0, 12.0, 0.1);
    setRotaryParameters(juce::degreesToRadians(135.0f), juce::degreesToRadians(405.0f), true);
    setDoubleClickReturnValue(true, 0.0);
    setVelocityBasedMode(true);
    setVelocityModeParameters(0.7, 1, 0.0, true);
    setMouseDragSensitivity(180);
    setWantsKeyboardFocus(true);
}

void HarmoTrimKnob::paint(juce::Graphics& g)
{
    drawTrimKnob(g, getLocalBounds().toFloat(), static_cast<float>(valueToProportionOfLength(getValue())));
}

void HarmoTrimKnob::valueChanged() { repaint(); }

HarmoHitAudioProcessorEditor::HarmoHitAudioProcessorEditor(HarmoHitAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), panelArtwork(loadPanelArtwork())
{
    setSize(static_cast<int>(HarmoHitMixLayout::width), static_cast<int>(HarmoHitMixLayout::height));
    setResizable(false, false);
    addAndMakeVisible(inputVu);
    addAndMakeVisible(outputVu);
    addAndMakeVisible(harmonicsKnob);
    addAndMakeVisible(exciterKnob);
    addAndMakeVisible(driveKnob);
    addAndMakeVisible(rangeSwitch);
    addAndMakeVisible(pushAllButton);
    addAndMakeVisible(outputTrimKnob);
    rangeSwitch.onClick = [this]
    {
        if (!syncingLegacyExciterMode && !pushAllButton.getToggleState())
            setLegacyExciterMode(rangeSwitch.getToggleState() ? 1 : 0);
    };
    pushAllButton.onClick = [this]
    {
        if (!syncingLegacyExciterMode)
            setLegacyExciterMode(pushAllButton.getToggleState() ? 2 : (rangeSwitch.getToggleState() ? 1 : 0));
    };
    if (auto* legacy = dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.parameters.getParameter("exciterMode")))
    {
        pushAllAttachment = std::make_unique<juce::ParameterAttachment>(*legacy,
            [this] (float value) { syncLegacyExciterMode(value); });
        pushAllAttachment->sendInitialUpdate();
    }
    if (auto* range = dynamic_cast<juce::RangedAudioParameter*>(audioProcessor.parameters.getParameter("exciterRange")))
    {
        rangeModeAttachment = std::make_unique<juce::ParameterAttachment>(*range,
            [this] (float value)
            {
                if (!syncingLegacyExciterMode && !pushAllButton.getToggleState())
                    setLegacyExciterMode(juce::roundToInt(value));
            });
        rangeModeAttachment->sendInitialUpdate();
    }
}

void HarmoHitAudioProcessorEditor::setLegacyExciterMode(int mode)
{
    if (pushAllAttachment != nullptr)
        pushAllAttachment->setValueAsCompleteGesture(static_cast<float>(juce::jlimit(0, 2, mode)));
}

void HarmoHitAudioProcessorEditor::syncLegacyExciterMode(float value)
{
    const auto mode = juce::jlimit(0, 2, juce::roundToInt(value));
    const juce::ScopedValueSetter<bool> syncing(syncingLegacyExciterMode, true);
    pushAllButton.setToggleState(mode == 2, juce::dontSendNotification);
    if (mode < 2)
    {
        const auto isEightK = mode == 1;
        rangeSwitch.setToggleState(isEightK, juce::dontSendNotification);
        if (auto* range = audioProcessor.parameters.getParameter("exciterRange"))
            if (std::abs(range->getValue() - (isEightK ? 1.0f : 0.0f)) > 1.0e-6f)
                range->setValueNotifyingHost(isEightK ? 1.0f : 0.0f);
    }
}

void HarmoHitAudioProcessorEditor::resized()
{
    const auto scaleX = static_cast<float>(getWidth()) / HarmoHitMixLayout::width;
    const auto scaleY = static_cast<float>(getHeight()) / HarmoHitMixLayout::height;
    const auto scaledBounds = [scaleX, scaleY] (juce::Rectangle<float> bounds)
    {
        return juce::Rectangle<int>(juce::roundToInt(bounds.getX() * scaleX), juce::roundToInt(bounds.getY() * scaleY),
                                    juce::roundToInt(bounds.getWidth() * scaleX), juce::roundToInt(bounds.getHeight() * scaleY));
    };
    inputVu.setBounds(scaledBounds(HarmoHitMixLayout::inputVu));
    outputVu.setBounds(scaledBounds(HarmoHitMixLayout::outputVu));
    const auto knobBounds = [&scaledBounds] (juce::Point<float> centre)
    {
        return scaledBounds({ centre.x - 98.0f, centre.y - 92.0f, 196.0f, 200.0f });
    };
    harmonicsKnob.setBounds(knobBounds(HarmoHitMixLayout::knobCentres[0]));
    exciterKnob.setBounds(knobBounds(HarmoHitMixLayout::knobCentres[1]));
    driveKnob.setBounds(knobBounds(HarmoHitMixLayout::knobCentres[2]));
    rangeSwitch.setBounds(scaledBounds({ HarmoHitMixLayout::rangeCentre.x - 30.0f, HarmoHitMixLayout::rangeCentre.y - 35.0f, 60.0f, 70.0f }));
    pushAllButton.setBounds(scaledBounds({ HarmoHitMixLayout::pushAllCentre.x - 26.0f, HarmoHitMixLayout::pushAllCentre.y - 25.0f, 52.0f, 50.0f }));
    outputTrimKnob.setBounds(scaledBounds({ HarmoHitMixLayout::trimCentre.x - 80.0f, HarmoHitMixLayout::trimCentre.y - 79.0f, 160.0f, 140.0f }));
}

void HarmoHitAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto sx = static_cast<float>(getWidth()) / HarmoHitMixLayout::width;
    const auto sy = static_cast<float>(getHeight()) / HarmoHitMixLayout::height;
    g.addTransform(juce::AffineTransform::scale(sx, sy));
    g.fillAll(juce::Colours::magenta);
    if (panelArtwork.isValid())
        g.drawImageAt(panelArtwork, 0, 0);
    g.setColour(juce::Colours::white);
    g.fillEllipse(8.0f, 8.0f, 12.0f, 12.0f);
}
