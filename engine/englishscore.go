package engine

import (
	_ "embed"
	"encoding/binary"
	"math"
)

//go:embed models/english-v1.bin
var englishModelData []byte

var englishModel = func() (model struct {
	bias, threshold float64
	weights         [4096]float32
}) {
	if len(englishModelData) != 4*(len(model.weights)+2) {
		panic("invalid embedded English model")
	}
	value := func(i int) float32 {
		return math.Float32frombits(binary.LittleEndian.Uint32(englishModelData[4*i:]))
	}
	model.bias, model.threshold = float64(value(0)), float64(value(1))
	for i := range model.weights {
		model.weights[i] = value(i + 2)
	}
	return
}()

// EnglishThreshold is selected on validation data, separately from the held-out test set.
func EnglishThreshold() float64 { return englishModel.threshold }

// EnglishScore is a local classifier score, not a probability of the user's intent.
// Unsupported inputs return a neutral score. Inference makes no allocations.
func EnglishScore(word string) float64 {
	if len(word) < 4 || len(word) > 32 {
		return 0.5
	}
	var text [34]byte
	text[0], text[len(word)+1] = '^', '$'
	for i := range word {
		c := word[i]
		if c >= 'A' && c <= 'Z' {
			c += 'a' - 'A'
		}
		if c < 'a' || c > 'z' {
			return 0.5
		}
		text[i+1] = c
	}
	sum, count := 0.0, 0
	for n := 2; n <= 4; n++ {
		for start := 0; start+n <= len(word)+2; start++ {
			hash := uint32(2166136261)
			for _, c := range text[start : start+n] {
				hash = (hash ^ uint32(c)) * 16777619
			}
			sum += float64(englishModel.weights[hash%4096])
			count++
		}
	}
	return 1 / (1 + math.Exp(-(englishModel.bias + sum/math.Sqrt(float64(count)))))
}

// PredictEnglish applies the model only to the Telex layouts it was trained on.
func (e *Engine) PredictEnglish() float64 {
	if !e.def.telex {
		return 0.5
	}
	return EnglishScore(e.Raw())
}
