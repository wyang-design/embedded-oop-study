# 嵌入式项目 CMake 构建流程(可复制版)

> 本文每一步都给出**可执行的验证命令**。没有验证命令的步骤视为未完成。
> 示例代码取自本仓库(`smartheat/`),所有命令都在 `3f2d882` 之后的修正版上实测通过。

---

## 0. 为什么要"可复制"

同一个 `-Wall` 在不同编译器上给出的结论不同;同一份源码在不同 GCC 上默认的 C 标准不同。
**不固定这些,构建结果就不可复制,**"在我机器上是好的"就无从排查。

本仓库实测到的两个具体例子:

| 现象 | 实测结果 |
|---|---|
| 未设置 `CMAKE_C_STANDARD` | GCC 15 默认 `gnu23`(`__STDC_VERSION__ = 202311L`),GCC 12 默认 `gnu17` —— **同一份代码,两个标准** |
| `hal/hal_pc.c:55` 的 `usleep` | 在 `gnu11`/`gnu23` 下正常,在 `-std=c11`(即 `CMAKE_C_EXTENSIONS OFF`)下变成隐式声明 |

---

## 1. 流程总览

```text
①固定环境 → ②顶层骨架 → ③目录=target → ④依赖边 → ⑤include平铺
   → ⑥编译选项 → ⑦自动校验 → ⑧测试集成 → ⑨交叉编译 → ⑩验收
```

每一步做完**立刻验证**,不要攒到最后。

---

## ① 固定环境

### 做什么

```cmake
cmake_minimum_required(VERSION 3.20)     # 声明最低版本

project(smartheat VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)                 # 显式钉死 C 标准
set(CMAKE_C_STANDARD_REQUIRED ON)        # 得不到就报错,不要静默降级
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)    # ⑦ 和 ⑨ 的校验脚本依赖它
```

### 为什么

- `CMAKE_C_STANDARD_REQUIRED ON` 是关键。少了它,CMake 找不到就**静默**用默认值,
  等于没设。本仓库当前**没有**设置这两行,所以实际是按 `gnu23` 编译的。
- `CMAKE_EXPORT_COMPILE_COMMANDS ON` 会生成 `build/compile_commands.json`,
  后面 ⑦ 的依赖校验、以及 IDE/LSP 的索引都靠它。

### 验证

```bash
cmake -S . -B build
cmake --build build --verbose 2>&1 | grep -o -- '-std=[^ ]*' | sort -u
# 期望: -std=gnu11   (而不是空,也不是 gnu23)
```

> ⚠️ 注意 `CMAKE_C_STANDARD 11` 默认带扩展(`gnu11`)。若额外设 `CMAKE_C_EXTENSIONS OFF`
> 会变成严格 `c11`,`usleep` 这类 POSIX 函数立刻报错 —— 要么别关扩展,要么加
> `-D_DEFAULT_SOURCE`。

---

## ② 顶层骨架

### 做什么

顶层只放四类东西,**不放任何具体源文件**:

```cmake
cmake_minimum_required(VERSION 3.20)
project(smartheat VERSION 0.1 DESCRIPTION "Smart Heat" LANGUAGES C)

# 1) 全局设置(①)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 2) 编译选项载体(⑥)
add_library(project_warning INTERFACE)
target_compile_options(project_warning INTERFACE -Wall -Wextra -Wpedantic -Werror)

# 3) 子目录,被依赖者在前(③④)
add_subdirectory(ports)
add_subdirectory(hal)
add_subdirectory(domain)
add_subdirectory(adapters)

# 4) 最终可执行 + 组装(④)
add_executable(smartheat main.c)
target_link_libraries(smartheat PRIVATE domain adapters hal project_warning)

# 5) 测试(⑧)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
```

### 验证

```bash
grep -c "\.c" CMakeLists.txt        # 除了最终可执行,顶层不应出现 .c 文件名
```

---

## ③ 目录 = target

### 做什么

| 目录性质 | 写法 |
|---|---|
| 有 .c 实现 | `add_library(<名> <文件>.c)` |
| 只有头文件(纯接口) | `add_library(<名> INTERFACE)` |

```cmake
# ports/CMakeLists.txt —— 纯接口
add_library(ports INTERFACE)
target_include_directories(ports INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# domain/CMakeLists.txt —— 有实现
add_library(domain thermostat.c)
target_include_directories(domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(domain PUBLIC ports)
```

每个 `CMakeLists.txt` **不再写** `cmake_minimum_required()` 和 `project()`
(子目录重复 `project()` 会另起一棵构建树,target 互相看不见)。

### 验证

```bash
grep -rn "project(" */CMakeLists.txt
# 期望: 无输出
```

---

## ④ 依赖边:只用 target_link_libraries

### 做什么

**判断规则(照抄即可):**

| 被依赖的东西出现在哪 | 关键字 |
|---|---|
| 本模块的**公开头文件**里 | `PUBLIC` |
| 只在 `.c` 文件里 | `PRIVATE` |

```cmake
# domain: thermostat.h 里 #include "temp_sensor.h" → 公开头文件 → PUBLIC
target_link_libraries(domain PUBLIC ports)

# adapters: lm75_i2c.h 里 #include "temp_sensor.h" → PUBLIC
#           hal.h 只在 lm75_i2c.c 里用       → PRIVATE
target_link_libraries(adapters PUBLIC ports PRIVATE hal)
```

### 绝对禁止

```cmake
# ❌ 打洞:把项目根塞进 include path
target_include_directories(domain PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

# ❌ 打洞:顶层为 app 兜底
target_include_directories(smartheat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ❌ 用相对路径 include 绕过依赖
target_include_directories(test_thermostat PRIVATE ../domain)
```

### 验证

```bash
grep -rn '\.\./\|CMAKE_SOURCE_DIR' */CMakeLists.txt CMakeLists.txt | grep -v "^Binary"
# 期望: 无输出(或仅有注释)
```

---

## ⑤ include 平铺

### 做什么

每个模块暴露**自己**的目录,使用方**平铺** include:

```c
/* ✅ */  #include "temp_sensor.h"
/* ❌ */  #include "ports/temp_sensor.h"
```

### 验证

```bash
grep -rn '#include "' --include='*.c' --include='*.h' domain adapters ports main.c
# 期望: 没有任何 "某模块/某头文件.h" 形式的 include
```

---

## ⑥ 编译选项挂到**每一个** target

### 做什么

用一个 `INTERFACE` target 当载体,然后 **link 到每个会编译代码的 target**。

```cmake
add_library(project_warning INTERFACE)
target_compile_options(project_warning INTERFACE -Wall -Wextra -Wpedantic -Werror)
```

```cmake
# 三个库也必须挂上,否则它们整个不加警告
domain/CMakeLists.txt:   target_link_libraries(domain PUBLIC ports PRIVATE project_warning)
adapters/CMakeLists.txt: target_link_libraries(adapters PUBLIC ports PRIVATE hal project_warning)
hal/CMakeLists.txt:      target_link_libraries(hal PRIVATE project_warning)
```

> **这是最容易漏的一步。** 本仓库当前只把 `project_warning` 挂在了 `smartheat` 上,
> 实测 `hal_pc.c` / `thermostat.c` / `lm75_i2c.c` 拿到的 `-W*` 参数是**空的** ——
> 等于三个模块完全没有警告检查。

### 验证(必做)

```bash
python3 -c "
import json
d=json.load(open('build/compile_commands.json'))
for e in d:
    w=[t for t in e['command'].split() if t.startswith('-W')]
    print(f\"{e['file'].split('/')[-1]:18} {w}\")
"
# 期望: 每一行都有 -Wall,没有空列表
```

---

## ⑦ 自动校验依赖方向(替代人肉 grep)

### 做什么

建 `tools/check_deps.py`:

```python
#!/usr/bin/env python3
"""校验模块依赖方向。用法: python3 tools/check_deps.py <build>/compile_commands.json"""
import json, sys, os

# 每个源文件【允许】拿到哪些模块的 include 目录
ALLOWED = {
    'hal/hal_pc.c':        {'hal'},
    'domain/thermostat.c': {'domain', 'ports'},
    'adapters/lm75_i2c.c': {'adapters', 'ports', 'hal'},
    'main.c':              {'hal', 'domain', 'ports', 'adapters'},
}

def main(path):
    root = os.path.dirname(os.path.dirname(os.path.abspath(path)))
    data = json.load(open(path))
    bad = 0
    for e in data:
        f = os.path.relpath(e['file'], root)
        incs = {os.path.relpath(os.path.normpath(i[2:]), root)
                for i in e['command'].split() if i.startswith('-I')}
        allowed = ALLOWED.get(f)
        if allowed is None:
            print(f"  ! 未登记的文件: {f}"); bad += 1; continue
        extra = incs - allowed
        if extra:
            print(f"  X {f}: 拿到了越权的 include 目录 {sorted(extra)}"); bad += 1
        if '.' in incs:
            print(f"  X {f}: 项目根被放进了 include path (打洞)"); bad += 1
    print("OK: 依赖方向合法,无打洞" if not bad else f"FAIL: {bad} 处违规")
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'build/compile_commands.json'))
```

### 为什么需要它

`grep -R '#include "hal' domain/` 这种检查**只有在依赖边做对之后才有效**。
在 ④ 没做完之前,项目根一直在 include path 里,任何越权 include 都能编过,只能靠事后 grep。
做完 ④ 之后,越权会直接**编译失败**;这个脚本是第二道保险。

### 验证(脚本本身也要验)

```bash
python3 tools/check_deps.py build/compile_commands.json
# 期望: OK

# 反向验证:故意让 domain 偷拿 hal
sed -i 's|target_link_libraries(domain PUBLIC ports)|target_link_libraries(domain PUBLIC ports PRIVATE hal)|' domain/CMakeLists.txt
rm -rf build && cmake -S . -B build && python3 tools/check_deps.py build/compile_commands.json
# 期望: X domain/thermostat.c: 拿到了越权的 include 目录 ['hal']   (退出码 1)
```

---

## ⑧ 测试集成到同一棵构建树

### 做什么

**根:** 见 ② 的 `include(CTest)` + `add_subdirectory(test)`。

**`test/CMakeLists.txt`:** 只留 target,不要 `project()`、不要 `cmake_minimum_required()`。

```cmake
add_executable(test_thermostat
    test_thermostat.c
    fake_sensor.c                 # 测试替身
)

target_include_directories(test_thermostat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_thermostat PRIVATE domain project_warning)

add_test(NAME thermostat COMMAND test_thermostat)
```

**三条铁律:**

| 规则 | 原因 |
|---|---|
| ❌ 不要第二个 `project()` | 会另起一棵构建树,`domain` target 在那里不存在 → 退化成 `-ldomain` |
| ❌ 不要 `../domain/thermostat.c` | 测试必须**链接** target,不重编译源码,否则编译选项/宏会漂移 |
| ❌ 不要 `../domain` include | include 由 `target_link_libraries(... domain)` 自动带过来 |

### 验证

```bash
rm -rf build
cmake -S . -B build && cmake --build build
ctest --test-dir build                    # 期望 100% tests passed
ctest --test-dir build -N                 # 期望列出测试名

# 测试必须能在 link 阶段看到 domain:检查链接行
grep -o '\-ldomain' build/test/CMakeFiles/test_thermostat.dir/link.txt || echo "OK: 没有 -ldomain,说明真的链接到 target"
```

---

## ⑨ 交叉编译与多目标

### 做什么

产品固件和测试**必须分树**——因为 `ctest` 要**执行**二进制,而 ARM 固件在 PC 上跑不了。

```bash
# 固件:交叉工具链,关掉测试
cmake -S . -B build-arm -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DBUILD_TESTING=OFF
cmake --build build-arm

# 测试:host 工具链
cmake -S . -B build-host
cmake --build build-host && ctest --test-dir build-host
```

`BUILD_TESTING` 就是那根分离的缝:`include(CTest)` 定义了它,默认 `ON`;
`-DBUILD_TESTING=OFF` 时 `add_subdirectory(test)` 整个不执行。

平台差异用**选择变量**而不是改源文件:

```cmake
# hal/CMakeLists.txt —— 不要写死 hal_pc.c
if(NOT DEFINED HAL_IMPL)
    set(HAL_IMPL hal_pc.c)          # 默认 PC 模拟
endif()
add_library(hal ${HAL_IMPL})
```

### 验证

```bash
cmake -S . -B build-arm -DBUILD_TESTING=OFF && cmake --build build-arm
ls build-arm/ | grep -i test || echo "OK: 固件构建里没有测试"
ctest --test-dir build-arm -N                 # 期望 Total Tests: 0
```

---

## ⑩ 验收清单

每次改动构建系统后,按顺序跑完这 6 条:

```bash
rm -rf build && cmake -S . -B build && cmake --build build      # 1. 一次配置能出全部目标
cmake --build build --verbose 2>&1 | grep -o -- '-std=[^ ]*' | sort -u   # 2. C 标准是钉死的
python3 tools/check_deps.py build/compile_commands.json          # 3. 依赖方向合法
python3 -c "import json;d=json.load(open('build/compile_commands.json'));print([e['file'].split('/')[-1] for e in d if not [t for t in e['command'].split() if t.startswith('-W')]] or 'OK: 所有文件都有警告选项')"   # 4. 无遗漏 target
ctest --test-dir build                                           # 5. 测试能跑
grep -rn '\.\./\|CMAKE_SOURCE_DIR' */CMakeLists.txt CMakeLists.txt | grep -v '^Binary' || echo "OK: 无打洞"   # 6. 无打洞
```

---

## 附:常见错误 → 症状 → 根因

| 症状 | 根因 | 归属步骤 |
|---|---|---|
| 删掉某行 include 后编不过 | 那行是**唯一的**依赖通道 → 依赖边没写 | ④ |
| `fatal error: 某某.h: No such file` | 缺 `target_link_libraries`,或 include 没平铺 | ④⑤ |
| 链接期报 `cannot find -lxxx` | target 名写错/不在同一棵构建树 | ④⑧ |
| 配置期 `CMake Error ... target was not found` | 用了 `::` 命名空间名字但 target 不存在 | (加命名空间的好处) |
| 某个 `.c` 完全没警告 | `project_warning` 没挂到那个 target | ⑥ |
| 改一个源的编译选项,测试行为变了 | 测试重编译了源码而不是 link target | ⑧ |
| 编译期 `implicit declaration of usleep` | 有人把 C 标准改成严格 `c11` | ① |

---

## 一句话总纲

> **目录定边界,`target_link_libraries` 定方向,`check_deps.py` 守底线,`ctest` 验行为。**
> 每一步都要有一条能跑的命令来证明它成立。
