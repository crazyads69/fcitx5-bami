package engine

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"math"
	"os"
	"strings"
	"testing"
)

func TestEnglishModelArtifact(t *testing.T) {
	data, err := os.ReadFile("models/english-v1.json")
	if err != nil {
		t.Fatal(err)
	}
	var report struct {
		Threshold   float64
		ModelSHA256 string `json:"model_sha256"`
		Probes      map[string]float64
		Test        struct {
			FalsePositive float64 `json:"vietnamese_false_english_rate"`
		}
	}
	if err := json.Unmarshal(data, &report); err != nil {
		t.Fatal(err)
	}
	sum := sha256.Sum256(englishModelData)
	if hex.EncodeToString(sum[:]) != report.ModelSHA256 {
		t.Fatal("model and validation report do not match")
	}
	if report.Threshold != EnglishThreshold() {
		t.Fatal("threshold drift")
	}
	if report.Test.FalsePositive >= 0.005 {
		t.Fatal("held-out Vietnamese regression")
	}
	for word, want := range report.Probes {
		if got := EnglishScore(word); math.Abs(got-want) > 1e-6 {
			t.Errorf("Go/trainer inference disagree for %q: %.8f vs %.8f", word, got, want)
		}
	}
}

func TestEnglishScore(t *testing.T) {
	for _, word := range []string{"", "as", "test1", "tiếng", "bad\xff", strings.Repeat("a", 33)} {
		if score := EnglishScore(word); score != 0.5 {
			t.Errorf("unsupported %q: %v", word, score)
		}
	}
	for _, word := range []string{"vieetj", "tieengs", "nguwowif", "hoaf"} {
		if EnglishScore(word) >= EnglishThreshold() {
			t.Errorf("Vietnamese falsely preferred English: %q", word)
		}
	}
	if EnglishScore("localhost") < EnglishThreshold() {
		t.Fatal("English probe not detected")
	}
	if EnglishScore("LOCALHOST") != EnglishScore("localhost") {
		t.Fatal("case changed score")
	}
	if allocs := testing.AllocsPerRun(100, func() { EnglishScore("typescript") }); allocs != 0 {
		t.Fatalf("prediction allocates: %v", allocs)
	}
	e := New(VNI, stdOpts)
	for _, k := range "localhost" {
		e.Key(k)
	}
	if e.PredictEnglish() != 0.5 {
		t.Fatal("Telex model used for VNI")
	}
}

func BenchmarkEnglishScore(b *testing.B) {
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		EnglishScore("typescript")
	}
}
