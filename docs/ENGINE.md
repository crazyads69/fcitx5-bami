# Engine and C API

`engine/` is pure Go over the vendored bamboo-core (`engine/third_party/bamboo-core`, patches 0001-0006 in its
`patches/` dir). `bridge/bami.go` exports it to C (`go build -buildmode=c-archive -trimpath -buildvcs=false ./bridge`;
`-buildvcs=false` is needed on Go 1.18 when git is missing).

## Variants (data, `engine/variant.go`)

| # | Variant | Table | LoneW default | Bracket default |
|---|---------|-------|---------------|-----------------|
| 0 | Telex | bamboo "Telex" | off | off |
| 1 | Telex2 | Telex + lone w + brackets (= bamboo "Telex 2") | everywhere | everywhere |
| 2 | TelexW | Telex + lone w (= bamboo "Telex W") | everywhere | off |
| 3 | SimpleTelex1 | Unikey Simple Telex: w only marks a vowel | off, locked | off, locked |
| 4 | SimpleTelex2 | Unikey Simple Telex 2: lone w -> ư, no brackets | everywhere, locked | off, locked |
| 5 | VNI | bamboo "VNI" | - | - |
| 6 | VIQR | bamboo "VIQR" (no `\` escape) | - | - |
| 7 | MS | bamboo "Microsoft layout": digits type marked letters; `[` -> ư, `]` -> ơ (swapped vs Telex) | - | - |
| 8 | TelexVNI | bamboo "Telex + VNI" | off | off |

Gates (Telex family only; Simple Telex ignores overrides): lone `w` -> ư, brackets `[` -> ơ, `]` -> ư (Telex) and
`{ }` -> Ơ Ư. Values: default (variant's), off, after-letter (literal at the start of a word), everywhere.
The gates add rules to the base table (`w` += `__Ư`, bracket keys), so "Telex2" is exactly bamboo's own table (tested).

## Options

| Option | Meaning |
|--------|---------|
| ToneStyle | 0 old (hòa, thúy), 1 new (hoà, thuý) |
| FreeTone | tone key may come before the syllable is finished and the tone moves (hofa -> hòa). Off: the tone stays on the vowel just typed (hoaf -> hoà in both styles) |
| SpellCheck | on: marks/tones only apply to syllables that can be Vietnamese (baow stays). Off: anything is marked (baơ) |
| AutoRestore | at Commit, a non-Vietnamese word returns to its raw keys (windows, text, Linux) |
| DdAnywhere | dd -> đ always applies, and AutoRestore keeps a word holding đ (add -> ađ) |
| QuickTelex | opt-in, plain Telex only: cc->ch gg->gi kk->kh nn->ng pp->ph qq->qu tt->th (no undo) |

## Go API

`New(v, opts)`, `Configure`, `Key(rune) (Edit, consumed)`, `Backspace() (Edit, consumed)`, `Commit() string`, `Reset()`,
`Preedit()`, `AckShown(s)`. `Edit{DeleteN runes, Insert}` is computed with `Diff` against the last REPORTED preedit
(`AckShown` overrides that baseline). Not consumed = the engine ignores the key (space, punctuation, a digit/tone key at
the start of a word, a key outside the variant): Commit the word, then pass the key on. Backspace removes the last composed
char (`vieetj`,BS -> `việ`; PRD F8). Break keys never reach bamboo, so AutoRestore runs in `Commit`.

## C API (bridge/bami.go)

```c
void      BamiInit(void);                                   // once per process
uintptr_t BamiNew(int variant, int toneStyle, unsigned flags);   // 0 on failure
void      BamiConfigure(uintptr_t h, int variant, int toneStyle, unsigned flags); // resets the word
void      BamiFree(uintptr_t h);
void      BamiReset(uintptr_t h);
int       BamiKey(uintptr_t h, unsigned ch, int *deleteN, char **insert, char **preedit);
int       BamiBackspace(uintptr_t h, int *deleteN, char **insert, char **preedit);
char     *BamiCommit(uintptr_t h);
char     *BamiPreedit(uintptr_t h);
void      BamiFreeString(char *s);
void      BamiAckShown(uintptr_t h, const char *shown);     // header spells it bami_cchar* (typedef const char)
```

* variant: numbers in the table above. toneStyle: 0 old, 1 new.
* flags: bit0 FreeTone (1), bit1 SpellCheck (2), bit2 AutoRestore (4), bit3 DdAnywhere (8), bit4 QuickTelex (16),
  bits5-6 LoneW gate (0 default, 1 off, 2 after-letter, 3 everywhere), bits7-8 Bracket gate (same). Suggested: `1|2|4`.
* BamiKey / BamiBackspace: 1 consumed, 0 pass-through, -1 internal error (panic recovered; the word is reset, outputs
  are 0/NULL). On 0 and 1: `*deleteN` counts runes, `*insert` and `*preedit` are NUL-terminated NFC UTF-8, non-NULL. On 0
  nothing changed (deleteN 0, insert "", preedit = current). Any output pointer may be NULL.
* Ownership: every `char *` returned or stored by the library (`BamiCommit`, `BamiPreedit`, `*insert`, `*preedit`) is
  malloc'd (C.CString): free with `BamiFreeString` (NULL-safe) exactly once. `BamiAckShown` copies its argument.
* `BamiCommit` returns the final text of the word (AutoRestore applied) and resets; the caller replaces what it showed for
  the word with it. NULL on error.
* `BamiAckShown(h, s)`: call when the front-end fell back to preedit or skipped an edit; the next Edit is relative to `s`.
* Handles are cgo handles; a stale or bogus handle returns -1/NULL, never crashes. Calls on one handle are serialised.
* The generated header is C++-clean (checked with `g++ -std=c++17 -Wall -Wextra -pedantic`). `BamiDiffDeleteN` from the M0
  scaffold is kept.

## Known limits

* VIQR `\` escape and MS alternate tone keys (`%^&*(`) are not implemented.
* English words that look Vietnamese (test1.c -> tét1.c) need a front-end dictionary.
* The bamboo spelling tables are lenient: `dd` is never refused, so DdAnywhere only matters with AutoRestore.
