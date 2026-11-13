# Study

![Study showing a rendered Markdown file next to its folder](docs/screenshot.png)

Study is a reader for folders of Markdown. Open a folder and its files appear in a tree on the left. Click a Markdown file and it opens rendered in a tab, with tables, task lists, highlighted code, math and images. When you want to edit, Ctrl+Shift+V switches the tab to its source and back.

It follows the Omarchy theme and language live, and it's the default app for `.md` files. A second launch, such as double-clicking another file, opens a tab in the running window.

## Using it

- **Opening files:** one click in the sidebar *peeks* at a file in a temporary (italic) tab, which the next peek replaces. A double click, or an edit, keeps the tab.
- **Other files:** files Study can't open stay visible in the tree but dimmed.
- **Links:** links to other Markdown files open them in a tab, `#headings` scroll, and web links open in your browser.
- **Editing:** double-clicking the rendered text jumps to that spot in the source. Edits show a dot on the tab, and Ctrl+S saves (atomically).
- **Changes on disk:** if a file changes elsewhere, a clean tab reloads by itself, and an edited tab asks whether to reload or keep your edits.
- **Session:** the folder, tabs, modes and scroll positions come back the next time you start Study.

| Keys | Action |
|---|---|
| Ctrl+O | Open a folder |
| Ctrl+P | Go to a Markdown file |
| Ctrl+Shift+V | Switch between preview and source |
| Double click | Edit the source at that spot |
| Ctrl+S | Save |
| Ctrl+F | Find (in the editor: find and replace) |
| Ctrl+W | Close the tab |
| Ctrl+Shift+T | Reopen the last closed tab |
| Ctrl+Tab, Ctrl+PgDn | Next tab |
| Ctrl+Shift+Tab, Ctrl+PgUp | Previous tab |
| Alt+1 … Alt+9 | Go to tab |
| Ctrl+K Ctrl+B | Show or hide the sidebar |
| Ctrl+= / Ctrl+- / Ctrl+0 | Zoom in, out, reset |
| Ctrl+Z / Ctrl+Shift+Z | Undo, redo (editing) |
| ? or F1 | All shortcuts |
| Q (outside the editor), Ctrl+Q | Quit, asking first about unsaved edits |

## Building

Building needs `qmake6`, the Qt 6 base, declarative, WebEngine, WebChannel, SVG and Wayland modules, and Node.js for the page bundle.

```sh
bin/build      # web/ bundle + qmake6 + make → build/study
bin/test       # Qt Test, offscreen
bin/install    # makepkg -fsi, then makes Study the default for .md files
```

The window is Qt Quick (`src/*.qml`), and the logic is C++ (`src/backend.cpp`). The document view is one web page (`web/app.js`) inside Qt WebEngine. It renders with markdown-it, the parser VS Code uses, in a Web Worker, patches the page with morphdom, and edits with CodeMirror 6. The page and the window talk over Qt WebChannel (`src/bridge.h`).
