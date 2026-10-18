# CMake 构建系统评审报告

> 评审对象:`3f2d882`(stage 2+3 完成后的构建系统重构)
> 评审环境:CMake 4.2.3 / GCC 15 / Linux x86-64
> 评审方式:静态阅读 + 9 组受控实验(在 `/tmp` 副本中进行,未改动仓库)
> 评审视角:嵌入式软件架构

---

## 0. 总体结论

**方向是对的,执行是没落地。**

选择的技术路线——「一个模块一个 CMake target + `add_subdirectory` + `target_link_libraries` 传递依赖」——正是嵌入式项目应当采用的路线,方向没有错。

但当前**真正让编译通过的,不是 `target_link_libraries`,而是「把项目根目录塞进 include path」**。也就是说:

- 在 **C 代码**层面,依赖倒置已经做对了(`domain` 只依赖 `ports`,不依赖 `hal`);
- 在 **CMake** 层面,依赖关系没有表达,而且用 `../` 把模块之间的墙打通了。

架构图上那条 `domain → ports`、`adapters → hal` 的线,**在 CMake 里是缺失的**。

最直接的后果:

> 现在往 `domain/` 里加一句 `#include "hal/hal.h"`,CMake **不会报任何错**。
> 阶段 2 用 `grep` 手工验证的那条"domain 不含 hal"不变式,**在构建系统层面没有任何强制力**。

---

## 1. 嵌入式项目 CMake 组织原则

| # | 条文 | 理由 |
|---|---|---|
| **P1** | 一个逻辑模块 = 一个 CMake target;目录边界 = target 边界 | target 是 CMake 唯一的封装单位 |
| **P2** | 模块间依赖**只**用 `target_link_libraries` 表达 | 依赖必须显式、可被工具读取 |
| **P3** | **禁止**用 `../`、`${CMAKE_SOURCE_DIR}` 等"打洞"式 include 绕过模块边界 | 打洞即抹掉边界,越权 include 无法被自动发现 |
| **P4** | 每个 target 只暴露**自己**的 include 目录,使用方一律**平铺** include | 双机制混用必然产生隐性耦合 |
| **P5** | `PUBLIC`/`PRIVATE` 严格按"是否出现在**公开头文件**里"划分 | 决定依赖是否向下游传递 |
| **P6** | `add_subdirectory` 自底向上:被依赖者先定义 | 依赖次序与阅读次序一致 |
| **P7** | 顶层只做组装:`project()` + 全局设置 + `add_subdirectory` + 最终可执行 | 顶层变胖 = 模块化失效 |
| **P8** | 编译选项(C 标准、警告)用 `target_compile_options` / INTERFACE target,不用全局 `set()` | 全局变量会污染所有 target |
| **P9** | 测试**链接**被测 target,不重新编译其源码;用 `enable_testing()` + `add_test()` 接入 ctest | 否则测试与产品代码必然漂移 |
| **P10** | 平台/BSP 差异用**选择变量 + 多个实现 target**,不在 CMakeLists 里写死文件名 | 硬编码文件名 = 无法做板级变体 |
| **P11** | 预留交叉编译接缝(toolchain file / 变量) | PC 模拟与目标板必须能共存 |
| **P12** | 全工程只有一个 `project()`;子目录不得重复 `project()` | 多个 `project()` = 多个独立构建树 |

---

## 2. 逐条对照

### 2.1 P2 + P3:依赖边没有落地【最关键】

#### 证据 A —— C 代码里的 include 写法

| 文件 | 行 | 内容 |
|---|---|---|
| `domain/thermostat.h` | 7 | `#include "ports/temp_sensor.h"` |
| `adapters/lm75_i2c.h` | 3 | `#include "ports/temp_sensor.h"` |
| `adapters/lm75_i2c.c` | 3 | `#include "hal/hal.h"` |

#### 证据 B —— CMake 里对应的不是 link,而是打洞

```cmake
# domain/CMakeLists.txt:6
target_include_directories(domain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

# adapters/CMakeLists.txt:9
target_include_directories(adapters PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ )

# CMakeLists.txt:24 —— 又把项目根暴露给 app
target_include_directories(smartheat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

`${CMAKE_CURRENT_SOURCE_DIR}/../` **就是项目根目录**。它存在的唯一作用,是让
`#include "ports/temp_sensor.h"` 这种带路径的写法能找到文件。

#### 证据 C —— 受控实验

逐个删掉这些"打洞"行,观察是否还能编译:

| 实验 | 删掉的行 | 结果 |
|---|---|---|
| **B** | `CMakeLists.txt:24` | ❌ `adapters/lm75_i2c.h:3:10: fatal error: ports/temp_sensor.h: No such file or directory` |
| **C** | `domain/CMakeLists.txt:6` | ❌ `domain/thermostat.h:7:10: fatal error: ports/temp_sensor.h: No such file or directory` |
| **D** | `adapters/CMakeLists.txt:9` | ❌ `adapters/lm75_i2c.h:3:10: fatal error: ports/temp_sensor.h: No such file or directory` |

#### 证据 D —— 各 target 实际拿到的 include path

```
main.c        -> ['<项目根>', 'hal', 'domain', 'adapters', 'ports']
hal_pc.c      -> ['hal']
thermostat.c  -> ['domain', 'domain/..']
lm75_i2c.c    -> ['adapters', 'adapters/..']
```

`main.c` 拿到 **5 个** include path,其中「项目根」和「各模块目录」同时存在——
这就是两套 include 机制能共存的唯一原因。

#### 结论

1. 打洞行**都是承重的**(删掉就编不过),说明它们不是冗余,而是**唯一的依赖通道**。
2. `domain` 从未 link `ports`;删掉打洞后 `domain` 找不到 `temp_sensor.h` ——
   这是铁证:**CMake 层面 `domain → ports` 这条边不存在**。
3. `adapters` 从未 link `hal`,同理。

---

### 2.2 P4:两套 include 风格混用

```c
/* 库内部:带路径前缀 */
#include "ports/temp_sensor.h"
#include "hal/hal.h"

/* app:平铺 */
#include "hal.h"
#include "lm75_i2c.h"
#include "thermostat.h"
```

P4 要求二选一。通常选**平铺**,因为 P2 已保证依赖边、P3 已保证不越权。

---

### 2.3 P5:PUBLIC/PRIVATE 语义未落地

`ports` 声明为 `INTERFACE` 库、只暴露自身目录 —— **这部分是写对的**。但:

| target | 应有依赖 | 实际 | 原因 |
|---|---|---|---|
| `domain` | `PUBLIC ports` | 无 | `thermostat.h` 是公开头文件且 include 了 `temp_sensor.h` |
| `adapters` | `PUBLIC ports` | 无 | `lm75_i2c.h` 公开 include 了 `temp_sensor.h` |
| `adapters` | `PRIVATE hal` | 无 | `hal.h` 只在 `.c` 里用 |

正是这个缺失,逼出了 `main.c` 的 5 个 include path。

---

### 2.4 P7:顶层在兜底

`CMakeLists.txt:24` 的
`target_include_directories(smartheat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})`
是典型的"模块化没做好、顶层打补丁"。实验 B 证明它是承重的。

#### 附带:app 层多链接了 `ports`(无效链接)

`CMakeLists.txt:22` 的
`target_link_libraries(smartheat PRIVATE hal domain adapters ports)`
中,`ports` 是 **no-op**:

| 实验 | 操作 | 结果 |
|---|---|---|
| **H** | 去掉 app 的 `ports` | ✅ 仍编译通过 → **无效链接** |

---

### 2.5 P6:顺序不合逻辑(但实测不影响编译)

`CMakeLists.txt:20` 把 `add_subdirectory(ports)` 放在 `domain`(:17)、
`adapters`(:18)**之后**,而前两者逻辑上都依赖 `ports`。

最初怀疑这会触发 "target not found",**实测证伪**:

| 实验 | 操作 | 结果 |
|---|---|---|
| **F** | 正确 link + 保持原始顺序 | ✅ 配置、编译、运行全通过 |

所以这是**可读性问题,不是 bug**。

> ⚠️ 注意:验证使用 CMake **4.2.3**,而声明的最低版本是 **3.20**(`CMakeLists.txt:1`)。
> 该 forward reference 在 3.20 上的行为**未实测**。建议仍按 P6 调整顺序以消除风险。

---

### 2.6 P10:HAL 写死单一实现

```cmake
# hal/CMakeLists.txt:1
add_library(hal hal_pc.c)
```

文件名写死。roadmap 阶段 9「多板卡构建变体」需要 `hal_pc.c` / `hal_stm32.c` 二选一,
当前没有这个接缝。

---

### 2.7 P9 + P12:test/ 是独立工程,且重编译源码

```cmake
# test/CMakeLists.txt
project(test_thermostat ...)                                              # :4
add_executable(test_thermostat test_thermostat.c ../domain/thermostat.c)  # :11
target_include_directories(test_thermostat PRIVATE ../domain)             # :13
```

三重问题:

1. **`:4` 第二个 `project()`** → 独立构建树,看不到顶层 target(违反 **P12**)。
2. **`:11` 直接编译 `../domain/thermostat.c`** → 违反 **P9**。后果是真实的:
   给 `domain` 加的任何编译选项(C 标准、`-Wall`、宏),**测试全都拿不到**,两边必然漂移。
3. **没有 `enable_testing()` / `add_test()`** → roadmap 阶段 4 要求的 `make test` 不存在。

> 这也是当前测试编译失败的**深层原因**:测试绕过了 `domain` target,
> 所以 `ports` 的 include 传不下来。

---

### 2.8 P8:没有 C 标准和警告

没有 `CMAKE_C_STANDARD`,也没有 `-Wall -Wextra`,靠编译器默认值。

**这不是理论问题**:`hal/hal_pc.c:55` 的 `usleep` 在显式 `-std=c11` 下会变成隐式声明
(`error: implicit declaration of function 'usleep'`)。当前能编过,纯粹因为默认是 `gnu11`。

---

### 2.9 命名与卫生

| 问题 | 位置 |
|---|---|
| `project(smartheat)` vs 目录名 `smarttemp` | `CMakeLists.txt:8` |
| 根文件每行前有一个空格 | `CMakeLists.txt` 第 1、8、14、16–24 行 |
| 文件末尾缺换行符 | `ports/`(:5)、`domain/`(:6)、`adapters/`(:9) |
| 连续空行 | `ports/` 开头 2 行、`adapters/` 开头 3 行、根文件 4–7 行 |
| target 名过于通用 | `hal`/`domain`/`ports`/`adapters`,大型工程会冲突,建议 `smartheat::hal` |

---

## 3. 改进方案(已验证)

### 3.1 补上依赖边

```cmake
# domain/CMakeLists.txt —— 删除打洞行,换成真依赖
target_link_libraries(domain PUBLIC ports)

# adapters/CMakeLists.txt —— 删除打洞行,换成真依赖
target_link_libraries(adapters PUBLIC ports PRIVATE hal)
```

```c
// domain/thermostat.h:7 和 adapters/lm75_i2c.h:3
-#include "ports/temp_sensor.h"
+#include "temp_sensor.h"

// adapters/lm75_i2c.c:3
-#include "hal/hal.h"
+#include "hal.h"
```

### 3.2 顶层去补丁、排好序

```cmake
cmake_minimum_required(VERSION 3.20)
project(smartheat VERSION 0.1 DESCRIPTION "Smart Heat" LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE -Wall -Wextra)

add_subdirectory(ports)     # 接口最先
add_subdirectory(hal)
add_subdirectory(domain)
add_subdirectory(adapters)

add_executable(smartheat main.c)
target_link_libraries(smartheat PRIVATE domain adapters hal project_warnings)
# 删除原 :24 的根 include;删除多余的 ports
```

`hal` **必须**显式链接 —— 实验 G 证明:

| 实验 | 操作 | 结果 |
|---|---|---|
| **G** | app 不 link `hal` | ❌ `main.c:3:10: fatal error: hal.h: No such file or directory` |

这正是 `adapters PRIVATE hal` 的正确语义:`hal` 是 adapters 的实现细节、不外传;
而 `main.c` 自己使用 HAL,所以自己链接。

### 3.3 验证结果

```text
=== BUILD (expect zero warnings) ===
clean, no warnings
=== RUN ===
[HAL] initialized.
[HAL] I2C read from addr: 0x48, reg: 0x00, len: 2
[HAL] GPIO set pin: 5 to value: 1
[HAL] UART print: Current Temp: 45.25 C, Heating: ON, Fault: NO
```

**打洞全部删掉后仍然编译运行正常**,且依赖图终于和架构图一致:

```
thermostat.c  -> ['domain', 'ports']
lm75_i2c.c    -> ['adapters', 'ports', 'hal']
```

---

## 4. 优先级

| 优先级 | 项目 | 条文 | 理由 |
|---|---|---|---|
| **P0** | 补 `domain→ports`、`adapters→hal/ports`;删 3 处打洞 | P2/P3 | 当前架构不变式无构建系统保护,`domain` 可偷偷 include `hal` 而不报错 |
| **P0** | test/ 改为 link `domain`;去掉第二个 `project()`;加 `enable_testing()` | P9/P12 | 测试绕过 target,已实际导致编译失败 |
| **P1** | 顶层加 `-Wall -Wextra` + C 标准 | P8 | `usleep` 问题已实际踩到 |
| **P2** | `hal` 改为可选实现(变量 + `target_sources`) | P10 | 阶段 9 的前置条件,可后置 |
| **P3** | 命名空间、缩进、空行、末尾换行、顺序 | P1/P6/P7 | 纯卫生,顺手清理 |

---

## 5. 实验证据索引

所有实验均在 `/tmp/opencode/` 的仓库副本中进行,**未改动仓库任何文件**。

| 实验 | 假设 / 操作 | 结论 |
|---|---|---|
| **B** | 根 include 是否承重 | 承重(删除即失败) |
| **C** | `domain` 的 `../` 是否承重 | 承重 → CMake 层 `domain→ports` 边缺失 |
| **D** | `adapters` 的 `../` 是否承重 | 承重 → CMake 层 `adapters→hal/ports` 边缺失 |
| **E** | 按 P2–P5 修正后能否编译 | ✅ 可编译、可运行 |
| **F** | 原始 `add_subdirectory` 顺序是否需要修正 | ❌ 假设证伪:不影响编译,仅可读性问题 |
| **G** | app 是否必须显式 link `hal` | 必须(`adapters PRIVATE hal` 不向下游传递) |
| **H** | app link `ports` 是否有效 | 无效(no-op) |
| **I** | 修正版 + C 标准 + `-Wall -Wextra` | ✅ 零警告通过 |

---

## 6. 结语:这不只是 CMake 问题

阶段 2 特意用 `grep -R '#include "hal/'` 手工验证不变式 —— 这个意识是对的,
说明"架构约束要能验证"这一点已经理解到位。

但本次评审说明:**当时那条 `grep` 通过,有相当一部分是运气**。因为 `domain` 从未
link `ports`、根目录一直开着,任何越权 include 都能编过,只能靠人肉 grep 事后自查。

补上依赖边之后,这条不变式就从"靠人检查"升级为"**违反就编不过**"。

这正是设计模式的核心收益:

> **让错误的做法在结构上不可能,而不是靠纪律去避免。**

这比 CMake 语法本身重要得多。
