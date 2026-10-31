package engine

import bamboo "github.com/crazyads69/fcitx5-bami/engine/third_party/bamboo-core"

// Variant selects an input method. The numbers are part of the C API (see bridge/bami.go).
type Variant int

const (
	Telex Variant = iota
	Telex2
	TelexW
	SimpleTelex1
	SimpleTelex2
	VNI
	VIQR
	MS
	TelexVNI
	numVariants
)

// Gate controls a context-sensitive key: lone w -> ư, and the brackets [ -> ơ, ] -> ư.
type Gate uint8

const (
	GateDefault     Gate = iota // use the variant's default
	GateOff                     // key is literal
	GateAfterLetter             // transforms only once a letter of the word was typed
	GateEverywhere              // transforms even at the start of a word
)

// variantDef is pure data: a bamboo table name plus the default gate values.
type variantDef struct {
	name    string
	base    string // key into bamboo.InputMethodDefinitions
	telex   bool   // gates apply (w and brackets are added to the base table)
	locked  bool   // Simple Telex: user gate overrides are ignored
	loneW   Gate
	bracket Gate
}

var variantDefs = [numVariants]variantDef{
	Telex:        {name: "Telex", base: "Telex", telex: true, loneW: GateOff, bracket: GateOff},
	Telex2:       {name: "Telex 2", base: "Telex", telex: true, loneW: GateEverywhere, bracket: GateEverywhere},
	TelexW:       {name: "Telex W", base: "Telex", telex: true, loneW: GateEverywhere, bracket: GateOff},
	SimpleTelex1: {name: "Simple Telex 1", base: "Telex", telex: true, locked: true, loneW: GateOff, bracket: GateOff},
	SimpleTelex2: {name: "Simple Telex 2", base: "Telex", telex: true, locked: true, loneW: GateEverywhere, bracket: GateOff},
	VNI:          {name: "VNI", base: "VNI"},
	VIQR:         {name: "VIQR", base: "VIQR"},
	MS:           {name: "Microsoft layout", base: "Microsoft layout"}, // [ -> ư, ] -> ơ (swapped vs Telex)
	TelexVNI:     {name: "Telex + VNI", base: "Telex + VNI", telex: true, loneW: GateOff, bracket: GateOff},
}

// Name is the display name of v ("" when out of range).
func (v Variant) Name() string {
	if v < 0 || v >= numVariants {
		return ""
	}
	return variantDefs[v].name
}

// gates resolves the effective lone-w and bracket gates for v under the user's overrides.
func (d variantDef) gates(o Options) (loneW, bracket Gate) {
	loneW, bracket = d.loneW, d.bracket
	if d.locked || !d.telex {
		return
	}
	if o.LoneW != GateDefault {
		loneW = o.LoneW
	}
	if o.Bracket != GateDefault {
		bracket = o.Bracket
	}
	return
}

// table builds the bamboo rule table for the variant.
func (d variantDef) table(loneW, bracket Gate) bamboo.InputMethodDefinition {
	t := bamboo.InputMethodDefinition{}
	for k, v := range bamboo.InputMethodDefinitions[d.base] {
		t[k] = v
	}
	if d.base == "Microsoft layout" {
		for _, pair := range []string{"5%", "6^", "7&", "8*", "9("} {
			t[pair[1:]] = t[pair[:1]]
		}
	}
	if d.telex && loneW != GateOff {
		t["w"] += "__Ư" // bare w -> ư when no mark rule applies
	}
	if d.telex && bracket != GateOff {
		t["["], t["]"], t["{"], t["}"] = "__ơ", "__ư", "_Ơ", "_Ư"
	}
	return t
}
