#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
class PreviewWindow final : public juce::DocumentWindow
{
public:
    explicit PreviewWindow(juce::Component* content)
        : DocumentWindow("VitaPlug HARMOHIT MIX Editor Preview", juce::Colours::black, juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(content, true);
        centreWithSize(content->getWidth(), content->getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class EditorPreviewApplication final : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "HARMOHIT MIX Editor Preview"; }
    const juce::String getApplicationVersion() override { return "0.1"; }

    void initialise(const juce::String&) override
    {
        processor = std::make_unique<HarmoHitAudioProcessor>();
        processor->setPlayConfigDetails(2, 2, 48000.0, 512);
        processor->prepareToPlay(48000.0, 512);
        window = std::make_unique<PreviewWindow>(processor->createEditor());
        startTimer(500);
    }

    void shutdown() override
    {
        window.reset();
        processor.reset();
    }

private:
    void timerCallback() override
    {
        stopTimer();
        auto* editor = window->getContentComponent();
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false, 1.0f);
        const auto outputDirectory = juce::File(HARMOHIT_PROJECT_DIR).getChildFile("Documentation/visual-checks");
        outputDirectory.createDirectory();
        const auto output = outputDirectory.getChildFile("HARMOHIT_MIX_engineering_smoke_r1_520x300.png");
        juce::FileOutputStream stream(output);
        if (stream.openedOk())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream(snapshot, stream);
            stream.flush();
        }
        quit();
    }

    std::unique_ptr<HarmoHitAudioProcessor> processor;
    std::unique_ptr<PreviewWindow> window;
};
}

START_JUCE_APPLICATION(EditorPreviewApplication)
