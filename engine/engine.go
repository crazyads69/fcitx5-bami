package engine

import (
	"strings"
	"unicode"
	"unicode/utf8"

	bamboo "github.com/crazyads69/fcitx5-bami/engine/third_party/bamboo-core"
)

// ToneStyle places the tone on oa/oe/uy.
type ToneStyle int

const (
	ToneOld ToneStyle = iota // hòa, thúy
	ToneNew                  // hoà, thuý
)

// Options are the user-visible switches. The zero value is: old style, no free tone, no spell check.
type Options struct {
	ToneStyle   ToneStyle
	FreeTone    bool // tone key may come before the syllable is complete (hofa -> hòa)
	LoneW       Gate // Telex family only; GateDefault = variant default
	Bracket     Gate // Telex family only
	QuickTelex  bool // opt-in, Telex only: cc->ch gg->gi kk->kh nn->ng pp->ph qq->qu tt->th
	AutoRestore bool // at Commit, a non-Vietnamese word returns to its raw keys (windows, text)
	SpellCheck  bool // marks/tones only apply when the syllable can be Vietnamese
	DdAnywhere  bool // dd -> đ always applies, and AutoRestore keeps a word that holds đ (add -> ađ)
}

// maxWordKeys bounds one word; 400 'a' keys cost ~10 s in total, 64 cost ~5 ms.
const maxWordKeys = 64

var quickMap = map[rune]rune{'c': 'h', 'g': 'i', 'k': 'h', 'n': 'g', 'p': 'h', 'q': 'u', 't': 'h'}

// Engine composes one word at a time. It is not safe for concurrent use.
type Engine struct {
	v       Variant
	o       Options
	def     variantDef
	loneW   Gate
	bracket Gate
	be      bamboo.IEngine
	flags   uint
	ddKeys  map[rune]bool
	appKeys map[rune]bool // keys that type a letter by themselves (MS 1->ă, [ -> ư)
	shown   string        // preedit last REPORTED to the caller (or acknowledged via AckShown)
	lastRaw rune          // previous consumed key, lower-cased, for Quick Telex
	raw     []rune        // input keys before transformations, including Quick Telex
	escape  bool          // pending VIQR backslash
}

// New returns an engine; an out-of-range variant falls back to Telex.
func New(v Variant, o Options) *Engine {
	e := &Engine{}
	e.Configure(v, o)
	return e
}

// Configure switches variant/options and resets the word.
func (e *Engine) Configure(v Variant, o Options) {
	if v < 0 || v >= numVariants {
		v = Telex
	}
	e.v, e.o, e.def = v, o, variantDefs[v]
	e.loneW, e.bracket = e.def.gates(o)
	if v != Telex {
		e.o.QuickTelex = false
	}
	im := bamboo.ParseInputMethod(map[string]bamboo.InputMethodDefinition{e.def.name: e.def.table(e.loneW, e.bracket)}, e.def.name)
	e.flags = bamboo.EautoCorrectEnabled
	if o.FreeTone {
		e.flags |= bamboo.EfreeToneMarking
	}
	if o.ToneStyle == ToneOld {
		e.flags |= bamboo.EstdToneStyle // bamboo's "std" style is the old one
	}
	if !o.SpellCheck {
		e.flags |= bamboo.EnoSpellCheck
	}
	e.be = bamboo.NewEngine(im, e.flags)
	e.ddKeys, e.appKeys = map[rune]bool{}, map[rune]bool{}
	for _, r := range im.Rules {
		if r.EffectType == bamboo.MarkTransformation && r.Effect != 0 && r.Result == 'đ' {
			e.ddKeys[r.Key] = true
		}
		if r.EffectType == bamboo.Appending {
			e.appKeys[r.Key] = true
		}
	}
	e.Reset()
}

// Reset drops the word without producing text.
func (e *Engine) Reset() {
	e.be.Reset()
	e.shown, e.lastRaw = "", 0
	e.raw = nil
	e.escape = false
}

// Preedit is the current composed word.
func (e *Engine) Preedit() string {
	return e.be.GetProcessedString(bamboo.VietnameseMode | bamboo.FullText)
}

// Raw returns the original keys; after Backspace it describes the remaining edited word.
func (e *Engine) Raw() string { return string(e.raw) }

// AckShown tells the engine what is really on screen, so the next Edit is relative to reality.
func (e *Engine) AckShown(s string) { e.shown = s }

func (e *Engine) edit() Edit {
	cur := e.Preedit()
	ed := Diff(e.shown, cur)
	e.shown = cur
	return ed
}

// Key feeds one typed rune. consumed=false means the engine ignores it: the caller should Commit the
// word (if any) and pass the key on. When consumed, Edit turns the last reported preedit into the new one.
func (e *Engine) Key(r rune) (Edit, bool) {
	if r < 0x20 || r == 0x7F || !utf8.ValidRune(r) {
		return Edit{}, false
	}
	if len(e.raw) >= maxWordKeys { // bamboo recomputes the word per key (~cubic): bound it, caller commits
		return Edit{}, false
	}
	if e.v == VIQR && (e.escape || r == '\\') {
		if e.escape {
			e.be.RemoveLastChar(false) // remove the displayed escape prefix
			e.escape = false
		} else {
			e.escape = true
		}
		e.raw = append(e.raw, r)
		e.be.ProcessKey(r, bamboo.EnglishMode)
		e.lastRaw = 0
		return e.edit(), true
	}
	lower := unicode.ToLower(r)
	empty := e.Preedit() == ""
	literal := false
	switch {
	case lower == 'w' && e.def.telex && e.loneW == GateAfterLetter && empty:
		literal = true // w at the start of a word stays w
	case !e.be.CanProcessKey(lower):
		return Edit{}, false
	case empty && !bamboo.IsAlpha(lower) && !(e.appKeys[lower] && !e.gatedStart(lower)):
		return Edit{}, false // digits, punctuation and tone keys do nothing at the start of a word
	}
	e.raw = append(e.raw, r)
	switch {
	case literal:
		e.be.ProcessKey(r, bamboo.EnglishMode)
	case e.o.QuickTelex && e.lastRaw == lower && quickMap[lower] != 0 && unicode.ToLower(lastRune(e.Preedit())) == lower:
		first := lastRune(e.Preedit())
		e.be.RemoveLastChar(false)
		e.be.ProcessKey(first, bamboo.VietnameseMode)
		add := quickMap[lower]
		if unicode.IsUpper(r) {
			add = unicode.ToUpper(add)
		}
		e.be.ProcessKey(add, bamboo.VietnameseMode)
		e.lastRaw = 0
		return e.edit(), true
	case e.o.DdAnywhere && e.ddKeys[lower]:
		e.be.SetFlag(e.flags | bamboo.EnoSpellCheck)
		e.be.ProcessKey(r, bamboo.VietnameseMode)
		e.be.SetFlag(e.flags)
	default:
		e.be.ProcessKey(r, bamboo.VietnameseMode)
	}
	e.lastRaw = lower
	return e.edit(), true
}

// gatedStart reports a bracket key that must stay literal at word start (GateAfterLetter).
func (e *Engine) gatedStart(r rune) bool {
	return e.bracket == GateAfterLetter && (r == '[' || r == ']' || r == '{' || r == '}')
}

func lastRune(s string) (r rune) {
	for _, r = range s {
	}
	return
}

// Backspace removes the last composed char and refreshes the tone (PRD F8). consumed=false when no
// word is being composed: the caller must let the app handle the key.
func (e *Engine) Backspace() (Edit, bool) {
	if e.Preedit() == "" {
		return Edit{}, false
	}
	e.be.RemoveLastChar(true)
	e.escape = false
	e.raw = []rune(e.be.GetProcessedString(bamboo.EnglishMode | bamboo.FullText))
	e.lastRaw = 0
	return e.edit(), true
}

// BackspaceRaw removes one displayed English key and rebuilds its Vietnamese alternative.
func (e *Engine) BackspaceRaw() (Edit, bool) {
	if len(e.raw) == 0 {
		return Edit{}, false
	}
	keys := append([]rune(nil), e.raw[:len(e.raw)-1]...)
	shown := e.shown
	e.Reset()
	for _, key := range keys {
		e.Key(key)
	}
	e.shown = shown
	return e.edit(), true
}

// Commit ends the word: with AutoRestore a non-Vietnamese word returns to its raw keys. It returns the
// final text and resets. The caller replaces whatever is on screen for this word with it.
func (e *Engine) Commit() string {
	if e.o.AutoRestore && !e.be.IsValid(true) && !(e.o.DdAnywhere && strings.ContainsAny(e.Preedit(), "đĐ")) {
		e.be.RestoreLastWord(false)
	}
	s := e.Preedit()
	e.Reset()
	return s
}
