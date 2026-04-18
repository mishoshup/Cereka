---
name: try-crka
description: Quickly compile a `.crka` snippet and dump its Instruction[] without booting the engine. Use when debugging the compiler, checking parser output, or showing what an op lowers to.
allowed-tools: Bash
---

# Compile a .crka snippet inline

Use the embedded Lua compiler directly — no C++ build, no SDL, no filesystem.

## One-liner — pipe a snippet
Replace the heredoc body with the `.crka` to test:

```bash
lua -e '
dofile("scripts/compiler.lua")
local src = [[
set gold 100
$ gold = gold * 2
if gold > 150
    narrate "rich"
endif
end
]]
local r = compile(src)
for _, ins in ipairs(r.instructions) do
  local parts = {}
  for k, v in pairs(ins) do parts[#parts+1] = k .. "=" .. tostring(v) end
  table.sort(parts)
  print(table.concat(parts, " "))
end
'
```

## Compile an existing file
```bash
lua -e '
dofile("scripts/compiler.lua")
local f = io.open(arg[1]); local src = f:read("*a"); f:close()
local r = compile(src)
for _, ins in ipairs(r.instructions) do
  local parts = {}
  for k, v in pairs(ins) do parts[#parts+1] = k .. "=" .. tostring(v) end
  table.sort(parts)
  print(table.concat(parts, " "))
end
' tests/compile/inputs/variables.crka
```

## Run from anywhere
The `dofile("scripts/compiler.lua")` path is **repo-relative**. From a different cwd, use the absolute path:
`dofile("/abs/path/to/cereka/scripts/compiler.lua")`.

## Errors
The compiler raises `error("line L col C: message")`. A failure prints `lua: line N col N: ...` — that's the actual user-facing diagnostic, not a Lua bug.

## When not to use this
- VM behavior questions (does the instruction *do* the right thing) — needs the runner, not the compiler.
- Multi-file `include`/`call` resolution — that happens in `src/compiler/vn_instruction.cpp::CompileFile`, not the Lua compiler. Use the C++ engine for those.
