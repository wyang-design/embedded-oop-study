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
main.c                # composition root ONLY: build the three adapters, inject them
ports/temp_sensor.h   # abstract sensor      — read_celsius(self, float *out)
ports/heater.h        # abstract heater      — set_power(self, float percent)
ports/status_sink.h   # abstract status sink — publish(self, temp, target, heating, fault)
                      # all three: vtable + inline dispatcher, no HAL, no device, no state
adapters/lm75_i2c.{h,c}    # LM75-over-I2C,           implements temp_sensor
adapters/gpio_heater.{h,c} # on/off heater on a pin,  implements heater
adapters/uart_status.{h,c} # formats + prints status,  implements status_sink
domain/thermostat.{h,c}  # the control policy — depends on ports/, never on hal/
hal/hal.h             # HAL interface (i2c/gpio/uart/delay primitives)
hal/hal_pc.c          # PC simulation of the HAL (the "mechanism")
tests/                # host unit tests for domain/, wired into the main build via CTest
roadmap.md            # the staged curriculum (Chinese) with per-stage tasks/answers
cmake-review.md       # build-system review: principles, violations, evidence
embedded-cmake-process.md  # the reproducible CMake workflow to follow
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

**1. `ports/` — the abstract seams.** Three vtable-based interfaces, the C stand-in for
abstract base classes:

```c
typedef struct temp_sensor temp_sensor_t;
typedef struct { bool (*read_celsius)(temp_sensor_t *self, float *out); } temp_sensor_ops_t;
struct temp_sensor { const temp_sensor_ops_t *ops; };
static inline bool temp_sensor_read(temp_sensor_t *self, float *out)
{ return self->ops->read_celsius(self, out); }
```

`heater.h` and `status_sink.h` are the same shape. The ops pointer is `const` in all
three, and should stay that way: the ops table is `static`, shared and immutable — only
the object's own state changes, never the vtable.

**2. `adapters/` — the concrete implementations.** Each embeds its port as the **first
member**, so an `lm75_t *` converts to a `temp_sensor_t *`, and a `gpio_heater_t *` to a
`heater_t *`; the adapter casts `self` back down. This is manual single inheritance in C.
`gpio_heater` stores only the pin; `uart_status` stores nothing and formats on each call.

**3. `domain/` — pure policy.** Depends on the ports, not the devices:

```c
void thermostat_init(thermostat_t *self, temp_sensor_t *sensor, heater_t *heater,
                     status_sink_t *status, const thermostat_config_t *config);
void thermostat_step(thermostat_t *self);   /* pulls the temperature, drives the rest */
```

`self` is threaded through explicitly — the C idiom standing in for `this`, which is the
whole point of the exercise. Keep it that way: **do not add global or `static` state to
`thermostat.c`**, and never let `domain/` include `hal.h`. Adding fakes for tests is
exactly the payoff of this design — do it by implementing `temp_sensor_t`, `heater_t` or
`status_sink_t`, not by poking at the thermostat.

**`hal/` — the hardware primitives.** Application code calls only these five functions
and knows nothing about hardware:

```c
void  hal_init(void);
bool  i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
void  gpio_set(uint8_t pin, bool value);
void  uart_print(const char *format);
void  delay_ms(uint32_t ms);
```

- **`main.c`** is now *only* assembly: it builds a `thermostat_config_t`, constructs the
  three adapters (`lm75_t`, `gpio_heater_t`, `uart_status_t`), and injects all of them.
  It holds no threshold logic and never calls `gpio_set`/`uart_print` itself — the
  adapters do.
- **`hal/hal_pc.c`** is a stand-in that `printf`s every call and fakes a drifting
  temperature (`fake_temp`, 40–90 °C) so the loop exercises every branch. It holds
  `static` state (`fake_temp`, `tick`), so it is not reentrant.

To add a new target platform, add a sibling of `hal_pc.c` implementing `hal.h` and swap
the source file in `hal/CMakeLists.txt`. Do not add `#ifdef PLATFORM` branches to
`hal_pc.c`. To add a new sensor, heater or status sink, implement the matching port in
`adapters/` — swapping one should touch `main.c` and nothing else. That is the test of
the seam: if changing a device forces an edit in `domain/`, the port is leaking.

### Data flow

`i2c_read` returns a 2-byte big-endian raw value from an LM75/TMP102-style sensor at
address `0x48`. The `(raw >> 5) * 0.125f` conversion now lives in
`adapters/lm75_i2c.c`, not `main.c` — it is an LM75 detail, not an application one. The
`>> 5` and the `0.125` scale factor are part of the sensor protocol, and `hal_pc.c`
inverts exactly this encoding when it fabricates data, so **the two must be changed
together**.

The output path mirrors this: `thermostat_step` ends with `heater_set_power` and
`status_sink_publish`. `adapters/gpio_heater.c` collapses percent to a boolean on the pin
(`percent > 0.0f`), and `adapters/uart_status.c` formats the line and calls `uart_print`.

### Control policy (`domain/thermostat.c`)

Driven entirely by `thermostat_config_t`; `main.c` currently supplies 50 °C target,
±2 °C hysteresis, 80 °C max, 100 ms period. Retuning means editing those values, not
the logic.

- sensor read fails → `fault = true`, `heating = false`, publish status, then early
  return. The heater is **not** touched on this path — see Gotchas.
- `t >= max_c` → `heating = false`, `fault = true`.
- `t < target_c - hysteresis_c` and not already heating → `heating = true`.
- `t > target_c + hysteresis_c` and currently heating → `heating = false`.
- otherwise → state unchanged, `fault` cleared.

Every step that gets past the sensor read ends with two unconditional output calls:
`heater_set_power` with `100.0f` when `heating && !fault` and `0.0f` otherwise, then
`status_sink_publish`. The heater is re-driven every cycle rather than only on change
(which is what the old `main.c` did) — fine for an idempotent GPIO, and what a real
percent-based heater port wants. The `!fault` term is belt-and-braces: the policy clears
`heating` whenever it sets `fault`, so the two are never true together.

### Staged workflow

`roadmap.md` defines stages 0–10, each adding exactly one concept (config → ports →
dependency injection → unit tests → FSM → strategy → events → RTOS → board variants).

Current state: **stages 1–4 complete**. Stages 2 and 3 now cover all three ports
(`temp_sensor`, `heater`, `status_sink`) — each has a port, an adapter and an injection
site — and the stage-4 suite is wired into the build and green. Stage 4 is not fully
done, though: the roadmap also asks for a sensor-fault test, and the new fake `heater` and
`status_sink` are injected but never asserted on. Stages 5+ are outstanding.

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

The suite is integrated into the main build tree with CTest, and it passes:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build          # 1/1 tests passed
```

`tests/CMakeLists.txt` links the `domain` target instead of recompiling
`../domain/thermostat.c`, so the test inherits `domain`'s include directories and
compile options rather than drifting from them. `tests/test_thermostat.c` implements
fakes for all three ports — a `temp_sensor_t` returning a scripted value (and optionally
failing), a `heater_t` recording `on`, and a `status_sink_t` recording the last line —
and injects them through `thermostat_init`. That is what makes the policy testable with
no hardware.

Three cases: over-temp (fault + heater off), low-temp (heater on), hysteresis (state
holds inside the band).

**Two gaps, both left over from stage 4.** The roadmap also asks for a sensor-fault test,
which does not exist yet — `fake_sensor_t.ok` supports it, but nothing exercises it. And
`fake_heater_t.on` / `fake_status_sink_t.last_msg` are written but never asserted on, so
the two new collaborators are reached yet unverified.

They are hand-rolled `assert()`-based checks — no framework. Each test prints
`PASS:<name>` and the runner prints `All tests passed!`. `ctest` reports only pass/fail,
and a bad `assert` aborts the run, so to see which case failed, or to see a partial run
as your signal, run the binary directly:

```bash
./build/tests/test_thermostat
```

The suite exercises `domain/thermostat.c` directly and touches no HAL — the payoff of
the domain/ports split: the control policy is testable on the host with no hardware.
Keep new domain tests hardware-free.

**`assert()` vanishes under `NDEBUG`.** A `-DCMAKE_BUILD_TYPE=Release` build compiles
the checks out, and the suite then reports `All tests passed!` unconditionally — a false
green. Always build tests in a debug/default configuration.

## Conventions

- C11-ish. Every code target links `project_warning`, so `-Wall -Wextra -Wpedantic
  -Werror` is active and the build is warning-clean; a clean build now means something.
  The C standard itself is still unpinned — see Gotchas.
- `snprintf` into a fixed buffer, then `uart_print`. `uart_print` takes a single
  fully-formatted string, not a format-plus-args; keep call sites that way.
- 4-space indent, `snake_case` for functions and variables. Tunable constants are
  `thermostat_config_t` fields, **not** `#define`s — keep it that way so retuning stays
  a data change.
- Headers use `#pragma once` (the roadmap sample shows `#ifndef` guards; `#pragma once`
  is what the code actually uses — follow the code).
- Fixed-width types from `<stdint.h>`; `bool` from `<stdbool.h>`.
- The status line is formatted in `adapters/uart_status.c` and passed to `uart_print` as
  a single string; `main.c` no longer formats anything.

## Gotchas

- **Exact-name trap:** the directory is `smarttemp` but the CMake project and
  executable are `smartheat`, so the binary is `build/smartheat`. Don't look for
  `build/smarttemp`.
- `CMakeLists.txt` line 11 (`project(smartheat `) has a trailing space. Harmless, but
  match the existing style when editing.
- **`-std=c11` breaks the build:** `hal/hal_pc.c` calls `usleep`, which strict c11 does
  not declare. The build pins `CMAKE_C_STANDARD 11` and leaves CMake's extensions on, so
  the effective flag is `-std=gnu11` — do **not** set `CMAKE_C_EXTENSIONS OFF`. If you
  follow the roadmap's suggested `-Wall -Wextra -std=c11` Makefile, add
  `-D_DEFAULT_SOURCE` or use `-std=gnu11`.
- **Warning flags are opt-in per target, and a miss is silent.** `project_warning` is an
  `INTERFACE` target carrying `-Wall -Wextra -Wpedantic -Werror`; there is no global
  `add_compile_options`, so every code target must link it by hand. All five do today
  (`smartheat`, `hal`, `domain`, `adapters`, `test_thermostat`), and the build is clean
  under `-Werror`. A target that forgets the link compiles with **no** warning flags and
  nothing reports it, so check coverage after adding one:
  `python3 -c "import json;d=json.load(open('build/compile_commands.json'));print([e['file'].split('/')[-1] for e in d if not [t for t in e['command'].split() if t.startswith('-W')]])"`
  — an empty list means full coverage.
- `thermostat.h` names its struct tags `_thermostat_t` / `_thermostat_config_t`.
  Leading-underscore identifiers are reserved in C. Harmless here, but prefer plain
  `thermostat` / `thermostat_config` in new code.
- `rand()` is used unseeded in the HAL, so the simulated temperature sequence is
  identical on every run. Seed it only if you actually want nondeterminism.
- `i2c_read`'s failure is now handled rather than swallowed: it propagates through
  `lm75_read` → `temp_sensor_read` → `thermostat_step`, which sets `fault` and clears
  `heating`. That resolved the old "bogus 0 °C" bug. **But `fault` is now overloaded** —
  it means *either* over-temperature *or* sensor failure, and nothing distinguishes them.
  The status sink prints the same `Fault: Yes` for both, and `last_temp` is stale on a
  sensor fault (it keeps the last good value). If a consumer ever needs to tell them apart
  (stage 7's event bus is the likely trigger), split it — e.g. add a `fault_kind` or a
  separate `sensor_ok` flag — rather than overloading `fault` further.
- **On a sensor fault the status line reports 0 °C.** `thermostat_step` initialises
  `float current_temp = 0.0f;` so the fault-path `status_sink_publish` has something
  defined to pass — and it passes that `0.0f`, not `self->last_temp`, which by then holds
  the last good reading. Publishing `last_temp` would be the honest value.
- **A sensor fault leaves the heater in its last physical state.** The fault branch sets
  `heating = false` and returns early, at `thermostat.c:23` — *before* the
  `heater_set_power` call in the tail of the function (`:51`/`:55`). So if the thermostat
  was heating when the sensor died, the GPIO stays driven while the status line reports
  `Heating: false`: the reported state and the physical state disagree, in the one case
  that is supposed to fail safe. Reproduced by injecting a fault after a normal step —
  `heater.on` stays `1` and the sink prints
  `Temp: 0.00, Target: 50.00, Heating: false, Fault: true`. This is a regression from the
  pre-extraction code: the old `main.c` compared `heating` to `last_heating` and turned
  the pin **off** when the fault step cleared `heating`. The fix is to drive the heater
  (to `0.0f`) on the fault path too, before returning.
- **The heater pin is a bare `5` in `main.c`, and its comment says pin 17.** Extracting
  `gpio_heater` dropped the old `#define HEATER_PIN 5`, so the pin no longer has a single
  named home. Worth restoring as a constant in `main.c`.
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
