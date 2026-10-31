// SPDX-License-Identifier: MIT
// Emit synthetic Telex examples, grouped by untoned syllable for leakage-free splits.
package main

import (
	"bufio"
	"fmt"
	bamboo "github.com/crazyads69/fcitx5-bami/engine/third_party/bamboo-core"
	"os"
	"strings"
)

func main() {
	first, vowel, last, cv, vc := bamboo.SpellingTables()
	replace := strings.NewReplacer("ă", "aw", "â", "aa", "ê", "ee", "ô", "oo", "ơ", "ow", "ư", "uw", "đ", "dd")
	be := bamboo.NewEngine(bamboo.ParseInputMethod(bamboo.InputMethodDefinitions, "Telex"), bamboo.EfreeToneMarking|bamboo.EstdToneStyle|bamboo.EautoCorrectEnabled)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	seen := map[string]bool{}
	for fi := -1; fi < len(first); fi++ {
		onsets := []string{""}
		if fi >= 0 {
			onsets = strings.Fields(first[fi])
		}
		for vi, row := range vowel {
			allowed := fi < 0
			if fi >= 0 {
				for _, v := range cv[fi] {
					allowed = allowed || v == vi
				}
			}
			if !allowed {
				continue
			}
			codas := []string{""}
			for _, li := range vc[vi] {
				codas = append(codas, strings.Fields(last[li])...)
			}
			for _, onset := range onsets {
				for _, nucleus := range strings.Fields(row) {
					for _, coda := range codas {
						group := onset + nucleus + coda
						for _, tone := range []string{"", "s", "f", "r", "x", "j"} {
							keys := replace.Replace(group) + tone
							if seen[keys] {
								continue
							}
							be.Reset()
							be.ProcessString(keys, bamboo.VietnameseMode)
							if !be.IsValid(true) {
								continue
							}
							seen[keys] = true
							fmt.Fprintf(out, "%s\t%s\n", group, keys)
						}
					}
				}
			}
		}
	}
	if len(seen) < 10000 {
		panic("unexpectedly small Vietnamese corpus")
	}
}
