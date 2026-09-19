<h1 align=center> <img src="www/favicon.svg" height"28" width="28"/> ORI - TUI</h1>

<div align=center>

![GitHub last commit](https://img.shields.io/github/last-commit/piratheon/ori?style=for-the-badge&labelColor=101418&color=9ccbfb)
![GitHub Repo stars](https://img.shields.io/github/stars/piratheon/ori?style=for-the-badge&labelColor=101418&color=b9c8da)
![GitHub repo size](https://img.shields.io/github/repo-size/piratheon/ori?style=for-the-badge&labelColor=101418&color=d3bfe6)

</div>


> A terminal‑first AI assistant for Linux with both a Text User Interface (TUI) and a web-based Graphical User Interface (GUI). Integrates with OpenRouter (Groq and G4F will be supported soon) to access multiple LLMs. Built for developers, power users, and sysadmins who want to run AI workflows from the terminal or the browser.

> [!IMPORTANT]
> The development of this repository is currently slowed down due to time constraints and competing priorities.

**Why archived?**
- Ori represents an ambitious integration of multiple technologies (C++/Textual TUI, multi-LLM orchestration, web UI etc ...) that requires sustained focus to maintain and evolve responsibly. Current commitments (education) limit capacity for meaningful updates.

**Will this resume?**
- Possibly. If circumstances change and development capacity returns, this will be unarchived and development will resume. The codebase is preserved in a working state for that eventuality.

---
## Quick links
- GitHub: https://github.com/piratheon/ori
- AUR: https://aur.archlinux.org/packages/ori
### Core
- **OpenRouter integration:** Connect to multiple AI models.
- **Auto-retry:** Automatically retries on transient network errors and rate limiting.
- **Plugin system & Orpm:** Extendable architecture with package management.

### TUI (Terminal)
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
   cd ori
   ```
2. Build (automated):
   ```
   ./build.sh
   ```
   cmake ..
   make
3. Executable will be at `build/ori`.

### Arch (AUR)
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
   ./build/ori --config cat model
   ./build/ori --config cat all

### TUI (interactive)
Start the interactive assistant:
```
./build/ori
```
Useful flags:
- `--help` — show CLI help
- `--version` — print version
- `/help`, `/clear`, `/quit`, `/cat`, `/exec` — available inside TUI
- `/autoexec [ask|yes|no]` — set auto-execute mode for commands: `ask` (prompt before executing), `yes` (automatically confirm and run), or `no` (never auto-execute). This value is saved to the persistent config.
- `/model <api_config_id>` — switch the active API/provider by its configuration id (as defined in your `keys.json`). If the id is not found the CLI will list available models.
- `/thinking` — activate the provider configured with the `role` set to `thinking` (if any). If no such provider is configured, a message will notify you.

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
├── include              # header files
│   └── external         # third-party headers
├── src                 # source files
│   ├── core          # core source files
│   └── gui             # GUI logic source files
└── www                # web assets
    ├── css
    ├── lib
    └── webfonts
```

Build & iterate locally

## Contributing
Contributions, issues, and PRs welcome. Open an issue to discuss larger changes before submitting PRs. Follow standard fork → branch → PR workflow.

## Changelog (1.x.x)
- 1.1.5 — Multi-API support (OpenRouter, Google, Hugging Face), centralized API key management (`keys.json`), first-use experience, new interactive commands (`/autoexec`, `/model`, `/thinking`), conditional debugging, and bug fixes.
- 1.1.4 — Remove old plugin manager and files; refactor config, GUI, edits, backup/restore, and streamline main/CMakelists.
- 1.1.3 — Fix Ctrl+C doesn't work on WebUI mode, and add a new debug option, also improve scripts to detect current distro and install deps for it.
- 1.1.2 — New cat command to print current configurations values.
- 1.1.1 — New slash commands, command log, keybindings, auto-retry, and code cleanup.
- 1.1.0 — Loading spinner, persistent config, new CLI flags.
- 1.0.0 — Add GUI mode.

## License
GNU GPL-3.0 — see [LICENSE](LICENSE)  file.
## ASCII Art
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
