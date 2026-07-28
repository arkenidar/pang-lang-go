// Ark snapshot: pang-001 — phrase_length with do/end block support
// Ported from ark/lua/pang-001.lua
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

	// do ... end blocks
	if word == "do" {
		for {
			endIdx := wordIndex + length - 1
			if endIdx >= len(words) || words[endIdx] == "end" {
				return length + 1
			}
			length += phraseLength(wordIndex + length)
		}
	}

	if _, err := strconv.ParseFloat(word, 64); err == nil {
		return 1
	}

	def, ok := wordDefs[word]
	if !ok {
		return 1
	}

	for i := 0; i < def.Arity; i++ {
		length += phraseLength(wordIndex + length)
	}
	return length
}

func main() {
	pn := "print 1 do print add 1 2 print 4 end"
	words = strings.Fields(pn)

	fmt.Printf("pang version: 001\n")
	fmt.Println(phraseLength(3)) // should be 8 (do print add 1 2 print 4 end)
}