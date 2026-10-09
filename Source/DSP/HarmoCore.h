#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

namespace vitaplug::harmohit::dsp
{
enum class ExciterMode : int { presence3k = 0, brilliance8k, pushAll };
enum class ProductVariant : int { mix, master };

struct HarmoSettings
{
    float harmonics = 0.0f;
    float exciter = 0.0f;
    float drive = 0.0f;
    float outputTrimDb = 0.0f;
    ExciterMode exciterMode = ExciterMode::presence3k;
};

// Fixed-storage core: it accepts host-owned channel pointers and never creates
// memory on the audio thread.  Branches produce residuals, which are summed
// with the unfiltered dry input; zero intensities are therefore transparent.
class HarmoCore
{
public:
    // Two 2x stages give the production 4x path. The stage-count constant is
    // deliberately separate so a future prepared 8x mode can add a third
    // stage without changing the branch processing architecture.
    static constexpr int oversamplingStages = 2;
    static constexpr int oversamplingFactor = 1 << oversamplingStages;
    static constexpr int futureOversamplingStages = 3;

    explicit HarmoCore(ProductVariant productVariant)
        : variant(productVariant), oversampling(2, oversamplingStages,
              juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true) {}

    void prepare(double newSampleRate, int maximumBlockSize, int newChannels)
    {
        baseSampleRate = std::max(1.0, newSampleRate);
        sampleRate = baseSampleRate * static_cast<double>(oversamplingFactor);
        channels = std::clamp(newChannels, 1, 2);
        oversampling.initProcessing(static_cast<size_t>(std::max(1, maximumBlockSize)));
        reset();
        for (auto& smoother : smoothers)
            smoother.reset();
        smoothers[3].reset(1.0f);
        exciterPresenceMix.reset(1.0f);
        exciterBrillianceMix.reset(0.0f);
    }

    void reset() noexcept
    {
        for (auto& channel : state) channel = {};
        oversampling.reset();
    }

    int getOversamplingFactor() const noexcept { return oversamplingFactor; }
    int getLatencySamples() const noexcept { return static_cast<int>(std::lround(oversampling.getLatencyInSamples())); }

    void process(float* const* data, int requestedChannels, int samples, const HarmoSettings& settings) noexcept
    {
        if (data == nullptr || samples <= 0) return;
        const auto activeChannels = std::min({ requestedChannels, channels, 2 });
        if (activeChannels <= 0) return;

        // The oversampler must never receive non-finite values: sanitise before
        // its filters, then again after downsampling as an output safety net.
        for (int channel = 0; channel < activeChannels; ++channel)
            for (int sample = 0; sample < samples; ++sample)
                data[channel][sample] = finiteOrZero(data[channel][sample]);

        const auto harmonicTarget = shapeHarmonicsIntensity(settings.harmonics);
        const auto exciterTarget = shapeIntensity(settings.exciter, 0.75f, 0.26f);
        const auto driveTarget = shapeIntensity(settings.drive, 1.75f, 0.65f);
        const auto trimTarget = dbToGain(std::clamp(settings.outputTrimDb, -24.0f, 12.0f));
        const auto ramp = std::max(1, static_cast<int>(sampleRate * 0.020));
        smoothers[0].setTarget(harmonicTarget, ramp);
        smoothers[1].setTarget(exciterTarget, ramp);
        smoothers[2].setTarget(driveTarget, ramp);
        smoothers[3].setTarget(trimTarget, ramp);
        const auto presenceTarget = settings.exciterMode == ExciterMode::brilliance8k ? 0.0f
                                  : settings.exciterMode == ExciterMode::pushAll ? 0.5f : 1.0f;
        const auto brillianceTarget = settings.exciterMode == ExciterMode::presence3k ? 0.0f
                                   : settings.exciterMode == ExciterMode::pushAll ? 0.5f : 1.0f;
        // Both spectral residuals are always calculated. Crossfading their
        // weights at the oversampled rate prevents a range/PUSH ALL switch from
        // creating a discontinuity, without changing Harmonics or Drive.
        exciterPresenceMix.setTarget(presenceTarget, ramp);
        exciterBrillianceMix.setTarget(brillianceTarget, ramp);

        std::array<const float*, 2> constData {};
        for (int channel = 0; channel < activeChannels; ++channel)
            constData[static_cast<size_t>(channel)] = data[channel];
        const juce::dsp::AudioBlock<const float> inputBlock { constData.data(), static_cast<size_t>(activeChannels), static_cast<size_t>(samples) };
        auto oversampled = oversampling.processSamplesUp(inputBlock).getSubsetChannelBlock(0, static_cast<size_t>(activeChannels));

        for (size_t sample = 0; sample < oversampled.getNumSamples(); ++sample)
        {
            const auto harmonicAmount = smoothers[0].next();
            const auto exciterAmount = smoothers[1].next();
            const auto driveAmount = smoothers[2].next();
            const auto trim = smoothers[3].next();
            const auto presenceMix = exciterPresenceMix.next();
            const auto brillianceMix = exciterBrillianceMix.next();
            for (int channel = 0; channel < activeChannels; ++channel)
            {
                auto& s = state[static_cast<size_t>(channel)];
                const float input = finiteOrZero(oversampled.getChannelPointer(static_cast<size_t>(channel))[sample]);
                const float harmonicInput = variant == ProductVariant::mix
                                              ? applyMixHarmonicsShelves(input, harmonicAmount, s)
                                              : input;
                const float masterHarmonics = variant == ProductVariant::master
                                                ? masterR2HarmonicResidual(input, harmonicAmount, s.masterHarmonicDc)
                                                : 0.0f;
                const float exciter = exciterResidual(harmonicInput, exciterAmount, presenceMix, brillianceMix, s);
                const float drive = driveResidual(harmonicInput, driveAmount, s.driveDc);
                oversampled.getChannelPointer(static_cast<size_t>(channel))[sample]
                    = finiteOrZero((harmonicInput + masterHarmonics + exciter + drive) * trim);
            }
        }
        juce::dsp::AudioBlock<float> outputBlock { data, static_cast<size_t>(activeChannels), static_cast<size_t>(samples) };
        oversampling.processSamplesDown(outputBlock);
        for (int channel = 0; channel < activeChannels; ++channel)
            for (int sample = 0; sample < samples; ++sample)
                data[channel][sample] = finiteOrZero(data[channel][sample]);
    }

private:
    struct OnePole { float z = 0.0f; };
    struct ChannelState
    {
        OnePole masterHarmonicDc, driveDc;
        OnePole harmonicBodyLp, harmonicPresenceLp, harmonicAirLp;
        OnePole presenceLp, presenceHp, brillianceLp, brillianceHp;
    };
    struct Smoother
    {
        float current = 0.0f, target = 0.0f, step = 0.0f; int remaining = 0;
        void reset(float value = 0.0f) noexcept { current = target = value; step = 0.0f; remaining = 0; }
        void setTarget(float value, int samples) noexcept
        {
            if (std::abs(value - target) < 1.0e-7f) return;
            target = value; remaining = std::max(1, samples); step = (target - current) / static_cast<float>(remaining);
        }
        float next() noexcept
        {
            if (remaining > 0)
            {
                current += step;
                if (--remaining == 0) current = target;
            }
            return current;
        }
    };

    static float finiteOrZero(float value) noexcept { return std::isfinite(value) ? value : 0.0f; }
    static float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
    float shapeIntensity(float control, float mixMaximum, float masterMaximum) const noexcept
    {
        const float x = std::clamp(finiteOrZero(control), 0.0f, 1.0f);
        // Progressive, monotonic law: the shallow initial slope gives useful
        // low/mid adjustment resolution, while the cubic rise reserves the
        // intentionally aggressive colour range for the upper control travel.
        // This is a mapping only: no output limiter or auto-normalisation is
        // applied anywhere after the artistic processing stages.
        const float curve = variant == ProductVariant::master
                              ? x * (0.60f + 0.40f * x * x)
                              : x * (0.38f + 0.62f * x * x);
        return curve * (variant == ProductVariant::mix ? mixMaximum : masterMaximum);
    }
    float shapeHarmonicsIntensity(float control) const noexcept
    {
        const float x = std::clamp(finiteOrZero(control), 0.0f, 1.0f);
        if (variant == ProductVariant::master)
            return x * (0.65f + 0.35f * x) * 0.34f;

        // MIX keeps the established HARMONICS parameter name and 0--1 macro
        // domain, but R4 maps it to three linear shelving stages. MASTER keeps
        // its previous nonlinear R2 mapping above.
        return x;
    }
    float dcBlock(float value, OnePole& dcState) const noexcept
    {
        const float pole = static_cast<float>(std::exp(-2.0 * 3.14159265358979323846 * 12.0 / sampleRate));
        const float output = value - dcState.z;
        dcState.z = value * (1.0f - pole) + dcState.z * pole;
        return output;
    }
    float lowpass(float x, float hz, OnePole& s) const noexcept
    {
        const float a = 1.0f - static_cast<float>(std::exp(-2.0 * 3.14159265358979323846 * hz / sampleRate));
        s.z += a * (x - s.z); return s.z;
    }
    float bandpass(float x, float low, float high, OnePole& lp, OnePole& hp) const noexcept
    {
        const float belowHigh = lowpass(x, high, lp);
        return belowHigh - lowpass(belowHigh, low, hp);
    }
    struct HarmonicShelfGains
    {
        float body = 1.0f;
        float presence = 1.0f;
        float air = 1.0f;
    };
    HarmonicShelfGains calibrateMixHarmonics(float macro) const noexcept
    {
        const float x = std::clamp(finiteOrZero(macro), 0.0f, 1.0f);
        // Independent concave laws move R4's useful upper-range colour into
        // the middle of the macro without collapsing the 0--25% adjustment
        // range. At 50%, the nominal 300/1800/12000 Hz gains closely match
        // R4 around 90%; each curve remains smooth and strictly increasing.
        const float bodyDb = 6.0f * x * (1.35f - 0.35f * x);
        const float presenceDb = 7.0f * x * (1.40f - 0.40f * x);
        const float airDb = 9.0f * x * (1.50f - 0.50f * x);
        return { dbToGain(bodyDb), dbToGain(presenceDb), dbToGain(airDb) };
    }
    float applyMixHarmonicsShelves(float x, float macro, ChannelState& channelState) const noexcept
    {
        const auto gains = calibrateMixHarmonics(macro);

        // Three inexpensive first-order shelves run continuously so their
        // states are already settled when the smoothed macro leaves zero.
        // With unity gains the algebra returns x exactly; no dry/wet
        // cancellation, saturation, DC blocker or output normalisation exists.
        const float bodyBand = lowpass(x, 300.0f, channelState.harmonicBodyLp);
        const float bodyShaped = x + (gains.body - 1.0f) * bodyBand;

        const float belowPresence = lowpass(bodyShaped, 1800.0f, channelState.harmonicPresenceLp);
        const float presenceBand = bodyShaped - belowPresence;
        const float presenceShaped = bodyShaped + (gains.presence - 1.0f) * presenceBand;

        const float belowAir = lowpass(presenceShaped, 12000.0f, channelState.harmonicAirLp);
        const float airBand = presenceShaped - belowAir;
        return presenceShaped + (gains.air - 1.0f) * airBand;
    }
    float masterR2HarmonicResidual(float x, float amount, OnePole& dc) const noexcept
    {
        // MASTER is outside R4's authorised scope. Keep its R2 generator and
        // macro relationship bit-for-bit independent of the new MIX path.
        const float bounded = std::tanh(x * (1.0f + 1.65f * amount));
        const float even = bounded * bounded;
        const float odd = bounded * bounded * bounded;
        return dcBlock((0.68f * even + 0.32f * odd) * amount, dc);
    }
    float exciterResidual(float x, float amount, float presenceMix, float brillianceMix, ChannelState& s) const noexcept
    {
        if (amount <= 0.0f) return 0.0f;
        const float presence = bandpass(x, 1800.0f, 5200.0f, s.presenceLp, s.presenceHp);
        const float brilliance = bandpass(x, 5500.0f, 14000.0f, s.brillianceLp, s.brillianceHp);
        const float p = std::tanh(presence * (1.0f + 5.0f * amount)) - presence;
        const float b = std::tanh(brilliance * (1.0f + 5.0f * amount)) - brilliance;
        return (p * presenceMix + b * brillianceMix) * amount;
    }
    float driveResidual(float x, float amount, OnePole& dc) const noexcept
    {
        if (amount <= 0.0f) return 0.0f;
        const float wet = std::tanh(x * (1.0f + 12.0f * amount));
        return dcBlock((wet - x) * amount, dc);
    }

    ProductVariant variant;
    double baseSampleRate = 44100.0, sampleRate = 176400.0;
    int channels = 2;
    juce::dsp::Oversampling<float> oversampling;
    std::array<ChannelState, 2> state {};
    std::array<Smoother, 4> smoothers {};
    Smoother exciterPresenceMix, exciterBrillianceMix;
};
} // namespace vitaplug::harmohit::dsp
