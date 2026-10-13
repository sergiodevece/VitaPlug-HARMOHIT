#include "HarmoHitSkin.h"
#include "HarmoHitSkinAssets.h"
#include <cmath>

namespace harmohit::ui
{
    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;
        struct Asset { const unsigned char* bytes; size_t size; };
        using namespace harmohit::ui::embedded;

        // Coordenadas originales comprobadas en UI04 / escala 760x380.
        const std::array<juce::Rectangle<int>, 4> knobRects {{
            { 212, 157, 88, 88 }, { 336, 157, 88, 88 },
            { 459, 157, 88, 88 }, { 615, 155, 48, 48 }
        }};
        const juce::Rectangle<int> rangeRect  { 68, 247, 38, 51 };
        const juce::Rectangle<int> pushRect   { 135, 254, 53, 41 };

        struct CalibrationPoint { float vu, x, y; };
        constexpr std::array<CalibrationPoint, 4> inputScale {{
            {-20.0f, 91.5f,183.1f}, {-10.0f,123.8f,171.9f},
            {  0.0f,148.1f,177.1f}, {  3.0f,156.8f,184.0f}
        }};
        constexpr std::array<CalibrationPoint, 4> outputScale {{
            {-20.0f,599.6f,272.0f}, {-10.0f,633.2f,260.1f},
            {  0.0f,656.2f,265.8f}, {  3.0f,665.2f,273.4f}
        }};
        constexpr juce::Point<float> pivots[2] { {123.0f,205.4f}, {633.3f,295.0f} };
    }

    class HarmoHitSkin::ImageKnob final : public juce::Slider
    {
    public:
        ImageKnob (juce::Image bodyImage, juce::Image pointerImage, juce::String accessibleName)
          : body(std::move(bodyImage)), pointer(std::move(pointerImage))
        {
            setName (accessibleName);
            setSliderStyle (juce::Slider::RotaryVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setRotaryParameters (-pi*0.75f, pi*0.75f, true);
            setMouseDragSensitivity (220);
            setWantsKeyboardFocus (true);
        }

        void paint (juce::Graphics& g) override
        {
            if (body.isValid())
                g.drawImageWithin (body, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);

            const auto minimum = getMinimum(), maximum = getMaximum();
            auto proportion = maximum > minimum
                              ? juce::jlimit (0.0, 1.0, (getValue()-minimum)/(maximum-minimum)) : 0.5;
            // The approved trim dial prints -12..+12. Preserve the host's
            // -24..+12 parameter range; values below -12 rest at the dial end.
            if (getComponentID() == "outputTrim")
                proportion = juce::jlimit(0.0, 1.0, (getValue() + 12.0) / 24.0);
            const auto theta = juce::degreesToRadians (static_cast<float>(-135.0+270.0*proportion));
            const juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (theta,
                              static_cast<float>(getWidth())*0.5f, static_cast<float>(getHeight())*0.5f));
            if (pointer.isValid())
                g.drawImageWithin (pointer, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);
        }
        void valueChanged() override
        {
            setTooltip(getName() + ": " + juce::String(getValue(), 1)
                + (getComponentID() == "outputTrim" ? " dB" : " %"));
        }
    private:
        juce::Image body, pointer;
    };

    class HarmoHitSkin::SilentButton final : public juce::Button
    {
    public:
        explicit SilentButton (juce::String name) : juce::Button (std::move(name))
        {
            setWantsKeyboardFocus (true);
            setTriggeredOnMouseDown (false);
        }
        void paintButton (juce::Graphics&, bool, bool) override {} // No se redibuja el arte.
    };

    juce::Image HarmoHitSkin::loadEmbedded (const unsigned char* bytes, size_t count)
    {
        return juce::ImageFileFormat::loadFrom (bytes, count);
    }

    HarmoHitSkin::HarmoHitSkin (juce::AudioProcessorValueTreeState& state,
                                ParameterIds parameterIds, MeterVuReader inputVu, MeterVuReader outputVu)
       : apvts(state), ids(std::move(parameterIds)),
         inputReader(std::move(inputVu)), outputReader(std::move(outputVu))
    {
        panel   = loadEmbedded (panelImage, sizeof(panelImage));
        range3k = loadEmbedded (range3kImage, sizeof(range3kImage));
        pushOff = loadEmbedded (pushOffImage, sizeof(pushOffImage));

        struct KnobSpec { const char* name; juce::String id; Asset base, indicator; };
        const std::array<KnobSpec,4> specs {{
            {"HARMONICS", ids.harmonics, {harmonicsBody, sizeof(harmonicsBody)}, {harmonicsPointer, sizeof(harmonicsPointer)}},
            {"EXCITER", ids.exciter, {exciterBody, sizeof(exciterBody)}, {exciterPointer, sizeof(exciterPointer)}},
            {"DRIVE", ids.drive, {driveBody, sizeof(driveBody)}, {drivePointer, sizeof(drivePointer)}},
            {"OUTPUT TRIM", ids.trim, {trimBody, sizeof(trimBody)}, {trimPointer, sizeof(trimPointer)}}
        }};

        for (size_t i=0; i<specs.size(); ++i)
        {
            const auto& s=specs[i];
            knobs[i] = std::make_unique<ImageKnob> (loadEmbedded(s.base.bytes,s.base.size),
                                                    loadEmbedded(s.indicator.bytes,s.indicator.size), s.name);
            knobs[i]->setComponentID (s.id);
            knobs[i]->setTooltip (juce::String (s.name) + " — parámetro real del host");
            addAndMakeVisible (*knobs[i]);
            if (apvts.getParameter(s.id) != nullptr)
                attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                                 (apvts, s.id, *knobs[i]);
            else
            {
                missingParameterIds.add (s.id);
                knobs[i]->setEnabled (false);
            }
        }

        rangeParameter=apvts.getParameter(ids.range);
        pushParameter=apvts.getParameter(ids.pushAll);
        rangeButton=std::make_unique<SilentButton> ("RANGE 3k/8k");
        pushButton=std::make_unique<SilentButton> ("PUSH ALL IN/OFF");
        rangeButton->setComponentID(ids.range);
        pushButton->setComponentID(ids.pushAll);
        rangeButton->setTooltip("RANGE — 3 kHz / 8 kHz");
        pushButton->setTooltip("PUSH ALL — IN / OFF");
        rangeButton->onClick=[this]{ toggleParameter(rangeParameter); };
        pushButton->onClick=[this]{ toggleParameter(pushParameter); };
        addAndMakeVisible (*rangeButton);
        addAndMakeVisible (*pushButton);
        if (!rangeParameter) { missingParameterIds.add(ids.range); rangeButton->setEnabled(false); }
        if (!pushParameter)  { missingParameterIds.add(ids.pushAll); pushButton->setEnabled(false); }

        if (!missingParameterIds.isEmpty())
            juce::Logger::writeToLog ("HARMOHIT UI05: IDs APVTS no encontrados: " + missingParameterIds.joinIntoString(", "));

        // Initialize visual states directly from APVTS: no first-frame flash.
        if (rangeParameter != nullptr)
            previousThreeK = ((rangeParameter->getValue() >= 0.5f) == ids.rangeThreeKAtMaximum);
        if (pushParameter != nullptr)
            previousPushIn = ((pushParameter->getValue() >= 0.5f) == ids.pushAllInAtMaximum);

        // Processor delivers smoothed RMS in dBFS; visual meter is calibrated
        // to 0 VU at -18 dBFS RMS (adjust reference after a physical check).
        meters[0].current = meters[0].target = readVu(inputReader);
        meters[1].current = meters[1].target = readVu(outputReader);
        setSize (designWidth, designHeight);
        startTimerHz (30); // Solo GUI: nunca procesa muestras en el hilo de audio.
    }

    HarmoHitSkin::~HarmoHitSkin() { stopTimer(); }

    void HarmoHitSkin::resized()
    {
        for (size_t i=0; i<knobs.size(); ++i)
            knobs[i]->setBounds (knobRects[i]);
        rangeButton->setBounds (rangeRect);
        pushButton->setBounds (pushRect);
    }

    float HarmoHitSkin::clampVu (float v)
    {
        return std::isfinite(v) ? juce::jlimit (-20.0f, 3.0f, v) : -20.0f;
    }

    float HarmoHitSkin::readVu (const MeterVuReader& reader)
    {
        return reader ? clampVu(reader()) : -20.0f;
    }

    juce::Point<float> HarmoHitSkin::scalePoint (int index, float v)
    {
        const auto& marks = index==0 ? inputScale : outputScale;
        const auto clamped = clampVu(v);
        for (size_t i=1; i<marks.size(); ++i)
        {
            if (clamped <= marks[i].vu)
            {
                const auto& a=marks[i-1]; const auto& b=marks[i];
                const auto t=(clamped-a.vu)/(b.vu-a.vu);
                return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t};
            }
        }
        return { marks.back().x, marks.back().y };
    }

    void HarmoHitSkin::drawNeedle (juce::Graphics& g, int index, float value) const
    {
        const auto pivot=pivots[index], target=scalePoint(index,value);
        const auto delta=target-pivot;
        const auto length=delta.getDistanceFromOrigin();
        if (length <= 0.001f) return;
        const auto start=pivot + delta*(6.0f/length);
        g.setColour (juce::Colour::fromRGBA(15,9,3,125));
        g.drawLine (start.x+0.42f,start.y+0.65f,target.x+0.42f,target.y+0.65f,2.4f);
        g.setColour (juce::Colour::fromRGB(49,39,25));
        g.drawLine (start.x,start.y,target.x,target.y,1.6f);
        g.setColour (juce::Colour::fromRGBA(201,171,108,123));
        g.drawLine (start.x-0.28f,start.y-0.22f,target.x-0.28f,target.y-0.22f,0.45f);
    }

    void HarmoHitSkin::paint (juce::Graphics& g)
    {
        if (panel.isValid())
            g.drawImageWithin (panel, 0, 0, designWidth, designHeight, juce::RectanglePlacement::stretchToFit);
        if (previousThreeK && range3k.isValid())
            g.drawImageWithin (range3k, 72, 250, 28, 42, juce::RectanglePlacement::stretchToFit);
        if (!previousPushIn && pushOff.isValid())
            g.drawImageWithin (pushOff, 155, 254, 41, 39, juce::RectanglePlacement::stretchToFit);
        drawNeedle (g,0,meters[0].current);
        drawNeedle (g,1,meters[1].current);
    }

    void HarmoHitSkin::toggleParameter (juce::RangedAudioParameter* parameter)
    {
        if (!parameter) return;
        // Válido para parámetro binario: bool o choice con exactamente dos elementos.
        const auto target = parameter->getValue() >= 0.5f ? 0.0f : 1.0f;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (target);
        parameter->endChangeGesture();
    }

    void HarmoHitSkin::timerCallback()
    {
        // Lectura del estado APVTS: sincroniza los sprites también ante automatización del host.
        const auto rangeHigh = rangeParameter && rangeParameter->getValue() >= 0.5f;
        const auto pushHigh  = pushParameter  && pushParameter->getValue()  >= 0.5f;
        const auto threeK = rangeParameter ? (rangeHigh == ids.rangeThreeKAtMaximum) : false;
        const auto pushIn = pushParameter ? (pushHigh == ids.pushAllInAtMaximum) : true;
        bool changed = (threeK!=previousThreeK || pushIn!=previousPushIn);
        previousThreeK=threeK; previousPushIn=pushIn;

        const MeterVuReader readers[2] {inputReader,outputReader};
        for (int i=0;i<2;++i)
        {
            auto& m=meters[i];
            m.target=readVu(readers[i]);
            const auto difference=m.target-m.current;
            if (std::abs(difference) > 0.01f)
            {
                // Aproximación balística visual. El detector real está en el procesador.
                constexpr auto frameSeconds=1.0f/30.0f;
                const auto tau=difference>0.0f ? 0.100f : 0.220f;
                m.current+=difference*(1.0f-std::exp(-frameSeconds/tau));
                changed=true;
            }
            else if (std::abs (difference) > 1.0e-6f) { m.current=m.target; changed=true; }
        }
        if (changed) repaint();
    }
}
