#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "BinaryData.h"

namespace
{
class PreviewWindow final : public juce::DocumentWindow
{
public:
    explicit PreviewWindow(juce::Component* content)
        : DocumentWindow("VitaPlug HARMOHIT MIX — Editor Preview", juce::Colours::black, juce::DocumentWindow::closeButton)
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
        writeResourceDiagnostics();
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
    void writeResourceDiagnostics()
    {
        const auto directory = juce::File(HARMOHIT_PROJECT_DIR).getChildFile("Documentation/visual-checks");
        directory.createDirectory();
        juce::String report;
        report << "resourcePointer=" << (BinaryData::HarmoHitMixPanelArtworkRuntime_jpg != nullptr ? "non-null" : "null") << "\n";
        report << "resourceBytes=" << BinaryData::HarmoHitMixPanelArtworkRuntime_jpgSize << "\n";
        const auto generic = juce::ImageFileFormat::loadFrom(BinaryData::HarmoHitMixPanelArtworkRuntime_jpg,
                                                               static_cast<size_t>(BinaryData::HarmoHitMixPanelArtworkRuntime_jpgSize));
        report << "genericValid=" << (generic.isValid() ? "true" : "false") << "\n";
        report << "genericSize=" << generic.getWidth() << "x" << generic.getHeight() << "\n";
        const auto pixel = generic.isValid() ? generic.getPixelAt(generic.getWidth() / 2, generic.getHeight() / 2) : juce::Colours::transparentBlack;
        report << "centrePixelARGB=" << juce::String::toHexString(static_cast<int>(pixel.getARGB())) << "\n";
        directory.getChildFile("HARMOHIT_MIX_editor_background_diagnostics.txt").replaceWithText(report);
    }

    void timerCallback() override
    {
        stopTimer();
        auto* editor = window->getContentComponent();
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false, 1.0f);
        const auto outputDirectory = juce::File(HARMOHIT_PROJECT_DIR).getChildFile("Documentation/visual-checks");
        outputDirectory.createDirectory();
        const auto output = outputDirectory.getChildFile("HARMOHIT_MIX_editor_background_1180x590.png");
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
