# tedit-cosmo

A FOSS clone of tEditor (MASM64 SDK editor) built with Cosmopolitan C. 
Single binary runs on Windows, Linux, and macOS across AMD64 and AArch64 architectures.

## Philosophy

tedit-cosmo follows tEditor's core design principle: **a minimal, portable core with maximum extensibility through configuration files and external tools**.

Instead of bloating the binary with every possible feature:

- **Core** handles text editing, history, and basic operations
- **INI files** configure menus, builds, backups, and tools
- **External programs** handle specialized tasks (compilers, archivers, cloud sync)
- **Scripts** automate repetitive workflows

This keeps the binary small (<1MB), truly portable, and easy to maintain.

### Why This Approach?

| Traditional IDE | tedit-cosmo |
|-----------------|-------------|
| Ships with bundled Git, debugger, linter... | Uses your existing tools |
| 500MB+ install size | <1MB single binary |
| Plugin system with API versioning headaches | INI config + shell commands |
| Breaks when dependencies update | External tools update independently |
| Complex build system | `make` produces one portable file |

## Features

- **Cross-platform**: Windows, Linux, macOS (single APE binary)
- **GUI via cimgui**: Dear ImGui C bindings for consistent UI everywhere
- **First-class languages**: Cosmopolitan C, AMD64, AArch64, MASM64, MASM32
- **Persistent history**: Every edit saved to `.tedit-history` for crash-proof undo
- **Extensible backup**: Define destinations in `backup.ini`, use any tool (rclone, aws, curl)
- **INI-based menus**: Add commands without recompiling
- **Build integration**: Configure compilers via `build.ini`
- **Template system**: Insert boilerplate from `textape/` directory

## Quick Start

```bash
# Clone
git clone https://github.com/user/tedit-cosmo.git
cd tedit-cosmo

# Build CLI version (most portable)
make cli

# Run
./tedit.com myfile.c
```

## Extensibility

### Menu System (menuini.txt)

Define custom menus with variable substitution:

```ini
[&Build]
Compile,cosmocc -O2 -o {n}.com {e}
Run,./{n}.com
Debug,gdb {n}.com

[&Tools]
Format Code,clang-format -i {e}
Count Lines,wc -l {e}
```

### Variables

| Variable | Expands To |
|----------|------------|
| `{b}` | Binary/install directory |
| `{e}` | Current file (full path) |
| `{n}` | File name without extension |
| `{p}` | Project directory |

### Build Configurations (build.ini)

Per-project build settings:

```ini
[build]
build_cmd=cosmocc -O2 -o {n}.com {e}
run_cmd=./{n}.com
clean_cmd=rm -f {n}.com
```

### Backup System (backup.ini)

Define backup destinations as command templates:

```ini
[destinations]
local=cp "{archive}" ~/backups/
s3=aws s3 cp "{archive}" s3://mybucket/backups/
gdrive=rclone copy "{archive}" gdrive:backups/

[settings]
threshold_mb=100   ; prompt when history exceeds this
```

Backup variables: `{archive}`, `{p}`, `{e}`, `{n}`, `{date}`, `{time}`

### Persistent History

Every edit is immediately written to a `.tedit-history` file alongside your source:

```
myfile.c                  <- your source code
myfile.c.tedit-history    <- operation log (binary)
```

Benefits:
- **Crash-proof**: History survives crashes, power loss
- **Cross-session**: Undo across editing sessions  
- **Audit trail**: Complete log of file evolution
- **No memory limits**: Disk-based, not RAM-limited

CLI history commands:
```bash
tedit --history-info myfile.c      # Show history stats
tedit --history-export myfile.c out.txt   # Export readable log
tedit --history-compact myfile.c   # Archive and reset history
tedit --history-clear myfile.c     # Delete all history
```

## Building

### Requirements

- [Cosmopolitan](https://github.com/jart/cosmopolitan) toolchain (`cosmocc`)

### CLI Build (Recommended)

```bash
make cli
```

Produces `tedit.com` - runs on Windows, Linux, macOS.

### GUI Build

Requires cimgui (vendored):

```bash
make setup-deps   # Clone cimgui
make gui
```

## Project Structure

```
tedit-cosmo/
├── include/          # Headers
│   ├── editor.h      # Editor state, text operations
│   ├── history.h     # Persistent undo/redo
│   ├── backup.h      # Archive and backup system
│   └── ...
├── src/              # Implementation
│   ├── platform/     # Platform backends (CLI, cimgui)
│   ├── history.c     # Write-through history log
│   ├── backup.c      # Tar archiver, backup destinations
│   └── ...
├── textape/          # Templates and snippets
├── menuini.txt       # Menu definitions
├── build.ini         # Build configuration
├── backup.ini        # Backup destinations
└── Makefile
```

## Command Line

```
tedit [options] [file]

Options:
  --help                      Show help
  --version                   Show version
  --history-info <file>       Show history statistics
  --history-export <file> <out>  Export history to text
  --history-compact <file>    Archive and reset history
  --history-clear <file>      Clear all history
  --backup <destination>      Backup project to destination
  --backup-list               List configured destinations
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow the existing code style (C11, 4-space indent)
4. Submit a pull request

## License

MIT License

## Acknowledgments

- [tEditor](http://www.masm32.com/) - Original inspiration
- [jart/cosmopolitan](https://github.com/jart/cosmopolitan) - Portable C library
- [cimgui](https://github.com/ludoplex/cimgui) - Dear ImGui C bindings
