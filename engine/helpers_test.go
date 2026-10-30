package engine

import bamboo "github.com/crazyads69/fcitx5-bami/engine/third_party/bamboo-core"

func bambooDef(name string) bamboo.InputMethodDefinition { return bamboo.InputMethodDefinitions[name] }
