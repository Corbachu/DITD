## Dream In the Dark
---
[![DITD.png](https://i.postimg.cc/gkWNhsjS/DITD.png)](https://postimg.cc/K40PSnnB)

# Dream in the Dark — DITD

**(C) 2025-2026 Isotope Softworks **  
**(C) 2003-2026 FITD Team **
**(C) The EDGE Team 1997-2026 **
---

## Overview

**Dream in the Dark (DITD)** is a reconstruction and modernization of the original **Alone in the Dark (FITD)** engine, designed to run **AITD 1–3** faithfully on **real Dreamcast hardware** and for developer/debugging on modern desktop systems.

DITD is **not a standalone game**. It is a **Dreamcast‑first FITD engine rewrite**, extended far beyond the original codebase while preserving the behavior, timing, and feel of the DOS classics.

### Why the name DITD and not FITD

The project has undergone **extensive, non‑portable refactoring** and includes Dreamcast‑specific systems and optimizations that diverge from upstream FITD. Because the codebase is architecturally distinct and not intended for upstream portability, the project identity is **Dream in the Dark (DITD)** rather than FITD.

---

## Supported games

DITD is designed to run:

- **Alone in the Dark** (1992)  
- **Alone in the Dark 2** (1993)  
- **Alone in the Dark 3** (1994)  
- **Jack in the Dark**

**Users must provide their own legally obtained game data.**

---

## Architecture overview

DITD is composed of several major layers, with **FitdLib** as the true engine.

### FitdLib — the engine layer

`FitdLib/` is the core of DITD and contains the extended FITD engine systems:

- Actor system  
- Script interpreter  
- Camera logic  
- Collision and triggers  
- Scene graph  
- Palette and texture handling  
- Timing and movement rules  
- High‑level engine logic not present in the original FITD

This is the **real engine** — not the `Fitd/` directory.

---

### Embedded EDGE platform layer

EDGE is embedded directly into FitdLib and provides:

- Cross‑platform initialization  
- Input abstraction  
- Virtual filesystem integration  
- Logging and debugging  
- Memory and container utilities

There is **no separate Edge directory** — the backend is embedded inside FitdLib.

---

### Dreamcast specific backend

`FitdLib/System/` contains Dreamcast‑specific systems:

- **Rewritten FITD renderer built on GLdc**  
  - **GLdc is used as provided**; the **FITD renderer itself** was rewritten to target GLdc and the Dreamcast PVR  
- Timing subsystem adapted to **SH‑4**  
- Sound and speech playback  
- Platform utilities and Dreamcast filesystem integration  
- **SH‑4 fast‑math routines**  
- **Custom SIMD backend (libSH4simd)** for accelerated memcpy, vector math, trigonometry, and 3D transforms

This is where Dreamcast hardware is fully leveraged.

---

### EPI backend

A Dreamcast‑specific EPI backend provides:

- Filesystem abstraction (`epi::file_c`)  
- Directory enumeration and path utilities  
- Timing and platform utilities  
- Fixed‑point math types

EPI integrates the Dreamcast backend with the engine and the custom archive system.

---

### Custom archive system

A custom PhysFS‑derived archive layer supports:

- Quake PAK  
- EPK/PK3 (ZIP‑based)  
- Modding‑friendly search paths  
- Virtual filesystem layering

This enables loading original AITD data, overriding assets with mods, and packaging Dreamcast‑friendly archives.

---

### ADF scripting language

**ADF (Alone Definition File)** focuses on audio customization:

- Sound customization  
- Music replacement  
- Speech override/remapping  
- User‑defined audio packs  
- Mixing original AITD audio with custom content

ADF is **not yet** a full gameplay scripting system. Future updates will expand ADF to support scene logic, actor behavior, and debugging hooks.

---

### COAL backend

COAL from EDGE is included for future HUD/UI work and will power:

- HUD elements  
- Menus  
- Debug overlays  
- UI widgets

COAL‑based features are planned for later milestones.

---

### Fitd entry point

`Fitd/` contains:

- Basic entry point code  
- Startup glue  
- Minimal bootstrap logic

All real engine functionality lives in **FitdLib**.

---

## Additional embedded components and dependencies

DITD bundles and depends on several Dreamcast‑specific or project‑specific components. These are required for Dreamcast builds and are tightly integrated.

### Audio

- **XingMP3 (KOS‑ports libmp3)** — required for MP3 music and SFX. Supported MP3 format: **22 kHz, 128 kbps**.

### Image loading

- **Custom EPI‑based stb_image** — Dreamcast‑safe texture decoding and palette extraction.

### Archive / VFS

- **PhysFS 1.x (custom KOSPorts by dreamEDGE)** — modified for KOS and integrated into the EPI layer to support PAK and EPK/PK3 archives.

### EDGE components embedded

- **m_misc** — configuration and options handling  
- **z_zone** — memory management and pooled allocators  
- **RGL init** — renderer bootstrap  
- **dreamEDGE init backend** — platform setup, logging, VFS initialization, input abstraction

### SIMD and math

- **libSH4simd** — custom SH‑4 SIMD backend linked into EPI for accelerated memcpy, vector math, matrix transforms, trigonometry, and 3D operations. This backend is Dreamcast‑exclusive and a major reason DITD is non‑portable.

---

## Repository structure

\`\`\`
Fitd/               — Entry point / bootstrap only
FitdLib/            — Actual engine: game logic, EDGE backend, Dreamcast backend
FitdLib/System/     — DC renderer, timing, sound, speech, platform code
EPI/                — Edge Platform Interface backend (shared + DC)
PhysFS/             — Custom PhysFS-derived archive system (PAK, EPK/PK3)
Scripts/            — ADF scripting language files and environment scripts
Data/               — Engine assets, PAK0 generation, ADF content
Tools/              — Asset pipeline and debugging utilities
\`\`\`

---

## Tools and asset pipeline

DITD includes a set of tools and converters used to prepare assets for the Dreamcast runtime and to support modding workflows.

### Primary toolchain and utilities

- `pvrtex` and custom converters — prepare textures for the Dreamcast PVR  
- `make_disc.sh` — disc asset conversion and packaging (produces `SPLASHX.mp3`, PAK0 generation helpers)  
- Custom Windows↔WSL helpers — environment scripts to ease cross‑platform builds and conversions

### Asset editors and packagers

- **SLADE3** — recommended for modern PAK/EPK/PK3 generation and editing; supports PNG import/export, VOC handling, DIR/LUMP management, and modern PAK workflows  
- **AITD‑Tools** (tigrouind) — extraction and analysis utilities used for data validation and reverse engineering support

### Pipeline notes

- Textures should be converted to Dreamcast‑friendly formats and palettes using the provided converters  
- Audio intended for Dreamcast must be encoded to MP3 at **22 kHz, 128 kbps**  
- Use SLADE3 to manage PAK contents and to create EPK/PK3 packages for mod distribution  
- Tools/ contains in‑house utilities for batch conversions, PAK generation, and debugging helpers

---

## Build instructions — Dreamcast focused

DITD is **Dreamcast‑first** and requires the KallistiOS toolchain and GLdc renderer. Desktop builds are for debugging only and are not authoritative for timing or final behavior.

### Required software

- **Windows 11** (development host, tested on 25H2)  
- **VSCode** (editor)  
- **WSL2 (Debian)** (build environment)  
- **KallistiOS 2.2.1 (stable)**  
- **KOS‑Ports**  
- **SH‑4 GCC toolchain** (dc‑chain GCC 15.1.0 under WSL2/Debian)  
- **GLdc** (by Kazade)  
- **XingMP3 (KOS‑ports libmp3)**  
- **Custom PhysFS 1.x (dreamEDGE KOSPorts)**  
- **Custom EPI‑stb_image**  
- **libSH4simd**

### Recommended host layout


/opt/toolchains/dc/    # KOS, toolchain, and ports /home//DITD       # repository


### Clone the repository

```bash
git clone https://github.com/Corbachu/DITD.git
cd DITD


### Configure the build (CMake)

mkdir build-dc
cd build-dc
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/dreamcast.toolchain.cmake


The toolchain file sets the SH‑4 cross compiler, KOS include paths, GLdc include paths, SH‑4 optimization flags, and Dreamcast‑specific defines.

Build

make -j$(nproc)


Artifacts produced

• DITD.elf
• DITD.bin (via scramble)
• SPLASHX.mp3 (converted via make_disc.sh)
• Optional CDI image (if your workflow includes mkdcdisc)
```

### Running on real hardware

Run DITD via:

• Burned CD‑R (development use not recommended, TESTED)
• Dreamcast SD adapter (untested)
• GDEMU (untested)
• MODE
• Serial loader (dcload / dc-tool for development use only, not recommended for playback)



DITD is validated on real Dreamcast hardware and common Dreamcast loaders.

### Desktop builds (unsupported)

Desktop builds exist but have not been tested in the slightest, and those builds currently fail. Desktop builds are not authoritative for timing or final behavior.

---

## FAQ — Dreamcast focused

Is Dream in the Dark a standalone game?
No. DITD is an engine. You must provide legally obtained AITD game data. The demo/preview release only contains demo/preview data assets. The front‑end includes art assets created by Isotope Softworks and fan renditions of soundtrack remixes with attribution included in the on‑disc PAK0.PAK.

Why is the project called DITD and not FITD?
DITD contains extensive Dreamcast‑specific refactors and non‑portable systems such as SH‑4 SIMD, libSH4simd, GLdc renderer integration, custom PhysFS, and dreamEDGE init. The architecture diverges significantly from upstream FITD.

Is GLdc rewritten for this project?
No. GLdc is used as provided. The FITD renderer and Dreamcast rendering paths were rewritten to target GLdc and the Dreamcast PVR.

What audio formats are supported?
MP3 playback for music and SFX is supported via XingMP3 (KOS‑ports libmp3). Supported MP3 format: 22 kHz, 128 kbps.

Why is DITD Dreamcast‑first and non‑portable?
DITD depends on SH‑4‑specific optimizations, Dreamcast timing, GLdc, KOS subsystems, and custom KOSPorts. These systems are deeply integrated and not portable without significant rework.

Do you use AI tools to write the engine?
GitHub Copilot is used responsibly for project organization, documentation, and debugging assistance only. All engine logic, Dreamcast backend code, renderer work, and SH‑4 optimizations are hand‑authored by the development team.

Will DITD support AITD4 or New Nightmare?
No. Those titles use different engines and are outside the scope of DITD.

Does DITD require Dreamcast overclocking?
No. DITD is designed to run on stock Dreamcast hardware.

---

## Credits

Original game credits

• Alone in the Dark series — original game design and assets are the property of Infogrames and their respective rights holders
• Frédérick Raynal and the original development team — acknowledged for the original game design and engine


Core development (DITD)

• Corbin “Corbachu” Annis — DITD / EDGE Programmer; lead developer of Dream in the Dark, DreamEDGE integration, FitdLib extensions, Dreamcast backend, renderer rewrite, archive system, and ADF language implementation
• Vincent “yaz0r” Hamm — FITD Programmer; original reverse‑engineering work and foundational FITD engine research
• Jmimu — FITD Programmer, Aitd‑PakEdit; engine research, tooling, and FITD behavior documentation
• The FITD Team

Tools and research

• tigrouind — AITD‑Tools; creator of essential AITD tooling used for data extraction, analysis, and engine validation


EDGE engine and contributors

• EDGE Engine — platform layer used by DITD
• Andrew Apted — EDGE Programmer; creator of the EDGE engine platform layer and COAL UI backend
• The EDGE Team (1997–Present) — developers of EPI and related backends


Third‑party libraries and ports

• GLdc — Dreamcast GL implementation by Kazade
• KallistiOS (KOS) — Dreamcast OS and toolchain
• XingMP3 (KOS‑ports libmp3) — MP3 decoding
• stb_image (EPI‑wrapped) — image decoding
• PhysFS 1.x (dreamEDGE KOSPorts) — virtual filesystem and archive support
• SLADE3 — modern PAK/EPK/PK3 editor and lump manager


Historical credits

This project builds on decades of community research and tooling. Foundational contributors include early FITD reverse‑engineering researchers, AITD tooling authors, and Dreamcast homebrew community members. See CHANGELOG.md and CONTRIBUTORS.md for detailed historical credits.

---

## License and legal

DITD is an engine and does not include or distribute original AITD 1-3 game data. Users must supply legally obtained game assets. Third‑party libraries included or referenced are subject to their own licenses. 

The provided DEMO CDI contains demo versions of AITD1, AITD2, and JITD, all of which are legally obtainable and free to distribute.

---

## Contributing and contact

Contributions, bug reports, and pull requests are welcome. Please follow CONTRIBUTING.md and CODE_OF_CONDUCT.md.

(C) Isotope Softworks