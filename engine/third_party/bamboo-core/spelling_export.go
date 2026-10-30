package bamboo

// SpellingTables exposes copies of the syllable tables so tests can generate words from the rules.
// first/vowel/last are space-separated rows; cv and vc say which rows may combine.
func SpellingTables() (first, vowel, last []string, cv, vc [][]int) {
	return append([]string(nil), firstConsonantSeqs...), append([]string(nil), vowelSeqs...),
		append([]string(nil), lastConsonantSeqs...), cvMatrix, vcMatrix
}
