Upstream: https://github.com/BambooEngine/bamboo-core (MIT) at b2e49a2.
Local changes = patches/*.patch (tone refresh on Backspace, lone-w undo, EautoRestore).
Dropped from the scratch tree: corpus_test.go + testdata/words.txt (word list of unknown license).
Patch 0004 (EnoSpellCheck) adds the flag used by SpellCheck=off and DdAnywhere in engine/.
Patch 0005 adds spelling_export.go (SpellingTables) for the engine corpus test.
Patch 0006 fixes tone placement on uyê in the MS layout (directly typed ê).
