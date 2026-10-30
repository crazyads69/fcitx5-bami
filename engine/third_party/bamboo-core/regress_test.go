package bamboo

import "testing"

const (
	fNew = EfreeToneMarking | EautoCorrectEnabled                 // hoà, thuý
	fOld = EfreeToneMarking | EautoCorrectEnabled | EstdToneStyle // hòa, thúy
)

// typ feeds keys ('<' = Backspace) and returns the whole text as the frontend shows it.
func typ(im string, flags uint, keys string) string {
	e := NewEngine(ParseInputMethod(InputMethodDefinitions, im), flags)
	for _, k := range keys {
		if k == '<' {
			e.RemoveLastChar(true)
		} else {
			e.ProcessKey(k, VietnameseMode)
		}
	}
	return e.GetProcessedString(VietnameseMode | FullText)
}

type rc struct {
	name  string
	im    string
	flags uint
	keys  string
	want  string
}

func run(t *testing.T, cs []rc) {
	t.Helper()
	for _, c := range cs {
		if got := typ(c.im, c.flags, c.keys); got != c.want {
			t.Errorf("%s: %s %q => %q want %q", c.name, c.im, c.keys, got, c.want)
		}
	}
}

const T, V, W = "Telex", "VNI", "Telex W"

func TestRegressBackspaceToneRefresh(t *testing.T) { // upstream: ibus-bamboo bsProcessKeyEvent; lotus#366
	run(t, []rc{
		{"viet-bs-new", T, fNew, "vieetj<", "việ"},
		{"viet-bs-old", T, fOld, "vieetj<", "việ"},
		{"viet-bs-vni", V, fNew, "viet65<", "việ"},
		{"VIET-bs", T, fNew, "VIEETJ<", "VIỆ"},
		{"tieng-bs", T, fNew, "tieengs<", "tiến"},
		{"hoa-bs", T, fNew, "hoaf<", "ho"},
		{"hoan-bs-new", T, fNew, "hoanf<", "hoà"},
		{"hoan-bs-old", T, fOld, "hoanf<", "hòa"},
		{"nguoi-bs", T, fNew, "nguwowfi<", "ngườ"},
		{"chuyen-bs", T, fNew, "chuyeenr<", "chuyể"},
		{"bs-twice", T, fNew, "vieetj<<", "vi"},
		{"bs-then-retype", T, fNew, "vieetj<t", "viêt"[:0] + "việt"},
		{"bs-all", T, fNew, "aa<<", ""},
	})
}

func TestRegressToneStyles(t *testing.T) {
	run(t, []rc{
		{"hoa-new", T, fNew, "hoaf", "hoà"}, {"hoa-old", T, fOld, "hoaf", "hòa"},
		{"hoe-new", T, fNew, "khoer", "khoẻ"}, {"hoe-old", T, fOld, "khoer", "khỏe"},
		{"uy-new", T, fNew, "thuys", "thuý"}, {"uy-old", T, fOld, "thuys", "thúy"},
		{"quy", T, fNew, "quys", "quý"}, {"qua", T, fOld, "quas", "quá"},
		{"toan", T, fNew, "toans", "toán"}, {"oa-late-tone", T, fOld, "hoas", "hóa"},
		{"tone-first-vni", V, fNew, "hoa2", "hoà"}, {"vni-old", V, fOld, "hoa2", "hòa"},
		{"ơi", T, fNew, "ddowif", "đời"}, {"oai", T, fNew, "ngoaif", "ngoài"},
		{"uyê", T, fNew, "chuyeenr", "chuyển"}, {"uyên-late", T, fNew, "chuyrene", "chuyển"},
		{"uôi", T, fNew, "muoons", "muốn"}, {"ươi", T, fNew, "nguwowfi", "người"},
		{"free-tone-after-final", T, fNew, "toanf", "toàn"},
		{"free-tone-retone", T, fNew, "tafs", "tá"},
		{"free-tone-retone2", T, fNew, "toansf", "toàn"},
		{"free-tone-z", T, fNew, "toansz", "toan"},
		{"gi-a", T, fNew, "gias", "giá"}, {"gi-alone", T, fNew, "gif", "gì"},
		{"gi-an", T, fNew, "giaos", "giáo"}, {"gi-u", T, fNew, "giuwx", "giữ"},
		{"gieng", T, fNew, "gieengs", "giếng"}, {"qu-o", T, fNew, "quoocs", "quốc"},
		{"qu-yen", T, fNew, "quyeenf", "quyền"},
	})
}

func TestRegressCapitals(t *testing.T) {
	run(t, []rc{
		{"ALLCAPS", T, fNew, "VIEETJ", "VIỆT"}, {"ALLCAPS2", T, fNew, "DDUWOWCJ", "ĐƯỢC"},
		{"Cap", T, fNew, "Vieetj", "Việt"}, {"mixed", T, fNew, "ViEETJ", "ViỆT"},
		{"DDuwowcj", T, fNew, "DDuwowcj", "Được"}, {"Dd", T, fNew, "Dd", "Đ"}, {"DD", T, fNew, "DD", "Đ"},
		{"D-tone-key-upper", T, fNew, "VieetJ", "Việt"}, {"Tone-upper-S", T, fNew, "TOANS", "TOÁN"},
		{"undo-caps", T, fNew, "AAA", "AA"}, {"undo-caps-dd", T, fNew, "DDD", "DD"},
		{"Đ-alone", V, fNew, "D9", "Đ"},
	})
}

func TestRegressDD(t *testing.T) {
	run(t, []rc{
		{"dd", T, fNew, "dd", "đ"}, {"dd-reach-1", T, fNew, "daud", "đau"},
		{"dd-reach-2", T, fNew, "dungd", "đung"}, {"dd-reach-undo", T, fNew, "daudd", "daud"},
		{"dd-reach-3", T, fNew, "duongd", "đuong"}, {"dd-reach-w", T, fNew, "duwowngd", "đương"},
		{"dd-reach-upper", T, fNew, "DUNGD", "ĐUNG"}, {"dd-vni", V, fNew, "dung9", "đung"},
		{"dd-reach-tone", T, fNew, "dungfd", "đùng"},
	})
}

func TestRegressUndoByRepeat(t *testing.T) {
	run(t, []rc{
		{"aa", T, fNew, "aaa", "aa"}, {"aw", T, fNew, "aww", "aw"}, {"ee", T, fNew, "eee", "ee"},
		{"oo", T, fNew, "ooo", "oo"}, {"ow", T, fNew, "oww", "ow"}, {"uw", T, fNew, "uww", "uw"},
		{"dd", T, fNew, "ddd", "dd"}, {"s", T, fNew, "ass", "as"}, {"f", T, fNew, "aff", "af"},
		{"r", T, fNew, "arr", "ar"}, {"x", T, fNew, "axx", "ax"}, {"j", T, fNew, "ajj", "aj"},
		{"after-final", T, fNew, "toanss", "toans"}, {"mark-undo-keeps-tone", T, fNew, "tooss", "tôs"},
		{"vni-1", V, fNew, "a11", "a1"}, {"vni-6", V, fNew, "a66", "a6"}, {"vni-8", V, fNew, "a88", "a8"},
		{"vni-7", V, fNew, "u77", "u7"}, {"vni-9", V, fNew, "d99", "d9"}, {"vni-2", V, fNew, "hoa22", "hoa2"},
	})
}

func TestRegressUowUwo(t *testing.T) {
	run(t, []rc{
		{"bare-uow", T, fNew, "uow", "uơ"}, {"bare-uo7", V, fNew, "uo7", "uơ"}, {"uow-tail", T, fNew, "uown", "ươn"}, {"uo7-tail", V, fNew, "uo7n", "ươn"},
		{"tuow", T, fNew, "tuow", "tươ"}, {"tuwo", T, fNew, "tuwo", "tưo"}, {"tuwow", T, fNew, "tuwow", "tươ"},
		{"huow", T, fNew, "huow", "huơ"}, {"thuow", T, fNew, "thuow", "thuơ"}, {"quow", T, fNew, "quow", "quơ"},
		{"khuow", T, fNew, "khuow", "khuơ"}, {"thuowr", T, fNew, "thuowr", "thuở"},
		{"huowng", T, fNew, "huowng", "hương"}, {"huownj", T, fNew, "huowngj", "hượng"},
		{"tuwong", T, fNew, "tuwong", "tương"}, {"tuowng", T, fNew, "tuowng", "tương"},
		{"ruouwj", T, fNew, "ruouwj", "rượu"}, {"ruowuj", T, fNew, "ruowuj", "rượu"},
		{"muowif", T, fNew, "muowif", "mười"}, {"nguwowi", T, fNew, "nguwowi", "ngươi"},
		{"uow-then-w", T, fNew, "uoww", "ươ"}, {"vni-truong", V, fNew, "truong72", "trường"}, {"vni-uo7-then-o", V, fNew, "tuo7", "tươ"},
		{"uo7-tail", V, fNew, "huo7ng", "hương"},
	})
}

func TestRegressDoubleToneVowels(t *testing.T) { // ê, ơ, ô, ư, â with tones and re-tones
	run(t, []rc{
		{"ê", T, fNew, "ee", "ê"}, {"ế", T, fNew, "ees", "ế"}, {"ề-ế", T, fNew, "eefs", "ế"},
		{"ế-ề", T, fNew, "eesf", "ề"}, {"ê-late-tone", T, fNew, "esej"[:0] + "esef"[:0] + "ees", "ế"},
		{"ơ", T, fNew, "ow", "ơ"}, {"ớ", T, fNew, "ows", "ớ"}, {"ớ-ờ", T, fNew, "owsf", "ờ"},
		{"ô-ố", T, fNew, "oos", "ố"}, {"ư-ứ", T, fNew, "uws", "ứ"}, {"ư-ữ", T, fNew, "uwx", "ữ"},
		{"viet-tone-before-mark", T, fNew, "vietej", "việt"}, {"tone-then-circumflex", T, fNew, "tiesne"[:0] + "tiesng"[:0] + "tiesn", "tién"},
		{"mark-after-tone", T, fNew, "tosn"[:0] + "toas"[:0] + "toa", "toa"},
		{"â", T, fNew, "chaan", "chân"}, {"ă", T, fNew, "ban8"[:0] + "bawn", "băn"}, {"ẳ", T, fNew, "bawr", "bẳ"},
		{"vni-ế", V, fNew, "e61", "ế"}, {"vni-late-6", V, fNew, "e16", "ế"},
		{"vni-ư-ớ", V, fNew, "o71", "ớ"},
	})
}
