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
preview-led, colourful, layered and immediately understandable without requiring
the user to think like a command-line administrator.

The intended spirit is the visual confidence of classic GUI environments such as
Amiga Workbench 2.x/3.x, translated into a modern native desktop application.
That means visible state, direct manipulation, strong grouping, clear affordances
and a recognisable product personality rather than acres of explanatory text.

## Visual language

The target direction is:

- a strong application shell with persistent category navigation;
- rich, clearly separated cards and panels instead of long flat forms;
- blue/cyan and warm gold accents over the project's dark Mercedes-grey base;
- large, purposeful icons and visual status indicators;
- live previews where a setting has a visual result;
- concise labels with secondary explanation only where it earns its space;
- useful system overview and quick-action surfaces;
- consistent rounded geometry, depth and restrained glow/highlight treatment;
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

The prototype establishes the desired level of finish, hierarchy, colour and
graphical density. Individual imagery, labels, sample modules and decorative
content may change as real modules are implemented. Hero artwork and any
recognisable third-party marks shown in concept imagery are mood references only
and are not a requirement for shipped assets.

Future shell and module UI work should be checked against one simple question:

> Does this move System Settings closer to a polished, graphical desktop
> environment, or back toward a text-heavy administrative utility?

The former is the project direction.
