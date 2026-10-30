package bamboo

import "testing"

func TestRegressLoneW(t *testing.T) {
	run(t, []rc{
		{"w->ư", W, fNew, "w", "ư"}, {"ww->w", W, fNew, "ww", "w"}, {"www->ww", W, fNew, "www", "ww"},
		{"wwww", W, fNew, "wwww", "www"}, {"wa", W, fNew, "wa", "ưa"}, {"tw", W, fNew, "tw", "tư"},
		{"W", W, fNew, "W", "Ư"}, {"Ww", W, fNew, "Ww", "W"},
		{"plain-telex-w-literal", T, fNew, "w", "w"}, {"plain-telex-ww", T, fNew, "ww", "ww"},
		{"uw", W, fNew, "uw", "ư"}, {"ow", W, fNew, "ow", "ơ"}, {"aw", W, fNew, "aw", "ă"},
		{"nhuw", W, fNew, "nhuw", "như"}, {"wuw-literal-after-undo", W, fNew, "uww", "uw"},
	})
}
