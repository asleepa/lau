# Lau

This is the repository of Lau development code. Unlike official Lau (plant with coding), it is based on the source code of Lua's 5/9/2026 development code (after 5.5) and C. It is supposed to be as accurate as possible to the game version Lau, however if there are any inconsistencies or bugs please feel free to submit a pull request.

Download official Lua releases from [Lua.org](https://www.lua.org/download.html).

## Changes
[/] Changed local → varol
[/] Changed function → func
[/] Changed require → req
[/] Changed nil → null
[/] Changed pairs → inpairs
[-] Disabled requirement for "in" keyword in for loops (forstat/forlist)
[-] Removed global
[-] Removed goto
[-] Removed ipairs