// Ark snapshot: pang-002 — evaluate_word, first working interpreter (print, add, do/end)
// Ported from ark/lua/pang-002.lua
package main

import (
	"fmt"
	"strconv"
	"strings"
)

type WordFunc func(args []int) interface{}

type WordDef struct {
	Arity int
	Fn    WordFunc
}

var wordDefs = map[string]WordDef{}
var words []string

func wordDefsInit() {
	wordDefs["print"] = WordDef{1, func(args []int) interface{} {
		val := evaluateWord(args[0])
		fmt.Println(val)
		return "(print returns no value)"
	}}
	wordDefs["add"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) + evaluateWord(args[1]).(float64)
	}}
}

func phraseLength(wordIndex int) int {
	idx := wordIndex - 1
	word := words[idx]
	length := 1

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

func evaluateWord(wordIndex int) interface{} {
	idx := wordIndex - 1
	word := words[idx]

	if val, err := strconv.ParseFloat(word, 64); err == nil {
		return val
	}

	if word == "do" {
		doWordIndex := wordIndex + 1
		var evaluated interface{}
		for {
			if doWordIndex-1 >= len(words) || words[doWordIndex-1] == "end" {
				return evaluated
			}
			evaluated = evaluateWord(doWordIndex)
			doWordIndex += phraseLength(doWordIndex)
		}
	}

	def, ok := wordDefs[word]
	if !ok {
		fmt.Printf("word:%s definition not found\n", word)
		return nil
	}

	args := make([]int, def.Arity)
	argWordIndex := wordIndex + 1
	for i := 0; i < def.Arity; i++ {
		args[i] = argWordIndex
		argWordIndex += phraseLength(argWordIndex)
	}

	return def.Fn(args)
}

func main() {
	wordDefsInit()

	pn := "do print 2 print add 1 2 print 4 end"
	words = strings.Fields(pn)

	fmt.Printf("pang version: 002\n")
	evaluateWord(1)
}