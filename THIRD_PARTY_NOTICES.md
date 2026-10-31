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

## Linked at run time: Enchant 2 (LGPL-2.1-or-later)

System library, dynamically linked; no Enchant source or dictionaries are bundled.
https://github.com/rrthomas/enchant

## English prediction model: SCOWL-derived training data

The English model was trained on base words from the system en_US SCOWL size-60
Hunspell dictionary. It does not bundle that dictionary. The full source notice
below is retained for attribution. Synthetic Vietnamese examples come from the
MIT-licensed vendored bamboo spelling tables. The training input hashes and model
hash are in engine/models/english-v1.json.



The English dictionaries come directly from SCOWL and is thus under
the same copyright terms as SCOWL.  The affix file is a heavily
modified version of the original english.aff file, which was
released as part of Geoff Kuenning's Ispell and as such is covered by
his BSD license.

Copyright 2000-2026 by Kevin Atkinson

Permission to use, copy, modify, distribute, and sell any part of SCOWLv2, or
word lists created from it, is hereby granted without fee, provided that the
above copyright notice appears in all copies and that both the above
copyright notice and this notice appear in supporting documentation.  Kevin
Atkinson makes no representations about the suitability of this database for
any purpose.  It is provided "as is" without express or implied warranty.

SCOWL is derived from many sources, most of which are in the Public Domain.
Data from the Corpus of Contemporary American English (COCA) was also used.

All data from COCA comes from 3-gram data that is not freely available;
however, the usage is within the rights given by the NDA that was signed when
purchasing the data.  More information on COCA is available at
https://www.english-corpora.org/coca/.

The primary source of words for SCOWL comes from 12dicts and ENABLE2K.  Both
are in the Public Domain, but Alan Beale <biljir@pobox.com> deserves special
credit as he is the author of 12dicts and a major contributor to ENABLE2K.  In
addition, he gave me an incredible amount of feedback and created a number of
special lists in order to help improve the overall quality of SCOWL.

The initial information for Australian English comes from Benjamin Titze:

  Copyright 2016 by Benjamin Titze

  Permission to use, copy, modify, distribute and sell this array, the
  associated software, and its documentation for any purpose is hereby
  granted without fee, provided that the above copyright notice appears
  in all copies and that both that copyright notice and this permission
  notice appear in supporting documentation. Benjamin Titze makes no
  representations about the suitability of this array for any
  purpose. It is provided "as is" without express or implied warranty.

Affix file Copyright:

  Copyright 1993, Geoff Kuenning, Granada Hills, CA
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. All modifications to the source code must be clearly marked as
     such.  Binary redistributions based on modified source code
     must be clearly marked as modified versions in the documentation
     and/or other materials provided with the distribution.
  (clause 4 removed with permission from Geoff Kuenning)
  5. The name of Geoff Kuenning may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY GEOFF KUENNING AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL GEOFF KUENNING OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

Build Date: Thu Apr 16 20:22:24 UTC 2026
Wordlist Command: mk-list --accents=strip en_US 60
