# Third-party notices

My original Hex Appeal code is available under the Zero-Clause BSD licence in `LICENSE`. The following third-party terms remain in force. This licence does not cover game assets or separately downloaded mods.

## CrimsonDesertTools

I referenced the native assembly layouts, outfit teardown behaviour, property-setter research and frame-update entry documented by [CrimsonDesertLiveTransmog](https://github.com/tkhquang/CrimsonDesertTools/tree/db82c301af48cbe6d1595fc76a8139b85f85d788/CrimsonDesertLiveTransmog), commit `db82c301af48cbe6d1595fc76a8139b85f85d788`. Copyright 2025-present Quang Trinh (tkhquang), MIT. Its licence is retained in `CrimsonDesertTools-MIT.txt`. The Wardrobe module adapts this research into checked game-thread switching, native assembly acknowledgement, restoration and refresh. Dancer switching and supplied global colours have sampled live coverage; per-witch colours are not supplied. Native part controls and authored combinations use my optional SDK extension.

This local runtime uses the overlay/input subset of ModActivator commit `abc673c6e7a4ebf44c3e4273088d6331f3ad9a43`, https://github.com/Alzucard/ModActivator. Its MIT-0 license is retained in `ModActivator-MIT-0.txt`. The source namespaces and upstream comments are retained where useful. The loot engine, mod discovery, shortcut editing, and ModActivator menus are not included.

## Trinity
The DX12 and controller overlay code was adapted upstream from Trinity, https://github.com/XeTrinityz/Trinity. That adapted code is retained in `src/hooks/`, along with the Master Looter MIT notice in `OVERLAY-LICENSE.txt`.

MIT License

Copyright (c) 2026 XeTrinityz

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Dear ImGui
Vendored from the inspected ModActivator tree, version 1.91.5, https://github.com/ocornut/imgui. MIT License; the full notice is in `IMGUI-LICENSE.txt`.

`src/gui/imgui_impl_dx12.cpp` uses the upstream adaptation with precompiled shaders in `imgui_dx12_shaders.h`, avoiding a runtime d3dcompiler_47.dll import. Same MIT licence.

## MinHook
Vendored from the inspected ModActivator tree, https://github.com/TsudaKageyu/minhook. BSD 2-Clause License, Copyright (C) 2009-2017 Tsuda Kageyu. Its bundled HDE disassembler is used by `src/hooks/farhook.cpp` to measure function prologues. Full notices, including HDE, remain in `MINHOOK-LICENSE.txt` and the vendored sources.

An x64 ASI loader is required to load the plugin and is not included. VENUS, Hex's preset, Instant Surgery, Legion, and game assets are not redistributed in the runtime archive.
