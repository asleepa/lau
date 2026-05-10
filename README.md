# Lau

This is the repository of Lau development code. Unlike official Lau (plant with coding), it is based on the source code of Lua's 5/9/2026 development code (after 5.5) and C. It is supposed to be as accurate as possible to the game version Lau, however if there are any inconsistencies or bugs please feel free to submit a pull request.

Download official Lua releases from [Lua.org](https://www.lua.org/download.html).

## Changes
- Added concatenation alongside pre-existing int & float addition to op_arithI (immediate operands) and op_arithf_aux (auxiliary function for floats and others; we include string concatenation here because op_arith_aux first checks if both sides of the operation are integers and otherwise calls this instead). Other arithmetic operation functions such as op_arithK (K operands) do not need changed as they point to op_arith_aux -> op_arithf_aux.
- Changed local → varol
- Changed function → func
- Changed require → req
- Changed nil → null
- Changed pairs → inpairs
- Disabled requirement for "in" keyword in for loops (forstat/forlist)
- Removed global
- Removed goto
- Removed ipairs
- Removed .. (Lua concatenation)