# Lau
<img width="1920" height="1032" alt="Testing LAU image" src="https://github.com/user-attachments/assets/126802fe-e135-4fca-950b-4d37c4e94621" /><br>

This is the repository of Lau development code. Unlike official Lau (plant with coding), it is based on the source code of Lua's 5/9/2026 development code (after 5.5) and C. It is supposed to be as accurate as possible to the game version Lau, however if there are any inconsistencies or bugs please feel free to submit a [pull request](https://github.com/asleepa/LAU/pulls).

Download official Lua releases from [Lua.org](https://www.lua.org/download.html).

## Installation
1. Download the zip of the repository and extract it so that it is a folder with the makefile and .C files.
<br>Alternatively, if you have git installed use: `git clone https://github.com/asleepa/LAU.git`
2. If you are on Linux, skip this step.
<br>Download [choco](https://github.com/chocolatey/choco/releases/tag/2.7.1) (.msi for the Windows installer) and finish setting it up.
3. Open a terminal and cd to the LAU folder:
<br>`cd C:\Users\user\Desktop\LAU` - replace the path after cd with your path to LAU
4. If you are on Linux, skip this step. Make comes pre-installed with most Linux distributions.
<br>Install GNU make via choco: `choco install make`
6. Close and reopen the terminal, again cd'ing to your LAU path (step 3).
7. If you are on Linux: delete the "makefile" file and rename the "makefile-linux" file to "makefile"
8. Run one simple command: `make`
9. You should have a "lau.exe" in the folder now. You can run commands such as:
<br>`lau` which will open a command-line interactive version which allows you to do i.e. `varol x = "a" print(x)`
<br> or `lau tests/inpairs.lau` which allows you to execute full lau files.

If there were any issues during the installation process, please submit an [issue](https://github.com/asleepa/LAU/issues).

## Known Issues
- Concatenation being incorrect in some areas
  <br>1 + "1" + "1" results in 111 instead of 3,
  <br>1 + "a" + "b" results in a1b instead of 1ab.

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
