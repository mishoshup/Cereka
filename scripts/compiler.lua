function compile(script_text)
    local lines = {}
    for line in script_text:gmatch("[^\r\n]+") do
        table.insert(lines, line)
    end

    local instructions = {}
    local i = 1

    local function trim(s)
        return s:match("^%s*(.-)%s*$")
    end

    local function get_indent(line)
        local leading = line:match("^%s*")
        return #leading
    end

    while i <= #lines do
        local raw = lines[i]
        local l = trim(raw)

        -- Skip empty lines and comments
        if l == "" or l:sub(1,1) == ";" then
            i = i + 1
            goto continue
        end

        -- MENU BLOCK
        if l:match("^menu") then
            table.insert(instructions, { op = "MENU" })
            local block_indent = get_indent(raw)
            i = i + 1

            while i <= #lines do
                local body_raw = lines[i]
                local body_l   = trim(body_raw)
                local body_ind = get_indent(body_raw)

                if body_l == "" or body_l:sub(1,1) == ";" then
                    i = i + 1
                elseif body_ind <= block_indent then
                    break
                elseif body_l:match("^bg%s+%S+%s+fade%s+") then
                    local file, dur = body_l:match("^bg%s+(%S+)%s+fade%s+(%d+%.?%d*)")
                    table.insert(instructions, { op = "FADE", a = file, b = dur })
                    i = i + 1
                elseif body_l:match("^bg%s+") then
                    local f = body_l:match("^bg%s+(%S+)")
                    table.insert(instructions, { op = "BG", a = f })
                    i = i + 1
                elseif body_l:match('^button%s+"') then
                    local text, rest = body_l:match('button%s+"(.-)"%s*(.*)')
                    local target  = rest:match("goto%s+(%S+)") or ""
                    local is_exit = rest:find("exit") ~= nil
                    table.insert(instructions, {
                        op = "BUTTON", a = text, b = target, exit_button = is_exit,
                    })
                    i = i + 1
                else
                    print("[COMPILER] Unknown inside menu: " .. body_l)
                    i = i + 1
                end
            end
            goto continue
        end

        -- UI BLOCK  (ui textbox / ui namebox / ui button / ui font)
        if l:match("^ui%s+") then
            local element     = l:match("^ui%s+(%S+)")
            local block_indent = get_indent(raw)
            i = i + 1

            while i <= #lines do
                local body_raw = lines[i]
                local body_l   = trim(body_raw)
                local body_ind = get_indent(body_raw)

                if body_l == "" or body_l:sub(1,1) == ";" then
                    i = i + 1
                elseif body_ind <= block_indent then
                    break
                else
                    local prop, val = body_l:match("^(%S+)%s+(.*)")
                    if prop then
                        table.insert(instructions, {
                            op = "UI_SET",
                            a  = element .. "." .. prop,
                            b  = trim(val or ""),
                        })
                    end
                    i = i + 1
                end
            end
            goto continue
        end

        -- Top-level commands
        if l:match("^bg%s+%S+%s+fade%s+") then
            local file, dur = l:match("^bg%s+(%S+)%s+fade%s+(%d+%.?%d*)")
            table.insert(instructions, { op = "FADE", a = file, b = dur })
        elseif l:match("^bg%s+") then
            local f = l:match("^bg%s+(%S+)")
            table.insert(instructions, { op = "BG", a = f })
        elseif l:match("^char%s+") then
            -- char <id> [left|center|right] <file>
            local words = {}
            for w in l:gmatch("%S+") do words[#words+1] = w end
            local name = words[2]
            local pos, file
            local valid_pos = { left=true, center=true, right=true }
            if words[4] and valid_pos[words[3]] then
                pos  = words[3]
                file = words[4]
            else
                pos  = "center"
                file = words[3]
            end
            table.insert(instructions, { op = "CHAR", a = name, b = file, c = pos })
        elseif l:match("^say%s+") then
            local speaker, text = l:match('say%s+(%S+)%s+"(.-)"')
            table.insert(instructions, { op = "SAY", a = speaker, b = text })
        elseif l:match("^narrate%s+") then
            local text = l:match('narrate%s+"(.-)"')
            table.insert(instructions, { op = "NARRATE", b = text })
        elseif l:match("^label%s+") then
            local name = l:match("^label%s+(%S+)")
            table.insert(instructions, { op = "LABEL", a = name })
        elseif l:match("^jump%s+") then
            local target = l:match("^jump%s+(%S+)")
            table.insert(instructions, { op = "JUMP", a = target })
        elseif l:match("^include%s+") then
            local file = l:match("^include%s+(%S+)")
            table.insert(instructions, { op = "INCLUDE", a = file })
        elseif l:match("^call%s+") then
            local file = l:match("^call%s+(%S+)")
            table.insert(instructions, { op = "CALL", a = file })
        elseif l:match("^hide%s+char%s+") then
            local id = l:match("^hide%s+char%s+(%S+)")
            table.insert(instructions, { op = "HIDE_CHAR", a = id })
        elseif l:match("^set%s+") then
            local var, val = l:match("^set%s+(%S+)%s+(.*)")
            val = val:match('^"(.-)"$') or val
            table.insert(instructions, { op = "SET_VAR", a = var, b = val })
        elseif l:match("^%$%s+%S+%s*[%+%-]?=") then
            -- Arithmetic: $ var += value, $ var -= value, $ var = expr
            local var, op, rhs = l:match("^%$%s+(%S+)%s*([%+%-/%*])=%s*(.+)")
            local real_op = op or "+"
            if not var then
                var = l:match("^%$%s+(%S+)%s*=")
                rhs = l:match("^%$%s+%S+%s*=%s*(.+)")
            end
            if var then
                rhs = rhs:match('^"(.-)"$') or rhs
                table.insert(instructions, { op = "SET_VAR_NUM", a = var, b = real_op, c = rhs })
            else
                print("[COMPILER] Bad arithmetic syntax: " .. l)
            end
        elseif l:match("^if%s+") then
            local var, val, opcode
            var, val = l:match("^if%s+(%S+)%s+==%s+(.*)")
            if var then
                opcode = "IF_EQ"
            else
                var, val = l:match("^if%s+(%S+)%s+!=%s+(.*)")
                opcode = "IF_NEQ"
            end
            if not var then
                var, val = l:match("^if%s+(%S+)%s+>%s+(.*)")
                opcode = "IF_GT"
            end
            if not var then
                var, val = l:match("^if%s+(%S+)%s+<%s+(.*)")
                opcode = "IF_LT"
            end
            if not var then
                var, val = l:match("^if%s+(%S+)%s+>=%s+(.*)")
                opcode = "IF_GE"
            end
            if not var then
                var, val = l:match("^if%s+(%S+)%s+<=%s+(.*)")
                opcode = "IF_LE"
            end
            if var and val then
                val = val:match('^"(.-)"$') or val
                table.insert(instructions, { op = opcode, a = var, b = val })
            else
                print("[COMPILER] Bad if syntax: " .. l)
            end
        elseif l == "endif" then
            table.insert(instructions, { op = "ENDIF" })
        elseif l:match("^bgm%s+") then
            local file = l:match("^bgm%s+(%S+)")
            table.insert(instructions, { op = "PLAY_BGM", a = file })
        elseif l:match("^stop_bgm") then
            table.insert(instructions, { op = "STOP_BGM" })
        elseif l:match("^sfx%s+") then
            local file = l:match("^sfx%s+(%S+)")
            table.insert(instructions, { op = "PLAY_SFX", a = file })
        elseif l == "end" then
            table.insert(instructions, { op = "END" })
        elseif l == "save_menu" then
            table.insert(instructions, { op = "SAVE_MENU" })
        elseif l == "load_menu" then
            table.insert(instructions, { op = "LOAD_MENU" })
        elseif l:match("^save%s+(%d+)") then
            local slot = l:match("^save%s+(%d+)")
            table.insert(instructions, { op = "SAVE", a = slot })
        elseif l:match("^load%s+(%d+)") then
            local slot = l:match("^load%s+(%d+)")
            table.insert(instructions, { op = "LOAD", a = slot })
        else
            print("[COMPILER] Unknown top-level: " .. l)
        end

        i = i + 1
        ::continue::
    end

    return { instructions = instructions }
end
