# Omawrite Standalone

A minimal, distraction-free Markdown writing application built with Qt Quick and C++.

Originally based on [omacom-io/omawrite](https://github.com/omacom-io/omawrite), **Omawrite Standalone** uses native Qt window controls and optional Linux desktop-portal integration. Other Qt 6 platforms fall back to Qt's system appearance support without requiring D-Bus.

![Main Canvas and Image Preview](assets/main_and_preview.png)

<p align="center">
  <img src="assets/focus_mode.png" width="48%" alt="Focus Mode & Paragraph Dimming" />
  <img src="assets/stats_hud.png" width="48%" alt="Extended Writing Statistics Popover" />
</p>

<p align="center">
  <img src="assets/headings_hud.png" width="48%" alt="Document Heading Navigation HUD" />
  <img src="assets/code_highlighting.png" width="48%" alt="Code Block Syntax Highlighting" />
</p>

---

## Features & Enhancements

### Ported Upstream Pull Requests
We have ported and integrated key upstream pull requests from [omacom-io/omawrite](https://github.com/omacom-io/omawrite/pulls):
- **[PR #8](https://github.com/omacom-io/omawrite/pull/8): Markdown link URLs on hover** — Hovering over Markdown links shows destination targets with `Ctrl+Click` to open in browser.
- **[PR #9](https://github.com/omacom-io/omawrite/pull/9): Fix false unsaved changes** — Prevents false-positive modified dirty state on unmodified documents.
- **[PR #12](https://github.com/omacom-io/omawrite/pull/12): Smart Return line breaks** — Automatically continues numbered, bulleted, and blockquote lists on `Enter`.
- **[PR #13](https://github.com/omacom-io/omawrite/pull/13): Robust new file handling** — Clean file lifecycle handling across new windows and uncommitted buffers.
- **[PR #15](https://github.com/omacom-io/omawrite/pull/15): Strikethrough formatting** — Adds `Ctrl+Shift+X` markdown strikethrough (`~~text~~`) with smart wrap/unwrap logic.
- **[PR #17](https://github.com/omacom-io/omawrite/pull/17): Focus mode** — Adds typewriter scrolling and paragraph dimming via `Ctrl+Shift+T`.
- **[PR #18](https://github.com/omacom-io/omawrite/pull/18): Viewport clamp** — Prevents editor clipping and horizontal text overflow in narrow window sizes.
- **[PR #22](https://github.com/omacom-io/omawrite/pull/22): File explorer sidebar** — Adds collapsible folder file tree view via `Ctrl+E`.
- **[PR #34](https://github.com/omacom-io/omawrite/pull/34): TOML inline comments** — Reads annotated theme values correctly.
- **[PR #43](https://github.com/omacom-io/omawrite/pull/43): Opaque footer** — Keeps document text clear of footer controls.
- **[PR #45](https://github.com/omacom-io/omawrite/pull/45): Rendered preview** — Toggles an in-app Markdown preview with `Ctrl+/`.
- **[PR #48](https://github.com/omacom-io/omawrite/pull/48): Literal inline code** — Prevents Markdown styling inside code spans.
- **[PR #50](https://github.com/omacom-io/omawrite/pull/50): Network-share saving** — Falls back safely when atomic saves are unavailable.
- **[PR #52](https://github.com/omacom-io/omawrite/pull/52): CJK word counts** — Counts Han and kana writing accurately.
- **[PR #55](https://github.com/omacom-io/omawrite/pull/55): Keyboard-scroll feedback** — Shows the scrollbar when caret navigation scrolls the document.
- **[PR #57](https://github.com/omacom-io/omawrite/pull/57): Qt 6.4 compatibility** — Builds on Ubuntu 24.04 LTS.
- **[PR #61](https://github.com/omacom-io/omawrite/pull/61): Dialog shortcuts** — Adds direct Cancel, Discard, and Save keys.
- **[PR #5](https://github.com/omacom-io/omawrite/pull/5): Outdented headings** — Places heading markers in a gutter so heading text aligns with body copy.
- **[PR #2](https://github.com/omacom-io/omawrite/pull/2): Code panel colors** — Uses a theme-aware background for inline and fenced code without replacing the newer parser.
- **[PR #115](https://github.com/omacom-io/omawrite/pull/115): Nested lists** — `Tab` / `Shift+Tab` nest and lift list items, alone or across a selection.
- **[PR #89](https://github.com/omacom-io/omawrite/pull/89): Stray asterisks** — `2 * 3 * 4` and `* bullet *emphasis*` no longer pair stray `*` as italics.
- **[PR #67](https://github.com/omacom-io/omawrite/pull/67): Print in points** — Printed text matches the editor's size instead of using device dots.
- **[PR #125](https://github.com/omacom-io/omawrite/pull/125): Find bar and input methods** — `Ctrl+F` keeps working after closing the find bar with an input method.

### Standalone Exclusive Features
- **Distro-Independent & Portal Theming:** Native desktop integration via standard XDG desktop portals or standalone config without requiring Omarchy D-Bus services.
- **Document Heading Navigation HUD (`Ctrl+Shift+O`):** Popover outline of all document headings (`#` through `######`) with instant top-aligned scrolling.
- **Image & Animated GIF Hover Previews:** Hovering over Markdown images (`![alt](url)`) or HTML `<img src="...">` tags displays a scaled popover supporting local files, remote URLs, and looping animated GIFs.
- **Auto-Pairing & Selection Wrapping:** Automatically pairs brackets `()`, `[]`, `{}`, quotes `""`, `''`, and wraps highlighted selections without interfering with triple-backtick fences.
- **Interactive Task Lists:** Smart `- [ ]` and `- [x]` task continuation on `Enter`, with instant toggling via `Ctrl+Shift+C`.
- **Extended Writing Statistics HUD:** Hovering over the bottom-right word count reveals a detailed card with word count, character count, paragraph count, and reading time.
- **Persistent Text Zooming HUD:** Adjust editor text independently (`Ctrl++` / `Ctrl+-` / `Ctrl+0`) with a transient scale overlay.
- **Live TOML Theming & Custom Fonts:** Supports live-reloading colors and fonts via `~/.config/omawrite/config.toml`.
- **Syntax Highlighting for 20 Languages:** Native code block syntax highlighting for Bash/Shell, C, C++, C#, Go, Java, JavaScript, JSON, Lua, Nim, Pascal, PHP, Python, Ruby, Rust, SQL, TOML, TypeScript, YAML, and Zig.

---

## Configuration & Theming

Omawrite Standalone supports live hot-reloading via `~/.config/omawrite/config.toml` without requiring an application restart.

<p align="center">
  <img src="assets/config_example.png" width="70%" alt="Omawrite TOML Configuration Example" />
</p>

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
code_background = "#24283b"
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
| `Ctrl+/` | Toggle Rendered Markdown Preview |
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

The unsaved-changes dialog accepts `C`, `D`, and `S` for Cancel, Discard, and Save.

### Command Line

```text
omawrite [FILE]
omawrite --help
```

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
