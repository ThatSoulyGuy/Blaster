# Blaster

A team-based multiplayer 3D shooter built with C++23, featuring client-server architecture, real-time physics, and networked gameplay.

## Overview

Blaster is a competitive multiplayer game where two teams (Red and Blue) battle to destroy the opposing team's beacon while defending their own. Players connect to a dedicated server, join a team, and engage in real-time combat with physics-based interactions.

## Screenshots

![Third-Person Gameplay](screenshots/Screenshot%202026-01-24%20223730.png)

![First-Person View](screenshots/Screenshot%202026-01-24%20223806.png)

![Team Blue Player](screenshots/Screenshot%202026-01-24%20223919.png)

## Features

### Gameplay
- **Team-Based Combat**: Two teams (Red and Blue) compete to destroy enemy beacons
- **Weapon System**: Assault rifle with raycast-based hit detection
- **Item System**: Hotbar inventory with weapons, medkits, and equipment
- **Respawn Mechanics**: Death and respawn system with team spawns
- **In-Game Chat**: Team and global communication

### Technical
- **Client-Server Architecture**: Authoritative server with client-side prediction
- **Real-Time Networking**: TCP-based communication using Boost.Asio
- **Physics Simulation**: Bullet Physics 3 integration for realistic movement and collisions
- **Entity Component System**: Flexible ECS architecture for game objects
- **State Synchronization**: Efficient delta-based network updates
- **3D Rendering**: OpenGL-based renderer with shader support

### UI Elements
- Minimap with team positions
- Health display
- Hotbar inventory (5 slots)
- Chat window
- Crosshair with kill/friendly-fire indicators
- Victory/Death menus

## Building

### Prerequisites

- **CMake** 3.16 or higher
- **C++23** compatible compiler:
  - GCC 13+ (Linux)
  - GCC/MinGW 13+ (Windows)
  - Clang 16+ (macOS)
- **Git** (for fetching dependencies)

### Dependencies

All dependencies are automatically fetched via CMake FetchContent:

| Library | Purpose |
|---------|---------|
| GLFW | Window management and input |
| OpenGL | Graphics rendering |
| Bullet Physics | Physics simulation |
| Boost.Asio | Networking |
| Boost.Serialization | Data serialization |
| Assimp | 3D model loading |
| FreeImage | Texture loading |
| FreeType | Font rendering |
| OpenAL Soft | Audio playback |
| Zstd | Compression |

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/blaster.git
cd blaster

# Create build directory
mkdir build && cd build

# Configure (Release build recommended for playing)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel

# The executables will be in the build directory:
# - Client (or Client.exe on Windows)
# - Server (or Server.exe on Windows)
```

### Platform-Specific Notes

#### Windows (MinGW)
```bash
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

#### macOS
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

#### Linux
```bash
# Install OpenGL development libraries first
sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BLASTER_USE_SYSTEM_OPENAL` | OFF | Use system OpenAL instead of bundled OpenAL-Soft |
| `BLASTER_SMALL_DEBUGINFO` | ON | Use minimal debug info to reduce binary size |

## Running

### Starting a Server

```bash
./Server
```

The server will start listening for connections. Use the console for server commands.

### Connecting as a Client

```bash
./Client
```

1. Enter the server IP address (use `127.0.0.1` for localhost)
2. Enter the port number (default: configured in server)
3. Enter your player name
4. Click Connect

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Movement |
| Mouse | Look around |
| Left Click | Fire weapon / Use item |
| 1-5 | Select hotbar slot |
| T | Open chat |
| Enter | Send chat message |
| Escape | Release mouse / Menu |

## Architecture

```
Blaster/
├── Header/
│   ├── Client/           # Client-specific headers
│   │   ├── Core/         # Window, input management
│   │   ├── Network/      # Client networking
│   │   ├── Render/       # Rendering system
│   │   └── UI/           # User interface
│   ├── Independent/      # Shared code (client & server)
│   │   ├── ECS/          # Entity Component System
│   │   ├── Entity/       # Game entities (Player, Beacon)
│   │   ├── Item/         # Item system
│   │   ├── Network/      # Common networking
│   │   ├── Physics/      # Physics integration
│   │   └── Utility/      # Math, time, helpers
│   └── Server/           # Server-specific headers
│       └── Network/      # Server networking
├── Source/
│   ├── Client/           # Client implementation
│   ├── Independent/      # Shared implementation
│   └── Server/           # Server implementation
└── Assets/
    ├── Models/           # 3D models (.fbx)
    ├── Textures/         # Images (.png)
    ├── Shaders/          # GLSL shaders
    ├── Fonts/            # Font files
    └── Sounds/           # Audio files
```

### Key Systems

- **ECS (Entity Component System)**: GameObjects hold Components that define behavior
- **Networking**: Server maintains authoritative state, clients receive snapshots
- **Physics**: Bullet Physics handles collisions, raycasts, and character movement
- **Rendering**: OpenGL 3.3+ with multiple shader pipelines
- **Synchronization**: Dirty-tracking system for efficient network updates

## Network Protocol

The game uses TCP for reliable communication with custom packet serialization:

- **C2S (Client-to-Server)**: Player input, damage requests, chat
- **S2C (Server-to-Client)**: State snapshots, transform corrections, announcements

State synchronization uses operation-based updates:
- `OpCreate` - New entity spawned
- `OpDestroy` - Entity removed
- `OpAddComponent` - Component added
- `OpRemoveComponent` - Component removed
- `OpSetField` - Component field changed

## [License](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Acknowledgments

- Bullet Physics for the physics engine
- The Boost community for networking and serialization libraries
- The OpenGL community for graphics resources
