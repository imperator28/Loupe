# Third-Party Notices

Loupe is distributed under Apache License 2.0. It is built with third-party
libraries that retain their own copyright and licensing terms. A binary
distribution must include the applicable notices, license texts, and source or
relinking materials required by the exact versions and linkage modes it ships.

## Direct dependencies

| Component | Role in Loupe | License family |
| --- | --- | --- |
| Qt 6 (`qtbase`, `qtdeclarative`, `qtquick3d`) | Desktop UI, QML, rendering, SQL, networking | LGPL-3.0-or-later or commercial, with module-specific exceptions |
| Open CASCADE Technology | STEP import, B-Rep geometry, mesh and export operations | LGPL-2.1-or-later with the OCCT exception |
| nlohmann/json | Protocol and structured data | MIT |
| Catch2 | Test framework | Boost Software License 1.0 |
| xxHash | Hashing | BSD-2-Clause |
| libpng | PNG capture encoding | libpng license |

## Distribution obligations

- Verify the licenses of every installed vcpkg package and transitive runtime
  dependency in the exact build being shipped. `vcpkg-configuration.json`
  pins the baseline used for reproducible review; it is not a substitute for a
  generated third-party license bundle.
- Qt's open-source terms require prominent notice, the relevant license text,
  and provision of the corresponding Qt source or a valid written offer. Qt
  recommends dynamic linking for applications that are not themselves licensed
  under the LGPL. Static linking introduces additional relinking and
  distribution obligations.
- OCCT's LGPL 2.1 terms and exception require a visible notice and accessible
  LGPL text. Preserve any OCCT notices and comply with the exception and LGPL
  requirements for the linkage method used.
- A packaged Windows installer, macOS app bundle, or app-store submission must
  be reviewed separately for third-party runtime files, notices, signing,
  notarization, and applicable platform-store terms.

This file is an engineering notice, not legal advice. Consult counsel familiar
with open-source software distribution before shipping binaries commercially.
