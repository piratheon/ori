```
    ███████    ███████████   █████            ███████████ █████  █████ █████
  ███▒▒▒▒▒███ ▒▒███▒▒▒▒▒███ ▒▒███            ▒█▒▒▒███▒▒▒█▒▒███  ▒▒███ ▒▒███ 
 ███     ▒▒███ ▒███    ▒███  ▒███            ▒   ▒███  ▒  ▒███   ▒███  ▒███ 
▒███      ▒███ ▒██████████   ▒███  ██████████    ▒███     ▒███   ▒███  ▒███ 
▒███      ▒███ ▒███▒▒▒▒▒███  ▒███ ▒▒▒▒▒▒▒▒▒▒     ▒███     ▒███   ▒███  ▒███ 
▒▒███     ███  ▒███    ▒███  ▒███                ▒███     ▒███   ▒███  ▒███ 
 ▒▒▒███████▒   █████   █████ █████               █████    ▒▒████████   █████
   ▒▒▒▒▒▒▒    ▒▒▒▒▒   ▒▒▒▒▒ ▒▒▒▒▒               ▒▒▒▒▒      ▒▒▒▒▒▒▒▒   ▒▒▒▒▒
```
## Ori Assistant v1.1.0

A terminal‑first AI assistant for Linux with both a Text User Interface (TUI) and a web-based Graphical User Interface (GUI). Integrates with OpenRouter to access multiple LLMs. Built for developers, power users, and sysadmins who want to run AI workflows from the terminal or the browser.

Version: 1.1.0 — Release highlights: loading spinner, persistent config, new CLI flags.

## Quick links
- Repository: https://github.com/piratheon/ori (this one)
- OpenRouter keys: https://openrouter.ai/settings/keys

## Features

### Core
- **OpenRouter integration:** Connect to multiple AI models.
- **Plugin system & Orpm:** Extendable architecture with package management.
- **Secure API key handling:** API key stored at `~/.config/ori/key`.
- **Automatic update checks** on startup.
- **Persistent config:** `~/.config/ori/config.json`.

### TUI (Terminal)
- Interactive conversation with session context.
- Slash commands: `/help`, `/clear`, `/quit`.
- Agentic command execution with confirmation.
- Multiline input and editor-friendly UX.

### GUI (Browser)
- Web-based chat UI with chat history and model selector.
- Code canvas for snippets and inline command execution.
- Runs a local web server (default port 8080).

## New in 1.1.0
- Loading spinner while waiting for responses.
- Config file support (`~/.config/ori/config.json`).
- CLI flags:
  - `--no-banner` — hide ASCII banner.
  - `--no-clear` — don’t clear terminal on start.
  - `--model` / `-m` — override default model.
  - `--port` / `-p` — override GUI port.
  - `--config` / `-c`:
    - `load <path>` — load JSON config.
    - `set <key> <value>` — set a config value.

## Prerequisites
- C++14-compatible compiler (GCC 5.0+ or Clang 3.4+)
- CMake 3.10+
- OpenRouter API key

## Install

### Build from source
1. Clone:
   ```
   git clone https://github.com/piratheon/ori.git
   cd ori
   ```
2. Build (automated):
   ```
   ./build.sh
   ```
   or manual:
   ```
   mkdir build && cd build
   cmake ..
   make
   ```
3. Executable will be at `build/ori`.

### Arch (AUR)
- Install with an AUR helper:
  ```
  yay -S aur/ori
  ```

## Configuration

- Default config: `~/.config/ori/config.json`
- API key file: `~/.config/ori/key` (or set `OPENROUTER_API_KEY` env var)
- Common config keys: `port`, `model`, `no_banner`, `no_clear`

Examples:
- Set a config value:
  ```
  ./build/ori --config set model qwen/qwen3-coder:free
  ```
- Load a JSON config:
  ```
  ./build/ori --config load /path/to/settings.json
  ```

## Usage

### TUI (interactive)
Start the interactive assistant:
```
./build/ori
```
Useful flags:
- `--help` — show CLI help
- `--version` — print version
- `/help`, `/clear`, `/quit` — available inside TUI

### Non-interactive
Run a one-off prompt:
```
./build/ori -y "what is my kernel version?"
```

### GUI
Start the web UI:
```
./build/ori --gui
```
Default: http://localhost:8080 (override with `--port`)

### ASCII banner (optional)
Disable with `--no-banner`.

## Development

Project layout:
```
.
├── build.sh
├── CMakeLists.txt
├── include/
├── src/
├── www/         # web UI assets
└── build/       # build output
```

Build & iterate locally

## Contributing
Contributions, issues, and PRs welcome. Open an issue to discuss larger changes before submitting PRs. Follow standard fork → branch → PR workflow.

## Changelog (selected)
- 1.1.0 — 
- 1.0.0 — Initial public release (TUI + GUI).

## License
GNU GPL-3.0 — see [LICENSE](LICENSE)  file.