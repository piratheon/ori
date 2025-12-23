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

# Ori Assistant v1.0.0

Ori Assistant is a powerful and versatile AI assistant for Linux, now with both a Text User Interface (TUI) and a web-based Graphical User Interface (GUI). It integrates with the OpenRouter API to provide access to a wide range of AI models. Ori is designed for developers, power users, and anyone who wants to leverage the power of AI from their terminal or browser.

## Features

### Core

- **OpenRouter API Integration**: Connects to various AI models through OpenRouter.
- **Plugin System**: Extensible architecture with plugin support.
- **Orpm Plugin Manager**: Built-in package manager for plugins.
- **Secure API Key Handling**: API key is stored securely in `~/.config/ori/key`.
- **Automatic Updates**: Silently checks for updates on startup.

### TUI (Text User Interface)

- **Slash Commands**: Internal commands like `/help` and `/quit`.
- **Agentic Command Execution**: The AI can run shell commands to perform tasks, with user confirmation.
- **Session Context**: Remembers conversation history in interactive mode.
- **Clear Command**: A `/clear` command to clear the terminal screen.

### GUI (Graphical User Interface)

- **Web-Based Interface**: A modern and intuitive web-based GUI.
- **Chat History**: View and manage your chat history.
- **Model Selection**: Choose from a list of available AI models.
- **Command Execution**: Execute shell commands directly from the GUI.
- **Code Canvas**: Display and interact with code snippets.

## Installation

### Prerequisites

- C++14 compatible compiler (GCC 5.0+ or Clang 3.4+)
- CMake 3.10+
- OpenRouter API key

### Building from Source

```bash
# Clone the repository
git clone https://github.com/piratheon/ori.git
cd ori

# Build using the provided script
./build.sh

# Or build manually
mkdir build
cd build
cmake ..
make
```

## Configuration

### API Key Setup

1.  **Get an Openrouter API Key**: from [here](https://openrouter.ai/settings/keys)
2.  **Set the key**:
    *   **Environment Variable**: Set `OPENROUTER_API_KEY` in your shell profile.
    *   **Interactive Prompt**: Run `ori` for the first time, and it will prompt you to enter your API key.

## Usage

### TUI Mode

```bash
# Start the interactive assistant
./build/ori

# Show help
./build/ori --help

# Show version
./build/ori --version

# Run a non-interactive command
./build/ori -y "install nmap for me"
```

In interactive mode, you can use slash commands like `/help`, `/quit`, and `/clear`.

### GUI Mode

To launch the GUI, run:

```bash
./build/ori --gui
```

This will start a web server on `http://localhost:8080`. Open this URL in your browser to access the GUI.

### Plugin Manager (orpm)

`orpm` is used to manage plugins.

```bash
# List available plugins
orpm --orpm-list

# Install a plugin
orpm --orpm-install "voice-chat"

# Remove a plugin
orpm --orpm-remove "voice-chat"
```

## Development

### Project Structure

```
.
├── build.sh
├── CMakeLists.txt
├── include
│   ├── ori_core.h
│   ├── ori_gui.h
│   └── orpm.h
├── src
│   ├── core
│   │   ├── ori_core.cpp
│   │   └── ...
│   ├── gui
│   │   └── gui.cpp
│   ├── main.cpp
│   └── plugins
│       └── orpm.cpp
└── www
    ├── index.html
    ├── css
    └── js
```

## License

This project is licensed under the GNU GPL-3.0 license - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.