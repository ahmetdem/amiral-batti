# Amiral Battı

Networked Battleship-style game written in modern C++ with [raylib](https://www.raylib.com/) providing the 2D UI and [ENet](http://enet.bespin.org/) handling client/server communication. The project ships with a simple launcher that lets you host a session or join an existing one.

## Features
- Main menu for hosting locally or joining another player's game by IP
- Raylib-powered board presentation, transitions, and menus
- Deterministic game-state updates shared between a server and client over ENet
- Makefile-driven workflow with debug, release, run, and clean targets

## Building
The Makefile expects the static libraries in `lib/` provided by the submodules. Once prerequisites are in place:

```bash
make        # default build (debug-friendly warnings)
make debug  # explicit debug build with symbols
make release  # optimised build
```

Build artifacts are written to `bin/` (executable) and `obj/` (intermediate objects).

## Running
The quickest way to start the application is:

```bash
make run
```

This will build (if necessary) and launch `bin/amiral`.

At runtime you will see a full-screen windowed menu:
1. Choose **Host Game** to create a local server. The waiting screen appears until a client connects.
2. Choose **Join Game** to connect to an existing server. Enter the host's IP address (defaults to `127.0.0.1`).
3. Press **Esc** to quit back to the desktop at any time.

For testing on a single machine, start one instance in host mode, then launch a second instance (or run the binary directly with `./bin/amiral`) and join using `127.0.0.1`.

## Project Layout
- `src/` – game logic, networking entry points, and raylib UI code
- `lib/` – git submodules containing raylib and ENet sources
- `bin/` – created by the build; contains the compiled executable
- `obj/` – generated object files and dependency manifests
- `Makefile` – build, run, and clean targets used throughout development

## Maintenance
- `make clean` removes `obj/` and the executable
- `make clean-all` removes `obj/` and the entire `bin/` directory
- Update submodules periodically to pick up raylib or ENet improvements

If you hit build issues, ensure your compiler supports C++17 and that submodules were initialised before running `make`.
