# tedit-cosmo

A FOSS clone of tEditor (MASM64 SDK editor) built with Cosmopolitan C.

## Features

- Cross-platform: Windows, Linux, macOS (single APE binary)
- GUI via cimgui (Dear ImGui C bindings)
- First-class support for: Cosmopolitan C, AMD64, AArch64, MASM64, MASM32
- INI-based configurable menus
- Build integration (cosmocc)
- Template/snippet system
- QSE script engine (subset)

## Building

```bash
# Clone with submodules
git clone --recursive https://github.com/user/tedit-cosmo.git
cd tedit-cosmo

# Build with cosmocc
make

# Or build CLI-only version
make cli
```

## Dependencies

- [Cosmopolitan](https://github.com/jart/cosmopolitan) - Build toolchain
- [cimgui](https://github.com/ludoplex/cimgui) - GUI (vendored)
- [GLFW](https://www.glfw.org/) - Windowing (optional, for GUI)

## License

MIT License

