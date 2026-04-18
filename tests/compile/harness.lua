#!/usr/bin/env lua
-- tests/compile/harness.lua
-- Snapshot tests for scripts/compiler.lua: run compile() on each input and
-- diff the serialized Instruction[] against a recorded expected file.
-- Usage:
--   lua tests/compile/harness.lua            run tests, exit 1 on any mismatch
--   lua tests/compile/harness.lua --update   regenerate expected files from current compiler

local function script_dir()
    local p = arg[0]
    return p:match("(.*/)") or "./"
end

local HERE = script_dir()
local ROOT = HERE .. "../../"
local INPUTS = HERE .. "inputs/"
local EXPECTED = HERE .. "expected/"

dofile(ROOT .. "scripts/compiler.lua")

local function read_file(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local s = f:read("*a")
    f:close()
    return s
end

local function write_file(path, s)
    local f = assert(io.open(path, "w"))
    f:write(s)
    f:close()
end

local function format_value(v)
    if type(v) == "boolean" then return v and "true" or "false" end
    return tostring(v)
end

local function serialize_instruction(ins)
    local keys = {}
    for k in pairs(ins) do keys[#keys+1] = k end
    table.sort(keys)
    local parts = {}
    for _, k in ipairs(keys) do
        parts[#parts+1] = k .. "=" .. format_value(ins[k])
    end
    return table.concat(parts, " ")
end

local function serialize(result)
    local lines = {}
    for _, ins in ipairs(result.instructions or {}) do
        lines[#lines+1] = serialize_instruction(ins)
    end
    return table.concat(lines, "\n") .. "\n"
end

local function list_inputs()
    local files = {}
    local p = io.popen("ls '" .. INPUTS .. "' 2>/dev/null | sort")
    if not p then return files end
    for line in p:lines() do
        if line:match("%.crka$") then files[#files+1] = line end
    end
    p:close()
    return files
end

local update = false
for _, a in ipairs(arg) do
    if a == "--update" then update = true end
end

local pass, fail = 0, 0
local failures = {}

for _, name in ipairs(list_inputs()) do
    local input = assert(read_file(INPUTS .. name), "cannot read " .. name)
    local result = compile(input)
    local actual = serialize(result)
    local exp_path = EXPECTED .. name:gsub("%.crka$", ".txt")

    if update then
        write_file(exp_path, actual)
        io.write(string.format("UPDATE  %s\n", name))
    else
        local expected = read_file(exp_path)
        if not expected then
            fail = fail + 1
            failures[#failures+1] = {name = name, reason = "no expected file at " .. exp_path}
        elseif actual == expected then
            pass = pass + 1
            io.write(string.format("PASS    %s\n", name))
        else
            fail = fail + 1
            failures[#failures+1] = {name = name, expected = expected, actual = actual}
        end
    end
end

if update then
    io.write(string.format("\nRegenerated expected files in %s\n", EXPECTED))
    os.exit(0)
end

for _, f in ipairs(failures) do
    io.write(string.format("\nFAIL    %s\n", f.name))
    if f.reason then
        io.write("  " .. f.reason .. "\n")
    else
        io.write("--- expected ---\n" .. f.expected .. "--- actual ---\n" .. f.actual .. "---\n")
    end
end

io.write(string.format("\n%d passed, %d failed\n", pass, fail))
os.exit(fail == 0 and 0 or 1)
