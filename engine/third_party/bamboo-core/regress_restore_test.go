package bamboo

import "testing"

// new tone style + auto-restore of non-Vietnamese words at a word break
const fRst = EfreeToneMarking | EautoCorrectEnabled | EautoRestore

func TestRegressAutoRestore(t *testing.T) { // upstream: ibus-bamboo#429, fcitx5-bamboo#19
	run(t, []rc{
		{"windows", T, fRst, "windows ", "windows "}, {"Linux", T, fRst, "Linux ", "Linux "},
		{"text", T, fRst, "text ", "text "}, {"text.c", T, fRst, "text.c", "text.c"},
		{"text#primary", T, fRst, "text#primary ", "text#primary "},
		{"cursor", T, fRst, "cursor ", "cursor "}, {"Cursor", T, fRst, "Cursor ", "Cursor "},
		{"ROMA", T, fRst, "ROMA ", "ROMA "}, {"roma", T, fRst, "roma ", "roma "},
		{"windows-nospace", T, fNew, "windows", "windớ"}, // flag off: unchanged until commit
		{"keep-valid", T, fRst, "vieetj ", "việt "}, {"keep-valid2", T, fRst, "Nguwowif ", "Người "},
		{"keep-hoa", T, fRst, "hoaf,", "hoà,"}, {"uynh", T, fRst, "quynhs ", "quýnh "}, {"oe", T, fRst, "khoer ", "khoẻ "},
		{"two-words", T, fRst, "windows vieetj ", "windows việt "},
		{"digit", T, fRst, "text1", "text1"}, {"vni-digit-in-word", T, fRst, "viet65", "viet65"},
		{"apostrophe", T, fRst, "don't ", "don't "}, {"its", T, fRst, "it's ", "it's "},
		{"viqr-apostrophe", "Telex + VNI + VIQR", fRst, "rock'n'roll ", "rock'n'roll "},
		{"ok-then-punct", T, fRst, "xin chaof!", "xin chào!"},
		{"quote", T, fRst, "\"windows\"", "\"windows\""},
		{"mixed-case-restore", T, fRst, "WINDOWS ", "WINDOWS "}, {"Windows", T, fRst, "Windows ", "Windows "},
	})
}

func TestRegressNumbersPunct(t *testing.T) {
	run(t, []rc{
		{"num-after", T, fRst, "vieetj1", "việt1"}, {"num-word-num", T, fRst, "a1b", "a1b"},
		{"comma", T, fRst, "chaof,", "chào,"}, {"dash", T, fRst, "tieengs-vieetj", "tiếng-việt"},
		{"num-resets-tone-target", T, fNew, "hoa1f", "hoa1f"[:0] + "hoa1f"},
		{"digits-vni", V, fNew, "viet65", "việt"},
		{"punct-between", T, fNew, "toans.s", "toán.s"},
		{"paren", T, fNew, "(vieetj)", "(việt)"},
	})
}
