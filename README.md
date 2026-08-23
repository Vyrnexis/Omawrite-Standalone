# Omawrite Standalone

A minimal, distraction-free Markdown writing application built with Qt Quick and C++.

Originally based on [omacom-io/omawrite](https://github.com/omacom-io/omawrite), **Omawrite Standalone** uses native Qt window controls and optional Linux desktop-portal integration. Other Qt 6 platforms fall back to Qt's system appearance support without requiring D-Bus.

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />

---

## Features

### Distraction-Free Writing & Ergonomics
- **Native Standalone Window:** Standard movable, resizable window chrome with a clean writing canvas, typewriter scrolling, and focus mode (`Ctrl+Shift+T`).
- **Auto-Pairing & Selection Wrapping:** Automatically pairs brackets `()`, `[]`, `{}`, quotes `""`, `''`, and wraps highlighted text.
- **Interactive Task Lists:** Smart `- [ ]` and `- [x]` task continuation on `Enter`, with instant toggling via `Ctrl+Shift+C`.
- **Smart Return:** Automatically continues bulleted, numbered, and blockquote lists; double-return cleanly terminates lists.
- **Dynamic Text Scaling HUD:** Seamless zooming (`Ctrl++` / `Ctrl+-` / `Ctrl+0`) with a floating scale indicator.
- **Writing Statistics HUD:** Real-time word counter with a hover card displaying character count, paragraph count, and estimated reading time.

### Navigation & Media
- **Document Heading HUD (`Ctrl+Shift+O`):** Popover outline of all document headings (`#` through `######`) with instant top-aligned scrolling.
- **File Explorer Sidebar (`Ctrl+E`):** Toggleable folder sidebar with inline document creation and directory navigation.
- **Image & Animated GIF Hover Previews:** Hovering over Markdown image links (`![alt](url)`) or HTML `<img src="...">` tags displays a thumbnail popover supporting local files, remote URLs, and looping animated GIFs.
- **Link Previews:** Hovering over Markdown URLs displays full link targets; click or press Enter to open them.

### Syntax Highlighting & Rendering
- **GitHub-Flavored Code Blocks:** Seamless backtick hiding for clean reading, with syntax highlighting for:
  - Bash / Shell
  - C / C++
  - JavaScript
  - JSON
  - Nim
  - Python
  - Ruby
  - TOML
- **RichText Inline Formatting:** Bold (`Ctrl+B`), Italic (`Ctrl+I`), Strikethrough (`Ctrl+Shift+X`), and Links (`Ctrl+K`).

---

## Configuration & Theming

Omawrite Standalone supports live hot-reloading via `~/.config/omawrite/config.toml` without requiring an application restart.

```toml
# Omawrite Custom Theme & Configuration

# Text scaling factor (1.0 is default)
text_scale = 1.0

# Font family (default: "iA Writer Mono S")
font_family = "iA Writer Mono S"

# --- Tokyo Night Theme Example ---
background = "#1a1b26"
foreground = "#c0caf5"
accent     = "#7aa2f7"
selection  = "#33467c"
```

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl+S` | Save Document |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+O` | Open File |
| `Ctrl+E` | Toggle Folder Sidebar |
| `Ctrl+Shift+O` | Document Heading Navigation HUD |
| `Ctrl+Shift+T` | Toggle Focus Mode (Typewriter Centering) |
| `Ctrl+Shift+C` | Toggle Task Checkbox (`- [ ]` / `- [x]`) |
| `Ctrl+B` | Toggle Bold (`**text**`) |
| `Ctrl+I` | Toggle Italic (`*text*`) |
| `Ctrl+Shift+X` | Toggle Strikethrough (`~~text~~`) |
| `Ctrl+K` | Insert Markdown Link |
| `Ctrl+F` | Find in Document (`Enter` / `Shift+Enter` for next/prev) |
| `Ctrl+H` | Find and Replace |
| `Ctrl++` / `Ctrl+-` | Zoom In / Out |
| `Ctrl+0` | Reset Zoom (100%) |
| `F11` / `Super+F` | Toggle Fullscreen |
| `Ctrl+N` | Open New Window |
| `Ctrl+P` | Print Document |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / Redo |
| `Ctrl+?` | Display Keyboard Shortcut HUD |
| `Ctrl+Q` / `Ctrl+W` | Quit / Close Window |

---

## Building from Source

### Prerequisites
- Qt 6 libraries (`qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`)
- `qmake` or `qmake6`
- C++17 compatible compiler (`g++` or `clang++`)
- `make`

### Compilation
```bash
git clone https://github.com/Vyrnexis/Omawrite-Standalone.git
cd Omawrite-Standalone

# Build an optimized release binary into ./bin/
qmake6 omawrite.pro
make -j"$(nproc)"

# Run
./bin/omawrite
```

---

## Upstream & Acknowledgements
- Forked from [omacom-io/omawrite](https://github.com/omacom-io/omawrite).
- The bundled `iA Writer Mono` font is licensed under the SIL Open Font License 1.1 (see `fonts/OFL.txt`), copyright Information Architects Inc., based on IBM Plex (copyright IBM Corp.).
