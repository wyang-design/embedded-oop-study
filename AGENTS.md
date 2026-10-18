# AGENTS.md

## Project Overview

`smarttemp` is a small embedded-style C program: a thermostat that reads temperature
over I2C, drives a heater via GPIO, and logs state changes over UART. It is an
embedded firmware skeleton built for **host-side development** — no MCU, no cross
toolchain. The HAL is simulated on the PC so the control loop can run and be debugged
as a normal Linux executable.

The repo sits under `c/oop/`, and is a teaching exercise in separating application
logic from hardware access (dependency inversion through `hal.h`) in C.

Work proceeds in numbered stages defined by `roadmap.md`. **Do not skip ahead**: read
the roadmap before changing structure, and see "Staged workflow" below.

## Layout

```
main.c              # application/composition root: read sensor, drive heater, report
domain/thermostat.h # control state and configuration types
domain/thermostat.c # the control policy — hardware-independent
hal/hal.h           # HAL interface — the only header main.c includes
hal/hal_pc.c        # PC simulation of the HAL (the "mechanism")
test/               # host unit tests for domain/ — a SEPARATE CMake project
roadmap.md          # the staged curriculum (Chinese) with per-stage tasks/answers
CMakeLists.txt      # build definition for the smartheat executable only
build/              # CMake build tree, git-ignored — never edit by hand
.serena/            # local Serena LSP state — git-ignored, not part of the project
```

## Architecture

Two layers, deliberately separated along the axis the roadmap is exploring.

**1. `domain/` — pure policy, no hardware.** The thermostat owns the decision logic and
its own state, and never includes `hal.h`. It receives a temperature as a `float` and
exposes its results as struct fields:

```c
typedef struct _thermostat_config_t {
    float target_c; float hysteresis_c; float max_c; uint32_t period_ms;
} thermostat_config_t;

typedef struct _thermostat_t {
    thermostat_config_t config; bool heating; bool fault; float last_temp;
} thermostat_t;

void thermostat_init(thermostat_t *self, thermostat_config_t *config);
void thermostat_step(thermostat_t *self, float current_temp);
```

`self` is threaded through explicitly — this is the C idiom standing in for `this`,
which is the whole point of the exercise. Keep it that way: **do not add global or
`static` state to `thermostat.c`**, and do not let it reach for the HAL.

**2. `hal/` — hardware access behind one interface.** Application code calls only these
five functions and knows nothing about hardware:

```c
void  hal_init(void);
bool  i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
void  gpio_set(uint8_t pin, bool value);
void  uart_print(const char *format);
void  delay_ms(uint32_t ms);
```

- **`main.c`** is the assembly point: it builds a `thermostat_config_t`, calls
  `thermostat_step`, and translates the resulting flags into `gpio_set`/`uart_print`.
  It contains **no** threshold logic any more.
- **`hal/hal_pc.c`** is a stand-in that `printf`s every call and fakes a drifting
  temperature (`fake_temp`, 40–90 °C) so the loop exercises every branch. It holds
  `static` state (`fake_temp`, `tick`), so it is not reentrant.

To add a new target platform, add a sibling of `hal_pc.c` implementing `hal.h` and
swap the source file in `CMakeLists.txt`. Do not add `#ifdef PLATFORM` branches to
`hal_pc.c`.

### Data flow

`i2c_read` returns a 2-byte big-endian raw value from a TMP102/LM75-style sensor at
address `0x48`; `main.c` converts it with `(raw >> 5) * 0.125f`. The `>> 5` and the
`0.125` scale factor are part of the sensor protocol — `hal_pc.c` inverts exactly this
encoding when it fabricates data, so **the two must be changed together**.

### Control policy (`domain/thermostat.c`)

Driven entirely by `thermostat_config_t`; `main.c` currently supplies 50 °C target,
±2 °C hysteresis, 80 °C max, 100 ms period. Retuning means editing those values, not
the logic.

- `t >= max_c` → `heating = false`, `fault = true`.
- `t < target_c - hysteresis_c` and not already heating → `heating = true`.
- `t > target_c + hysteresis_c` and currently heating → `heating = false`.
- otherwise → state unchanged.

`heating` and `fault` are the only state. `main.c` calls `gpio_set` **only when
`heating` changes**, so the heater is not re-driven every cycle — that is deliberate,
not an oversight.

### Staged workflow

`roadmap.md` defines stages 0–10, each adding exactly one concept (config → ports →
dependency injection → unit tests → FSM → strategy → events → RTOS → board variants).

Current state: **stage 1 complete**, plus stage 4's host unit tests written early —
`domain/` was already hardware-free, so it could be tested without first doing stages
2–3. Stages 2 and 3 (ports/adapters, then constructor injection) are still outstanding.

The roadmap's own rules apply to AI edits as much as to the human's:

- One stage at a time; each stage must build and run before the next.
- **Do not abstract ports or introduce DI early**, even if the code looks like it wants
  it. That premature refactor is precisely what the staged plan exists to avoid.
- Record blockers instead of jumping ahead.

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

## Tests

`test/` is a **standalone CMake project**: the root `CMakeLists.txt` does not
`add_subdirectory(test)`, so `cmake --build build` above does **not** build or run the
tests. Build and run them separately:

```bash
cmake -S test -B build-test
cmake --build build-test
./build-test/test_thermostat
```

They are hand-rolled `assert()`-based checks — no framework, no CTest, no `ctest`
target. Each test prints `PASS:<name>` and the runner prints `All tests passed!`.
A failure aborts at the first bad `assert`, so a partial run is your signal.

Add `build-test/` to the ignore list if you build in-tree.

These tests exercise `domain/thermostat.c` directly and touch no HAL, which is exactly
the payoff of the domain/hal split — the control policy is testable on the host with no
stubs. Keep new domain tests hardware-free.

**`assert()` vanishes under `NDEBUG`.** A `-DCMAKE_BUILD_TYPE=Release` build compiles
the checks out, and the suite then reports `All tests passed!` unconditionally — a false
green. Always build tests in a debug/default configuration.

## Conventions

- C11-ish, compiled by CMake's default flags. No warnings are enabled — do not assume
  a clean build means no bugs; compile with `-Wall -Wextra` when checking your work.
- `snprintf` into a fixed buffer, then `uart_print`. `uart_print` takes a single
  fully-formatted string, not a format-plus-args; keep call sites that way.
- 4-space indent, `snake_case` for functions and variables. Tunable constants are
  `thermostat_config_t` fields, **not** `#define`s — keep it that way so retuning stays
  a data change.
- Headers use `#pragma once` (the roadmap sample shows `#ifndef` guards; `#pragma once`
  is what the code actually uses — follow the code).
- Fixed-width types from `<stdint.h>`; `bool` from `<stdbool.h>`.
- Messages use `%.2f°C` — the degree sign is already UTF-8 in the source.

## Gotchas

- **Exact-name trap:** the directory is `smarttemp` but the CMake project and
  executable are `smartheat`, so the binary is `build/smartheat`. Don't look for
  `build/smarttemp`.
- `CMakeLists.txt` currently has a leading space on every line. Harmless, but match
  the existing style when editing.
- **`-std=c11` breaks the build:** `hal/hal_pc.c` calls `usleep`, which strict c11 does
  not declare. CMake's default (`gnu11`) is fine. If you follow the roadmap's suggested
  `-Wall -Wextra -std=c11` Makefile, add `-D_DEFAULT_SOURCE` or use `gnu11`.
- `thermostat.h` names its struct tags `_thermostat_t` / `_thermostat_config_t`.
  Leading-underscore identifiers are reserved in C. Harmless here, but prefer plain
  `thermostat` / `thermostat_config` in new code.
- `rand()` is used unseeded in the HAL, so the simulated temperature sequence is
  identical on every run. Seed it only if you actually want nondeterminism.
- `i2c_read`'s return value is ignored in `main.c`; a failed read leaves `data`
  as `{0,0}` and produces a bogus 0 °C reading. `thermostat_t.fault` currently reports
  **over-temperature only**, not sensor failure — don't conflate the two. A natural
  stage-4+ exercise is propagating this error instead of swallowing it.
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
- Keep one commit per roadmap stage so each step of the exercise stays bisectable and
  reviewable, and write messages that explain *which design concept* the commit adds.
