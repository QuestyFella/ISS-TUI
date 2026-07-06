# ISS-TUI

A low-latency terminal ISS tracker written in pure C99. It pulls live position data and renders the space station gliding over a braille world map, complete with a fading trail of where it has been. Think of it as a live wallpaper, but for people who like terminals.

## What You Get

- A real-time map in your terminal, scaling to whatever size you give it
- The actual ISS, live, right now, with lat/lon/altitude/velocity/timestamp
- A fading trail (`* + : .`) so you can see where it came from
- Smooth coastlines thanks to Unicode braille sub-cell resolution
- Truecolor earth tones for land, plain space for water — no garish blue wall
- A bright red-on-yellow marker so you never lose the station

## How It Works

Built with CMake. Zero external runtime dependencies beyond `curl` (to fetch the position) and a UTF-8 terminal.

It polls once per second from `https://api.wheretheiss.at/v1/satellites/25544`, falling back to `http://api.open-notify.org/iss-now.json` if needed. If both APIs are down, it shows the error and waits patiently — it does NOT make up a position.

The map itself is a baked Natural Earth 110m land mask (180x60), rendered with Unicode braille characters (U+2800-U+28FF) for sub-cell resolution. That is why the coastlines look surprisingly smooth for something made of dots.

Non-interactive mode (piped output) just prints `ISS-TUI` so smoke tests stay fast.

Press `Ctrl-C` to quit.

## Build

```sh
cmake -S . -B build && cmake --build build
```

## Test

```sh
ctest --test-dir build -V
```

## Run

```sh
./build/src/iss_tui
```

## Install

```sh
cmake --install build --prefix /usr/local
```

## What's Next

There is a whole bunch of stuff on the roadmap. Pass predictions. More satellites. TUI controls. Maybe some surprises. Stay tuned :D

---

QuestyFella / ISS-TUI
