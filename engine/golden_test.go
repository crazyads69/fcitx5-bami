package engine

import (
	"testing"
	"unicode"
)

var stdOpts = Options{FreeTone: true, SpellCheck: true, AutoRestore: true}

// typeKeys feeds keys like a front-end: '<' = Backspace, a key the engine does not consume commits the
// word and is appended as is. It checks after EVERY step that applying the reported Edit to the
// simulated screen gives exactly the preedit, and returns the whole text.
func typeKeys(t testing.TB, v Variant, o Options, keys string) string {
	t.Helper()
	e := New(v, o)
	done, screen := "", ""
	check := func(ed Edit, step string) {
		screen = Apply(screen, ed)
		if screen != e.Preedit() {
			t.Fatalf("%s %q after %s: screen %q != preedit %q", v.Name(), keys, step, screen, e.Preedit())
		}
	}
	for _, k := range keys {
		if k == '<' {
			if ed, ok := e.Backspace(); ok {
				check(ed, "BS")
			} else {
				done = trimLast(done)
			}
			continue
		}
		ed, ok := e.Key(k)
		if ok {
			check(ed, string(k))
			continue
		}
		done += e.Commit() + string(k)
		screen = ""
	}
	return done + e.Commit()
}

func trimLast(s string) string {
	r := []rune(s)
	if len(r) == 0 {
		return s
	}
	return string(r[:len(r)-1])
}

type row struct{ keys, want string }

func runRows(t *testing.T, v Variant, o Options, rows []row) {
	t.Helper()
	if len(rows) < 15 {
		t.Fatalf("%s: only %d golden rows", v.Name(), len(rows))
	}
	for _, r := range rows {
		if got := typeKeys(t, v, o, r.keys); got != r.want {
			t.Errorf("%s %q => %q want %q", v.Name(), r.keys, got, r.want)
		}
	}
}

// Rows marked (corpus) are verified outputs from the variant research (bamboo-core, Unikey, OpenKey agree).
var telexRows = []row{
	{"Vieetj", "Việt"}, {"dduwowcj", "được"}, {"dduowcj", "được"}, {"hoaf", "hòa"}, {"thuys", "thúy"},
	{"quyeenf", "quyền"}, {"nhuw", "như"}, {"DDuwowcj", "Được"}, {"VIEEJT", "VIỆT"}, {"tieesng", "tiếng"},
	{"ddoongf", "đồng"}, {"thuowng", "thương"}, {"ddi", "đi"}, {"khoer", "khỏe"},
	{"nguwowfi", "người"}, {"chuyeenr", "chuyển"}, {"quaanj", "quận"}, {"hofa", "hòa"},
	{"xin chaof, cacs banj", "xin chào, các bạn"}, {"windows text Linux", "windows text Linux"},
	{"tieengs Vieetj", "tiếng Việt"}, {"nhw w", "nhw w"}, {"dep", "dep"}, {"zoo", "zô"},
}

func TestGoldenTelex(t *testing.T) { runRows(t, Telex, stdOpts, telexRows) }

func TestGoldenTelex2(t *testing.T) { // lone w and brackets on
	runRows(t, Telex2, stdOpts, append(append([]row{}, telexRows[:20]...),
		row{"nhw", "như"}, row{"w", "ư"}, row{"wa", "ưa"}, row{"nh]", "như"}, row{"[t", "ơt"}, row{"]", "ư"},
		row{"www", "ww"}, row{"windows", "windows"}))
}

func TestGoldenTelexW(t *testing.T) { // lone w on, brackets literal
	runRows(t, TelexW, stdOpts, append(append([]row{}, telexRows[:20]...),
		row{"nhw", "như"}, row{"w", "ư"}, row{"wa", "ưa"}, row{"nh]", "nh]"}, row{"[t", "[t"}, row{"]", "]"},
		row{"ww", "w"}, row{"www", "ww"}))
}

func TestGoldenSimpleTelex1(t *testing.T) { // w only modifies a vowel; never a letter by itself
	runRows(t, SimpleTelex1, stdOpts, append(append([]row{}, telexRows[:20]...),
		row{"nhw", "nhw"}, row{"w", "w"}, row{"wa", "wa"}, row{"nh]", "nh]"}, row{"[t", "[t"}, row{"uw", "ư"},
		row{"ow", "ơ"}, row{"aw", "ă"}))
}

func TestGoldenSimpleTelex2(t *testing.T) { // lone w -> ư, no brackets
	runRows(t, SimpleTelex2, stdOpts, append(append([]row{}, telexRows[:20]...),
		row{"nhw", "như"}, row{"w", "ư"}, row{"wa", "ưa"}, row{"nh]", "nh]"}, row{"[t", "[t"}, row{"uw", "ư"},
		row{"ow", "ơ"}, row{"aw", "ă"}))
}

func TestSimpleTelexGatesAreLocked(t *testing.T) {
	o := stdOpts
	o.LoneW, o.Bracket = GateOff, GateEverywhere
	if got := typeKeys(t, SimpleTelex2, o, "nhw nh]"); got != "như nh]" {
		t.Errorf("Simple Telex 2 must ignore gate overrides, got %q", got)
	}
	if got := typeKeys(t, SimpleTelex1, o, "nhw nh]"); got != "nhw nh]" {
		t.Errorf("Simple Telex 1 must ignore gate overrides, got %q", got)
	}
}

func TestGoldenVNI(t *testing.T) {
	runRows(t, VNI, stdOpts, []row{
		{"Vie6t5", "Việt"}, {"d9u7o7c5", "được"}, {"hoa2", "hòa"}, {"thuy1", "thúy"}, {"quye6n2", "quyền"},
		{"nhu7", "như"}, {"thu7o7ng", "thương"}, {"D9u7o7c5", "Được"}, {"a8n", "ăn"}, {"to6t1", "tốt"},
		{"d9i", "đi"}, {"tie6ng1", "tiếng"}, {"d9o6ng2", "đồng"}, {"ngu7o7i2", "người"},
		{"chuye6n3", "chuyển"}, {"khoe3", "khỏe"}, {"hello 2024", "hello 2024"}, {"1", "1"}, {"Vie6t5 nam", "Việt nam"},
	})
}

func TestGoldenVIQR(t *testing.T) {
	runRows(t, VIQR, stdOpts, []row{
		{"Vie^.t", "Việt"}, {"ddu+o+c.", "được"}, {"hoa`", "hòa"}, {"thuy'", "thúy"}, {"quye^n`", "quyền"},
		{"nhu+", "như"}, {"thu+o+ng", "thương"}, {"Ddu+o+c.", "Được"}, {"a(n", "ăn"}, {"to^t'", "tốt"},
		{"ddi", "đi"}, {"tie^'ng", "tiếng"}, {"khoe?", "khỏe"}, {"nguo*i`", "người"},
		{"chuye^n?", "chuyển"}, {"xin chao`", "xin chào"}, {"ddo^ng`", "đồng"},
	})
}

func TestGoldenMS(t *testing.T) {
	runRows(t, MS, stdOpts, []row{
		{"Vi3t9", "Việt"}, {"0[]c9", "được"}, {"hoa5", "hòa"}, {"thuy8", "thúy"}, {"quy3n5", "quyền"},
		{"nh[", "như"}, {"th[]ng", "thương"}, {")[]c9", "Được"}, {"1n", "ăn"}, {"t4t8", "tốt"}, {"0i", "đi"},
		{"ti3ng8", "tiếng"}, {"04ng5", "đồng"}, {"ng[]i5", "người"}, {"khoe6", "khỏe"}, {"!n", "Ăn"}, {"@m", "Âm"},
	})
}

func TestGoldenTelexVNI(t *testing.T) {
	runRows(t, TelexVNI, stdOpts, []row{
		{"Vieetj", "Việt"}, {"Vie6t5", "Việt"}, {"dduwowcj", "được"}, {"d9u7o7c5", "được"}, {"hoaf", "hòa"},
		{"hoa2", "hòa"}, {"thuys", "thúy"}, {"thuy1", "thúy"}, {"quyeenf", "quyền"}, {"quye6n2", "quyền"},
		{"nhuw", "như"}, {"nhu7", "như"}, {"tieesng", "tiếng"}, {"tie6ng1", "tiếng"}, {"ddi d9i", "đi đi"},
		{"hello", "hello"},
	})
}

// MS swaps the brackets relative to Telex: Telex [ -> ơ ] -> ư, MS [ -> ư ] -> ơ.
func TestBracketSwapMSvsTelex(t *testing.T) {
	telex, ms := stdOpts, stdOpts
	telex.Bracket = GateEverywhere
	if got := typeKeys(t, Telex, telex, "h[ h]"); got != "hơ hư" {
		t.Errorf("Telex brackets: %q", got)
	}
	if got := typeKeys(t, MS, ms, "h[ h]"); got != "hư hơ" {
		t.Errorf("MS brackets: %q", got)
	}
	if got := typeKeys(t, MS, ms, "{ }"); got != "Ư Ơ" {
		t.Errorf("MS shifted brackets: %q", got)
	}
	if got := typeKeys(t, Telex, telex, "{ }"); got != "Ơ Ư" {
		t.Errorf("Telex shifted brackets: %q", got)
	}
}

func TestVariantTablesMatchBamboo(t *testing.T) { // Telex2 and TelexW are exactly bamboo's own tables
	for _, c := range []struct {
		v    Variant
		bamb string
	}{{Telex2, "Telex 2"}, {TelexW, "Telex W"}, {Telex, "Telex"}} {
		got := variantDefs[c.v].table(variantDefs[c.v].loneW, variantDefs[c.v].bracket)
		want := bambooDef(c.bamb)
		if len(got) != len(want) {
			t.Errorf("%s: %d keys, bamboo %d", c.bamb, len(got), len(want))
		}
		for k, v := range want {
			if got[k] != v {
				t.Errorf("%s key %q: %q want %q", c.bamb, k, got[k], v)
			}
		}
	}
}

func TestGates(t *testing.T) {
	o := stdOpts
	o.LoneW = GateAfterLetter
	for _, c := range []row{{"w", "w"}, {"wa", "wa"}, {"nhw", "như"}, {"tw", "tư"}, {"ww", "ww"}} {
		if got := typeKeys(t, Telex, o, c.keys); got != c.want {
			t.Errorf("LoneW after-letter %q => %q want %q", c.keys, got, c.want)
		}
	}
	o = stdOpts
	o.Bracket = GateAfterLetter
	for _, c := range []row{{"[a", "[a"}, {"h[", "hơ"}, {"nh]", "như"}, {"]", "]"}} {
		if got := typeKeys(t, Telex, o, c.keys); got != c.want {
			t.Errorf("Bracket after-letter %q => %q want %q", c.keys, got, c.want)
		}
	}
	o = stdOpts
	o.LoneW, o.Bracket = GateEverywhere, GateEverywhere
	if got := typeKeys(t, Telex, o, "w nh] [t"); got != "ư như ơt" {
		t.Errorf("Telex with both gates everywhere: %q", got)
	}
	o.LoneW = GateOff
	if got := typeKeys(t, Telex2, o, "w nhw"); got != "w nhw" {
		t.Errorf("Telex2 with LoneW off: %q", got)
	}
}

func TestToneStyleAndFreeTone(t *testing.T) {
	old, nw := stdOpts, stdOpts
	nw.ToneStyle = ToneNew
	for _, c := range []struct {
		keys, old, new string
	}{{"hoaf", "hòa", "hoà"}, {"thuys", "thúy", "thuý"}, {"khoer", "khỏe", "khoẻ"}, {"hoanf", "hoàn", "hoàn"}, {"tieengs", "tiếng", "tiếng"}} {
		if got := typeKeys(t, Telex, old, c.keys); got != c.old {
			t.Errorf("old %q => %q want %q", c.keys, got, c.old)
		}
		if got := typeKeys(t, Telex, nw, c.keys); got != c.new {
			t.Errorf("new %q => %q want %q", c.keys, got, c.new)
		}
	}
	free, nofree := stdOpts, stdOpts
	nofree.FreeTone = false
	if got := typeKeys(t, Telex, free, "hofa"); got != "hòa" {
		t.Errorf("free tone: %q", got)
	}
	if got := typeKeys(t, Telex, nofree, "hoaf"); got != "hoà" { // no free tone: the tone stays on the vowel just typed
		t.Errorf("no free tone: %q", got)
	}
}

func TestQuickTelex(t *testing.T) {
	q := stdOpts
	q.QuickTelex = true
	for _, c := range []row{{"ccan", "chan"}, {"nnha", "ngha"}, {"kko", "kho"}, {"ggas", "giá"}, {"qqa", "qua"}, {"ttoan", "thoan"}, {"Cca", "Cha"}, {"CCa", "CHa"}} {
		if got := typeKeys(t, Telex, q, c.keys); got != c.want {
			t.Errorf("quick %q => %q want %q", c.keys, got, c.want)
		}
	}
	if got := typeKeys(t, Telex, stdOpts, "ccan"); got != "ccan" {
		t.Errorf("quick telex is opt-in, got %q", got)
	}
	if got := typeKeys(t, VNI, q, "cca"); got != "cca" {
		t.Errorf("quick telex must be Telex only, got %q", got)
	}
	if got := typeKeys(t, Telex2, q, "cca"); got != "cca" {
		t.Errorf("quick telex must be Telex only, got %q", got)
	}
}

func TestSpellCheckAndDdAnywhere(t *testing.T) {
	spell, nospell := Options{FreeTone: true, SpellCheck: true}, Options{FreeTone: true}
	if got := typeKeys(t, Telex, spell, "baow"); got != "baow" {
		t.Errorf("spell check on must not put a horn on a non-Vietnamese syllable: %q", got)
	}
	if got := typeKeys(t, Telex, nospell, "baow"); got != "baơ" {
		t.Errorf("spell check off must mark anything: %q", got)
	}
	ar, dd := spell, spell
	ar.AutoRestore = true
	dd.AutoRestore, dd.DdAnywhere = true, true
	if got := typeKeys(t, Telex, ar, "add "); got != "add " {
		t.Errorf("AutoRestore restores add: %q", got)
	}
	if got := typeKeys(t, Telex, dd, "add "); got != "ađ " {
		t.Errorf("dd anywhere keeps đ: %q", got)
	}
	if got := typeKeys(t, Telex, dd, "ddi vieetj "); got != "đi việt " {
		t.Errorf("dd anywhere keeps normal words: %q", got)
	}
}

func TestAutoRestore(t *testing.T) {
	off := Options{FreeTone: true, SpellCheck: true}
	for _, c := range []row{{"windows ", "windows "}, {"text ", "text "}, {"Linux ", "Linux "}, {"cursor,", "cursor,"}, {"vieetj ", "việt "}, {"xin chaof!", "xin chào!"}, {"don't ", "don't "}} {
		if got := typeKeys(t, Telex2, stdOpts, c.keys); got != c.want {
			t.Errorf("restore %q => %q want %q", c.keys, got, c.want)
		}
	}
	if got := typeKeys(t, Telex, off, "text"); got != "tẽt" {
		t.Errorf("without AutoRestore bamboo output stays: %q", got)
	}
}

func TestBackspaceTable(t *testing.T) {
	nw := stdOpts
	nw.ToneStyle = ToneNew
	for _, c := range []struct {
		v    Variant
		o    Options
		keys string
		want string
	}{
		{Telex, stdOpts, "vieetj<", "việ"}, {Telex, stdOpts, "vieetj<<", "vi"}, {Telex, stdOpts, "vieetj<<<", "v"},
		{Telex, stdOpts, "vieetj<<<<", ""}, {Telex, stdOpts, "vieetj<t", "việt"}, {Telex, stdOpts, "VIEETJ<", "VIỆ"},
		{Telex, stdOpts, "tieengs<", "tiến"}, {Telex, stdOpts, "hoaf<", "hò"}, {Telex, stdOpts, "hoanf<", "hòa"},
		{Telex, nw, "hoanf<", "hoà"}, {Telex, stdOpts, "nguwowfi<", "ngườ"}, {Telex, stdOpts, "chuyeenr<", "chuyể"},
		{Telex, stdOpts, "aa<<", ""}, {Telex, stdOpts, "dd<", ""}, {Telex, stdOpts, "<", ""}, {Telex, stdOpts, "ab <c", "abc"},
		{VNI, stdOpts, "viet65<", "việ"}, {VNI, stdOpts, "d9<", ""}, {Telex2, stdOpts, "www<", "w"},
		{Telex, stdOpts, "hoaf<<<", ""}, {Telex, stdOpts, "xin chaof<", "xin chà"},
	} {
		if got := typeKeys(t, c.v, c.o, c.keys); got != c.want {
			t.Errorf("%s %q => %q want %q", c.v.Name(), c.keys, got, c.want)
		}
	}
	e := New(Telex, stdOpts)
	if _, ok := e.Backspace(); ok {
		t.Error("Backspace on an empty word must pass through")
	}
}

func TestAckShown(t *testing.T) {
	e := New(Telex, stdOpts)
	e.Key('v')
	e.Key('i')
	e.Key('e') // reported "vie", but the app skipped the last insert
	e.AckShown("vi")
	ed, _ := e.Key('e')
	if ed != (Edit{0, "ê"}) {
		t.Errorf("edit after ack = %+v, want insert ê", ed)
	}
	if got := Apply("vi", ed); got != e.Preedit() {
		t.Errorf("ack-relative edit gave %q, preedit %q", got, e.Preedit())
	}
}

func TestPassThroughAndCommit(t *testing.T) {
	e := New(Telex, stdOpts)
	for _, r := range " .,1;(" {
		if _, ok := e.Key(r); ok {
			t.Errorf("%q at word start must pass through", r)
		}
	}
	e.Key('a')
	e.Key('a')
	if got := e.Commit(); got != "â" {
		t.Errorf("commit = %q", got)
	}
	if e.Preedit() != "" {
		t.Error("commit must reset")
	}
	if ed, ok := e.Key('z'); !ok || ed.Insert == "" {
		t.Errorf("z starts a word in Telex: %+v %v", ed, ok)
	}
	e.Configure(VNI, stdOpts)
	if e.Preedit() != "" {
		t.Error("Configure must reset")
	}
	if New(Variant(99), stdOpts).v != Telex || Variant(99).Name() != "" {
		t.Error("out-of-range variant falls back to Telex")
	}
}

func isNFCish(s string) bool {
	for _, r := range s {
		if unicode.Is(unicode.Mn, r) {
			return false
		}
	}
	return true
}

func TestUndoAndEscape(t *testing.T) { // double key undoes its effect; AutoRestore off so the raw result shows
	o := Options{FreeTone: true, SpellCheck: true}
	for _, c := range []struct {
		v    Variant
		keys string
		want string
	}{{Telex, "aa", "â"}, {Telex, "aaa", "aa"}, {Telex, "aww", "aw"}, {Telex, "dddi", "ddi"}, {Telex2, "www", "ww"},
		{VNI, "a66", "a6"}, {VNI, "a88", "a8"}, {VIQR, "a^^", "a^"}, {Telex, "vieetjz", "viêt"}} {
		if got := typeKeys(t, c.v, o, c.keys); got != c.want {
			t.Errorf("%s %q => %q want %q", c.v.Name(), c.keys, got, c.want)
		}
	}
}

func TestRaw(t *testing.T) {
	for _, keys := range []string{"as", "test", "windows", "Tieengs", "vieetj"} {
		e := New(Telex, stdOpts)
		for _, k := range keys {
			e.Key(k)
		}
		if got := e.Raw(); got != keys {
			t.Errorf("Raw(%q) = %q", keys, got)
		}
		e.Reset()
		if e.Raw() != "" {
			t.Fatal("Reset retained raw text")
		}
	}
	e := New(Telex, stdOpts)
	e.Key('a')
	e.Key('s')
	e.Backspace()
	if e.Raw() != "" {
		t.Fatalf("Backspace left raw text: %q", e.Raw())
	}
}

func TestRawQuickTelex(t *testing.T) {
	e := New(Telex, Options{QuickTelex: true})
	for _, k := range "access" {
		e.Key(k)
	}
	if got := e.Raw(); got != "access" {
		t.Fatalf("Quick Telex lost input: %q", got)
	}
	if e.Raw() == e.Preedit() {
		t.Fatal("test did not exercise a transformation")
	}
}

func TestBackspaceRaw(t *testing.T) {
	e := New(Telex, stdOpts)
	e.Key('a')
	e.Key('s')
	e.AckShown("as")
	ed, ok := e.BackspaceRaw()
	if !ok || Apply("as", ed) != "a" || e.Raw() != "a" || e.Preedit() != "a" {
		t.Fatalf("English Backspace: %+v, %q, %q", ed, e.Raw(), e.Preedit())
	}
	e.BackspaceRaw()
	if _, ok := e.BackspaceRaw(); ok {
		t.Fatal("consumed empty Backspace")
	}
}

func TestVIQREscape(t *testing.T) {
	for keys, want := range map[string]string{
		`a\.`: "a.", `a\'`: "a'", `a\?`: "a?", `a\^`: "a^", `a\(`: "a(",
		`a\\`: `a\`, `\d`: "d", `a\`: `a\`, `a.`: "ạ",
	} {
		if got := typeKeys(t, VIQR, Options{FreeTone: true}, keys); got != want {
			t.Errorf("VIQR %q -> %q, want %q", keys, got, want)
		}
	}
	e := New(VIQR, Options{})
	e.Key('a')
	e.Key('\\')
	e.Backspace()
	e.Key('.')
	if e.Preedit() != "ạ" {
		t.Fatalf("escape state survived Backspace: %q", e.Preedit())
	}
	e.Reset()
	e.Key('\\')
	e.Reset()
	e.Key('a')
	e.Key('.')
	if e.Preedit() != "ạ" {
		t.Fatal("escape state survived Reset")
	}
}

func TestMSShiftedTones(t *testing.T) {
	for _, pair := range []string{"5%", "6^", "7&", "8*", "9("} {
		a := typeKeys(t, MS, Options{FreeTone: true}, "a"+pair[:1])
		b := typeKeys(t, MS, Options{FreeTone: true}, "a"+pair[1:])
		if a != b || a == "a" {
			t.Errorf("MS keys %q disagree: %q / %q", pair, a, b)
		}
	}
}

func TestVIQREscapeRejectsInvalidRunes(t *testing.T) {
	e := New(VIQR, Options{})
	e.Key('\\')
	for _, r := range []rune{0, '\n', 0x7f, 0xd800, 0x110000, -1} {
		if _, ok := e.Key(r); ok {
			t.Fatalf("escaped invalid rune accepted: %U", r)
		}
		if e.Preedit() != "\\" {
			t.Fatal("invalid key modified composition")
		}
	}
}
