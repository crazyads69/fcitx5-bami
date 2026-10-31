# 03 Performance and resource use

Machine: Apple M1 (4 Firestorm + 4 Icestorm), aarch64 Fedora Asahi Remix 44, kernel 7.2.12, Go 1.26.10. Containers: Debian 12 arm64, Fcitx 5.0.21, Go 1.19.8. Timings are pinned to one Firestorm core and interleaved before/after (benchstat, n=8); unpinned runs varied 2x because other jobs shared the box. All work was done on a copy of the repo. Patches (not applied): `/tmp/claude-1000/-home-trilm-orca-projects-fcitx-bami/8ab763c3-7b8a-45b6-8169-e1cde1e09410/scratchpad/perf/patches/`.

## Verdict

Steady state is cheap: 6-10 us per key in the engine, about 25 us through the whole Fcitx stack, against a 1 ms target. No leak, timer, file I/O or D-Bus call in the key path. The real problems are tails and unbounded cases:

1. Composition is O(n^2) per key: a 60-key word costs 0.3 ms per key, an 80-key word 0.53 ms.
2. Edit mode copies the whole surrounding text every key and scans it three times.
3. A GC runs every ~800 keys (3.5 KiB, 93 allocations per key).
4. Edit mode corrupts text above ~250 cps (25 and 100 cps are clean).

## 1. Go engine (100k-key stream, 80% Vietnamese words, 20% English, ~4% Backspace)

| Metric | Before | After patches 1-2 |
|---|---|---|
| Key, Telex | 10.8 us | 8.0 us (-26%) |
| Key, VNI / MS / Telex+VNI | 10.4 / 7.5 / 11.4 us | 7.7 / 5.5 / 8.5 us |
| Backspace | 34.6 us, 330 allocs | 26.2 us, 297 |
| Commit | 9.2 us, 91 allocs | 7.0 us, 71 |
| Key bytes, allocs | 3.5 KiB, 93 | 2.3 KiB, 82 |
| Telex latency p50/p90/p99/p99.9/max | 7.2/21.3/35.8/72.6/237 us | 5.6/15.5/26.7/61.9/265 us |

The nine variants are within 10% of each other (MS fastest). `New()` costs 14 us and 24.6 KiB. The profile is flat inside bamboo: `lookup` re-converts constant syllable tables on every call (40% of CPU), `getCanvas` allocates a map, `extractLastSyllable` re-validates every prefix.

Per-key cost by word length (no commit):

| Keys | 5 | 10 | 20 | 40 | 80 |
|---|---|---|---|---|---|
| Before (us) | 11 | 25 | 62 | 171 | 531 |
| After (us) | 8 | 16 | 39 | 59 | 29 (capped) |

## 2. cgo boundary and Go runtime

- Per call: empty 54-75 ns, BamiAckShown ~100, BamiReset ~95, BamiPreedit plus free 267 ns, including `cgo.Handle` and the mutex.
- A key crosses ~8 times (AckShown, Key, two frees, Raw and free): at most 0.8 us, about 3% end to end. Batching is not worth an ABI change.
- Leak tests: 1M keys moved RSS 1.2 to 2.8 MB. 10k create/key/free cycles stayed flat (3.0-3.8 MB). 10k live contexts cost 22 MB (2.2 KB each, `BamiNew` 22-28 us). Real Instance, 5M keys: RSS 5.1 to 7.0 MB, same as after 1M.
- GC: 1.24 per 1000 keys through cgo, 1.59 in plain Go. STW p50 22 us, p99 0.28 ms, max 1.3-1.8 ms.

| 1M keys, C harness | GCs in 8 s | STW max | us/key | RSS | Threads |
|---|---|---|---|---|---|
| default (GOGC 100, 8 Ps) | 680-950 | 1.8 ms | 14.7 | 2.9 MB | 8-13 |
| patch 4 (GOGC 400, 2 Ps) | 114-171 | 0.46 ms | 8.2-12.8 | 6.1 MB | 7-8 |

## 3. Real Fcitx process (Debian 12)

| Item | Without bami | With bami |
|---|---|---|
| fcitx5 RSS idle | 31.4 MB, 2 threads | 36.3 MB, 8 threads |
| After 544 xdotool keys | 31.4 MB | 38.0 MB preedit, 40.0 MB edit |
| Addon load | n/a | 17-29 ms, +3.4 MB |
| Per key via testfrontend | 0.95 us | 26 us avg, p50 24, p99 51-77, p99.9 170-300 |

Ablation (p50 per key): default 24.7 us, no dictionary 22.8, no dictionary and no model 19.9, ProtectEnglish off 18.9. English protection costs ~6 us per key (BamiRaw, score, enchant); the remaining ~11 us is Fcitx panel and preedit updates plus cgo and string copies. 10k context cycles cost 0.1 MB. The enchant load did not show in the 17 ms load time.

## 4. C++ addon against R8

- Clean: no timer, lock, file I/O or D-Bus in `keyEvent` (grep-verified); `safeSaveAsIni` runs only in `setConfig`.
- Violation: `inputFor()` copies the whole surrounding text per key; `decide()` then runs `validUtf8`, `utf8Length` and `utf8Prefix` (a second copy). `apply()` also calls BamiRaw plus an enchant lookup per key, and strings are copied 3-4 times.

| Surrounding chars | 157 | 1,075 | 4,288 (Qt cap) | 20,863 | 104,095 | 1,040,863 |
|---|---|---|---|---|---|---|
| Before (us) | 0.75 | 4.5 | 17.4 | 84 | 422 | 4,276 |
| After, patch 3 (us) | 0.12 | 0.25 | 0.75 | 3.1 | capped | capped |

Capped means text over 32 KiB makes that word use preedit.

## 5. The zip design (`base.gob`)

Its Makefile only `curl`s four public word lists into `data/`, then runs `go run`. I read it first and reused lists another agent had fetched. `base.gob` is 2.6 MB.

| Item | Value |
|---|---|
| Load | gob decode 34 ms, reindex 237-309 ms (blocking), heap 22 MB, Sys 53-70 MB |
| Key with LM completion | p50 207-216 us, p99 5.5-5.9 ms, p99.9 11-13 ms, max 35 ms |
| First letter of a word | p50 2.1-2.7 ms, p99 10-11 ms |
| Telex `Feed` alone | p50 0.46 us, p99 1.2-2.9 us |
| GC | 550 per 124k keys, 55-65 ms total pause |
| Learn a new word | 0.9 ms (sorted-index memmove) |

Candidates can leave the key path with no timer. Prototype: the key thread does `Feed`, an atomic pointer store and a non-blocking channel send; one worker serves only the newest request and drops stale ones by generation. Measured key path p50 4.1 us, p99 17 us; candidates ready p50 192 us, p99 9.2 ms; 42% of requests served, the rest coalesced. In Fcitx, return results with `EventDispatcher::schedule` (eventfd). Separately, `capi.with()` runs sessions outside the global lock and `st.out` is unsynchronized.

## 6. End-to-end typing (Xvfb, Fcitx, bami, Mousepad/GTK; 544 keys, 96 words)

cps is effective speed. Exact means readback matched.

| Speed | No IM | Preedit (default) | Edit mode |
|---|---|---|---|
| 24-90 cps, idle and 12-busy-loop load | exact | 10/10 exact | 10/10 exact |
| ~175 cps, idle | exact | exact | exact |
| ~290 cps, idle | exact | 2/2 exact | 0/2 exact |
| ~640-1040 cps, idle | exact | 2/2 exact | 1/2 exact |
| 110-215 cps, load | n/a | 3/5 exact | 2/5 exact |

Edit-mode errors (`ddẹp`, `lâpj`, `qúa`, `hown`) are the word downgraded after a stale surrounding-text update, so later keys pass raw. Preedit failures under load were dropped spaces that joined words. Failures hit both builds and are random with few runs, so I cannot say the patches help or hurt there. Zero wrong characters holds up to ~100 cps in both modes; it does not hold above ~250 cps in edit mode.

## Patches

| Patch | Change | Result |
|---|---|---|
| 0001-bamboo-precompute-syllable-tables-no-map-in-getCanvas | Split tables once; no map | -26% time, -35% bytes, -12% allocs |
| 0002-engine-bound-word-length | Word ends at 32 keys | 531 to 29 us per key at 80 keys |
| 0003-strategy-no-surrounding-text-copy-bounded-scan | No copy, counted offset, window validation, 32 KiB cap | 17.4 to 0.75 us at 4,288 chars |
| 0004-bridge-gc-percent-gomaxprocs | GOGC 400, 2 Ps, 64 MiB limit | 4-6x fewer GCs, fewer threads, +3 MB RSS |

Validated: engine and bamboo `go test`, and ctest 5/5 in Debian 12. Not checked: Fedora's Fcitx, Wayland, Qt, Chromium. Patch 2 and the cap in patch 3 change behavior for very long words and texts.
