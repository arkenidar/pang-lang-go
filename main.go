// Pang: Polish notation language interpreter (v028) — Go port
//
// Directly ported from pangea/src/pangea1/main.lua (canonical v028, 561 LOC).
// Structural reorganization: word definitions scattered throughout main.lua
// are gathered into wordDefsInit() for Go's compilation model.
// For a Lua file matching this Go structure 1:1, see main_go_structured.lua.
//
// Provenance chain:
//   pangea/src/pangea1/main.lua → main.go → main_go_structured.lua
//   (canonical v028)              (Go port)  (Lua structural mirror)
//   pangea/ark/lua/latest.lua — near-canonical subset (no file_directory_stack etc.)
//   pangea/ark/lua/pang-028.lua — historical snapshot (legacy `:` string syntax)
package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

// =============================================================================
// Types
// =============================================================================

type WordFunc func(args []int) interface{}

type WordDef struct {
	Arity int
	Fn    WordFunc
}

// =============================================================================
// Global State
// =============================================================================

var wordDefs = map[string]WordDef{}
var words []string
var stringLiterals []bool // true if word is a string literal
var callStack = []map[string]interface{}{{}}
var fileDirectoryStack []string

// Italian translation support
var language string
var translateItalian = map[string]string{
	"pang version: ":      "pang versione: ",
	"exit":                "esci",
	"print":               "stampa",
	"define_word":         "definisci_parola",
	"multiply":            "moltiplica",
	"argument":            "argomento",
	"do":                  "fai",
	"end":                 "fine",
	"set":                 "metti",
	"get":                 "prendi",
	"variable_set":        "metti_variabile",
	"variable_get":        "prendi_variabile",
	"caller_set":          "metti_chiamante",
	"caller_get":          "prendi_chiamante",
	"while":               "mentre",
	"not":                 "non",
	"greater":             "maggiore",
	"if":                  "se",
	"equal":               "uguale",
	"modulus":             "modulo",
	"string":              "stringa",
	"add":                 "somma",
	"true":                "vero",
	"false":               "falso",
	"dont":                "non_fare",
	"word:":               "parola:",
	" definition not found": " definizione non trovata",
	"command_prompt":      "richiesta_comandi",
	"read_text":           "leggi_testo",
	"to_number":           "numero_da_testo",
	"repeat":              "ripeti",
	"increment":           "incrementa",
	"and":                 "e",
	"or":                  "o",
}

func tr(s string) string {
	if language == "italian" {
		if t, ok := translateItalian[s]; ok {
			return t
		}
		fmt.Printf("can't translate: %s\n", s)
		return s
	}
	return s
}

func truthy(v interface{}) bool {
	if v == nil {
		return false
	}
	if b, ok := v.(bool); ok {
		return b
	}
	return true
}

// =============================================================================
// File path utilities (port of Lua path functions)
// =============================================================================

func pathIsAbsolute(path string) bool {
	if strings.HasPrefix(path, "/") {
		return true
	}
	if len(path) >= 2 && path[1] == ':' && (path[0] >= 'A' && path[0] <= 'Z' || path[0] >= 'a' && path[0] <= 'z') {
		return true
	}
	return false
}

func pathDirname(path string) string {
	normalized := filepath.ToSlash(path)
	dir := filepath.Dir(normalized)
	if dir == "" || dir == "." {
		return "."
	}
	return dir
}

func resolveWordsFileName(fileName string) string {
	if pathIsAbsolute(fileName) {
		return fileName
	}
	if len(fileDirectoryStack) == 0 {
		return fileName
	}
	return fileDirectoryStack[len(fileDirectoryStack)-1] + "/" + fileName
}

// =============================================================================
// Built-in Word Definitions
// =============================================================================

func wordDefsInit() {
	// print <printable>
	wordDefs[tr("print")] = WordDef{1, func(args []int) interface{} {
		val := evaluateWord(args[0])
		fmt.Println(val)
		return val
	}}

	// read_text
	wordDefs[tr("read_text")] = WordDef{0, func(args []int) interface{} {
		reader := bufio.NewReader(os.Stdin)
		text, err := reader.ReadString('\n')
		if err != nil {
			return ""
		}
		return strings.TrimRight(text, "\r\n")
	}}

	// to_number <text>
	wordDefs[tr("to_number")] = WordDef{1, func(args []int) interface{} {
		val := evaluateWord(args[0])
		if s, ok := val.(string); ok {
			n, err := strconv.ParseFloat(s, 64)
			if err != nil {
				return float64(0)
			}
			return n
		}
		return val
	}}

	// add <number> <number>
	wordDefs[tr("add")] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) + evaluateWord(args[1]).(float64)
	}}

	// multiply <number> <number>
	wordDefs[tr("multiply")] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) * evaluateWord(args[1]).(float64)
	}}

	// true
	wordDefs[tr("true")] = WordDef{0, func(args []int) interface{} {
		return true
	}}

	// false
	wordDefs[tr("false")] = WordDef{0, func(args []int) interface{} {
		return false
	}}

	// if <condition> <if true> <if false>
	wordDefs[tr("if")] = WordDef{3, func(args []int) interface{} {
		if truthy(evaluateWord(args[0])) {
			return evaluateWord(args[1])
		}
		return evaluateWord(args[2])
	}}

	// while <condition> <do while true>
	wordDefs[tr("while")] = WordDef{2, func(args []int) interface{} {
		for truthy(evaluateWord(args[0])) {
			result := evaluateWord(args[1])
			if s, ok := result.(string); ok && s == "break" {
				break
			}
		}
		return nil
	}}

	// repeat <times_count> <deferred_code>
	wordDefs[tr("repeat")] = WordDef{2, func(args []int) interface{} {
		total := evaluateWord(args[0])
		n, ok := total.(float64)
		if !ok {
			return nil
		}
		var result interface{}
		for i := int64(0); i < int64(n); i++ {
			result = evaluateWord(args[1])
		}
		return result
	}}

	// not <boolean>
	wordDefs[tr("not")] = WordDef{1, func(args []int) interface{} {
		return !truthy(evaluateWord(args[0]))
	}}

	// and <boolean> <boolean>
	wordDefs[tr("and")] = WordDef{2, func(args []int) interface{} {
		return truthy(evaluateWord(args[0])) && truthy(evaluateWord(args[1]))
	}}

	// or <boolean> <boolean>
	wordDefs[tr("or")] = WordDef{2, func(args []int) interface{} {
		return truthy(evaluateWord(args[0])) || truthy(evaluateWord(args[1]))
	}}

	// equal <first> <second>
	wordDefs[tr("equal")] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]) == evaluateWord(args[1])
	}}

	// set <variable name> <value>
	wordDefs[tr("set")] = WordDef{2, func(args []int) interface{} {
		vars := callStack[len(callStack)-1]
		varName := evaluateWord(args[0]).(string)
		vars[varName] = evaluateWord(args[1])
		return nil
	}}

	// get <variable name>
	wordDefs[tr("get")] = WordDef{1, func(args []int) interface{} {
		vars := callStack[len(callStack)-1]
		varName := evaluateWord(args[0]).(string)
		val, ok := vars[varName]
		if !ok {
			fmt.Println("nil returning from get_function()")
			return nil
		}
		return val
	}}

	// variable_set <namespace> <variable name> <value>
	wordDefs[tr("variable_set")] = WordDef{3, func(args []int) interface{} {
		namespace := evaluateWord(args[0]).(map[string]interface{})
		varName := evaluateWord(args[1]).(string)
		namespace[varName] = evaluateWord(args[2])
		return nil
	}}

	// variable_get <namespace> <variable name>
	wordDefs[tr("variable_get")] = WordDef{2, func(args []int) interface{} {
		namespace := evaluateWord(args[0]).(map[string]interface{})
		varName := evaluateWord(args[1]).(string)
		return namespace[varName]
	}}

	// namespace — returns current call stack frame
	wordDefs["namespace"] = WordDef{0, func(args []int) interface{} {
		return callStack[len(callStack)-1]
	}}

	// modulus <dividend> <divisor>
	wordDefs[tr("modulus")] = WordDef{2, func(args []int) interface{} {
		a := int64(evaluateWord(args[0]).(float64))
		b := int64(evaluateWord(args[1]).(float64))
		return float64(a % b)
	}}

	// greater <lesser> <greater>
	wordDefs[tr("greater")] = WordDef{2, func(args []int) interface{} {
		return evaluateWord(args[0]).(float64) > evaluateWord(args[1]).(float64)
	}}

	// ? — list word definitions
	wordDefs["?"] = WordDef{0, func(args []int) interface{} {
		for word, wd := range wordDefs {
			fmt.Printf("%s<%d ", word, wd.Arity)
		}
		fmt.Println()
		return nil
	}}

	// ! — execute_words_file <filename>
	// (reads raw word text, like the Lua version uses words[arguments[1]])
	wordDefs["!"] = WordDef{1, func(args []int) interface{} {
		fileName := words[args[0]-1]
		executeWordsFile(fileName)
		return nil
	}}

	// dont <skip this>
	wordDefs[tr("dont")] = WordDef{1, func(args []int) interface{} {
		return nil
	}}

	// define_word <name> <arity> <action>
	wordDefs[tr("define_word")] = WordDef{3, func(args []int) interface{} {
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

	// argument <argument index>
	wordDefs[tr("argument")] = WordDef{1, func(args []int) interface{} {
		frame := callStack[len(callStack)-1]
		argIndex := strconv.Itoa(int(evaluateWord(args[0]).(float64)))
		return frame[argIndex]
	}}

	// command_prompt — REPL
	wordDefs[tr("command_prompt")] = WordDef{0, func(args []int) interface{} {
		readExecuteLoop()
		return nil
	}}

	// increment <variable name>
	wordDefs[tr("increment")] = WordDef{1, func(args []int) interface{} {
		vars := callStack[len(callStack)-1]
		varName := evaluateWord(args[0]).(string)
		val, ok := vars[varName].(float64)
		if !ok {
			val = 0
		}
		val++
		vars[varName] = val
		return val
	}}
}

// =============================================================================
// Tokenizer / Lexer (port of program_words)
// =============================================================================

// programWords tokenizes pnProgram into words and stringLiterals slices.
// Handles quoted string literals with escape sequences.
func programWords(pnProgram string) {
	token := ""
	inString := false
	escaping := false

	flushToken := func() {
		if token != "" {
			words = append(words, token)
			stringLiterals = append(stringLiterals, false)
			token = ""
		}
	}

	flushQuoted := func(quoted string) {
		words = append(words, quoted)
		stringLiterals = append(stringLiterals, true)
	}

	// Process character by character.
	var quoted strings.Builder
	for _, ch := range pnProgram {
		c := string(ch)
		if inString {
			if escaping {
				switch c {
				case `"`:
					quoted.WriteString(`"`)
				case `\`:
					quoted.WriteString(`\`)
				case "n":
					quoted.WriteString("\n")
				case "t":
					quoted.WriteString("\t")
				default:
					fmt.Fprintf(os.Stderr, "invalid escape sequence: \\%s\n", c)
				}
				escaping = false
			} else if c == `\` {
				escaping = true
			} else if c == `"` {
				flushQuoted(quoted.String())
				quoted.Reset()
				inString = false
			} else {
				quoted.WriteString(c)
			}
		} else if isWhitespace(ch) {
			flushToken()
		} else if c == `"` {
			flushToken()
			inString = true
		} else {
			token += c
		}
	}

	if escaping {
		fmt.Fprintln(os.Stderr, "unterminated escape sequence in string literal")
	}
	if inString {
		fmt.Fprintln(os.Stderr, "unterminated string literal")
	}

	flushToken()
}

func isWhitespace(ch rune) bool {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'
}

// hashbangRemove strips a hashbang line if present.
func hashbangRemove(program string) string {
	if strings.HasPrefix(program, "#") {
		idx := strings.Index(program, "\n")
		if idx >= 0 {
			return program[idx+1:]
		}
		return ""
	}
	return program
}

// =============================================================================
// Core Evaluator
// =============================================================================

func phraseLength(wordIndex int) int {
	idx := wordIndex - 1
	word := words[idx]
	length := 1

	// String literals always have length 1.
	if stringLiterals[idx] {
		return 1
	}

	// do ... end blocks
	if word == tr("do") {
		for {
			lookAhead := wordIndex + length - 1
			if lookAhead >= len(words) || words[lookAhead] == tr("end") {
				return length + 1
			}
			length += phraseLength(wordIndex + length)
		}
	}

	// Numbers have length 1.
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

	// String literals return their raw value.
	if stringLiterals[idx] {
		return word
	}

	// Number literals.
	if val, err := strconv.ParseFloat(word, 64); err == nil {
		return val
	}

	// do ... end blocks
	if word == tr("do") {
		doWordIndex := wordIndex + 1
		var evaluated interface{}
		for {
			if doWordIndex-1 >= len(words) || words[doWordIndex-1] == tr("end") {
				return evaluated
			}
			evaluated = evaluateWord(doWordIndex)
			doWordIndex += phraseLength(doWordIndex)
		}
	}

	def, ok := wordDefs[word]
	if !ok {
		fmt.Printf("%s\"%s\""+tr(" definition not found")+"\n", tr("word:"), word)
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

// =============================================================================
// Program Execution
// =============================================================================

func executeProgram(pnProgram string) {
	pnProgram = tr("do") + " " + pnProgram + " " + tr("end")

	wordsBefore := len(words)
	programWords(pnProgram)

	if len(words) == wordsBefore {
		return
	}

	evaluateWord(wordsBefore + 1)
}

func executeWordsFile(fileName string) {
	resolvedFileName := resolveWordsFileName(fileName)
	data, err := os.ReadFile(resolvedFileName)

	// Fallback to original name for compatibility.
	if err != nil && resolvedFileName != fileName {
		data, err = os.ReadFile(fileName)
		if err == nil {
			resolvedFileName = fileName
		}
	}

	if err != nil {
		fmt.Fprintf(os.Stderr, "cannot open words file: %s\n", fileName)
		return
	}

	program := string(data)
	program = hashbangRemove(program)

	fileDirectoryStack = append(fileDirectoryStack, pathDirname(resolvedFileName))
	executeProgram(program)
	fileDirectoryStack = fileDirectoryStack[:len(fileDirectoryStack)-1]
}

func readExecuteLoop() {
	scanner := bufio.NewScanner(os.Stdin)
	for {
		if !scanner.Scan() {
			break
		}
		line := scanner.Text()
		if line == "" || line == tr("exit") {
			break
		}
		executeProgram(line)
	}
}

// =============================================================================
// Main
// =============================================================================

func main() {
	// Parse arguments.
	filename := ""
	for i := 1; i < len(os.Args); i++ {
		arg := os.Args[i]
		if arg == "italian" {
			language = "italian"
		} else {
			filename = arg
		}
	}

	wordDefsInit()

	fmt.Print(tr("pang version: ") + "028\n")
	fmt.Println("? for help")

	if filename != "" {
		if filename == "-" {
			readExecuteLoop()
		} else {
			executeWordsFile(filename)
			// Check if second argument is "-" for REPL after file.
			// (file was already consumed above; check if any remaining arg is "-")
			for i := 1; i < len(os.Args); i++ {
				if os.Args[i] == "-" && i > 1 {
					readExecuteLoop()
					break
				}
			}
		}
	} else {
		readExecuteLoop()
	}

	fmt.Println("bye")
}