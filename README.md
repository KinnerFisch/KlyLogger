# KlyLogger

![License](https://img.shields.io/badge/License-BSL%201.0-blue) ![C++20](https://img.shields.io/badge/C++20-Standard-purple)

**A lightweight, color console and file logging library for C and C++.**

**一个支持彩色控制台输出和日志文件写入的轻量级 C/C++ 日志库。**

![Demo](https://raw.githubusercontent.com/KinnerFisch/KlyLogger/refs/heads/main/demo.jpg)

---

## Platforms / 平台支持

- Windows (tested on Windows 7, Windows 10 and Windows 11)
- Linux (tested on Ubuntu and Kali)

---

## Usage / 使用示例

Language requirements: `KlyLogger.hpp` requires C++20 or newer;
`KlyLogger.h` requires C11 or newer for its `_Generic` automatic formatting
arguments. The low-level non-formatting C declarations themselves use only
C99 features, but C11 is the supported minimum for the complete public C API.

语言标准要求：`KlyLogger.hpp` 最低需要 C++20；`KlyLogger.h` 的 `_Generic`
自动格式参数最低需要 C11。底层不带格式参数的 C 声明本身只使用 C99 特性，
但若要使用完整的公开 C API，受支持的最低版本是 C11。

```cpp
#include "KlyLogger.hpp"

int main() {
	// Initialize loggers / 初始化日志器
	KlyLogger logger; // Default logger / 默认日志器

	// Logging examples / 日志示例
	logger.info(L"Application started: {}", L"MyApp");

	std::string user = "Lucy";
	std::wstring action = L"logged in";
	logger.info("User {} {}", user, action); // Mixed string and wstring / string 与 wstring 混用

	logger.warn(L"Low memory warning: {} MB left", 100);
	logger.error(L"File not found: {}", "config.txt");
	logger.fatal(L"Unexpected crash!");

	// Minecraft-style colored output / Minecraft 风格彩色字符示例
	// Example: §c red, §a green, §e yellow, §b aqua, §f white, §5 purple
	KlyLogger("Colorful").info(L"§c红色 §a绿色 §e黄色 §b水蓝色 §f白色 §5紫色");

	return 0;
}
```

### C API / C 接口

The C header keeps the object-oriented logger through an opaque handle. C11 `_Generic` lets the static library retain argument types and apply fmt-style `{}` formatting. The default C APIs use wide strings; narrow-string APIs have the `_narrow` suffix.

C 头文件通过 opaque handle 保留日志器对象。C11 `_Generic` 让静态库能够保留参数类型，并应用 fmt 风格的 `{}` 格式化。默认 C API 使用宽字符串，窄字符串 API 使用 `_narrow` 后缀。

```c
#include "KlyLogger.h"

int main(void) {
	KlyLogger logger = kly_logger_create_named(L"C_App");
	const char *narrow_user = "Lucy";
	if (!logger) return 1;

	kly_logger_info(logger, L"Application started");
	kly_logger_info(logger, L"User {} has {:04} points; pi={:.2f}", L"Lucy", 42, 3.14159);
	// A normal char string used as a formatting argument is marked explicitly / 普通 char 字符串作为格式参数时需要显式标记
	kly_logger_info(logger, L"Narrow user: {}", KLY_STRING_ARG(narrow_user));
	kly_logger_warn_narrow(logger, "Narrow text: {}; value: {}", KLY_STRING_ARG("warning"), 7);
	kly_logger_wait();
	return 0;
}
```

The same `info`, `warn`, `error`, and `fatal` names accept either a plain message or a format string followed by up to 16 arguments.
C11 `_Generic` retains each argument's type and the macro derives the argument count,
so users do not need to construct tagged arrays, provide a count, or call a separate `_format` function.
Explicit constructors such as `KLY_INT_ARG` remain available for uncommon types or exact conversion control.

同一组 `info`、`warn`、`error`、`fatal` 名称既可接收普通消息，也可在格式串后直接接收最多 16 个参数。
C11 `_Generic` 会保留每个参数的类型，宏会自动计算参数数量，因此用户不需要构造类型标记数组、手写参数数量或调用单独的 `_format` 函数。
遇到少见类型或需要精确控制转换时，仍可使用 `KLY_INT_ARG` 等显式构造宏。

On unusual C implementations where `wchar_t` is compatible with `char`, the two pointer types cannot coexist in one `_Generic` association list.
KlyLogger therefore treats the ambiguous type as `wchar_t *` by default; wrap a narrow string argument with `KLY_STRING_ARG(value)` explicitly.
Platforms with distinct `wchar_t` and `char` types continue to recognize both automatically.

在少数 `wchar_t` 与 `char` 属于兼容类型的 C 实现中，两种指针无法同时写入同一个 `_Generic` 关联列表。
KlyLogger 会优先将这个有歧义的类型视为 `wchar_t *`；窄字符串参数请显式写成 `KLY_STRING_ARG(value)`。
在 `wchar_t` 与 `char` 类型不同的常规平台上，两者仍会被自动识别。

The C formatter supports `{}`, positional fields such as `{1}`, ordinary format specifications such as `{:08x}` and `{:.2f}`,
escaped braces `{{` and `}}`, and nested dynamic width/precision fields such as `{0:{1}}` and `{0:.{1}f}`.
C tagged arguments are passed to fmt's dynamic argument store and the complete format string is processed in one pass.

C 格式化接口支持 `{}`、`{1}` 等位置参数、`{:08x}` 和 `{:.2f}` 等普通格式说明，`{{`、`}}` 转义花括号，以及 `{0:{1}}`、`{0:.{1}f}` 这类嵌套动态宽度或精度。
C 的类型标记参数会传入 fmt 的动态参数存储，完整格式字符串只进行一次格式化。

## Static build / 静态库构建

Clone the repository together with its submodule / 克隆仓库及其子模块:

```sh
git clone --recursive https://github.com/KinnerFisch/KlyLogger.git
```

If the repository was cloned without `--recursive` / 如果克隆时未使用 `--recursive`:

```sh
git submodule update --init --recursive
```

```sh
mkdir build
cmake -S . -B build
cmake --build build --config Release
```

With CMake, link the selected target directly / 使用 CMake 时直接链接所选目标:

```cmake
add_subdirectory(path/to/KlyLogger)
target_link_libraries(your_target PRIVATE KlyLogger::KlyLogger)
```

All four combinations are built together / 四种组合会同时生成:

| Log file / 日志文件 | Handle cache / 句柄缓存 | CMake target | Windows | Linux |
|---|---|---|---|---|
| Enabled / 启用 | Enabled / 启用 | `KlyLogger::KlyLogger` | `KlyLogger.lib` | `libKlyLogger.a` |
| Disabled / 禁用 | Enabled / 启用 | `KlyLogger::NoLogFile` | `KlyLoggerNoLogFile.lib` | `libKlyLoggerNoLogFile.a` |
| Enabled / 启用 | Disabled / 禁用 | `KlyLogger::NoOutputHandleCache` | `KlyLoggerNoOutputHandleCache.lib` | `libKlyLoggerNoOutputHandleCache.a` |
| Disabled / 禁用 | Disabled / 禁用 | `KlyLogger::NoLogFileNoOutputHandleCache` | `KlyLoggerNoLogFileNoOutputHandleCache.lib` | `libKlyLoggerNoLogFileNoOutputHandleCache.a` |

Output-handle caching is a Windows-specific behavior. The corresponding Linux archives are still generated so release packages have the same four-name layout on every platform.

输出句柄缓存是 Windows 特有行为；Linux 仍会生成对应的静态库，以便各平台发布包保持相同的四种命名布局。

When linking the C API manually, use the C++ linker driver (or explicitly add the platform C++ runtime), because the static implementation is written in C++.

手动链接 C API 时请使用 C++ 链接器驱动，或显式加入平台的 C++ 运行库，因为静态库的内部实现使用 C++。

## Key Features / 主要功能

- Thread-safe operations / 多线程安全操作
- Color-coded console output / 控制台彩色输出
- Supports logging to both console and file / 同时支持控制台和文件日志
- Default and named logger objects ready-to-use / 提供默认和自定义名称日志器, 开箱即用
- Easy-to-use [{fmt}](https://fmt.dev/) formatting, including dynamic width and precision / 提供简单易用的 [{fmt}](https://fmt.dev/) 格式化，支持动态宽度与精度
- Supports Minecraft-style color codes in console output / 支持类似 Minecraft 的彩色字符输出
- Supports multiple log levels: info, warn, error, fatal / 支持多种日志等级：info, warn, error, fatal
- Supports mixed usage of `std::string` and `std::wstring` for logging / 支持 `std::string` 与 `std::wstring` 混合使用
- All log files are automatically stored under the `logs` folder located beside the executable, not in the working directory / 所有日志文件会自动保存到**程序所在位置**（非工作目录）下的 `logs` 文件夹中
> ⚠️ Note: Using `std::string` with non-ASCII characters is **not recommended** to avoid decoding issues.
>
> ⚠️ 注意：不建议在 `std::string` 中使用非 ASCII 字符, 避免解码出现乱码

## Licenses and third-party software / 许可证与第三方软件

KlyLogger is distributed under the [Boost Software License 1.0](LICENSE).
Its formatting implementation uses [{fmt}](https://github.com/fmtlib/fmt),
Copyright © 2012–present Victor Zverovich and {fmt} contributors, which is distributed under the MIT License.
The complete fmt license text is retained in [`fmt/LICENSE`](fmt/LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md),
and is installed alongside KlyLogger binaries by CMake.

KlyLogger 使用 [Boost Software License 1.0](LICENSE) 发布。
格式化实现使用 [{fmt}](https://github.com/fmtlib/fmt)，版权归 Victor Zverovich 及 {fmt} 贡献者所有，并使用 MIT 许可证。
fmt 的完整许可文本保留于 [`fmt/LICENSE`](fmt/LICENSE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，
CMake 安装静态库时也会一并安装这些许可证文件。

## Inspiration / 灵感来源

The log output format of **KlyLogger** was inspired by [PaperMC](https://github.com/PaperMC/Paper), a well-known Minecraft server project.
The color scheme and visual style were then customized to my own preference.
This project also marks my first step into modern C++ development.

KlyLogger 的日志输出格式灵感来自知名的 Minecraft 服务器项目 [PaperMC](https://github.com/PaperMC/Paper)，并在此基础上根据个人喜好进行了颜色与视觉样式的改进。
同时，这也是本人踏入现代化 C++ 开发的入门作品。
