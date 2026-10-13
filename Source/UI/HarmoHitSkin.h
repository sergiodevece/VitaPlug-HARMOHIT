#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

// UI05: solo capa de presentación. No contiene procesamiento de muestras de audio.
namespace harmohit::ui
{
    struct ParameterIds
    {
        juce::String harmonics { "harmonics" };
        juce::String exciter   { "exciter" };
        juce::String drive     { "drive" };
        juce::String trim      { "outputTrim" };
        juce::String range     { "exciterRange" };
        juce::String pushAll   { "pushAll" };

        // IMPORTANTE: verificar la polaridad contra la implementación real APVTS.
        bool rangeThreeKAtMaximum = false;
        bool pushAllInAtMaximum = true;
    };

    // Devuelve dB en la escala VISUAL VU -20...+3, no RMS ni dBFS sin calibrar.
    // Proveedor de niveles leído solo desde el hilo de interfaz.
    using MeterVuReader = std::function<float()>;

    class HarmoHitSkin final : public juce::Component, private juce::Timer
    {
    public:
        static constexpr int designWidth = 760;
        static constexpr int designHeight = 380;

        HarmoHitSkin (juce::AudioProcessorValueTreeState&, ParameterIds,
                      MeterVuReader readInputVu = {}, MeterVuReader readOutputVu = {});
        ~HarmoHitSkin() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        juce::StringArray getMissingParameterIds() const { return missingParameterIds; }

    private:
        class ImageKnob;
        class SilentButton;
        struct Meter { float current = -20.0f; float target = -20.0f; };

        void timerCallback() override;
        void toggleParameter (juce::RangedAudioParameter*);
        static float clampVu (float);
        static juce::Point<float> scalePoint (int meterIndex, float vu);
        void drawNeedle (juce::Graphics&, int meterIndex, float value) const;
        static juce::Image loadEmbedded (const unsigned char*, size_t);
        static float readVu (const MeterVuReader&);

        juce::AudioProcessorValueTreeState& apvts;
        ParameterIds ids;
        MeterVuReader inputReader, outputReader;
        juce::Image panel, range3k, pushOff;
        std::array<std::unique_ptr<ImageKnob>, 4> knobs;
        std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> attachments;
        std::unique_ptr<SilentButton> rangeButton, pushButton;
        juce::RangedAudioParameter* rangeParameter = nullptr;
        juce::RangedAudioParameter* pushParameter = nullptr;
        juce::StringArray missingParameterIds;
        Meter meters[2];
        bool previousThreeK = false, previousPushIn = true;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmoHitSkin)
    };

    // Adaptador de editor de tamaño fijo: el procesador existente conserva su DSP.
    class HarmoHitEditorAdapter final : public juce::AudioProcessorEditor
    {
    public:
        HarmoHitEditorAdapter (juce::AudioProcessor& audioProcessor,
                               juce::AudioProcessorValueTreeState& state,
                               ParameterIds ids,
                               MeterVuReader input = {}, MeterVuReader output = {})
            : juce::AudioProcessorEditor (&audioProcessor), skin (state, std::move(ids), std::move(input), std::move(output))
        {
            addAndMakeVisible (skin);
            setSize (HarmoHitSkin::designWidth, HarmoHitSkin::designHeight);
            setResizable (false, false);
        }
        void resized() override { skin.setBounds (getLocalBounds()); }
        juce::StringArray getMissingParameterIds() const { return skin.getMissingParameterIds(); }
    private:
        HarmoHitSkin skin;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmoHitEditorAdapter)
    };
}
