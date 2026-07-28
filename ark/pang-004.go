// Ark snapshot: pang-004 — while, not, equal, set/get, string, modulus, <= (FizzBuzz-capable)
// Ported from ark/lua/pang-004.lua
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
var variables = map[string]interface{}{}

func truthy(v interface{}) bool {
	if v == nil {
		return false
	}
	if b, ok := v.(bool); ok {
		return b
	}
	return true
}

func wordDefsInit() {
	wordDefs["print"] = WordDef{1, func(args []int) interface{} {
		val := evaluateWord(args[0])
		fmt.Println(val)
		return "(print returns no value)"
	}}
	wordDefs["add"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) + evaluateWord(args[1]).(float64)
	}}
	wordDefs["true"] = WordDef{0, func(args []int) interface{} { return true }}
	wordDefs["false"] = WordDef{0, func(args []int) interface{} { return false }}
	wordDefs["if"] = WordDef{3, func(args []int) interface{} {
		if truthy(evaluateWord(args[0])) {
			return evaluateWord(args[1])
		}
		return evaluateWord(args[2])
	}}
	wordDefs["while"] = WordDef{2, func(args []int) interface{} {
		for truthy(evaluateWord(args[0])) {
			evaluateWord(args[1])
		}
		return nil
	}}
	wordDefs["not"] = WordDef{1, func(args []int) interface{} {
		return !truthy(evaluateWord(args[0]))
	}}
	wordDefs["equal"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]) == evaluateWord(args[1])
	}}
	wordDefs["set"] = WordDef{2, func(args []int) interface{} {
		varName := words[args[0]-1]
		variables[varName] = evaluateWord(args[1])
		return nil
	}}
	wordDefs["get"] = WordDef{1, func(args []int) interface{} {
		return variables[words[args[0]-1]]
	}}
	wordDefs["string"] = WordDef{1, func(args []int) interface{} {
		return words[args[0]-1]
	}}
	wordDefs["modulus"] = WordDef{2, func(args []int) interface{} {
		a := int64(evaluateWord(args[0]).(float64))
		b := int64(evaluateWord(args[1]).(float64))
		return float64(a % b)
	}}
	wordDefs["lesser_than_or_equal"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) <= evaluateWord(args[1]).(float64)
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

	// FizzBuzz demo from pang-004.lua
	pn := "do set i 1 while lesser_than_or_equal get i 20 do if equal 0 modulus get i 15 print string FizzBuzz if equal 0 modulus get i 3 print string Fizz if equal 0 modulus get i 5 print string Buzz print get i set i add get i 1 end end"
	words = strings.Fields(pn)

	fmt.Printf("pang version: 004\n")
	evaluateWord(1)
}