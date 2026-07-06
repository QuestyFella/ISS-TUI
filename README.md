# ISS-TUI

A low-latency terminal multi-satellite tracker written in pure C99. It pulls live position data and renders satellites gliding over a braille world map, complete with a fading trail of where they have been. Think of it as a live wallpaper, but for people who like terminals.

## What You Get

- A real-time map in your terminal, scaling to whatever size you give it
- Multiple satellites: ISS, Tiangong, and Hubble, each with a coloured marker and trail
- Live lat/lon/altitude/velocity/timestamp for each satellite
- A fading trail (`* + : .`) so you can see where each one came from
- Smooth coastlines thanks to Unicode braille sub-cell resolution
- Truecolor earth tones for land, plain space for water — no garish blue wall

## How It Works

Built with CMake. Zero external runtime dependencies beyond `curl` (to fetch positions) and a UTF-8 terminal.

Data sources by satellite:
- **ISS** (NORAD 25544): primary `https://api.wheretheiss.at/v1/satellites/25544`, fallback `http://api.open-notify.org/iss-now.json`.
- **Tiangong** (NORAD 48274) and **Hubble** (NORAD 20580): `https://satlas.app/api/satellite-info?query={NORAD_ID}` (Satlas is the only free API found that serves non-ISS live orbital data).

If all APIs are down for a satellite, it shows the error and waits patiently — it does NOT make up a position.

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
