# HARMOHIT MIX vintage recovery — pending visual approval

Baseline: `8a3e05a`, DSP R5 from `34a1909`. Work is isolated on
`codex/harmohit-vintage-recovery`. No Windows run or publication is authorized
before the user's macOS visual approval and physical smoke.

## Evidence and provenance

All branches and reflogs were inspected. There are no stashes. The unreachable
objects returned by `git fsck --no-reflogs --unreachable` contain DSP/test blobs
and trees, not a missing vintage editor commit. The Code/User/History index
contains no HARMOHIT or PluginEditor match. Project copies and UI05 archives in
Downloads were inspected read-only.

The key recovery is **not** a different `PluginEditor.cpp`: UI05 kept that file
as the engineering editor and implemented vintage in `Source/UI/HarmoHitSkin.h`,
`HarmoHitSkin.cpp`, and `HarmoHitSkinAssets.h`. These three sources are recovered
from `/Users/serx/Downloads/HARMOHIT_UI05_FIX2/harmohit/Source/UI/`.
The earlier `HARMOHIT_UI05_integracion_fuentes.zip` also contains them. FIX2
corrects JUCE compilation details (non-constexpr rectangles, String tooltip,
explicit float conversions). No visual assets have been regenerated.

The installed macOS AU contains `harmohit::ui::HarmoHitSkin` symbols. Its binary
filesystem creation/modification timestamp is 2026-10-09 22:24:21 +0200. This
is a timestamp observation, not an independently certified build date. Older
retained release builds are dated 12:03:29 and 17:58:02 that day. Binaries were
not treated as recoverable source. The real Logic capture of October 9 shows
the vintage layout; it cannot by itself identify an exact source revision.

Git references were preserved in `git-before.bundle`; the pre-existing untracked
engineering capture and Windows packages were preserved in
`untracked-before.tar.gz` before edits. Their original files remain present.

## Integration

MIX's existing editor now hosts the recovered skin at exactly 760×380. MASTER
continues using the engineering presentation. `createEditor`, the processor,
parameter IDs, DSP and existing artwork remain unchanged. CMake now includes
the recovered skin for plugin, tests and preview targets. Slider attachments
bind harmonics/exciter/drive/outputTrim; RANGE/PUSH ALL send bounded host gestures
and read the host's current state on the GUI timer. Meters read existing atomic
RMS levels with visual 0 VU = -18 dBFS.

One presentation correction was necessary: the recovered OUTPUT TRIM drawing
mapped the full -24..+12 parameter range onto the printed -12..+12 scale, placing
0 dB away from the printed 0. The pointer now follows the printed scale. Values
below -12 remain supported by the unchanged parameter and rest at the dial's
lower end; the tooltip shows the actual value. This limitation needs human
acceptance. No gain or audio mapping changed.

The recovered embedded-assets header is byte-identical to FIX2:
`6009803205a8f71cb144c61d384c6fc794b8b385acaeb8dbd97d6bb08a470341`.

Unchanged source SHA-256:

- HarmoCore.h: `50bffa577531a25ba24c5dadf2efc7d0e5b60d19bb8a468755da36eb867c527e`
- PluginProcessor.cpp: `de6623e6936ba7cbbe4d86876e7fd821ffd55fefa3b6ee09e3f1a6e4eba26e63`
- PluginProcessor.h: `008b7fd61db2758dfa4d50a6d04043a0d9b087afdd13af6d740fe195504a010f`

## Verification and human gate

Release arm64 MIX AU, DSP tests and real JUCE editor preview were built with
AppleClang and pinned JUCE 9.0.3. CTest passes. The preview exercises actual
sliders/buttons, GUI-to-parameter values, external host parameter updates,
serialized state restore and metering from a 1 kHz signal through the real
processor. It captures four real component snapshots: initial, controls raised,
external automation to minimum, and saved-state restored. Exit code zero is
required. These are rendered by the compiled editor, not conceptual images.

The preview signal runs in a test harness without opening an audio output;
these checks do not claim physical audio, DAW automation recording or Logic
session restoration has passed. The existing installed AU has not been replaced.

Pending: user approval of appearance, physical mouse/keyboard interaction,
Logic smoke with audio, meter calibration, host automation, saved/reopened Logic
session, and acceptance of the trim dial's below--12 presentation. Windows
rebuild/download is deliberately gated on these results.
