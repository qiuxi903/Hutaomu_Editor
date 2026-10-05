# Third-Party Components

This document lists third-party components used by Hutaomu Editor.
Distribution of the application must include the corresponding
license texts and attributions, as required by each license.

## Current dependencies

### Qt 6 (Qt Widgets, QtCore, Qt6::Core5Compat)

- License: GNU Lesser General Public License v3.0 (LGPL-3.0)
- Project: https://www.qt.io/
- Used via **dynamic linking**, which is permitted for proprietary
  applications under LGPL-3.0.
- Compliance obligations for binary distribution: provide the means
  for users to relink the application against a modified Qt build
  (object files or equivalent), include the LGPL-3.0 license text,
  and do not modify the Qt libraries themselves.
- LGPL-3.0 license text: https://www.gnu.org/licenses/lgpl-3.0.html

## Planned dependencies (integrated in M1/M2 per PLAN.md)

### tree-sitter

- License: MIT License
- Project: https://tree-sitter.github.io/
- Purpose: incremental syntax parsing for code highlighting.

### md4c

- License: MIT License (Copyright (c) Martin Paces)
- Project: https://github.com/mity/md4c
- Purpose: CommonMark/GFM Markdown parsing.

### pdfium

- License: Apache 2.0 (Copyright Chromium/PDFium contributors)
- Project: https://pdfium.googlesource.com/pdfium/
- Binaries: https://github.com/bblanchon/pdfium-binaries (chromium/8076)
- Purpose: PDF page rendering for the PDF viewer.
- License text: third_party/pdfium/LICENSE

### zlib

- License: zlib License
- Project: https://zlib.net/
- Purpose: FlateDecode decompression (PDF) and OOXML package reading.

### Media Foundation / MFPlay (Windows system component)

- License: Windows operating system component (not redistributed).
- Purpose: native video/audio playback in the media viewer.
- Availability: Windows only; other platforms show a placeholder and
  will gain playback through QtMultimedia in the M3 milestone.

## License compatibility note

The intended future license for this project is GPL-3.0-or-later
(see LICENSE). LGPL-3.0 and MIT components are compatible with
GPL-3.0-or-later.
