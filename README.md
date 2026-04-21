# Sigil Qt 6

Sigil EPUB Editor — Qt 6 Migration based on Sigil 0.7.4

## About

This is a Qt 6.9.2 port of [Sigil](https://github.com/user-none/Sigil) (version 0.7.4), the free open-source EPUB editor. The original Sigil 0.7.4 was built with Qt 5.2 and Qt WebKit. This fork replaces WebKit with QWebEngineView and modernizes the codebase for Qt 6.

## Changes from Original

- **Qt 6.9.2** — Full migration from Qt 5.2
- **CodeView-Only** — BookView (WebKit WYSIWYG) replaced with read-only Preview panel (QWebEngineView)
- **Preview Panel** — Split view: CodeView editor + live Preview, debounced updates (400ms)
- **Block Translator** — Translate XHTML blocks via LLM (right-click in CodeView)
- **User Templates** — Customizable templates from `~/.sigil/`
- **Paste Image** — Paste images from clipboard as PNG via right-click in Images folder
- **13pt UI Font** — Application-wide font size for readability
- **Linux-only** — Windows/macOS platform code removed

## Requirements

- Qt 6.9.2 (QtWidgets, QtCore, QtWebEngineWidgets)
- CMake 3.16+
- C++11 compiler (GCC 12+)
- Xerces-C++ 3.1+
- Apache Xerces (bundled)

## Build

```bash
mkdir build && cd build
cmake ..
make -j4
```

The binary is at `build/bin/sigil`.

## Block Translator

Right-click any XHTML block in CodeView to translate it via an OpenAI-compatible LLM server.

**How it works:**
1. Right-click in CodeView → "Translate" → choose direction (EN→DE / DE→EN) and model
2. The current block is sent to the LLM with inline HTML tags encoded as placeholders
3. The translated text replaces the original block in-place, preserving all HTML tags and attributes

**Configuration:**

Server URL is set in Edit → Preferences → Translation. Default: `http://localhost:9080`

**Model Whitelist:**

If `~/.sigil/translator_models.json` exists, only the listed models appear in the menu. If the file is absent, all server models are shown.

```json
{
  "models": [
    "claude-sonnet-4-6",
    "gemini-2.5-flash",
    "gpt-4.1-mini"
  ]
}
```

**Compatible servers:** Any OpenAI-compatible API endpoint (e.g. [sigoREST](https://github.com/user-none/sigoREST), Ollama, LM Studio, vLLM, LiteLLM).

## User Templates

Create `~/.sigil/` and place template files:

```bash
mkdir -p ~/.sigil
```

| File | Purpose | Fallback |
|------|---------|----------|
| `~/.sigil/default.xhtml` | HTML template for new sections | Hardcoded XHTML 1.1 template |
| `~/.sigil/style.css` | Default stylesheet | Empty |
| `~/.sigil/impressum.xhtml` | Impressum page (full XHTML) | No impressum created |
| `~/.sigil/translator_models.json` | LLM model whitelist for translator | All server models |

CSS links in XHTML templates are automatically adjusted to match the EPUB-internal filename (e.g. `Style0001.css`).

## License

GNU General Public License v3 — see [COPYING.txt](COPYING.txt)

Original Sigil Copyright (C) 2009-2013 Strahinja Markovic and contributors.
Qt 6 migration Copyright (C) 2026 Gerhard Quell.
