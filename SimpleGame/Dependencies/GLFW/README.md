# GLFW dependency

This directory contains the official GLFW 3.5.1 source required by the game.

- Source: https://github.com/glfw/glfw/releases/tag/3.5.1
- License: `LICENSE.md`
- Build mode: compiled directly into `SimpleGame` as a static Win32 backend
- Runtime DLL: none

The Visual Studio project disables precompiled headers for GLFW's C sources and
defines `_GLFW_WIN32`. Do not add a separately installed GLFW library to the
linker settings, because that would duplicate the symbols compiled here.

The game requests an OpenGL 3.3 compatibility context. This is intentional:
the current text renderer uses Windows GDI glyph generation and OpenGL display
lists. GLFW only owns the window and context, while font selection and Korean
fallback behavior remain in `PrototypeRenderer`.
