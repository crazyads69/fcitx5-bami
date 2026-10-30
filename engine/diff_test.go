package engine

import (
	"math/rand"
	"testing"
)

func TestDiff(t *testing.T) {
	cases := []struct {
		old, new string
		want     Edit
	}{
		{"viê", "việ", Edit{1, "ệ"}},
		{"viet", "việt", Edit{2, "ệt"}},
		{"a", "a", Edit{0, ""}},
		{"", "tiếng", Edit{0, "tiếng"}},
		{"đ", "", Edit{1, ""}},
	}
	for _, c := range cases {
		if got := Diff(c.old, c.new); got != c.want {
			t.Errorf("Diff(%q,%q)=%+v want %+v", c.old, c.new, got, c.want)
		}
	}
}

func TestDiffApplyProperty(t *testing.T) {
	alphabet := []rune("aăâeêioôơuưyđsfrxjzáàảãạếệ")
	rnd := rand.New(rand.NewSource(1))
	gen := func() string {
		b := make([]rune, rnd.Intn(8))
		for i := range b {
			b[i] = alphabet[rnd.Intn(len(alphabet))]
		}
		return string(b)
	}
	for i := 0; i < 5000; i++ {
		a, b := gen(), gen()
		if got := Apply(a, Diff(a, b)); got != b {
			t.Fatalf("Apply(%q, Diff)=%q want %q", a, got, b)
		}
	}
}
