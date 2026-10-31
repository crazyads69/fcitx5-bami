// Package main is built with -buildmode=c-archive; it only exports C functions over engine/.
//
// C API (generated header: libbami.h / whatever -o names). All functions recover Go panics (R7).
//
//	void      BamiInit(void);                      // once per process; no-op, kept for ABI
//	uintptr_t BamiNew(int variant, int toneStyle, unsigned flags);   // 0 on failure
//	void      BamiConfigure(uintptr_t h, int variant, int toneStyle, unsigned flags); // also resets the word
//	void      BamiFree(uintptr_t h);               // h is invalid afterwards
//	void      BamiReset(uintptr_t h);              // drop the word, produce no text
//	int       BamiKey(uintptr_t h, unsigned ch, int *deleteN, char **insert, char **preedit);
//	int       BamiBackspace(uintptr_t h, int *deleteN, char **insert, char **preedit);
//	int       BamiBackspaceRaw(uintptr_t h, int *deleteN, char **insert, char **preedit);
//	char     *BamiCommit(uintptr_t h);             // final word text (AutoRestore applied), resets; NULL on error
//	char     *BamiRaw(uintptr_t h);                // original keys; caller frees
//	double    BamiEnglishScore(uintptr_t h);        // local score; -1 invalid handle
//	double    BamiEnglishThreshold(void);           // validation-selected threshold
//	char     *BamiPreedit(uintptr_t h);            // current composed word; NULL on error
//	void      BamiFreeString(char *s);             // NULL-safe
//	void      BamiAckShown(uintptr_t h, const char *shown);
//
// BamiKey / BamiBackspace return 1 consumed, 0 pass-through, -1 internal error.
//   - On 1 and 0, *deleteN (runes, not bytes) and the NUL-terminated UTF-8 NFC strings *insert and
//     *preedit are set; the caller owns both strings and must release them with BamiFreeString.
//     deleteN/insert are the Edit that turns the last REPORTED preedit into the new preedit.
//   - On 0 nothing changed: *deleteN = 0, *insert = "" and *preedit = current preedit. The caller should
//     BamiCommit the word (if any) and let the key through.
//   - On -1 the outputs are untouched (*deleteN = 0, strings NULL) and the word was reset.
//   - Any output pointer may be NULL if the caller does not want it.
//
// BamiAckShown(h, s): tell the engine what is really on screen for the current word (e.g. the app
// skipped an edit, or the front-end fell back to preedit). The next Edit is relative to s.
//
// variant:   0 Telex, 1 Telex2, 2 TelexW, 3 SimpleTelex1, 4 SimpleTelex2, 5 VNI, 6 VIQR, 7 MS, 8 TelexVNI
// toneStyle: 0 old (hòa), 1 new (hoà)
// flags bits:
//
//	bit 0  (1)    FreeTone      bit 1  (2)   SpellCheck   bit 2 (4)  AutoRestore
//	bit 3  (8)    DdAnywhere    bit 4  (16)  QuickTelex (Telex only)
//	bits 5-6      LoneW gate: 0 variant default, 1 off, 2 after a letter, 3 everywhere
//	bits 7-8      Bracket gate (same values); both gates only affect the Telex family
package main

/*
#include <stdint.h>
#include <stdlib.h>
typedef const char bami_cchar;
*/
import "C"

import (
	"runtime"
	"runtime/cgo"
	"runtime/debug"
	"sync"
	"unsafe"

	"github.com/crazyads69/fcitx5-bami/engine"
)

type state struct {
	mu sync.Mutex
	e  *engine.Engine
}

func opts(toneStyle C.int, flags C.uint) engine.Options {
	f := uint(flags)
	return engine.Options{
		ToneStyle:   engine.ToneStyle(toneStyle),
		FreeTone:    f&1 != 0,
		SpellCheck:  f&2 != 0,
		AutoRestore: f&4 != 0,
		DdAnywhere:  f&8 != 0,
		QuickTelex:  f&16 != 0,
		LoneW:       engine.Gate(f >> 5 & 3),
		Bracket:     engine.Gate(f >> 7 & 3),
	}
}

func get(h C.uintptr_t) *state { return cgo.Handle(h).Value().(*state) }

//export BamiInit
func BamiInit() {
	// Leave the host's signal dispositions alone (Go never writes to a pipe). The engine is serial and
	// small: 2 Ps keep the added thread count low and a 400% GC target cuts collections ~4x.
	// shortcut: no SetMemoryLimit, the embedded English model makes a tight limit risk GC thrashing.
	runtime.GOMAXPROCS(2)
	debug.SetGCPercent(400)
}

//export BamiNew
func BamiNew(variant, toneStyle C.int, flags C.uint) (h C.uintptr_t) {
	defer func() {
		if recover() != nil {
			h = 0
		}
	}()
	return C.uintptr_t(cgo.NewHandle(&state{e: engine.New(engine.Variant(variant), opts(toneStyle, flags))}))
}

//export BamiConfigure
func BamiConfigure(h C.uintptr_t, variant, toneStyle C.int, flags C.uint) {
	defer func() { _ = recover() }()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	s.e.Configure(engine.Variant(variant), opts(toneStyle, flags))
}

//export BamiFree
func BamiFree(h C.uintptr_t) {
	defer func() { _ = recover() }()
	cgo.Handle(h).Delete()
}

//export BamiReset
func BamiReset(h C.uintptr_t) {
	defer func() { _ = recover() }()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	s.e.Reset()
}

func report(s *state, ed engine.Edit, deleteN *C.int, insert, preedit **C.char) {
	if deleteN != nil {
		*deleteN = C.int(ed.DeleteN)
	}
	if insert != nil {
		*insert = C.CString(ed.Insert)
	}
	if preedit != nil {
		*preedit = C.CString(s.e.Preedit())
	}
}

func zeroOut(deleteN *C.int, insert, preedit **C.char) {
	if deleteN != nil {
		*deleteN = 0
	}
	if insert != nil {
		*insert = nil
	}
	if preedit != nil {
		*preedit = nil
	}
}

//export BamiKey
func BamiKey(h C.uintptr_t, ch C.uint, deleteN *C.int, insert, preedit **C.char) (ret C.int) {
	defer func() {
		if recover() != nil {
			zeroOut(deleteN, insert, preedit)
			ret = -1
		}
	}()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	defer func() { // a panic inside the engine leaves it in an unknown state
		if recover() != nil {
			s.e.Reset()
			zeroOut(deleteN, insert, preedit)
			ret = -1
		}
	}()
	ed, ok := s.e.Key(rune(ch))
	report(s, ed, deleteN, insert, preedit)
	if ok {
		return 1
	}
	return 0
}

//export BamiBackspace
func BamiBackspace(h C.uintptr_t, deleteN *C.int, insert, preedit **C.char) C.int {
	return backspace(h, false, deleteN, insert, preedit)
}

//export BamiBackspaceRaw
func BamiBackspaceRaw(h C.uintptr_t, deleteN *C.int, insert, preedit **C.char) C.int {
	return backspace(h, true, deleteN, insert, preedit)
}

func backspace(h C.uintptr_t, raw bool, deleteN *C.int, insert, preedit **C.char) (ret C.int) {
	defer func() {
		if recover() != nil {
			zeroOut(deleteN, insert, preedit)
			ret = -1
		}
	}()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	defer func() {
		if recover() != nil {
			s.e.Reset()
			zeroOut(deleteN, insert, preedit)
			ret = -1
		}
	}()
	var ed engine.Edit
	var ok bool
	if raw {
		ed, ok = s.e.BackspaceRaw()
	} else {
		ed, ok = s.e.Backspace()
	}
	report(s, ed, deleteN, insert, preedit)
	if ok {
		return 1
	}
	return 0
}

//export BamiCommit
func BamiCommit(h C.uintptr_t) (out *C.char) {
	defer func() {
		if recover() != nil {
			out = nil
		}
	}()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	return C.CString(s.e.Commit())
}

//export BamiPreedit
func BamiPreedit(h C.uintptr_t) (out *C.char) {
	defer func() {
		if recover() != nil {
			out = nil
		}
	}()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	return C.CString(s.e.Preedit())
}

//export BamiRaw
func BamiRaw(h C.uintptr_t) (out *C.char) {
	defer func() {
		if recover() != nil {
			out = nil
		}
	}()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	return C.CString(s.e.Raw())
}

//export BamiEnglishScore
func BamiEnglishScore(h C.uintptr_t) (score C.double) {
	defer func() {
		if recover() != nil {
			score = -1
		}
	}()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	return C.double(s.e.PredictEnglish())
}

//export BamiEnglishThreshold
func BamiEnglishThreshold() C.double { return C.double(engine.EnglishThreshold()) }

//export BamiFreeString
func BamiFreeString(p *C.char) {
	if p != nil {
		C.free(unsafe.Pointer(p))
	}
}

//export BamiAckShown
func BamiAckShown(h C.uintptr_t, shown *C.bami_cchar) {
	defer func() { _ = recover() }()
	s := get(h)
	s.mu.Lock()
	defer s.mu.Unlock()
	s.e.AckShown(C.GoString((*C.char)(unsafe.Pointer(shown))))
}

// BamiDiffDeleteN is the M0 round-trip proof (kept for the C++ scaffold): rune count to delete.
//
//export BamiDiffDeleteN
func BamiDiffDeleteN(oldS, newS *C.char) (n C.int) {
	defer func() {
		if recover() != nil {
			n = -1
		}
	}()
	return C.int(engine.Diff(C.GoString(oldS), C.GoString(newS)).DeleteN)
}

func main() {}
