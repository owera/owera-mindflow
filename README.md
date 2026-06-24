# Owera MindFlow

A MindNode-inspired open source mind map app for Linux, built with C++17 and
Qt 6 (`QGraphicsView`).

![Owera MindFlow](resources/mindflow.svg)

## Features

- **Infinite canvas** with pan/zoom, drag-to-rearrange, and undo/redo on every action.
- **Layouts**: organic (balanced two-sided), right, left, up, down, and compact —
  with collapsible branches and smooth animated transitions.
- **Multiple roots / detached trees** on one canvas.
- **Styling & themes**: 8 node shapes (rectangle, rounded, pill, cloud, hexagon,
  octagon, line, embedded), per-node and per-branch colors, fonts, rounded/orthogonal
  connectors, and switchable light/dark themes with colorful branch palettes.
- **Rich content**: notes, images, emoji/stickers, visual tags, and task checkboxes
  with subtree progress.
- **Cross-connections** between any two nodes, with draggable waypoints and labels.
- **Focus mode** and **tag highlighting** to spotlight part of a map.
- **Outline view** always kept in sync with the map.
- **Import**: OPML, Markdown, FreeMind, plain text.
- **Export**: PNG, SVG, PDF, Markdown, OPML, FreeMind, CSV, plain text.

## Build

Requires CMake ≥ 3.21, a C++17 compiler, Ninja, and Qt 6 (Widgets, Svg, PrintSupport,
Test).

```sh
# Debian/Ubuntu deps:
sudo apt-get install -y cmake ninja-build qt6-base-dev qt6-base-dev-tools \
  libqt6svg6-dev qt6-svg-dev

cmake -S . -B build -G Ninja
cmake --build build
./build/mindflow
```

## Test

```sh
ctest --test-dir build --output-on-failure
# or run the binary directly (headless):
QT_QPA_PLATFORM=offscreen ./build/mindflow_tests
```

## Packaging

- **Flatpak**: `flatpak-builder --user --install --force-clean build-flatpak \
  packaging/flatpak/com.owera.MindFlow.yaml`
- **AppImage**: `./packaging/appimage/build-appimage.sh`
  (needs `linuxdeploy` + `linuxdeploy-plugin-qt`).
- **Distro install**: `cmake --install build --prefix /usr/local` installs the binary,
  `.desktop` entry, AppStream metainfo, and scalable icon.

## Documents

Maps are saved as `.mindflow` files — a single JSON document (schema-versioned;
images are embedded as base64). See `src/io/DocumentStore.*`.

## Architecture

`src/model` (Document/Node/Connection/Theme + `QUndoCommand`s) is the single source of
truth; `src/canvas` (QGraphicsView) and `src/outline` (QTreeWidget) are two presenters
that observe it, so the map and outline never diverge. `src/layout` computes geometry;
`src/io` handles persistence and import/export. See the milestone history for details.
