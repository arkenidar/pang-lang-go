-- Pang: Polish notation language interpreter (v028) — Lua structural mirror of main.go
-- This file is a REVERSE-PORT: it takes main.go's structural organization
-- (sections, centralized definitions, closure style) and writes it back into
-- idiomatic Lua. Functionally equivalent to pangea/src/pangea1/main.lua (v028).
--
-- Provenance chain:
--   pangea/src/pangea1/main.lua  (original canonical v028, 561 LOC)
--     → main.go                   (Go port, reorganized structurally, 615 LOC)
--       → main_go_structured.lua  (this file — reverse-port for 1:1 correspondence)
--
-- The Go port gathered all word_definitions assignments into wordDefsInit() and
-- reordered sections for Go's compilation model. This mirror preserves that
-- structure so the mapping between main.go lines and this file is direct.

-- =============================================================================
-- Global State
-- =============================================================================

local pang_version = "028"
local language = nil -- "italian"

local word_definitions = {}
local words = {}
local string_literals = {} -- true if word is a string literal
local call_stack = {{}}
local file_directory_stack = {}

-- =============================================================================
-- Italian Translation Support
-- =============================================================================

local translate_italian = {
    ["pang version: "] = "pang versione: ",
    ["exit"] = "esci",
    ["print"] = "stampa",
    ["define_word"] = "definisci_parola",
    ["multiply"] = "moltiplica",
    ["argument"] = "argomento",
    ["do"] = "fai",
    ["end"] = "fine",
    ["set"] = "metti",
    ["get"] = "prendi",
    ["variable_set"] = "metti_variabile",
    ["variable_get"] = "prendi_variabile",
    ["caller_set"] = "metti_chiamante",
    ["caller_get"] = "prendi_chiamante",
    ["while"] = "mentre",
    ["and"] = "e",
    ["or"] = "o",
    ["not"] = "non",
    ["greater"] = "maggiore",
    ["if"] = "se",
    ["equal"] = "uguale",
    ["modulus"] = "modulo",
    ["string"] = "stringa",
    ["add"] = "somma",
    ["true"] = "vero",
    ["false"] = "falso",
    ["dont"] = "non_fare",
    ["word:"] = "parola:",
    [" definition not found"] = " definizione non trovata",
    ["command_prompt"] = "richiesta_comandi",
    ["read_text"] = "leggi_testo",
    ["to_number"] = "numero_da_testo",
    ["repeat"] = "ripeti",
    ["increment"] = "incrementa",
}

function tr(s)
    if language == "italian" then
        local t = translate_italian[s]
        if t ~= nil then
            return t
        end
        print("can't translate: " .. s)
        return s
    end
    return s
end

function truthy(v)
    if v == nil then return false end
    if type(v) == "boolean" then return v end
    return true
end

-- =============================================================================
-- File Path Utilities (port of Lua path functions)
-- =============================================================================

function path_is_absolute(path)
    if path:sub(1, 1) == "/" then return true end
    if string.match(path, "^[A-Za-z]:[\\/]") ~= nil then return true end
    return false
end

function path_dirname(path)
    local normalized = path:gsub("\\", "/")
    local dirname = normalized:match("^(.*)/[^/]*$")
    if dirname == nil or dirname == "" then return "." end
    return dirname
end

function path_join(base, name)
    if base == "" or base == "." then return name end
    if base:sub(-1) == "/" then return base .. name end
    return base .. "/" .. name
end

function resolve_words_file_name(file_name)
    if path_is_absolute(file_name) then return file_name end
    if #file_directory_stack == 0 then return file_name end
    return path_join(file_directory_stack[#file_directory_stack], file_name)
end

-- =============================================================================
-- Built-in Word Definitions (centralized — mirrors main.go wordDefsInit)
-- =============================================================================

function word_defs_init()
    -- print <printable>
    word_definitions[tr("print")] = {1, function(arguments)
        local val = evaluate_word(arguments[1])
        print(val)
        return val
    end}

    -- read_text
    word_definitions[tr("read_text")] = {0, function()
        local text = io.read()
        if text == nil then return "" end
        return text:gsub("[\r\n]+$", "")
    end}

    -- to_number <text>
    word_definitions[tr("to_number")] = {1, function(arguments)
        local val = evaluate_word(arguments[1])
        return tonumber(val) or 0
    end}

    -- add <number> <number>
    word_definitions[tr("add")] = {2, function(arguments)
        return evaluate_word(arguments[1]) + evaluate_word(arguments[2])
    end}

    -- multiply <number> <number>
    word_definitions[tr("multiply")] = {2, function(arguments)
        return evaluate_word(arguments[1]) * evaluate_word(arguments[2])
    end}

    -- true
    word_definitions[tr("true")] = {0, function() return true end}

    -- false
    word_definitions[tr("false")] = {0, function() return false end}

    -- if <condition> <if true> <if false>
    word_definitions[tr("if")] = {3, function(arguments)
        if truthy(evaluate_word(arguments[1])) then
            return evaluate_word(arguments[2])
        end
        return evaluate_word(arguments[3])
    end}

    -- while <condition> <do while true>
    word_definitions[tr("while")] = {2, function(arguments)
        while truthy(evaluate_word(arguments[1])) do
            local result = evaluate_word(arguments[2])
            if result == "break" then break end
        end
    end}

    -- repeat <times_count> <deferred_code>
    word_definitions[tr("repeat")] = {2, function(arguments)
        local total = tonumber(evaluate_word(arguments[1]))
        if total == nil then return end
        local result
        for _ = 1, total do
            result = evaluate_word(arguments[2])
        end
        return result
    end}

    -- and <boolean> <boolean>
    word_definitions[tr("and")] = {2, function(arguments)
        return truthy(evaluate_word(arguments[1])) and truthy(evaluate_word(arguments[2]))
    end}

    -- or <boolean> <boolean>
    word_definitions[tr("or")] = {2, function(arguments)
        return truthy(evaluate_word(arguments[1])) or truthy(evaluate_word(arguments[2]))
    end}

    -- not <boolean>
    word_definitions[tr("not")] = {1, function(arguments)
        return not truthy(evaluate_word(arguments[1]))
    end}

    -- equal <first> <second>
    word_definitions[tr("equal")] = {2, function(arguments)
        return evaluate_word(arguments[1]) == evaluate_word(arguments[2])
    end}

    -- set <variable name> <value>
    word_definitions[tr("set")] = {2, function(arguments)
        local vars = call_stack[#call_stack]
        local var_name = evaluate_word(arguments[1])
        vars[var_name] = evaluate_word(arguments[2])
    end}

    -- get <variable name>
    word_definitions[tr("get")] = {1, function(arguments)
        local vars = call_stack[#call_stack]
        local var_name = evaluate_word(arguments[1])
        local val = vars[var_name]
        if val == nil then
            print("nil returning from get_function()")
        end
        return val
    end}

    -- variable_set <namespace> <variable name> <value>
    word_definitions[tr("variable_set")] = {3, function(arguments)
        local namespace = evaluate_word(arguments[1])
        local var_name = evaluate_word(arguments[2])
        namespace[var_name] = evaluate_word(arguments[3])
    end}

    -- variable_get <namespace> <variable name>
    word_definitions[tr("variable_get")] = {2, function(arguments)
        local namespace = evaluate_word(arguments[1])
        local var_name = evaluate_word(arguments[2])
        return namespace[var_name]
    end}

    -- namespace — returns current call stack frame
    word_definitions["namespace"] = {0, function()
        return call_stack[#call_stack]
    end}

    -- modulus <dividend> <divisor>
    word_definitions[tr("modulus")] = {2, function(arguments)
        return evaluate_word(arguments[1]) % evaluate_word(arguments[2])
    end}

    -- greater <lesser> <greater>
    word_definitions[tr("greater")] = {2, function(arguments)
        return evaluate_word(arguments[1]) > evaluate_word(arguments[2])
    end}

    -- ? — list word definitions
    word_definitions["?"] = {0, function()
        for word, wd in pairs(word_definitions) do
            io.write(word .. "<" .. wd[1] .. " ")
        end
        io.write("\n")
    end}

    -- ! — execute_words_file <filename>
    -- (reads raw word text, like the Lua version uses words[arguments[1]])
    word_definitions["!"] = {1, function(arguments)
        local file_name = words[arguments[1]]
        execute_words_file(file_name)
    end}

    -- dont <skip this>
    word_definitions[tr("dont")] = {1, function() end}

    -- define_word <name> <arity> <action>
    word_definitions[tr("define_word")] = {3, function(arguments)
        local name = evaluate_word(arguments[1])
        local arity = evaluate_word(arguments[2])
        local body_index = arguments[3]

        local word_fn = function(word_arguments)
            local value_arguments = {}
            for i, wa in pairs(word_arguments) do
                value_arguments[i] = evaluate_word(wa)
            end
            table.insert(call_stack, value_arguments)
            local result = evaluate_word(arguments[3])
            table.remove(call_stack)
            return result
        end
        word_definitions[name] = {arity, word_fn}
    end}

    -- argument <argument index>
    word_definitions[tr("argument")] = {1, function(arguments)
        local frame = call_stack[#call_stack]
        local arg_index = evaluate_word(arguments[1])
        return frame[arg_index]
    end}

    -- command_prompt — REPL
    word_definitions[tr("command_prompt")] = {0, function()
        read_execute_loop()
    end}

    -- increment <variable name>
    word_definitions[tr("increment")] = {1, function(arguments)
        local vars = call_stack[#call_stack]
        local var_name = evaluate_word(arguments[1])
        local val = vars[var_name]
        if val == nil then val = 0 end
        val = val + 1
        vars[var_name] = val
        return val
    end}
end

-- =============================================================================
-- Tokenizer / Lexer (port of program_words)
-- =============================================================================

function program_words(pn_program)
    local token = {}
    local quoted = {}
    local in_string = false
    local escaping = false

    local function flush_token()
        if #token > 0 then
            table.insert(words, table.concat(token))
            table.insert(string_literals, false)
            token = {}
        end
    end

    local function flush_quoted()
        table.insert(words, table.concat(quoted))
        table.insert(string_literals, true)
        quoted = {}
    end

    local function append_escape(char)
        if char == '"' then
            table.insert(quoted, '"')
        elseif char == "\\" then
            table.insert(quoted, "\\")
        elseif char == "n" then
            table.insert(quoted, "\n")
        elseif char == "t" then
            table.insert(quoted, "\t")
        else
            io.stderr:write("invalid escape sequence: \\" .. char .. "\n")
        end
    end

    for i = 1, #pn_program do
        local c = pn_program:sub(i, i)
        if in_string then
            if escaping then
                append_escape(c)
                escaping = false
            elseif c == "\\" then
                escaping = true
            elseif c == '"' then
                flush_quoted()
                in_string = false
            else
                table.insert(quoted, c)
            end
        elseif string.match(c, "%s") then
            flush_token()
        elseif c == '"' then
            flush_token()
            in_string = true
        else
            table.insert(token, c)
        end
    end

    if escaping then
        io.stderr:write("unterminated escape sequence in string literal\n")
    end
    if in_string then
        io.stderr:write("unterminated string literal\n")
    end

    flush_token()
end

function hashbang_remove(program)
    if program:sub(1, 1) == "#" then
        local i = string.find(program, "\n")
        if i ~= nil then
            return program:sub(i + 1)
        end
        return ""
    end
    return program
end

-- =============================================================================
-- Core Evaluator
-- =============================================================================

function phrase_length(word_index)
    local word = words[word_index]
    local length = 1

    -- String literals always have length 1.
    if string_literals[word_index] then
        return 1
    end

    -- do ... end blocks
    if word == tr("do") then
        while true do
            local look_ahead = word_index + length
            if look_ahead > #words or words[look_ahead] == tr("end") then
                return length + 1
            end
            length = length + phrase_length(word_index + length)
        end
    end

    -- Numbers have length 1.
    if tonumber(word) ~= nil then
        return 1
    end

    local wd = word_definitions[word]
    if wd == nil then
        return 1
    end

    for _ = 1, wd[1] do
        length = length + phrase_length(word_index + length)
    end
    return length
end

function evaluate_word(word_index)
    local word = words[word_index]

    -- String literals return their raw value.
    if string_literals[word_index] then
        return word
    end

    -- Number literals.
    local num = tonumber(word)
    if num ~= nil then
        return num
    end

    -- do ... end blocks
    if word == tr("do") then
        local do_word_index = word_index + 1
        local evaluated
        while do_word_index <= #words and words[do_word_index] ~= tr("end") do
            evaluated = evaluate_word(do_word_index)
            do_word_index = do_word_index + phrase_length(do_word_index)
        end
        return evaluated
    end

    local wd = word_definitions[word]
    if wd == nil then
        print(tr("word:") .. word .. tr(" definition not found"))
        return
    end

    local arguments = {}
    local arg_word_index = word_index + 1
    for _ = 1, wd[1] do
        table.insert(arguments, arg_word_index)
        arg_word_index = arg_word_index + phrase_length(arg_word_index)
    end

    return wd[2](arguments)
end

-- =============================================================================
-- Program Execution
-- =============================================================================

function execute_program(pn_program)
    pn_program = tr("do") .. " " .. pn_program .. " " .. tr("end")

    local words_before = #words
    program_words(pn_program)

    if #words == words_before then
        return
    end

    evaluate_word(words_before + 1)
end

function execute_words_file(file_name)
    local resolved_file_name = resolve_words_file_name(file_name)
    local file = io.open(resolved_file_name, "r")

    -- Fallback to original name for compatibility.
    if file == nil and resolved_file_name ~= file_name then
        file = io.open(file_name, "r")
        if file ~= nil then
            resolved_file_name = file_name
        end
    end

    if file == nil then
        io.stderr:write("cannot open words file: " .. file_name .. "\n")
        return
    end

    local program = ""
    while true do
        local program_line = file:read()
        if program_line == nil then break end
        program = program .. program_line .. "\n"
    end
    file:close()

    program = hashbang_remove(program)

    table.insert(file_directory_stack, path_dirname(resolved_file_name))
    local ok, runtime_error = pcall(execute_program, program)
    table.remove(file_directory_stack)
    if not ok then
        error(runtime_error)
    end
end

function read_execute_loop()
    while true do
        local program = io.read()
        if program == nil or program == tr("exit") then
            break
        end
        execute_program(program)
    end
end

-- =============================================================================
-- Main
-- =============================================================================

function main(arg)
    -- Parse arguments.
    local filename = nil
    for i = 1, #arg do
        if arg[i] == "italian" then
            language = "italian"
        else
            filename = arg[i]
        end
    end

    word_defs_init()

    print(tr("pang version: ") .. pang_version)
    print("? for help")

    if filename ~= nil then
        if filename == "-" then
            read_execute_loop()
        else
            execute_words_file(filename)
            -- Check if second argument is "-" for REPL after file.
            if arg[2] == "-" then
                read_execute_loop()
            end
        end
    else
        read_execute_loop()
    end

    print("bye")
end

main(arg)