# Omawrite-Standalone

A minimal, distraction-free Markdown writing application built with Qt Quick and C++.

This is a fork of the original Omawrite editor. The original project was built exclusively for the Omarchy desktop environment and relied on its D-Bus services for theming and configuration. This fork decouples the editor from those native desktop dependencies, allowing it to be compiled and run as a standalone application on any Linux distribution or any operating system that supports Qt 6.

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />

## Standalone Features & Enhancements

- **Distro-Independent:** Fully decoupled from Omarchy D-Bus scaling and theming services.
- **Custom Configuration:** Supports live-reloading via a standard `~/.config/omawrite/config.toml` file.
- **Custom Themes & Fonts:** Allows custom hex colors for background, foreground, accents, and selections, as well as custom font families.
- **Distraction-Free UI:** Native OS title bars have been stripped away for a frameless writing experience.
- **Dynamic Zooming HUD:** Zoom in and out dynamically using `Ctrl++` and `Ctrl+-`, with a transient heads-up display.
- **Robust Syntax Highlighting:** Includes GitHub-style code blocks with seamless backtick hiding, native syntax highlighting for Bash, Nim, Python, C/C++, JSON, TOML, JavaScript, and Ruby, and proper rendering of inline markdown formatting in Qt 6 RichText mode.
- **Upstream Integrations:** Manually ported upstream enhancements including [PR #8](https://github.com/omacom-io/omawrite/pull/8) (Markdown link URLs on hover), [PR #9](https://github.com/omacom-io/omawrite/pull/9) (Fix false unsaved changes), [PR #12](https://github.com/omacom-io/omawrite/pull/12) (Smart Return line breaks), [PR #13](https://github.com/omacom-io/omawrite/pull/13) (Robust new file handling), and [PR #15](https://github.com/omacom-io/omawrite/pull/15) (Strikethrough formatting with smart toggle logic).

## Build and Install

### Requirements

- Qt 6 libraries: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- `qmake` (Qt 6)
- A C++17 compatible compiler (e.g., `clang++` or `g++`)
- `make`

### Building from Source

1. Clone the repository and navigate into the project directory.
2. Run the provided build script (which automatically detects `qmake` or `qmake6` and compiles the project into a `build/` directory):
   ```bash
   ./bin/build
   ```
3. Run the compiled binary:
   ```bash
   ./build/omawrite
   ```

## Configuration

Upon the first run, the application will create a configuration file at `~/.config/omawrite/config.toml`. You can edit this file to customize the editor in real time without restarting:

```toml
# Omawrite Custom Theme & Configuration

# Text scaling factor. 1.0 is default.
text_scale = 1.0

# Font family. The default is "iA Writer Mono S".
# font_family = "iA Writer Mono S"

# --- Tokyo Night Theme Example ---
# background = "#1a1b26"
# foreground = "#c0caf5"
# accent = "#7aa2f7"
# selection = "#33467C"
```

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file.
- `Ctrl+N` opens a new Omawrite window.
- `Ctrl+Q` or `Ctrl+W` closes the current window.
- `Ctrl++` / `Ctrl+-` dynamically zooms the text scale in and out.
- `Ctrl+0` resets the text scale to 1.0.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` or `F11` toggles fullscreen. 
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+Shift+X` toggle bold, italic, and strikethrough Markdown. `Ctrl+K` inserts a link.
- `Ctrl+?` displays the keyboard shortcut reference HUD.

## License

The bundled `iA Writer Mono` font is under the SIL Open Font License 1.1; see `fonts/OFL.txt`. 
The font is copyright Information Architects Inc. and based on IBM Plex, copyright IBM Corp.
