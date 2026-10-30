# Third-party notices

fcitx5-bami itself is MIT (see LICENSE). It includes or links the following.
Full texts live in `LICENSES/` (REUSE layout) and must ship in every
binary package (e.g. /usr/share/doc/fcitx5-bami/).

## Vendored: bamboo-core  (MIT)
Path: `engine/third_party/bamboo-core/` (keep upstream LICENSE + CREDITS there)
Source: https://github.com/BambooEngine/bamboo-core  (pin commit: b2e49a2, plus the local patches in engine/third_party/bamboo-core/patches)
Copyright (C) 2018 Luong Thanh Lam <ltlam93@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND ... (verbatim
as in upstream LICENSE; paste the full upstream text here when releasing).

Upstream credits: Trung Ngo (bogo.js); Tran Ky Nam (GoTiengViet).

## Linked at run time: Fcitx5 (LGPL-2.1-or-later)
libFcitx5Core, libFcitx5Config, libFcitx5Utils (+ module headers). Not
redistributed by this package; dynamically linked from the system.
Copyright CSSlayer <wengxt@gmail.com> and Fcitx contributors.
https://github.com/fcitx/fcitx5  (SPDX: LGPL-2.1-or-later)

## Embedded in the Go archive: Go runtime and standard library (BSD-3-Clause)
Copyright (c) 2009 The Go Authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
   * Neither the name of Google Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

(Source of truth: $(go env GOROOT)/LICENSE of the toolchain used to build;
if the build links any golang.org/x/* module, add its notice too -- same text.)

## Build-time only: extra-cmake-modules (BSD-3-Clause)
Copyright KDE contributors. Not distributed in the output; no notice needed
unless a .cmake file is copied into this repo (then keep its SPDX header and
add LICENSES/BSD-3-Clause.txt).

## Data
No third-party word lists or dictionaries are bundled. Test fixtures, if any,
are listed here with source and license. See "Do not copy" in docs/licensing.
