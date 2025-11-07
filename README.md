# KlyLogger

![License](https://img.shields.io/badge/License-BSL%201.0-blue) ![C++20](https://img.shields.io/badge/C++20-Standard-purple)

**A lightweight, color console and file logging library for C++.**
**一个支持彩色控制台输出和日志文件写入的轻量级 C++ 日志库.**

![Demo](https://raw.githubusercontent.com/KinnerFisch/KlyLogger/refs/heads/main/demo.jpg)

---

## Platforms / 平台支持

- Windows (tested on Windows 7, Windows 10 and Windows 11)
- Linux (tested on Ubuntu and Kali)

---

## Usage / 使用示例

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

## Key Features / 主要功能

- Thread-safe operations / 多线程安全操作
- Color-coded console output / 控制台彩色输出
- Supports logging to both console and file / 同时支持控制台和文件日志
- Default and named logger objects ready-to-use / 提供默认和自定义名称日志器, 开箱即用
- Easy-to-use API with `std::format` style formatting / 提供 `std::format` 风格的简单易用 API
- Supports Minecraft-style color codes in console output / 支持类似 Minecraft 的彩色字符输出
- Supports multiple log levels: info, warn, error, fatal / 支持多种日志等级：info, warn, error, fatal
- Supports mixed usage of `std::string` and `std::wstring` for logging / 支持 `std::string` 与 `std::wstring` 混合使用
- All log files are automatically stored under the `logs` folder located beside the executable, not in the working directory / 所有日志文件会自动保存到**程序所在位置**（非工作目录）下的 `logs` 文件夹中
> ⚠️ Note: Using `std::string` with non-ASCII characters is **not recommended** to avoid decoding issues.
>
> ⚠️ 注意：不建议在 `std::string` 中使用非 ASCII 字符, 避免解码出现乱码

---

## Configuration / 配置选项

You can control KlyLogger\'s behavior using the following preprocessor macros:
在引入 `KlyLogger.hpp` 之前定义以下预处理宏可调整 KlyLogger 的行为:

- `KLY_LOGGER_OPTION_NO_LOG_FILE`
  Disable log file output.
  禁用日志文件输出.

- `KLY_LOGGER_DISABLE_EXTERN_RTL_GET_VERSION`
  Prevent duplicate definition of `RtlGetVersion` (used internally by KlyLogger from `ntdll.dll`).
  防止 `RtlGetVersion` 函数重复定义 (KlyLogger 内部使用该函数指向 `ntdll.dll`).

- `KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE`
  Disable caching of output handles, useful for programs that dynamically change console handles.
  禁用输出句柄缓存, 适用于动态改变控制台句柄的程序.
  **Example / 示例:**
  ```cpp
  #define KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE
  #include "KlyLogger.hpp"
  ...
  
  FreeConsole();
  AttachConsole(getParentProcessId());

  // If KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE is not defined,
  // logger output may not appear after reattaching the console.
  // 如果没有定义 KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE,
  // 那么在 attachConsoleToParent() 之后可能看不到日志输出.
  KlyLogger logger;
  logger.info(L"Console reattached successfully!");
  ```

---

## Inspiration / 灵感来源

The log output format of **KlyLogger** was inspired by [PaperMC](https://github.com/PaperMC/Paper), a well-known Minecraft server project.
The color scheme and visual style were then customized to my own preference.
This project also marks my first step into modern C++ development.

KlyLogger 的日志输出格式灵感来自知名的 Minecraft 服务器项目 [PaperMC](https://github.com/PaperMC/Paper), 并在此基础上根据个人喜好进行了颜色与视觉样式的改进.
同时, 这也是本人踏入现代化 C++ 开发的入门作品.
