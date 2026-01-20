# Extending tedit-cosmo

This guide covers how to customize and extend tedit-cosmo without modifying source code.

## Table of Contents

1. [Menu System](#menu-system)
2. [Build Configurations](#build-configurations)
3. [Backup System](#backup-system)
4. [Syntax Highlighting](#syntax-highlighting)
5. [QSE Scripts](#qse-scripts)
6. [Variable Reference](#variable-reference)

---

## Menu System

### Configuration File: `menuini.txt`

The menu system is defined in `menuini.txt` using a simple INI-like format.

### Structure

```ini
[&MenuName]
Item Label,command
Submenu Item,subcommand
-                           ; Separator
Another Item,another_command
```

The `&` before a letter creates a keyboard accelerator (Alt+M for &Menu).

### Built-in Commands

| Command | Description |
|---------|-------------|
| `new` | Create new file |
| `open` | Open file dialog |
| `save` | Save current file |
| `saveas` | Save as dialog |
| `close` | Close current file |
| `exit` | Exit application |
| `undo` | Undo last operation |
| `redo` | Redo last undone operation |
| `cut` | Cut selection |
| `copy` | Copy selection |
| `paste` | Paste from clipboard |
| `find` | Open find dialog |
| `replace` | Open replace dialog |
| `goto` | Go to line |
| `build` | Run build command |
| `run` | Run executable |
| `lang <mode>` | Set syntax mode |
| `history <cmd>` | History operations |
| `backup <dest>` | Run backup |

### External Commands

Any command not recognized as built-in is executed as a shell command with variable substitution:

```ini
[&Tools]
Open in Explorer,explorer {p}
Terminal Here,cmd /k "cd /d {p}"
Format Code,clang-format -i {e}
```

### Example: Custom Build Menu

```ini
[&Build]
Compile (Debug),cosmocc -g -O0 {e} -o {n}.com
Compile (Release),cosmocc -O3 {e} -o {n}.com
-
Run,{n}.com
Run with Args,{n}.com --help
-
Clean,del {p}\*.o {p}\*.com
```

---

## Build Configurations

### Configuration File: `build.ini`

Project-specific build settings that override global defaults.

### Structure

```ini
[project]
name=MyProject
type=executable

[build]
compiler=cosmocc
flags=-O2 -Wall
include=-I./include -I./deps
libs=-lm
output={n}.com

[run]
args=--verbose
workdir={p}

[debug]
debugger=gdb
args=-tui {n}.com
```

### Sections

#### `[project]`
- `name` - Project display name
- `type` - `executable`, `library`, or `script`

#### `[build]`
- `compiler` - Compiler command (default: `cosmocc`)
- `flags` - Compiler flags
- `include` - Include paths
- `libs` - Libraries to link
- `output` - Output file pattern

#### `[run]`
- `args` - Arguments passed to executable
- `workdir` - Working directory for execution

#### `[debug]`
- `debugger` - Debugger command (default: `gdb`)
- `args` - Debugger arguments

### Multiple Configurations

Use numbered sections for multiple build configs:

```ini
[build.debug]
flags=-g -O0 -DDEBUG

[build.release]
flags=-O3 -DNDEBUG

[build.asan]
flags=-fsanitize=address -g
```

---

## Backup System

### Configuration File: `backup.ini`

Defines backup destinations and archive settings.

### Structure

```ini
[settings]
threshold_mb=100
archive_format=tar.gz
temp_dir=./temp

[destinations]
local=cp "{archive}" "{project_path}/backups/{n}_{date}.tar.gz"
cloud=rclone copy "{archive}" "remote:backups/"

[schedule]
interval=3600
destination=local
```

### Settings Section

| Key | Description | Default |
|-----|-------------|---------|
| `threshold_mb` | Prompt when history exceeds this size | 100 |
| `archive_format` | Archive format (`tar.gz`, `zip`) | tar.gz |
| `temp_dir` | Temporary directory for archives | ./temp |

### Destinations Section

Each destination is a command template. Variables are substituted at runtime:

| Variable | Description |
|----------|-------------|
| `{archive}` | Full path to created archive |
| `{project_path}` | Project root directory |
| `{n}` | Project/file name |
| `{date}` | Current date (YYYY-MM-DD) |
| `{time}` | Current time (HH-MM-SS) |

### Example Destinations

```ini
[destinations]
; Local backup
local=cp "{archive}" "D:/Backups/{n}_{date}.tar.gz"

; SMB/Network share via rclone
smb=rclone copy "{archive}" "mysmb:tedit-backups/"

; AWS S3
s3=aws s3 cp "{archive}" "s3://mybucket/backups/{n}_{date}.tar.gz"

; Google Drive via rclone
gdrive=rclone copy "{archive}" "gdrive:tedit-backups/"

; FTP server
ftp=curl -T "{archive}" "ftp://user:pass@ftp.example.com/{n}_{date}.tar.gz"

; 7-Zip encrypted archive
encrypted=7z a -p{PASSWORD} "{project_path}/secure/{n}_{date}.7z" "{archive}"
```

### Schedule Section

| Key | Description |
|-----|-------------|
| `interval` | Seconds between auto-backups (0 = manual only) |
| `destination` | Default destination for scheduled backups |

---

## Syntax Highlighting

### Configuration File: `syntax.ini` (Planned)

Define custom syntax highlighting rules.

### Structure

```ini
[language.mylang]
extensions=.ml,.myl
line_comment=//
block_comment_start=/*
block_comment_end=*/

[language.mylang.keywords]
fn,let,mut,if,else,while,for,return,struct,enum

[language.mylang.types]
int,float,string,bool,void,auto

[language.mylang.operators]
+,-,*,/,%,=,==,!=,<,>,<=,>=,&&,||,!
```

### Built-in Language Modes

| Mode | Extensions | Description |
|------|------------|-------------|
| `cosmo` | `.c`, `.h` | Cosmopolitan C |
| `amd64` | `.s`, `.asm` (x86) | AMD64 Assembly |
| `aarch64` | `.s`, `.asm` (arm) | AArch64 Assembly |
| `masm64` | `.asm` (ml64) | MASM64 Assembly |
| `masm32` | `.asm` (ml) | MASM32 Assembly |

---

## QSE Scripts

### Script Location

Place `.qse` files in the project directory or `scripts/` subdirectory.

### Basic Syntax

```qse
; Comment
MACRO name
    ; commands
ENDM

; Variables
SET var=value
SET result={var}+1

; File operations
OPEN "filename.c"
SAVE
CLOSE

; Text operations
FIND "pattern"
REPLACE "old" "new"
GOTO 100

; Control flow
IF condition
    ; ...
ENDIF
```

### Example: Auto-format on Save

```qse
; auto-format.qse
; Run clang-format before saving

MACRO BeforeSave
    SHELL clang-format -i {e}
ENDM
```

### Example: Project Template

```qse
; new-project.qse
; Create a new Cosmopolitan C project

SET name={INPUT "Project name:"}

MKDIR {name}
MKDIR {name}/src
MKDIR {name}/include

CREATE {name}/src/main.c
INSERT "#include <cosmo.h>"
INSERT ""
INSERT "int main(int argc, char **argv) {"
INSERT "    return 0;"
INSERT "}"
SAVE

CREATE {name}/Makefile
INSERT "CC=cosmocc"
INSERT "all: {name}.com"
INSERT "{name}.com: src/main.c"
INSERT "\t$(CC) -o $@ $<"
SAVE

MESSAGE "Project '{name}' created!"
```

---

## Variable Reference

### File Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `{e}` | Full file path | `C:\proj\src\main.c` |
| `{n}` | File name without extension | `main` |
| `{x}` | File extension | `.c` |
| `{d}` | File directory | `C:\proj\src` |
| `{p}` | Project directory | `C:\proj` |

### Path Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `{b}` | Binary/tools directory | `C:\tedit-cosmo\bin` |
| `{h}` | Home directory | `C:\Users\user` |
| `{t}` | Temp directory | `C:\Users\user\AppData\Local\Temp` |

### Date/Time Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `{date}` | Current date | `2026-01-19` |
| `{time}` | Current time | `14-30-45` |
| `{year}` | Current year | `2026` |
| `{month}` | Current month | `01` |
| `{day}` | Current day | `19` |

### Build Variables

| Variable | Description |
|----------|-------------|
| `{cc}` | C compiler |
| `{cflags}` | Compiler flags |
| `{ldflags}` | Linker flags |
| `{out}` | Output file |
| `{in}` | Input file(s) |

### Environment Variables

Access environment variables with `$VAR` or `${VAR}`:

```ini
[build]
compiler=$CC
flags=${CFLAGS} -Wall
```

---

## Tips and Best Practices

### 1. Keep configurations in version control

Track `menuini.txt`, `build.ini`, and `backup.ini` alongside your code.

### 2. Use project-local configs

Place project-specific configs in the project root. Global configs in the tedit-cosmo installation directory serve as defaults.

### 3. Chain commands

Use shell operators to chain commands:

```ini
Build & Run,cosmocc {e} -o {n}.com && {n}.com
```

### 4. Cross-platform commands

Use Cosmopolitan tools or write portable scripts:

```ini
; Works on all platforms with cosmocc
Format,cosmocc -E -P {e} > formatted.c
```

### 5. Document your extensions

Add comments to your INI files:

```ini
; Custom menu for embedded development
; Requires: arm-none-eabi-gcc, openocd
[&Embedded]
Flash,openocd -f board.cfg -c "program {n}.elf verify reset exit"
```

---

## Troubleshooting

### Command not found

Ensure the command is in your PATH or use full paths:

```ini
Compile,C:\cosmos\bin\cosmocc.exe {e} -o {n}.com
```

### Variable not expanding

Check for typos. Variables are case-sensitive. Use `{e}` not `{E}`.

### Menu not appearing

Verify `menuini.txt` syntax:
- Section headers need `[` and `]`
- Items need exactly one comma separator
- No trailing whitespace

### Backup failing

1. Test the command manually in terminal
2. Check destination permissions
3. Verify external tools (rclone, aws-cli) are installed

---

## Contributing

To add new features to the extension system:

1. Propose the feature in a GitHub issue
2. Follow the existing INI-based pattern
3. Document new variables and commands
4. Add examples to this guide

See [CONTRIBUTING.md](./CONTRIBUTING.md) for code contribution guidelines.

