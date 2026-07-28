// Ark snapshot: pang-010 — multiply, define_word/argument, call_stack, dont
// Ported from ark/lua/pang-010.lua
//
// NOTE: This is an historical transitional version. The define_word
// mechanism here (using the "string" keyword for unquoted names) is
// work-in-progress and may panic. See main.go for the canonical v028
// implementation using proper string literals.
package main

import (
	"bufio"
	"fmt"
	"os"
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
var callStack = []map[string]interface{}{{}}

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
		return val
	}}
	wordDefs["add"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) + evaluateWord(args[1]).(float64)
	}}
	wordDefs["multiply"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) * evaluateWord(args[1]).(float64)
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
		vars := callStack[len(callStack)-1]
		varName := words[args[0]-1]
		vars[varName] = evaluateWord(args[1])
		return nil
	}}
	wordDefs["get"] = WordDef{1, func(args []int) interface{} {
		vars := callStack[len(callStack)-1]
		varName := words[args[0]-1]
		return vars[varName]
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
	wordDefs["greater"] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) > evaluateWord(args[1]).(float64)
	}}
	wordDefs["execute_words_file"] = WordDef{1, func(args []int) interface{} {
		fileName := words[args[0]-1]
		executeWordsFile(fileName)
		return nil
	}}
	wordDefs["dont"] = WordDef{1, func(args []int) interface{} { return nil }}
	wordDefs["define_word"] = WordDef{3, func(args []int) interface{} {
		name := evaluateWord(args[0]).(string)
		arity := int(evaluateWord(args[1]).(float64))
		bodyIndex := args[2]
		wd := WordDef{
			Arity: arity,
			Fn: func(wordArgs []int) interface{} {
				valueArgs := make(map[string]interface{})
				for i, wa := range wordArgs {
					valueArgs[strconv.Itoa(i+1)] = evaluateWord(wa)
				}
				callStack = append(callStack, valueArgs)
				result := evaluateWord(bodyIndex)
				callStack = callStack[:len(callStack)-1]
				return result
			},
		}
		wordDefs[name] = wd
		return nil
	}}
	wordDefs["argument"] = WordDef{1, func(args []int) interface{} {
		frame := callStack[len(callStack)-1]
		argIndex := strconv.Itoa(int(evaluateWord(args[0]).(float64)))
		return frame[argIndex]
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
	if !ok || (wordIndex > 1 && (words[wordIndex-2] == "string" || words[wordIndex-2] == "define_word" || words[wordIndex-2] == "set")) {
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

func executeProgram(pnProgram string) {
	wordsBefore := len(words)
	for _, w := range strings.Fields(pnProgram) {
		words = append(words, w)
	}
	if len(words) == wordsBefore {
		return
	}
	evaluateWord(wordsBefore + 1)
}

func executeWordsFile(fileName string) {
	data, err := os.ReadFile(fileName)
	if err != nil {
		fmt.Printf("cannot open file: %s\n", fileName)
		return
	}
	executeProgram(string(data))
}

func readExecuteLoop() {
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := scanner.Text()
		if line == "exit" {
			break
		}
		executeProgram(line)
	}
}

func main() {
	wordDefsInit()

	fmt.Printf("pang version: 010\n")

	// Test square: define_word string square 1 multiply argument 1 argument 1
	executeProgram("define_word string square 1 multiply argument 1 argument 1")
	// Test factorial (recursive)
	executeProgram("define_word string factorial 1 if equal 0 argument 1 1 multiply argument 1 factorial add -1 argument 1")

	fmt.Println("square 4:")
	executeProgram("print square 4")
	fmt.Println("factorial 0:")
	executeProgram("print factorial 0")
	fmt.Println("factorial 4:")
	executeProgram("print factorial 4")

	fmt.Println("bye")
}