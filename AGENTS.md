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
main.c                # composition root ONLY: build sensor, inject, drive heater
ports/temp_sensor.h   # abstract sensor: vtable + inline dispatcher. No HAL, no device
adapters/lm75_i2c.{h,c}  # LM75-over-I2C implementation of the sensor port
domain/thermostat.{h,c}  # the control policy — depends on ports/, never on hal/
hal/hal.h             # HAL interface (i2c/gpio/uart/delay primitives)
hal/hal_pc.c          # PC simulation of the HAL (the "mechanism")
test/                 # host unit tests for domain/ — a SEPARATE CMake project
roadmap.md            # the staged curriculum (Chinese) with per-stage tasks/answers
CMakeLists.txt        # top-level build: links hal/domain/adapters/ports
build/                # CMake build tree, git-ignored — never edit by hand
.serena/              # local Serena LSP state — git-ignored, not part of the project
```

Each of `hal/`, `domain/`, `adapters/` has its own `CMakeLists.txt` defining a library;
`ports/` is an `INTERFACE` library (headers only). Include paths flow through the
dependency graph via `target_link_libraries`, so a new file inherits the right includes
only if it links the library it needs.

## Architecture

Three layers, wired so that dependencies point **inward**: `main` → `domain` → `ports`,
with `adapters` → `ports` and `adapters` → `hal`. Nothing depends on `adapters`, and
`domain` never sees `hal`.

**1. `ports/` — the abstract seam.** A vtable-based interface, the C stand-in for an
abstract base class:

```c
typedef struct temp_sensor temp_sensor_t;
typedef struct { bool (*read_celsius)(temp_sensor_t *self, float *out); } temp_sensor_ops_t;
struct temp_sensor { temp_sensor_ops_t *ops; };
static inline bool temp_sensor_read(temp_sensor_t *self, float *out)
{ return self->ops->read_celsius(self, out); }
```

**2. `adapters/` — the concrete implementation.** `lm75_t` embeds `temp_sensor_t` as its
**first member**, so an `lm75_t *` converts to a `temp_sensor_t *`; the adapter's
`read_celsius` casts `self` back down. This is manual single inheritance in C.

**3. `domain/` — pure policy.** Depends on the port, not the device:

```c
void thermostat_init(thermostat_t *self, temp_sensor_t *sensor, const thermostat_config_t *config);
void thermostat_step(thermostat_t *self);   /* pulls the temperature itself */
```

`self` is threaded through explicitly — the C idiom standing in for `this`, which is the
whole point of the exercise. Keep it that way: **do not add global or `static` state to
`thermostat.c`**, and never let `domain/` include `hal.h`. Adding a fake sensor for tests
is exactly the payoff of this design — do it by implementing `temp_sensor_t`, not by
poking at the thermostat.

**`hal/` — the hardware primitives.** Application code calls only these five functions
and knows nothing about hardware:

```c
void  hal_init(void);
bool  i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
void  gpio_set(uint8_t pin, bool value);
void  uart_print(const char *format);
void  delay_ms(uint32_t ms);
```

- **`main.c`** is now *only* assembly: it builds a `thermostat_config_t`, constructs an
  `lm75_t`, injects it, and translates the resulting flags into `gpio_set`/`uart_print`.
  It no longer reads I2C and contains no threshold logic.
- **`hal/hal_pc.c`** is a stand-in that `printf`s every call and fakes a drifting
  temperature (`fake_temp`, 40–90 °C) so the loop exercises every branch. It holds
  `static` state (`fake_temp`, `tick`), so it is not reentrant.

To add a new target platform, add a sibling of `hal_pc.c` implementing `hal.h` and swap
the source file in `hal/CMakeLists.txt`. Do not add `#ifdef PLATFORM` branches to
`hal_pc.c`. To add a new sensor, implement `temp_sensor_t` in `adapters/` — swapping it
should touch `main.c` and nothing else.

### Data flow

`i2c_read` returns a 2-byte big-endian raw value from an LM75/TMP102-style sensor at
address `0x48`. The `(raw >> 5) * 0.125f` conversion now lives in
`adapters/lm75_i2c.c`, not `main.c` — it is an LM75 detail, not an application one. The
`>> 5` and the `0.125` scale factor are part of the sensor protocol, and `hal_pc.c`
inverts exactly this encoding when it fabricates data, so **the two must be changed
together**.

### Control policy (`domain/thermostat.c`)

Driven entirely by `thermostat_config_t`; `main.c` currently supplies 50 °C target,
±2 °C hysteresis, 80 °C max, 100 ms period. Retuning means editing those values, not
the logic.

- sensor read fails → `fault = true`, `heating = false`, early return (`last_temp` keeps
  its previous value).
- `t >= max_c` → `heating = false`, `fault = true`.
- `t < target_c - hysteresis_c` and not already heating → `heating = true`.
- `t > target_c + hysteresis_c` and currently heating → `heating = false`.
- otherwise → state unchanged, `fault` cleared.

`main.c` calls `gpio_set` **only when `heating` changes**, so the heater is not re-driven
every cycle — that is deliberate, not an oversight.

### Staged workflow

`roadmap.md` defines stages 0–10, each adding exactly one concept (config → ports →
dependency injection → unit tests → FSM → strategy → events → RTOS → board variants).

Current state: **stages 1, 2 and 3 complete** (config, ports/adapters, injection). Stage
4's unit tests exist but **are currently broken** — see Tests. Stages 5+ are outstanding.

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

> **BROKEN as of stage 2+3.** `test/test_thermostat.c` still calls the pre-injection
> API, so the test build fails and does not compile:

```text
domain/thermostat.h:7:10: fatal error: ports/temp_sensor.h: No such file or directory
```

Two separate causes, both from the stage 2+3 refactor:

1. `test/CMakeLists.txt` never adds the project root to its include path, so
   `thermostat.h`'s `#include "ports/temp_sensor.h"` cannot resolve.
2. Even once it resolves, the test uses the old signatures —
   `thermostat_init(&t, &cfg)` (now takes a `temp_sensor_t *`) and
   `thermostat_step(&t, 47.0f)` (now takes no temperature and pulls it from the sensor).

Fixing it is the intended exercise: implement a fake `temp_sensor_t` whose
`read_celsius` returns scripted values, inject it, and call `thermostat_step(&t)` with
no argument. A fake that can *fail* lets you test the sensor-fault path too. Prefer that
over restoring a temperature parameter — the injection is the point.

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

The suite exercises `domain/thermostat.c` directly and touches no HAL — the payoff of
the domain/ports split: the control policy is testable on the host with no hardware.
Keep new domain tests hardware-free.

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
- `i2c_read`'s failure is now handled rather than swallowed: it propagates through
  `lm75_read` → `temp_sensor_read` → `thermostat_step`, which sets `fault` and forces the
  heater off. That resolved the old "bogus 0 °C" bug. **But `fault` is now overloaded** —
  it means *either* over-temperature *or* sensor failure, and nothing distinguishes them.
  `main.c` prints the same `Fault: YES` for both, and `last_temp` is stale on a sensor
  fault (it keeps the last good value). If a consumer ever needs to tell them apart
  (stage 7's event bus is the likely trigger), split it — e.g. add a `fault_kind` or a
  separate `sensor_ok` flag — rather than overloading `fault` further.
- On a sensor fault `thermostat_step` returns early **before** clearing anything, so the
  thermostat latches `fault = true` until a subsequent read succeeds. That is intended
  fail-safe behaviour, not a stuck state.
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
