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

    void initialise(const juce::String& commandLine) override
    {
        processor = std::make_unique<HarmoHitAudioProcessor>();
        processor->setPlayConfigDetails(2, 2, 48000.0, 512);
        processor->prepareToPlay(48000.0, 512);
        window = std::make_unique<PreviewWindow>(processor->createEditor());
        interactive = commandLine.contains("--interactive");
        startTimerHz(30);
    }

    void shutdown() override
    {
        window.reset();
        processor.reset();
    }

private:
    void timerCallback() override
    {
        // Exercise the real processor and editor; no hardware output is opened.
        juce::AudioBuffer<float> audio(2, 512);
        for (int frame = 0; frame < 512; ++frame)
        {
            const float sample = 0.178f * std::sin(phase);
            phase += juce::MathConstants<double>::twoPi * 1000.0 / 48000.0;
            audio.setSample(0, frame, sample);
            audio.setSample(1, frame, sample);
        }
        juce::MidiBuffer midi;
        processor->processBlock(audio, midi);
        if (interactive) return;
        ++ticks;
        if (ticks != 30 && ticks != 60 && ticks != 90 && ticks != 120) return;
        auto* editor = window->getContentComponent();
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false, 1.0f);
        const auto outputDirectory = juce::File(HARMOHIT_PROJECT_DIR).getChildFile("Documentation/visual-checks");
        outputDirectory.createDirectory();
        const auto output = outputDirectory.getChildFile("HARMOHIT_MIX_vintage_live_" + juce::String(ticks / 30) + "_760x380.png");
        juce::FileOutputStream stream(output);
        if (stream.openedOk())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream(snapshot, stream);
            stream.flush();
        }
        if (ticks == 30)
        {
            // GUI -> host: drive the actual attached sliders and selector buttons.
            auto* skin = editor->getChildComponent(0);
            for (const auto* id : {"harmonics", "exciter", "drive", "outputTrim"})
            {
                auto* slider = dynamic_cast<juce::Slider*>(skin->findChildWithID(id));
                if (slider == nullptr) { setApplicationReturnValue(1); quit(); return; }
                slider->setValue(juce::String(id) == "outputTrim" ? 3.0 : 70.0,
                                 juce::sendNotificationSync);
                const auto* parameter = processor->parameters.getParameter(id);
                if (std::abs(parameter->convertFrom0to1(parameter->getValue()) - slider->getValue()) > 0.01f)
                { setApplicationReturnValue(2); quit(); return; }
            }
            for (const auto* id : {"exciterRange", "pushAll"})
            {
                auto* button = dynamic_cast<juce::Button*>(skin->findChildWithID(id));
                if (button == nullptr) { setApplicationReturnValue(3); quit(); return; }
                button->onClick();
                if (processor->parameters.getParameter(id)->getValue() < 0.5f)
                { setApplicationReturnValue(4); quit(); return; }
            }
            processor->getStateInformation(savedState);
        }
        else if (ticks == 60)
        {
            // Host automation -> editor, followed by restoring a saved session.
            for (const auto* id : {"harmonics", "exciter", "drive", "outputTrim", "exciterRange", "pushAll"})
                processor->parameters.getParameter(id)->setValueNotifyingHost(0.0f);
        }
        else if (ticks == 90)
        {
            auto* skin = editor->getChildComponent(0);
            auto* slider = dynamic_cast<juce::Slider*>(skin->findChildWithID("harmonics"));
            if (slider == nullptr || std::abs(slider->getValue()) > 0.01)
            { setApplicationReturnValue(6); quit(); return; }
            processor->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
        }
        else
        {
            auto* skin = editor->getChildComponent(0);
            auto* slider = dynamic_cast<juce::Slider*>(skin->findChildWithID("harmonics"));
            if (editor->getWidth() != 760 || editor->getHeight() != 380 ||
                slider == nullptr || std::abs(slider->getValue() - 70.0) > 0.01 ||
                processor->parameters.getParameter("exciterRange")->getValue() < 0.5f ||
                processor->getVuUpdateCounter().load() == 0)
                setApplicationReturnValue(5);
            quit();
        }
    }

    std::unique_ptr<HarmoHitAudioProcessor> processor;
    std::unique_ptr<PreviewWindow> window;
    juce::MemoryBlock savedState;
    int ticks = 0;
    double phase = 0.0;
    bool interactive = false;
};
}

START_JUCE_APPLICATION(EditorPreviewApplication)
