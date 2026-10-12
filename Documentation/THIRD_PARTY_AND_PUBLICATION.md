# Third-party and public-publication notes

## Repository publication state

This repository is intended to be visible publicly for study and presentation. No `LICENSE` file is included, by the project owner’s explicit decision. Public visibility alone does not grant a general license to reuse, modify or redistribute VitaPlug HARMOHIT code or visual assets.

## JUCE

HARMOHIT uses JUCE headers and CMake targets, but does not include JUCE source in this repository. `CMakeLists.txt` expects a local JUCE 9.0.3 checkout supplied through `JUCE_DIR`.

JUCE is licensed separately by its provider. Anyone building, distributing or otherwise using this project must review the then-current JUCE licensing terms and satisfy the option that applies to their use, including any applicable commercial or GPL obligations. This repository does not provide JUCE rights, a JUCE sublicense, legal advice or a claim that a particular distribution model is compliant.

## Assets and dependencies

The HARMOHIT panel artwork and visual-check images are included as project assets. They are not offered under a separate reuse license. Before adding a dependency, binary, sample, font, image or code contribution to the public repository, record its source and license and confirm it can be published.

## Publication hygiene performed for this snapshot

- Checked tracked source, documentation and configuration for common credential/private-key patterns.
- Confirmed that the product repository does not vendor JUCE or compiled plug-in bundles.
- Added ignore rules for local environment files and common credential-bearing local files.
- Kept the existing Git history and branches intact; no reset, clean, force push, rebase or deletion was used.

The commit history currently contains the literal placeholder author address `TU_EMAIL_DE_GITHUB`. It is not a credential, but it will be visible if the complete history is published. It is preserved because rewriting existing history is outside this publication preparation and would conflict with the project’s preservation requirement.
