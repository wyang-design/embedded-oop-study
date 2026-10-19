给你定制一个项目：**SmartTemp 智能温控器**。  
它足够小，能在 1~2 周走完第一轮；又足够真实，能把前面所有概念练一遍。我按阶段带你，每个阶段只做一件事，做完验证，再进下一阶段。

---

## 一、项目总览

### 需求（V1）

1. 每 100ms 读一次温度。
2. 目标温度 50℃，迟滞 2℃。
3. 低于 48℃ 开加热，高于 52℃ 关加热。
4. 超过 80℃ 过温故障，强制关加热。
5. 通过 UART 上报：温度、目标、是否加热。
6. 以后可能换 NTC 传感器、加 CAN、加 PID、上 RTOS、出多板卡。

### 预埋变化点

| 变化轴 | V1 | 未来 |
|---|---|---|
| 传感器 | LM75 (I2C) | NTC (ADC) |
| 加热器 | GPIO 继电器 | PWM |
| 上报 | UART | CAN |
| 算法 | 迟滞 | PID |
| 并发 | 裸机 | FreeRTOS |
| 板卡 | Board A | Board B/C |
| 参数 | 硬编码 | 客户配置 |

### 最终架构目标

```text
domain  --->  ports  <---  adapters  --->  hal
                ^
                |
              main 组装
```

### 硬件方案（二选一）

- **有硬件**：STM32 + LM75 + 继电器 + UART。
- **没硬件**：PC 模拟，用 `printf` 假装 UART，用随机数/文件假装传感器。  
  **建议先用 PC 模拟**，跑通架构再上硬件。

### HAL 接口（PC 和 MCU 都实现它）

```c
// hal/hal.h
void hal_init(void);
bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);
void gpio_set(int pin, int value);
void uart_print(const char *str);
void delay_ms(uint32_t ms);
```

PC 版用 `printf` 和 `usleep`；MCU 版用真实 HAL。这样你不需要硬件就能开始。

---

## 二、阶段路线图

| 阶段 | 主题 | 核心概念 | 产出 |
|---|---|---|---|
| 0 | 能跑的烂代码 | 基线 | 一个 main.c |
| 1 | 配置化 + 参数化 | 值变化 | config + domain 雏形 |
| 2 | 端口 + 适配器 | 实现替换 | ports/ + adapters/ |
| 3 | 依赖倒置 + 组装 | 依赖方向 | main 注入 |
| 4 | PC 单元测试 | fake + 反馈 | test/ 通过 |
| 5 | 状态机 | 行为随状态变 | FSM |
| 6 | 策略接口 | 算法替换 | 迟滞 + PID |
| 7 | 事件总线 | 一对多解耦 | event bus |
| 8 | RTOS 活动对象 | 并发隔离 | task + queue |
| 9 | 多板卡构建变体 | 链接期替换 | 多 target |
| 10 | 复盘 + 架构文档 | 形成框架 | 依赖图 + 卡片 |

**节奏**：阶段 0~4 是地基，必须扎实；5~7 是进阶；8~9 是大型项目必备；10 是内化。

---

## 三、阶段 0：能跑的烂代码

### 目标

不设计，不抽象，把功能用最直接的方式写出来。**先有基线，才知道后面在改什么。**

### 任务

1. 建目录：

```text
smart_temp/
  hal/
    hal.h
    hal_pc.c      # PC 模拟实现
  main.c
  Makefile
```

2. 实现 HAL PC 版：

```c
// hal/hal_pc.c
#include "hal.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static float fake_temp = 45.0f;

void hal_init(void) {
    printf("[HAL] init\n");
}

bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    // 模拟 LM75：温度在 40~60 之间缓慢变化
    static int tick = 0;
    tick++;
    fake_temp += ((rand() % 100) - 50) / 100.0f;
    if (fake_temp < 40) fake_temp = 40;
    if (fake_temp > 90) fake_temp = 90;

    int16_t raw = (int16_t)(fake_temp / 0.125f) << 5;
    buf[0] = (raw >> 8) & 0xFF;
    buf[1] = raw & 0xFF;
    return true;
}

void gpio_set(int pin, int value) {
    printf("[GPIO] pin=%d value=%d\n", pin, value);
}

void uart_print(const char *str) {
    printf("[UART] %s", str);
}

void delay_ms(uint32_t ms) {
    usleep(ms * 1000);
}
```

3. 写 `main.c`，全塞在一起：

```c
// main.c
#include "hal/hal.h"
#include <stdio.h>
#include <stdbool.h>

#define HEATER_PIN 5
#define TARGET_C 50.0f
#define HYST_C 2.0f
#define MAX_C 80.0f

int main(void) {
    hal_init();
    bool heating = false;

    while (1) {
        uint8_t buf[2];
        i2c_read(0x48, 0x00, buf, 2);
        int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
        float temp = (raw >> 5) * 0.125f;

        char msg[64];

        if (temp >= MAX_C) {
            gpio_set(HEATER_PIN, 0);
            heating = false;
            snprintf(msg, sizeof(msg), "FAULT temp=%.1f\n", temp);
        } else if (!heating && temp < TARGET_C - HYST_C) {
            gpio_set(HEATER_PIN, 1);
            heating = true;
            snprintf(msg, sizeof(msg), "HEAT temp=%.1f\n", temp);
        } else if (heating && temp > TARGET_C + HYST_C) {
            gpio_set(HEATER_PIN, 0);
            heating = false;
            snprintf(msg, sizeof(msg), "IDLE temp=%.1f\n", temp);
        } else {
            snprintf(msg, sizeof(msg), "HOLD temp=%.1f\n", temp);
        }

        uart_print(msg);
        delay_ms(100);
    }
}
```

4. 写 `Makefile`：

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.

SRC = main.c hal/hal_pc.c
TARGET = smart_temp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
```

### 验证

```bash
make run
```

看到类似输出：

```text
[HAL] init
[GPIO] pin=5 value=1
[UART] HEAT temp=45.0
[GPIO] pin=5 value=0
[UART] IDLE temp=53.0
...
```

### 常见坑

- 温度解析算错：LM75 是 11 位，高 8 位 + 低 3 位，右移 5。
- 迟滞逻辑写反：加热时用上限，停止时用下限。
- 忘了 `heating` 状态，导致反复开关。

### 完成后回答

1. 这段代码里，哪些值以后会变？
答：1. 温度相关的参数:目标温度、迟滞的范围、最高温度
2. 哪些部分和硬件强绑定？
答：1.温度传感器 2.gpio 3. 串口
3. 如果换 NTC，要改哪几行？
答：1.需要将i2c_read 换成 NTC 相关的代码
4. 如果要在 PC 上测过温逻辑，现在能测吗？
答：可以测试，将所有代码接口不变，内部实现全部删除改为PC模拟

把答案记下来，进入阶段 1。

---

## 四、阶段 1：配置化 + 参数化

### 目标

把“值变化”从代码里拿出来。**不改逻辑，只改数据。**

### 任务

1. 建 `domain/thermostat.h`：

```c
#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <stdbool.h>

typedef struct {
    float target_c;
    float hysteresis_c;
    float max_c;
    uint32_t period_ms;
} thermostat_config_t;

typedef struct {
    thermostat_config_t cfg;
    bool heating;
    bool fault;
    float last_temp;
} thermostat_t;

void thermostat_init(thermostat_t *self, const thermostat_config_t *cfg);
void thermostat_step(thermostat_t *self, float temp);

#endif
```

2. 建 `domain/thermostat.c`：

```c
#include "thermostat.h"

void thermostat_init(thermostat_t *self, const thermostat_config_t *cfg) {
    self->cfg = *cfg;
    self->heating = false;
    self->fault = false;
    self->last_temp = 0.0f;
}

void thermostat_step(thermostat_t *self, float temp) {
    self->last_temp = temp;

    if (temp >= self->cfg.max_c) {
        self->fault = true;
        self->heating = false;
        return;
    }

    self->fault = false;

    if (!self->heating && temp < self->cfg.target_c - self->cfg.hysteresis_c) {
        self->heating = true;
    } else if (self->heating && temp > self->cfg.target_c + self->cfg.hysteresis_c) {
        self->heating = false;
    }
}
```

3. `main.c` 改成：

```c
#include "hal/hal.h"
#include "domain/thermostat.h"
#include <stdio.h>

#define HEATER_PIN 5

static void apply_heater(bool on) {
    gpio_set(HEATER_PIN, on ? 1 : 0);
}

int main(void) {
    hal_init();

    thermostat_config_t cfg = {
        .target_c = 50.0f,
        .hysteresis_c = 2.0f,
        .max_c = 80.0f,
        .period_ms = 100,
    };

    thermostat_t thermo;
    thermostat_init(&thermo, &cfg);

    bool last_heating = false;

    while (1) {
        uint8_t buf[2];
        i2c_read(0x48, 0x00, buf, 2);
        int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
        float temp = (raw >> 5) * 0.125f;

        thermostat_step(&thermo, temp);

        if (thermo.heating != last_heating) {
            apply_heater(thermo.heating);
            last_heating = thermo.heating;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "%s temp=%.1f target=%.1f\n",
                 thermo.fault ? "FAULT" : (thermo.heating ? "HEAT" : "IDLE"),
                 temp, cfg.target_c);
        uart_print(msg);

        delay_ms(cfg.period_ms);
    }
}
```

4. `Makefile` 加 `domain/thermostat.c`：

```makefile
SRC = main.c hal/hal_pc.c domain/thermostat.c
CFLAGS = -Wall -Wextra -std=c11 -I.
```

### 验证

- `make run`，行为不变。
- 改 `cfg.target_c = 60.0f`，行为跟着变，逻辑代码不动。
- 改 `cfg.period_ms = 500`，周期变慢。

### 常见坑

- 不要一上来抽端口。阶段 1 只做配置。
- `thermostat.c` 不 include `hal.h`，它只接收 `float temp`。
- 加热器控制还在 `main`，正常。阶段 2 再抽。

### 完成后回答

1. 现在换 NTC，要改哪几个文件？
答：main.c hal.c   还是要改2个文件
2. 现在测过温逻辑，还需要硬件吗？
答：现在测试过温逻辑，不需要硬件了，里面并没有包含硬件相关的头文件
3. `thermostat.c` 依赖了什么？
答：依赖一些系统头文件，他并不清楚使用了那些硬件

---

## 五、阶段 2~10 概要

### 阶段 2：端口 + 适配器

- 定义 `ports/temp_sensor.h`、`ports/heater.h`、`ports/status_sink.h`。
- 写 `adapters/lm75_i2c.c`、`adapters/gpio_heater.c`、`adapters/uart_status.c`。
- 验证：`grep -R "#include \"hal/" src/domain` 为空。

### 阶段 3：依赖倒置 + 组装

- `thermostat_init` 接收 `temp_sensor_t*`、`heater_t*`、`status_sink_t*`。
- `main.c` 创建具体对象，注入。
- 验证：换传感器只改 `main.c`。

### 阶段 4：PC 单元测试

- 写 `fake_sensor`、`fake_heater`、`fake_status`。
- 测：过温关加热、低温开加热、迟滞、传感器故障。
- 验证：`make test` 在 PC 上通过。

### 阶段 5：状态机

- `STATE_IDLE / STATE_HEATING / STATE_FAULT`。
- 每个状态一个处理函数，事件驱动转移。
- 验证：画得出状态图，测试覆盖所有转移。

### 阶段 6：策略接口

- `ports/control_strategy.h`，实现 `hysteresis_control` 和 `pid_control`。
- `thermostat` 只调 `control_compute`。
- 验证：换算法不改 `thermostat.c`。

### 阶段 7：事件总线

- 发布 `EVT_OVER_TEMP`、`EVT_HEATER_CHANGED`。
- 多个订阅者：日志、告警、上报。
- 验证：加订阅者不改发布者。

### 阶段 8：RTOS 活动对象

- `thermo_task` + `queue`，消息驱动。
- 共享数据只在一个任务里。
- 验证：加任务不改业务逻辑。

### 阶段 9：多板卡构建变体

- `bsp/board_a.c`、`bsp/board_b.c` 提供配置表。
- CMake 分 target，每个板卡一个可执行文件。
- 验证：加板卡只加 `board_*.c` 和 `app_*.c`。

### 阶段 10：复盘 + 架构文档

- 画依赖图。
- 写 20 张卡片：`问题 → 概念 → 代码 → 验证 → 何时不用`。
- 写一篇复盘。
- 验证：能讲清楚每个抽象为什么存在。

---

## 六、现在开始做什么

**今天只做阶段 0。**

1. 建目录 `smart_temp/`。
2. 写 `hal/hal.h`、`hal/hal_pc.c`、`main.c`、`Makefile`。
3. `make run`，看到温度在 40~90 之间波动，加热器开关，UART 打印。
4. 回答阶段 0 的 4 个问题。

做完把代码和答案发我。我确认后带你进阶段 1。

**规则**：

- 每个阶段只做一件事。
- 每个阶段结束必须能跑、能验证。
- 不提前抽端口，不提前上 RTOS。
- 遇到卡点先记录，不要跳步。
