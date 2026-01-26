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

A terminal‑first AI assistant for Linux with both a Text User Interface (TUI) and a web-based Graphical User Interface (GUI). Integrates with OpenRouter (Groq and G4F will be supported soon) to access multiple LLMs. Built for developers, power users, and sysadmins who want to run AI workflows from the terminal or the browser.

## Quick links
- GitHub: https://github.com/piratheon/ori
- AUR: https://aur.archlinux.org/packages/ori
- AUR-GitLab: https://gitlab.archlinux.org/piratheon/ori
- OpenRouter keys: https://openrouter.ai/settings/keys

## Features

### Core
- **OpenRouter integration:** Connect to multiple AI models.
- **Auto-retry:** Automatically retries on transient network errors and rate limiting.
- **Plugin system & Orpm:** Extendable architecture with package management.
- **Secure API key handling:** API key stored at `~/.config/ori/key`.
- **Automatic update checks** on startup.
- **Persistent config:** `~/.config/ori/config.json`.

### TUI (Terminal)
- Interactive conversation with session context.
- Slash commands: `/help`, `/clear`, `/quit`, `/cat`, `/exec`.
- Command execution log with `Ctrl+F` toggle.
- Agentic command execution with confirmation.
- Multiline input and editor-friendly UX.
- Keybindings:
  - `Ctrl+F`: Toggle command execution log.
  - `Ctrl+C` / `ESC`: Cancel running command or clear prompt.

### GUI (Browser)
- Web-based chat UI with chat history and model selector.
- Code canvas for snippets and inline command execution.
- Runs a local web server (default port 8080).


## Prerequisites
- C++14-compatible compiler (GCC 5.0+ or Clang 3.4+)
- CMake 3.10+
- OpenRouter API key

### External libraries 

| Distribution | Packages | Install command |
|---|---:|---|
| Debian / Ubuntu | libjsoncpp-dev, libcurl4-openssl-dev | sudo apt-get update && sudo apt-get install -y libjsoncpp-dev libcurl4-openssl-dev |
| Fedora | jsoncpp-devel, libcurl-devel | sudo dnf install -y jsoncpp-devel libcurl-devel |
| Arch Linux | jsoncpp, curl (with libcurl) | sudo pacman -Syu --noconfirm jsoncpp curl 

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
 - Print a config value or all values:
   ```
   ./build/ori --config cat model
   ./build/ori --config cat all
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
- `/help`, `/clear`, `/quit`, `/cat`, `/exec` — available inside TUI

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
├── build.sh               # build script
├── CMakeLists.txt   # CMake build file
├── install.sh             # install script
├── aur                      # AUR  files
├── build                   # build output
├── include               # header files
│   └── external       # external library headers
├── src                      # source files
│   ├── core             # core logic
│   └──  gui             # GUI server code
└── www                # GUI assets
    ├── css
    ├── lib
    └── webfonts
```

Build & iterate locally

## Contributing
Contributions, issues, and PRs welcome. Open an issue to discuss larger changes before submitting PRs. Follow standard fork → branch → PR workflow.

## Changelog (1.x.x)
- 1.1.4 — Remove old plugin manager and files; refactor config, GUI, edits, backup/restore, and streamline main/CMakelists.
- 1.1.3 — Fix Ctrl+C doesn't work on WebUI mode, and add a new debug option, also improve scripts to detect current distro and install deps for it.
- 1.1.2 — New cat command to print current configurations values.
- 1.1.1 — New slash commands, command log, keybindings, auto-retry, and code cleanup.
- 1.1.0 — Loading spinner, persistent config, new CLI flags.
- 1.0.0 — Add GUI mode.

## License
GNU GPL-3.0 — see [LICENSE](LICENSE)  file.
