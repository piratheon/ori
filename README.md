<h1 align=center> <img src="www/favicon.svg" height"28" width="28"/> ORI - TUI</h1>

<div align=center>

![GitHub last commit](https://img.shields.io/github/last-commit/piratheon/ori?style=for-the-badge&labelColor=101418&color=9ccbfb)
![GitHub Repo stars](https://img.shields.io/github/stars/piratheon/ori?style=for-the-badge&labelColor=101418&color=b9c8da)
![GitHub repo size](https://img.shields.io/github/repo-size/piratheon/ori?style=for-the-badge&labelColor=101418&color=d3bfe6)

</div>


> A terminal‑first AI assistant for Linux with both a Text User Interface (TUI) and a web-based Graphical User Interface (GUI). Integrates with OpenRouter (Groq and G4F will be supported soon) to access multiple LLMs. Built for developers, power users, and sysadmins who want to run AI workflows from the terminal or the browser.

> [!IMPORTANT]
> The development of this repository is currently slowed down due to time constraints and competing priorities.

**Why?**
- Ori represents an ambitious integration of multiple technologies (C++/Textual TUI, multi-LLM orchestration, web UI etc ...) that requires sustained focus to maintain and evolve responsibly. Current commitments (education) limit capacity for meaningful updates.

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
- Common config keys: `port`, `model`, `no_banner`, `no_clear`, `rag_memory_enabled`, `skills_memory_enabled`, `show_command_output`, `git_backup_enabled`
  (keys added in newer versions are filled in with their defaults automatically the first time Ori starts)

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
- `/init` — initialize the current directory as an Ori project (see [Git snapshots and `/undo`](#git-snapshots-and-undo)).
- `/undo` — restore the files Ori changed in its last response and drop that exchange from the conversation.
- `/gitbackup [on|off]` — switch pre-edit snapshots on or off (they only run in initialized projects).
- `/cmdoutput [on|off]` — show or hide the output of commands Ori runs (default: on).
- `/rag`, `/skills`, `/agents`, `/task` — RAG memory (capped at 500 chunks, oldest evicted first), skills, AGENTS.md rules and the task dispatcher.

#### Git snapshots and `/undo`
Ori can snapshot your working tree before it edits files so `/undo` can put things back. This is opt-in per project:

```
cd ~/projects/my-app
ori --init        # or type /init inside Ori
```

- Ori never runs `git init` on its own. `/init` (or `ori --init`) asks before creating a repository (`-y` skips the question) and refuses to initialize your home directory or `/`.
- In a directory that was not initialized, no snapshot is taken and `/undo` only removes the exchange from the conversation history.
- Snapshots live on a private ref (`refs/ori/backups/...`) inside the repository. They do not add commits to your branch, touch your index, or change your git config.
- `/undo` restores only the files Ori created, edited or renamed in its last response. Your other uncommitted work is left alone. The effects of shell commands Ori ran are not reverted.

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
- 1.1.6 — Safety and correctness release.
  - **Git safety:** Ori no longer runs `git init` and stages everything in whatever directory it was launched from (which could be your home directory). Snapshots are only taken in projects initialized with the new `/init` command or `--init` flag (never in `~` or `/`), they are stored on a private ref instead of a commit on your branch, and `/undo` restores only the files Ori changed instead of running `git reset --hard`, so unrelated uncommitted work is no longer lost.
  - `pkexec` is only substituted for `sudo` when a graphical session is detected (`DISPLAY` or `WAYLAND_DISPLAY`); on a headless TTY or plain SSH session `sudo` is kept.
  - `-DORI_VERSION=...` is honoured by CMake, and `.version` now reads `1.1.6` so local builds report the right version.
  - New config keys (`rag_memory_enabled`, `skills_memory_enabled`, `show_command_output`, `git_backup_enabled`) are written into existing `config.json` files on startup.
  - Long sessions no longer re-send AGENTS.md/skills/RAG context on every earlier turn: the context is attached to the current request only, so token usage grows linearly instead of quadratically.
  - RAG memory is capped at 500 chunks (oldest evicted first) and tokenizes each chunk once instead of on every query.
  - **Behaviour change:** command output is no longer printed unconditionally. It is controlled by `show_command_output` (default `true`, toggle with `/cmdoutput on|off`). With the default, output is still shown but is now wrapped in `[ COMMAND OUTPUT ]` / `[ END COMMAND OUTPUT ]` banner lines instead of being printed bare, and when the option is off Ori prints a one-line hint on stdout instead. Scripts that parse Ori's stdout should be checked against this.
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
