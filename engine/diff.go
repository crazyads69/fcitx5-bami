// Package engine is the pure-Go core of bami: no cgo, no I/O, no timers.
package engine

// Edit is what the app must do to turn the old text into the new one.
type Edit struct {
	DeleteN int    // runes to delete before the cursor
	Insert  string // NFC text to commit after the delete
}

// Diff keeps the longest common rune prefix, deletes the rest of old, inserts the rest of new.
func Diff(old, new string) Edit {
	o, n := []rune(old), []rune(new)
	i := 0
	for i < len(o) && i < len(n) && o[i] == n[i] {
		i++
	}
	return Edit{DeleteN: len(o) - i, Insert: string(n[i:])}
}

// Apply runs an Edit on s; used by tests to check Apply(a, Diff(a, b)) == b.
func Apply(s string, e Edit) string {
	r := []rune(s)
	return string(r[:len(r)-e.DeleteN]) + e.Insert
}
