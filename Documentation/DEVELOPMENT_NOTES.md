# HARMOHIT development notes

This document records the engineering decisions represented by the current repository. It is not a release claim or a substitute for listening tests in a target host.

## Product state

The repository builds two variants from the same core: HARMOHIT MIX and HARMOHIT MASTER. The MIX DSP is the active subject of the R5 HARMONICS recalibration in commit `34a1909`; MASTER deliberately keeps its separate, earlier nonlinear HARMONICS behavior. The variants have different plug-in identifiers and are not interchangeable session formats.

The historical revision markers retained in Git are:

| Reference | Meaning |
| --- | --- |
| `664a8a2` | HARMOHIT baseline |
| `09f2b3f` | Engineering-editor R1 work |
| `34a1909` | MIX spectral-macro reconstruction/recalibration |
| `recovery/dsp-v2` | Recovery branch, currently pointing at `34a1909` |
| `feature/harmohit-vintage-ui` | Current branch at publication preparation |

These references provide history, not a guarantee of the latest state. The files and tests at the checked-out commit are authoritative.

## Why HARMONICS became a spectral macro

The original HARMONICS direction was a nonlinear harmonic generator. Physical smoke/listening work showed that its main contribution was distortion that did not serve the intended MIX character. Rather than preserve a misleading implementation, the MIX path was changed to a three-shelf spectral macro while keeping the familiar control name.

The change produces a clear naming caveat:

- **Correct for current MIX:** HARMONICS is a cascaded, linear spectral-shaping macro.
- **Incorrect for current MIX:** HARMONICS is a nonlinear harmonic generator.

The `HarmoCore` implementation processes 300 Hz low-shelf body shaping, followed by 1.8 kHz and 12 kHz high-shelf shaping. The R5 end-point gains are +6 dB, +7 dB and +9 dB respectively. The two high shelves do not share a combined ceiling: in their overlapping region their gain contributions add in dB, with the low shelf’s response also present according to frequency.

## R5 calibration and tests

The code’s `calibrateMixHarmonics()` maps the 0–100% macro through three independent concave laws. Its intent is to bring useful upper-range R4 colour into the middle of the R5 control travel without losing low-range resolution. Tests compare actual coherent-tone measurements with a matching R5 transfer-function reference for 44.1, 48 and 96 kHz, several frequencies and several macro positions.

The test suite also asserts that the MIX HARMONICS path remains linear under the tested conditions: it checks DC, level invariance across input levels, and second/third harmonic suppression. That is a behavioral test of the current implementation, not a blanket guarantee for all host chains or operating conditions.

## Lessons carried forward

1. A musically familiar control name needs a precise implementation note when its architecture changes.
2. Audible gain accumulation must be documented at the cascade level, not only per filter stage.
3. A visual concept is not a product claim. The runtime editor and the visual assets are documented separately here.
4. Host-facing behavior deserves tests beyond DSP tone response: state migration, bypass latency, automation and meter tap position are all part of product reliability.
5. The lack of automatic output normalization is deliberate, so gain staging and listening comparisons must be handled by the user or host session.

## Current limits and follow-up work

- The checked-in runtime editor is an engineering UI, not the vintage panel shown in artwork and visual explorations.
- Automated DSP tests run independently of a DAW. AU/VST3 validation in target hosts remains a separate release gate.
- The CMake project depends on a user-supplied JUCE checkout and currently has no hosted CI configuration; this avoids publishing an automated build that cannot obtain JUCE under a verified licensing setup.
- The `futureOversamplingStages` constant reserves an architectural direction for 8× processing but does not expose or implement a runtime 8× mode.
