// Ark snapshot: pang-000 — phrase_length with arity-based word definitions
// Ported from ark/lua/pang-000.lua
package main

import (
	"fmt"
	"strconv"
	"strings"
)

type WordDef struct {
	Arity int
}

var wordDefs = map[string]WordDef{
	"print": {1},
	"add":   {2},
}

var words []string

func phraseLength(wordIndex int) int {
	idx := wordIndex - 1
	word := words[idx]
	length := 1

	if _, err := strconv.ParseFloat(word, 64); err == nil {
		return 1
	}

	def, ok := wordDefs[word]
	if !ok {
		return 1
	}

	argumentLength := def.Arity
	for i := 0; i < argumentLength; i++ {
		length += phraseLength(wordIndex + length)
	}
	return length
}

func main() {
	pn := "print 1 print add 1 2 print 4"
	words = strings.Fields(pn)

	fmt.Printf("pang version: 000\n")
	fmt.Println(phraseLength(3)) // expect 4
}