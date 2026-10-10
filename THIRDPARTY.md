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

## Dependencies

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

This project is licensed under AGPL-3.0-only (see LICENSE).
LGPL-3.0 and MIT components are compatible with AGPL-3.0.

## 本程序的许可证

Hutaomu Editor 以 **GNU Affero General Public License v3.0 (AGPL-3.0-only)**
许可发布：任何人都可以使用、修改和再分发，但衍生版本（包括以网络服务形式
提供的版本）必须继续以同一许可证开放源码。上述第三方组件保持其自身许可，
均与 AGPL-3.0 兼容（LGPL/MIT/BSD/Apache）。
