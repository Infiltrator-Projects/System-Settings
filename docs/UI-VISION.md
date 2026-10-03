<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# System Settings UI Vision

## Status

This document records the current visual north star for the System Settings user
interface. The reference image is:

`docs/design/system-settings-ui-north-star.webp`

![System Settings UI North Star](design/system-settings-ui-north-star.webp)

The image is a prototype reference. It is not a pixel-perfect implementation
contract and it does not override platform usability, accessibility or native
system behaviour.

## Product direction

System Settings should feel like a complete visual desktop product rather than a
developer control surface. The interface should be graphical first: icon-led,
preview-led, layered and immediately understandable without requiring the user
to think like a command-line administrator.

The intended spirit is the visual confidence of classic GUI environments such as
Amiga Workbench 2.x/3.x, translated into a modern native desktop application.
That means visible state, direct manipulation, strong grouping, clear affordances
and a recognisable product personality rather than acres of explanatory text.

## Visual language

The target direction is:

- a strong application shell with persistent category navigation;
- rich, clearly separated cards and panels instead of long flat forms;
- Common's semantic System/Day/Night palettes, with the neutral accent used for selection/focus and warning/success/fault colours reserved for those actual states;
- Common's heading, summary, kicker, detail-label and note roles rather than local approximations of text hierarchy;
- Common's 6/10/12/18 radius hierarchy and 6/10/18/16/20 spacing/padding vocabulary as the canonical geometry contract;
- large, purposeful icons and visual status indicators;
- live previews where a setting has a visual result;
- concise labels with secondary explanation only where it earns its space;
- useful system overview and quick-action surfaces;
- restrained depth and highlight treatment rather than decorative colour noise;
- a coherent family resemblance with the rest of the Infiltrator desktop suite;
- responsive layout that remains readable at smaller window sizes.

## Interaction principles

The GUI is the primary product. A command-line tool may exist for automation,
testing or recovery, but the desktop experience must not be designed as a thin
wrapper around CLI concepts.

Every major setting should answer three questions visually and quickly:

1. What is the current state?
2. What can I change?
3. What will change when I do it?

Where practical, controls should show the result rather than explain it in prose.
The user should not need to read a paragraph to discover whether networking is
connected, which theme is active, what the current time system is, or which
display profile is selected.

## Relationship to the prototype

The prototype establishes the desired level of finish, hierarchy and graphical
density, but the canonical visual contract is the current Common
`infiltrator-design-v1` contract. Shared typography, semantic palette roles,
radii and spacing follow Common; product-specific imagery remains local. Shell
styling should consume those Common roles directly so a suite-wide design change
does not leave System Settings carrying an older local interpretation.
Individual imagery, labels and sample modules may change as real modules are
implemented. Hero artwork and any recognisable third-party marks shown in
concept imagery are mood references only and are not a requirement for shipped
assets.

Future shell and module UI work should be checked against one simple question:

> Does this move System Settings closer to a polished, graphical desktop
> environment, or back toward a text-heavy administrative utility?

The former is the project direction.
