# bloom-valuation

Minimal C ISS tracker using CMake. It renders a low-latency terminal map with the live ISS position and trail. The map scales to your terminal and uses a baked Natural Earth land mask.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
ctest --test-dir build -V
```

## Run

Requires `curl` on macOS/Linux for live position. If the public ISS APIs are unavailable, it falls back to a clearly labeled rough orbital estimate instead of drawing fake zero coordinates.

```sh
./build/src/bloom_valuation
```

Press `Ctrl-C` to quit. In non-interactive output, the binary prints `bloom-valuation` so the smoke test stays fast.

## Install

```sh
cmake --install build --prefix /usr/local
```
