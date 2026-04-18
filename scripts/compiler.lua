-- Cereka .crka compiler
-- Pipeline: source text -> lines -> tokens -> AST -> Instruction[]
--
-- Output contract (consumed by src/compiler/vn_instruction.cpp):
--   return { instructions = { {op=<str>, a=?, b=?, c=?, exit_button=?, line=?, col=?}, ... } }
--
-- Errors are raised with `error("line L col C: message")`. sol2 surfaces this
-- through the protected_function_result in C++ so the developer sees the location.

-- ============================================================================
-- Utilities
-- ============================================================================

local function at(lineno, col)
    return "line " .. tostring(lineno) .. " col " .. tostring(col)
end

local function die(lineno, col, msg)
    error(at(lineno, col) .. ": " .. msg, 0)
end

-- ============================================================================
-- 1. Line splitter (preserves line numbers, including blank lines)
-- ============================================================================

local function split_lines(text)
    local out = {}
    local lineno = 1
    local start = 1
    local i = 1
    local n = #text
    while i <= n do
        local c = text:sub(i, i)
        if c == "\n" or c == "\r" then
            out[#out + 1] = { text = text:sub(start, i - 1), lineno = lineno }
            if c == "\r" and text:sub(i + 1, i + 1) == "\n" then i = i + 1 end
            lineno = lineno + 1
            start = i + 1
        end
        i = i + 1
    end
    if start <= n then
        out[#out + 1] = { text = text:sub(start), lineno = lineno }
    end
    return out
end

local function indent_of(s)
    return #(s:match("^[ \t]*") or "")
end

local function rtrim(s)
    return (s:gsub("[ \t]+$", ""))
end

-- ============================================================================
-- 2. Tokenizer (per line, after indent has been stripped)
--    Tokens: { type, value, col, lineno }
--    Types : IDENT, STRING, NUMBER, OP
-- ============================================================================

local TWO_CHAR_OPS = {
    ["=="] = true, ["!="] = true, [">="] = true, ["<="] = true,
    ["+="] = true, ["-="] = true, ["*="] = true, ["/="] = true,
}

local function tokenize(line_text, lineno, base_col)
    -- line_text is the source line (not yet trimmed)
    -- base_col is 1 for a whole line; for re-tokenizing a substring it's the offset.
    base_col = base_col or 1
    local tokens = {}
    local i = 1
    local n = #line_text
    while i <= n do
        local c = line_text:sub(i, i)
        if c == " " or c == "\t" then
            i = i + 1
        elseif c == ";" then
            break  -- comment eats rest of line
        elseif c == '"' then
            local start_i = i
            local j = line_text:find('"', i + 1, true)
            if not j then die(lineno, base_col + i - 1, "unterminated string literal") end
            tokens[#tokens + 1] = {
                type = "STRING",
                value = line_text:sub(i + 1, j - 1),
                col = base_col + start_i - 1,
                lineno = lineno,
            }
            i = j + 1
        elseif c:match("%d") or (c == "-" and (line_text:sub(i + 1, i + 1)):match("%d")) then
            local start_i = i
            if c == "-" then i = i + 1 end
            while i <= n and line_text:sub(i, i):match("[%d%.]") do i = i + 1 end
            tokens[#tokens + 1] = {
                type = "NUMBER",
                value = line_text:sub(start_i, i - 1),
                col = base_col + start_i - 1,
                lineno = lineno,
            }
        elseif c == "$" then
            tokens[#tokens + 1] = { type = "OP", value = "$", col = base_col + i - 1, lineno = lineno }
            i = i + 1
        elseif c:match("[%a_]") then
            local start_i = i
            -- Identifier: letters, digits, underscore, dot, slash, backslash, hyphen
            -- (to accommodate file paths like "assets/ui/bg.png" and keywords like "stop_bgm")
            while i <= n and line_text:sub(i, i):match("[%w_%./\\%-]") do i = i + 1 end
            tokens[#tokens + 1] = {
                type = "IDENT",
                value = line_text:sub(start_i, i - 1),
                col = base_col + start_i - 1,
                lineno = lineno,
            }
        elseif c:match("[=!<>+%-*/%%]") then
            local two = line_text:sub(i, i + 1)
            if TWO_CHAR_OPS[two] then
                tokens[#tokens + 1] = { type = "OP", value = two, col = base_col + i - 1, lineno = lineno }
                i = i + 2
            else
                tokens[#tokens + 1] = { type = "OP", value = c, col = base_col + i - 1, lineno = lineno }
                i = i + 1
            end
        else
            die(lineno, base_col + i - 1, "unexpected character '" .. c .. "'")
        end
    end
    return tokens
end

-- A line context wraps tokens + source text so parsers can either consume
-- tokens structurally or grab the raw suffix for free-form values.
local function make_line_ctx(raw, tokens, lineno, indent, raw_offset)
    return {
        raw = raw,                    -- trimmed source line
        raw_offset = raw_offset or 1, -- 1-based col where `raw` starts in the original line
        tokens = tokens,
        lineno = lineno,
        indent = indent,
        pos = 1,                      -- index into tokens
    }
end

local function peek(ctx, k) return ctx.tokens[ctx.pos + (k or 0)] end
local function take(ctx)
    local t = ctx.tokens[ctx.pos]
    ctx.pos = ctx.pos + 1
    return t
end
local function eof(ctx) return ctx.pos > #ctx.tokens end

-- Grab source text from current token position to end of line (for free-form values).
local function rest_text(ctx)
    local t = ctx.tokens[ctx.pos]
    if not t then return "" end
    local start = t.col - ctx.raw_offset + 1
    return rtrim(ctx.raw:sub(start))
end

-- Consume a required token of a given type; optionally require a specific value.
local function expect(ctx, want_type, want_value, err_msg)
    local t = ctx.tokens[ctx.pos]
    if not t then
        local last_col = ctx.raw_offset + #ctx.raw
        die(ctx.lineno, last_col, err_msg or ("expected " .. want_type))
    end
    if t.type ~= want_type or (want_value ~= nil and t.value ~= want_value) then
        die(ctx.lineno, t.col, err_msg or ("expected " .. (want_value or want_type) ..
            ", got " .. t.type .. " '" .. tostring(t.value) .. "'"))
    end
    ctx.pos = ctx.pos + 1
    return t
end

-- Strip surrounding quotes from a bare value if present.
local function maybe_unquote(s)
    local inner = s:match('^"(.*)"$')
    return inner or s
end

-- ============================================================================
-- 3. Parser — produces AST nodes
--    Each AST node carries `kind` plus kind-specific fields and `line`/`col`.
-- ============================================================================

local function parse_bg(ctx)
    -- bg <file>
    -- bg <file> fade <duration>
    local kw = take(ctx)  -- "bg"
    local file_t = expect(ctx, "IDENT", nil, "expected filename after 'bg'")
    local file = file_t.value
    local node = { kind = "Bg", file = file, line = kw.lineno, col = kw.col }
    if not eof(ctx) then
        local nxt = peek(ctx)
        if nxt and nxt.type == "IDENT" and nxt.value == "fade" then
            take(ctx)
            local dur_t = peek(ctx)
            if not dur_t or (dur_t.type ~= "NUMBER" and dur_t.type ~= "IDENT") then
                die(kw.lineno, (dur_t and dur_t.col) or (ctx.raw_offset + #ctx.raw),
                    "expected duration after 'fade'")
            end
            take(ctx)
            node.kind = "Fade"
            node.duration = dur_t.value
        end
    end
    return node
end

local function parse_char(ctx)
    -- char <id> [left|center|right] <file>
    local kw = take(ctx)
    local id_t = expect(ctx, "IDENT", nil, "expected character id after 'char'")
    local t2 = peek(ctx)
    local t3 = peek(ctx, 1)
    local pos, file
    local VALID_POS = { left = true, center = true, right = true }
    if t2 and t3 and t2.type == "IDENT" and VALID_POS[t2.value] then
        pos = t2.value
        take(ctx)
        local ft = expect(ctx, "IDENT", nil, "expected filename after position")
        file = ft.value
    else
        pos = "center"
        local ft = expect(ctx, "IDENT", nil, "expected filename")
        file = ft.value
    end
    return { kind = "Char", id = id_t.value, pos = pos, file = file,
             line = kw.lineno, col = kw.col }
end

local function parse_hide_char(ctx)
    -- hide char <id>
    local kw = take(ctx)  -- "hide"
    expect(ctx, "IDENT", "char", "expected 'char' after 'hide'")
    local id_t = expect(ctx, "IDENT", nil, "expected character id")
    return { kind = "HideChar", id = id_t.value, line = kw.lineno, col = kw.col }
end

local function parse_say(ctx)
    -- say <id> "text"
    local kw = take(ctx)
    local id_t = expect(ctx, "IDENT", nil, "expected speaker after 'say'")
    local text_t = expect(ctx, "STRING", nil, 'expected "text" after speaker')
    return { kind = "Say", speaker = id_t.value, text = text_t.value,
             line = kw.lineno, col = kw.col }
end

local function parse_narrate(ctx)
    local kw = take(ctx)
    local text_t = expect(ctx, "STRING", nil, 'expected "text" after narrate')
    return { kind = "Narrate", text = text_t.value, line = kw.lineno, col = kw.col }
end

local function parse_label(ctx)
    local kw = take(ctx)
    local id_t = expect(ctx, "IDENT", nil, "expected label name")
    return { kind = "Label", name = id_t.value, line = kw.lineno, col = kw.col }
end

local function parse_jump(ctx)
    local kw = take(ctx)
    local id_t = expect(ctx, "IDENT", nil, "expected jump target")
    return { kind = "Jump", target = id_t.value, line = kw.lineno, col = kw.col }
end

local function parse_include(ctx)
    local kw = take(ctx)
    local f = expect(ctx, "IDENT", nil, "expected filename after 'include'")
    return { kind = "Include", file = f.value, line = kw.lineno, col = kw.col }
end

local function parse_call(ctx)
    local kw = take(ctx)
    local f = expect(ctx, "IDENT", nil, "expected filename after 'call'")
    return { kind = "Call", file = f.value, line = kw.lineno, col = kw.col }
end

local function parse_set(ctx)
    -- set <var> <value>  (value = rest of line, may be quoted or bare)
    local kw = take(ctx)
    local var_t = expect(ctx, "IDENT", nil, "expected variable name after 'set'")
    local val = rest_text(ctx)
    if val == "" then die(kw.lineno, kw.col, "expected value after variable") end
    val = maybe_unquote(val)
    return { kind = "SetVar", name = var_t.value, value = val,
             line = kw.lineno, col = kw.col }
end

local function parse_arith(ctx)
    -- $ <var> (= | += | -= | *= | /=) <rhs>
    local dollar = take(ctx)  -- "$"
    local var_t = expect(ctx, "IDENT", nil, "expected variable after '$'")
    local op_t = peek(ctx)
    if not op_t or op_t.type ~= "OP" then
        die(dollar.lineno, (op_t and op_t.col) or dollar.col, "expected assignment operator")
    end
    local op
    if op_t.value == "=" then op = "="
    elseif op_t.value == "+=" then op = "+"
    elseif op_t.value == "-=" then op = "-"
    elseif op_t.value == "*=" then op = "*"
    elseif op_t.value == "/=" then op = "/"
    else
        die(op_t.lineno, op_t.col, "expected one of = += -= *= /=")
    end
    take(ctx)
    local rhs = rest_text(ctx)
    if rhs == "" then die(dollar.lineno, dollar.col, "expected rhs expression") end
    rhs = maybe_unquote(rhs)
    return { kind = "SetVarNum", name = var_t.value, op = op, rhs = rhs,
             line = dollar.lineno, col = dollar.col }
end

local IF_OPS = {
    ["=="] = "IF_EQ", ["!="] = "IF_NEQ",
    [">"] = "IF_GT", ["<"] = "IF_LT",
    [">="] = "IF_GE", ["<="] = "IF_LE",
}

local function parse_if(ctx)
    -- if <var> <cmp> <value>
    local kw = take(ctx)
    local var_t = expect(ctx, "IDENT", nil, "expected variable after 'if'")
    local op_t = peek(ctx)
    if not op_t or op_t.type ~= "OP" or not IF_OPS[op_t.value] then
        die(kw.lineno, (op_t and op_t.col) or kw.col, "expected comparison operator")
    end
    take(ctx)
    local rhs = rest_text(ctx)
    if rhs == "" then die(kw.lineno, kw.col, "expected value after comparison") end
    rhs = maybe_unquote(rhs)
    return { kind = "If", name = var_t.value, op = IF_OPS[op_t.value], rhs = rhs,
             line = kw.lineno, col = kw.col }
end

local function parse_single_keyword(kind)
    return function(ctx)
        local kw = take(ctx)
        return { kind = kind, line = kw.lineno, col = kw.col }
    end
end

local function parse_bgm(ctx)
    local kw = take(ctx)
    local f = expect(ctx, "IDENT", nil, "expected filename after 'bgm'")
    return { kind = "PlayBgm", file = f.value, line = kw.lineno, col = kw.col }
end

local function parse_sfx(ctx)
    local kw = take(ctx)
    local f = expect(ctx, "IDENT", nil, "expected filename after 'sfx'")
    return { kind = "PlaySfx", file = f.value, line = kw.lineno, col = kw.col }
end

local function parse_save(ctx)
    local kw = take(ctx)
    local n = expect(ctx, "NUMBER", nil, "expected slot number after 'save'")
    return { kind = "Save", slot = n.value, line = kw.lineno, col = kw.col }
end

local function parse_load(ctx)
    local kw = take(ctx)
    local n = expect(ctx, "NUMBER", nil, "expected slot number after 'load'")
    return { kind = "Load", slot = n.value, line = kw.lineno, col = kw.col }
end

local function parse_menu_button(ctx)
    local kw = take(ctx)  -- "button"
    local text_t = expect(ctx, "STRING", nil, 'expected "text" after button')
    local nxt = peek(ctx)
    if not nxt then die(kw.lineno, kw.col, "expected 'goto <label>' or 'exit'") end
    if nxt.type == "IDENT" and nxt.value == "goto" then
        take(ctx)
        local tgt = expect(ctx, "IDENT", nil, "expected label after 'goto'")
        return { kind = "Button", text = text_t.value, target = tgt.value,
                 exit = false, line = kw.lineno, col = kw.col }
    elseif nxt.type == "IDENT" and nxt.value == "exit" then
        take(ctx)
        return { kind = "Button", text = text_t.value, target = "",
                 exit = true, line = kw.lineno, col = kw.col }
    else
        die(nxt.lineno, nxt.col, "expected 'goto <label>' or 'exit'")
    end
end

-- ============================================================================
-- Top-level dispatch
-- ============================================================================

local STMT_HANDLERS = {
    bg        = parse_bg,
    char      = parse_char,
    hide      = parse_hide_char,
    say       = parse_say,
    narrate   = parse_narrate,
    label     = parse_label,
    jump      = parse_jump,
    include   = parse_include,
    call      = parse_call,
    set       = parse_set,
    ["if"]    = parse_if,
    ["else"]  = parse_single_keyword("Else"),
    endif     = parse_single_keyword("Endif"),
    bgm       = parse_bgm,
    stop_bgm  = parse_single_keyword("StopBgm"),
    sfx       = parse_sfx,
    ["end"]   = parse_single_keyword("End"),
    save_menu = parse_single_keyword("SaveMenu"),
    load_menu = parse_single_keyword("LoadMenu"),
    save      = parse_save,
    load      = parse_load,
}

local MENU_HANDLERS = {
    bg     = parse_bg,
    button = parse_menu_button,
}

-- ============================================================================
-- Block-aware program parser
-- ============================================================================

local function is_blank_or_comment(line_text)
    local trimmed = line_text:gsub("^%s+", "")
    return trimmed == "" or trimmed:sub(1, 1) == ";"
end

local function parse_line_statement(line, handlers)
    -- line = {text, lineno}. Returns AST node or nil if blank/comment.
    if is_blank_or_comment(line.text) then return nil end
    local indent = indent_of(line.text)
    local raw = rtrim(line.text:sub(indent + 1))
    local tokens = tokenize(raw, line.lineno, indent + 1)
    if #tokens == 0 then return nil end
    local ctx = make_line_ctx(raw, tokens, line.lineno, indent, indent + 1)

    local first = tokens[1]

    -- Arithmetic starts with a bare '$'.
    if first.type == "OP" and first.value == "$" then
        return parse_arith(ctx)
    end

    if first.type ~= "IDENT" then
        die(first.lineno, first.col, "unexpected " .. first.type .. " at start of line")
    end

    local handler = handlers[first.value]
    if not handler then
        die(first.lineno, first.col, "unknown statement '" .. first.value .. "'")
    end
    return handler(ctx)
end

-- Collect a block: consumes consecutive non-blank lines with indent > header_indent.
-- Returns (child AST nodes, new i).
local function collect_block(lines, i, header_indent, handlers)
    local children = {}
    while i <= #lines do
        local line = lines[i]
        if is_blank_or_comment(line.text) then
            i = i + 1
        else
            local body_indent = indent_of(line.text)
            if body_indent <= header_indent then break end
            local node = parse_line_statement(line, handlers)
            if node then children[#children + 1] = node end
            i = i + 1
        end
    end
    return children, i
end

local function parse_menu_block(lines, i, header_indent, header_line)
    local children, newi = collect_block(lines, i, header_indent, MENU_HANDLERS)
    return { kind = "Menu", children = children, line = header_line.lineno,
             col = indent_of(header_line.text) + 1 }, newi
end

local function parse_ui_block(lines, i, header_indent, header_line, element)
    -- Body lines: "<prop> <value rest of line>"
    local children = {}
    while i <= #lines do
        local line = lines[i]
        if is_blank_or_comment(line.text) then
            i = i + 1
        else
            local body_indent = indent_of(line.text)
            if body_indent <= header_indent then break end
            local raw = rtrim(line.text:sub(body_indent + 1))
            local tokens = tokenize(raw, line.lineno, body_indent + 1)
            if #tokens == 0 then
                i = i + 1
            else
                local first = tokens[1]
                if first.type ~= "IDENT" then
                    die(first.lineno, first.col, "expected property name in ui block")
                end
                local ctx = make_line_ctx(raw, tokens, line.lineno, body_indent, body_indent + 1)
                take(ctx)  -- property name
                local value = rest_text(ctx)
                children[#children + 1] = {
                    kind = "UiProp", prop = first.value, value = value,
                    line = first.lineno, col = first.col,
                }
                i = i + 1
            end
        end
    end
    return { kind = "UiBlock", element = element, children = children,
             line = header_line.lineno,
             col = indent_of(header_line.text) + 1 }, i
end

local function parse_program(text)
    local lines = split_lines(text)
    local program = { kind = "Program", children = {} }
    local i = 1
    while i <= #lines do
        local line = lines[i]
        if is_blank_or_comment(line.text) then
            i = i + 1
        else
            local indent = indent_of(line.text)
            local raw = rtrim(line.text:sub(indent + 1))
            local tokens = tokenize(raw, line.lineno, indent + 1)
            if #tokens == 0 then
                i = i + 1
            else
                local first = tokens[1]
                -- Block headers: "menu" and "ui <element>"
                if first.type == "IDENT" and first.value == "menu" and #tokens == 1 then
                    local header_line = line
                    i = i + 1
                    local node, newi = parse_menu_block(lines, i, indent, header_line)
                    program.children[#program.children + 1] = node
                    i = newi
                elseif first.type == "IDENT" and first.value == "ui" and #tokens >= 2
                       and tokens[2].type == "IDENT" then
                    local element = tokens[2].value
                    -- Special case: "ui advance_keys [keys...]" is a single-line statement,
                    -- not a block header — same behavior as the previous compiler.
                    if element == "advance_keys" then
                        local ctx = make_line_ctx(raw, tokens, line.lineno, indent, indent + 1)
                        take(ctx)  -- "ui"
                        take(ctx)  -- "advance_keys"
                        local keys = rest_text(ctx)
                        if keys == "" then keys = "space enter" end
                        program.children[#program.children + 1] = {
                            kind = "UiAdvanceKeys", keys = keys,
                            line = line.lineno, col = first.col,
                        }
                        i = i + 1
                    else
                        local header_line = line
                        i = i + 1
                        local node, newi = parse_ui_block(lines, i, indent, header_line, element)
                        program.children[#program.children + 1] = node
                        i = newi
                    end
                else
                    local node = parse_line_statement(line, STMT_HANDLERS)
                    if node then program.children[#program.children + 1] = node end
                    i = i + 1
                end
            end
        end
    end
    return program
end

-- ============================================================================
-- 4. Lowerer — AST -> Instruction[]
-- ============================================================================

local function emit(out, ins)
    out[#out + 1] = ins
end

local LOWERERS
LOWERERS = {
    Bg = function(n, out)
        emit(out, { op = "BG", a = n.file, line = n.line, col = n.col })
    end,
    Fade = function(n, out)
        emit(out, { op = "FADE", a = n.file, b = n.duration, line = n.line, col = n.col })
    end,
    Char = function(n, out)
        emit(out, { op = "CHAR", a = n.id, b = n.file, c = n.pos, line = n.line, col = n.col })
    end,
    HideChar = function(n, out)
        emit(out, { op = "HIDE_CHAR", a = n.id, line = n.line, col = n.col })
    end,
    Say = function(n, out)
        emit(out, { op = "SAY", a = n.speaker, b = n.text, line = n.line, col = n.col })
    end,
    Narrate = function(n, out)
        emit(out, { op = "NARRATE", b = n.text, line = n.line, col = n.col })
    end,
    Label = function(n, out)
        emit(out, { op = "LABEL", a = n.name, line = n.line, col = n.col })
    end,
    Jump = function(n, out)
        emit(out, { op = "JUMP", a = n.target, line = n.line, col = n.col })
    end,
    Include = function(n, out)
        emit(out, { op = "INCLUDE", a = n.file, line = n.line, col = n.col })
    end,
    Call = function(n, out)
        emit(out, { op = "CALL", a = n.file, line = n.line, col = n.col })
    end,
    SetVar = function(n, out)
        emit(out, { op = "SET_VAR", a = n.name, b = n.value, line = n.line, col = n.col })
    end,
    SetVarNum = function(n, out)
        emit(out, { op = "SET_VAR_NUM", a = n.name, b = n.op, c = n.rhs,
                    line = n.line, col = n.col })
    end,
    If = function(n, out)
        emit(out, { op = n.op, a = n.name, b = n.rhs, line = n.line, col = n.col })
    end,
    Else = function(n, out)
        emit(out, { op = "ELSE", line = n.line, col = n.col })
    end,
    Endif = function(n, out)
        emit(out, { op = "ENDIF", line = n.line, col = n.col })
    end,
    PlayBgm = function(n, out)
        emit(out, { op = "PLAY_BGM", a = n.file, line = n.line, col = n.col })
    end,
    StopBgm = function(n, out)
        emit(out, { op = "STOP_BGM", line = n.line, col = n.col })
    end,
    PlaySfx = function(n, out)
        emit(out, { op = "PLAY_SFX", a = n.file, line = n.line, col = n.col })
    end,
    ["End"] = function(n, out)
        emit(out, { op = "END", line = n.line, col = n.col })
    end,
    SaveMenu = function(n, out)
        emit(out, { op = "SAVE_MENU", line = n.line, col = n.col })
    end,
    LoadMenu = function(n, out)
        emit(out, { op = "LOAD_MENU", line = n.line, col = n.col })
    end,
    Save = function(n, out)
        emit(out, { op = "SAVE", a = n.slot, line = n.line, col = n.col })
    end,
    Load = function(n, out)
        emit(out, { op = "LOAD", a = n.slot, line = n.line, col = n.col })
    end,
    Menu = function(n, out)
        emit(out, { op = "MENU", line = n.line, col = n.col })
        for _, child in ipairs(n.children) do
            local lower = LOWERERS[child.kind]
            if lower then lower(child, out) end
        end
    end,
    Button = function(n, out)
        emit(out, { op = "BUTTON", a = n.text, b = n.target, exit_button = n.exit,
                    line = n.line, col = n.col })
    end,
    UiBlock = function(n, out)
        for _, child in ipairs(n.children) do
            -- Child is UiProp
            emit(out, {
                op = "UI_SET",
                a = n.element .. "." .. child.prop,
                b = child.value,
                line = child.line, col = child.col,
            })
        end
    end,
    UiAdvanceKeys = function(n, out)
        emit(out, { op = "UI_SET", a = "advance_keys", b = n.keys,
                    line = n.line, col = n.col })
    end,
}

local function lower_program(ast)
    local out = {}
    for _, node in ipairs(ast.children) do
        local lower = LOWERERS[node.kind]
        if not lower then
            die(node.line or 1, node.col or 1, "internal: no lowerer for " .. tostring(node.kind))
        end
        lower(node, out)
    end
    return out
end

-- ============================================================================
-- 5. Public entry point (global, called from C++ via sol2)
-- ============================================================================

function compile(script_text)
    local ast = parse_program(script_text)
    local instructions = lower_program(ast)
    return { instructions = instructions }
end
