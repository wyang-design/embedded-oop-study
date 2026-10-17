# AGENTS.md

## Project Overview

`smarttemp` is a small embedded-style C program: a thermostat that reads temperature
over I2C, drives a heater via GPIO, and logs state changes over UART. It is an
embedded firmware skeleton built for **host-side development** — no MCU, no cross
toolchain. The HAL is simulated on the PC so the control loop can run and be debugged
as a normal Linux executable.

The repo sits under `c/oop/`, and is a teaching exercise in separating application
logic from hardware access (dependency inversion through `hal.h`) in C.

## Layout

```
main.c            # application: thermostat control loop (the "policy")
hal/hal.h         # HAL interface — the only header main.c includes
hal/hal_pc.c      # PC simulation of the HAL (the "mechanism")
CMakeLists.txt    # build definition
build/            # CMake build tree, git-ignored — never edit by hand
.serena/          # Serena LSP config; project.yml is versioned, project.local.yml is not
```

## Architecture

One seam: `hal/hal.h`. Application code calls only these five functions and knows
nothing about hardware:

```c
void  hal_init(void);
bool  i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
void  gpio_set(uint8_t pin, bool value);
void  uart_print(const char *format);
void  delay_ms(uint32_t ms);
```

- **`main.c`** = policy. Contains all control logic, thresholds, and hysteresis.
- **`hal/hal_pc.c`** = a stand-in implementation that `printf`s every call and fakes a
  drifting temperature (`fake_temp`, 40–90 °C) so the loop exercises every branch.
  It holds `static` state (`fake_temp`, `tick`), so it is not reentrant.

To add a new target platform, add a sibling of `hal_pc.c` implementing `hal.h` and
swap the source file in `CMakeLists.txt`. Do not add `#ifdef PLATFORM` branches to
`hal_pc.c`.

### Data flow

`i2c_read` returns a 2-byte big-endian raw value from a TMP102-style sensor at
address `0x48`; `main.c` converts it with `(raw >> 5) * 0.125f`. The `>> 5` and the
`0.125` scale factor are part of the sensor protocol — `hal_pc.c` inverts exactly this
encoding when it fabricates data, so **the two must be changed together**.

### Control policy (`main.c`)

Target 50 °C with ±2 °C hysteresis, hard cut-off at 80 °C:

- `t >= 80 °C` → heater off, unconditionally (safety).
- `t < 48 °C` and not already heating → heater on.
- `t > 52 °C` and currently heating → heater off.
- otherwise → no state change.

The `heating` flag is the only state; the loop is a single-threaded
read-decide-act-sleep cycle with a 1 s period.

## Build and Run

```bash
cmake -S . -B build
cmake --build build
./build/smartheat
```

Runs forever — it is an infinite `while(1)` loop. Stop with Ctrl-C. Buffered stdout
means output can look empty when piped; use `stdbuf -oL ./build/smartheat` to see it
in real time.

Rebuild from scratch with `rm -rf build` (it is git-ignored, safe to delete).

## Conventions

- C11-ish, compiled by CMake's default flags. No warnings are enabled — do not assume
  a clean build means no bugs; compile with `-Wall -Wextra` when checking your work.
- `snprintf` into a fixed buffer, then `uart_print`. `uart_print` takes a single
  fully-formatted string, not a format-plus-args; keep call sites that way.
- 4-space indent, `snake_case` for functions and variables, `UPPER_CASE` for the
  threshold `#define`s at the top of `main.c`.
- Fixed-width types from `<stdint.h>`; `bool` from `<stdbool.h>`.
- Messages use `%.2f°C` — the degree sign is already UTF-8 in the source.

## Gotchas

- **Exact-name trap:** the directory is `smarttemp` but the CMake project and
  executable are `smartheat`, so the binary is `build/smartheat`. Don't look for
  `build/smarttemp`.
- `CMakeLists.txt` currently has a leading space on every line. Harmless, but match
  the existing style when editing.
- `rand()` is used unseeded in the HAL, so the simulated temperature sequence is
  identical on every run. Seed it only if you actually want nondeterminism.
- `i2c_read`'s return value is ignored in `main.c`; a failed read leaves `data`
  as `{0,0}` and produces a bogus 0 °C reading. Handle it if you touch that call.
- `int main(void)` never returns, so there is no cleanup or exit path.

## Repo Purpose and Git

This repo is `github.com/wyang-design/embedded-oop-study`. Its purpose is **learning
object-oriented / structured design in embedded C with AI assistance**, using
`smarttemp` as the first worked example — not shipping firmware. So when making
changes here, favour clarity and explanation over cleverness, and expect the code to
be refactored as the exercise evolves.

- `origin` is `git@github.com:wyang-design/embedded-oop-study.git` (SSH, already
  authenticated — no token needed to push).
- Commit identity is set **locally only**: `wyang-design <wyang0126@outlook.com>`.
- Default branch is `main`, tracking `origin/main`.
