# VitaPlug HARMOHIT — Phase 1

Two AU/VST3 effects share the header-only `HarmoCore` DSP engine: **VitaPlug
HARMOHIT MIX** (`com.vitaplug.harmohit.mix`, `HhMx`) and **VitaPlug HARMOHIT
MASTER** (`com.vitaplug.harmohit.master`, `HhMs`).  Their stable identifiers
are deliberately distinct.

## Signal path

`dry input -> [Harmonics residual | Exciter residual | Drive residual] in
parallel -> sum with dry -> Output Trim`

Each branch is a residual generator, so it cannot filter or delay the dry
path. Harmonics combines even/odd terms and removes its DC component. Exciter
uses independent 1.8–5.2 kHz and 5.5–14 kHz filter paths before generating
nonlinear residuals; PUSH ALL averages their residuals to prevent unbounded
level doubling. Drive uses a progressive tanh transfer and DC removal.

MIX and MASTER share algorithms but differ in the control law and maxima:
MASTER uses a lower, progressive curve for precise small adjustments; MIX
retains a broader high-end creative range. The curves are monotonic and reserve
the strongest colour for the final control travel. All continuous controls ramp
over 20 ms. There is deliberately no post-DSP limiter, mandatory output
normalisation or clipping: samples above 0 dBFS remain valid floating-point
output. The only bounded transfers are the explicit artistic saturators in the
Harmonics, Exciter and Drive branches; Output Trim remains entirely manual.

`HarmoCore` has fixed two-channel state and no allocation in `process`.
NaN/Inf input samples are made safe before they reach the resampling filters.
All three nonlinear branches run after two JUCE polyphase-IIR 2× stages (4×
effective). The complete dry-plus-residual sum is downsampled together, so
there is no separate dry/wet path to misalign. JUCE is configured for integer
latency and that exact latency is reported to the host at prepare time; bypass
continues through the same resampling route. The stage-count architecture is
ready for a future prepared third stage (8×), but exposes no runtime mode yet.

## Build and test

```sh
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target VitaPlugHarmoHitMix_AU VitaPlugHarmoHitMix_VST3 VitaPlugHarmoHitMaster_AU VitaPlugHarmoHitMaster_VST3 VitaPlugHarmoHitDSPTests
ctest --test-dir build --output-on-failure
```

The processor intentionally reports no editor in Phase 1. Parameters are
automatable and serialised through `AudioProcessorValueTreeState`.
