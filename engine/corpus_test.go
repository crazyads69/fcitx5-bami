package engine

import (
	"math/rand"
	"strings"
	"testing"

	bamboo "github.com/crazyads69/fcitx5-bami/engine/third_party/bamboo-core"
)

// Tone rows, index = none, grave, acute, hook, tilde, dot. Literal NFC strings: independent of bamboo's charset.
var toneRows = map[rune][]rune{
	'a': []rune("aàáảãạ"), 'ă': []rune("ăằắẳẵặ"), 'â': []rune("âầấẩẫậ"), 'e': []rune("eèéẻẽẹ"),
	'ê': []rune("êềếểễệ"), 'i': []rune("iìíỉĩị"), 'o': []rune("oòóỏõọ"), 'ô': []rune("ôồốổỗộ"),
	'ơ': []rune("ơờớởỡợ"), 'u': []rune("uùúủũụ"), 'ư': []rune("ưừứửữự"), 'y': []rune("yỳýỷỹỵ"),
}

// Keys that type each Vietnamese letter, and the tone keys (none, grave, acute, hook, tilde, dot).
type scheme struct {
	v     Variant
	keys  map[rune]string
	tones [6]string
}

var schemes = []scheme{
	{Telex, map[rune]string{'â': "aa", 'ă': "aw", 'ê': "ee", 'ô': "oo", 'ơ': "ow", 'ư': "uw", 'đ': "dd"}, [6]string{"", "f", "s", "r", "x", "j"}},
	{VNI, map[rune]string{'â': "a6", 'ă': "a8", 'ê': "e6", 'ô': "o6", 'ơ': "o7", 'ư': "u7", 'đ': "d9"}, [6]string{"", "2", "1", "3", "4", "5"}},
	{VIQR, map[rune]string{'â': "a^", 'ă': "a(", 'ê': "e^", 'ô': "o^", 'ơ': "o+", 'ư': "u+", 'đ': "dd"}, [6]string{"", "`", "'", "?", "~", "."}},
	{MS, map[rune]string{'â': "2", 'ă': "1", 'ê': "3", 'ô': "4", 'ơ': "]", 'ư': "[", 'đ': "0"}, [6]string{"", "5", "8", "6", "7", "9"}},
}

func (s scheme) typ(letters string) string {
	var b strings.Builder
	for _, r := range letters {
		if k, ok := s.keys[r]; ok {
			b.WriteString(k)
		} else {
			b.WriteRune(r)
		}
	}
	return b.String()
}

// tonePos is the independent oracle for where a tone lands in a vowel run.
func tonePos(vo []rune, coda string, style ToneStyle) int {
	if len(vo) == 1 {
		return 0
	}
	for i, r := range vo {
		if strings.ContainsRune("êôơâă", r) {
			return i
		}
	}
	if len(vo) == 3 {
		return 1
	}
	if coda != "" {
		return 1
	}
	if s := string(vo); style == ToneNew && (s == "oa" || s == "oe" || s == "uy") {
		return 1
	}
	return 0
}

// compose builds the expected word. lastVowel=true is the FreeTone-off rule: the tone stays on the
// vowel that was last typed when the tone key came.
func compose(fc, vo, lc string, tone int, style ToneStyle, lastVowel bool) string {
	v := []rune(vo)
	if tone > 0 {
		p := tonePos(v, lc, style)
		if lastVowel {
			p = len(v) - 1
		}
		v[p] = toneRows[v[p]][tone]
	}
	return fc + string(v) + lc
}

type syl struct{ fc, vo, lc string }

// genSyllables enumerates onset x nucleus x coda from bamboo's own spelling tables.
func genSyllables() []syl {
	first, vowel, last, cv, vc := bamboo.SpellingTables()
	var out []syl
	skipVo := map[string]bool{"oo": true, "uơ": true, "oă": true, "uya": true, "uyu": true} // ambiguous or rare, no stable oracle
	for fi := -1; fi < len(first); fi++ {
		var fcs []string
		if fi < 0 {
			fcs = []string{""}
		} else {
			fcs = strings.Fields(first[fi])
		}
		for vi, row := range vowel {
			if fi >= 0 {
				ok := false
				for _, c := range cv[fi] {
					ok = ok || c == vi
				}
				if !ok {
					continue
				}
			}
			for _, vo := range strings.Fields(row) {
				if skipVo[vo] {
					continue
				}
				if vo == "uâ" || vo == "uô" {
					// uâ / uô only exist before a coda or i; bare they are not syllables
					for _, fc := range fcs {
						for _, li := range vc[vi] {
							for _, lc := range strings.Fields(last[li]) {
								out = append(out, syl{fc, vo, lc})
							}
						}
					}
					continue
				}
				for _, fc := range fcs {
					if (fc == "qu" && strings.HasPrefix(vo, "u")) || ((fc == "gi" || fc == "g") && strings.HasPrefix(vo, "i")) {
						continue
					}
					out = append(out, syl{fc, vo, ""})
					for _, li := range vc[vi] {
						for _, lc := range strings.Fields(last[li]) {
							if lc != "k" { // ăk only exists in Đắk Lắk style names and takes no tone
								out = append(out, syl{fc, vo, lc})
							}
						}
					}
				}
			}
		}
	}
	return out
}

func isStop(lc string) bool { return lc == "c" || lc == "ch" || lc == "p" || lc == "t" }

func TestCorpusGeneratedFromSpellingRules(t *testing.T) {
	syls := genSyllables()
	if len(syls) < 3000 {
		t.Fatalf("only %d syllables generated", len(syls))
	}
	rnd := rand.New(rand.NewSource(7))
	var total, bad int
	for _, sc := range schemes {
		for _, style := range []ToneStyle{ToneOld, ToneNew} {
			for i, s := range syls {
				if (sc.v != Telex && i%4 != 0) || (raceOn && i%8 != 0) {
					continue // Telex gets every syllable; the other schemes a quarter
				}
				tone := 1 + (i+int(style))%5
				if isStop(s.lc) && tone != 2 && tone != 5 {
					tone = []int{2, 5}[rnd.Intn(2)]
				}
				// tone key at the end, and (when FreeTone is off) right after the nucleus
				for _, free := range []bool{true, false} {
					word := compose(s.fc, s.vo, s.lc, tone, style, !free)
					if !isNFCish(word) {
						t.Fatalf("oracle word %q is not NFC", word)
					}
					o := Options{ToneStyle: style, FreeTone: free, SpellCheck: true}
					nucleus := sc.typ(s.fc + s.vo)
					var keys string
					if free {
						keys = nucleus + sc.typ(s.lc) + sc.tones[tone]
					} else {
						keys = nucleus + sc.tones[tone] + sc.typ(s.lc)
					}
					total++
					got := typeKeys(t, sc.v, o, keys)
					if !isNFCish(got) {
						t.Fatalf("%s %q output %q is not NFC", sc.v.Name(), keys, got)
					}
					if got != word {
						bad++
						if bad <= 25 {
							t.Errorf("%s style=%d free=%v %q => %q want %q", sc.v.Name(), style, free, keys, got, word)
						}
					}
				}
			}
		}
	}
	t.Logf("%d syllables, %d typed words, %d mismatches", len(syls), total, bad)
	if bad > 0 {
		t.Fatalf("%d mismatches", bad)
	}
}

// Backspace on a generated word: the last composed char goes, the tone is refreshed.
func TestCorpusBackspace(t *testing.T) {
	syls := genSyllables()
	var total, bad int
	for _, sc := range schemes[:2] { // Telex, VNI
		for _, style := range []ToneStyle{ToneOld, ToneNew} {
			for i, s := range syls {
				if raceOn && i%8 != 0 {
					continue
				}
				tone := 1 + i%5
				if isStop(s.lc) {
					tone = []int{2, 5}[i%2]
				}
				if s.lc == "" {
					continue // removing a vowel makes an arbitrary run; the coda case is the interesting one
				}
				lc := s.lc[:len(s.lc)-1]
				want := compose(s.fc, s.vo, lc, tone, style, false)
				if (lc != "" && isStop(lc)) || (lc == "" && (s.vo == "uâ" || s.vo == "uô")) {
					continue
				}
				keys := sc.typ(s.fc+s.vo) + sc.typ(s.lc) + sc.tones[tone] + "<"
				total++
				if got := typeKeys(t, sc.v, Options{ToneStyle: style, FreeTone: true, SpellCheck: true}, keys); got != want {
					bad++
					if bad <= 25 {
						t.Errorf("%s %q => %q want %q", sc.v.Name(), keys, got, want)
					}
				}
			}
		}
	}
	t.Logf("%d backspace words, %d mismatches", total, bad)
	if bad > 0 {
		t.Fatalf("%d mismatches", bad)
	}
}

// Random key soup on every variant: never panics, the Edit stream always reproduces the preedit.
func TestRandomKeysScreenConsistency(t *testing.T) {
	rnd := rand.New(rand.NewSource(11))
	alphabet := []rune("abcdeghiklmnopqrstuvxyzwfjAEOUD0123456789[]{}'`?~.^+*()<  ")
	for v := Variant(0); v < numVariants; v++ {
		for _, o := range []Options{stdOpts, {}, {FreeTone: true, QuickTelex: true, DdAnywhere: true, LoneW: GateAfterLetter, Bracket: GateAfterLetter, ToneStyle: ToneNew}} {
			for i := 0; i < 300; i++ {
				b := make([]rune, 1+rnd.Intn(25))
				for j := range b {
					b[j] = alphabet[rnd.Intn(len(alphabet))]
				}
				typeKeys(t, v, o, string(b))
			}
		}
	}
}
