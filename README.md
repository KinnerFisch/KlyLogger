# KlyLogger

![License](https://img.shields.io/badge/License-BSL%201.0-blue)
![C++](https://img.shields.io/badge/C++20-Standard-purple)

**A lightweight, color console and file logging library for C++**
**一个支持彩色控制台输出和日志文件写入的轻量级 C++ 日志库**

---

## Platforms / 平台支持

- Windows (tested on Win7, Win10, Win11)
- Linux (tested on Ubuntu)

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

---

## Key Features / 主要功能

- Thread-safe operations / 多线程安全操作
- Color-coded console output / 控制台彩色输出
- Supports logging to both console and file / 同时支持控制台和文件日志
- Default and named logger objects ready-to-use / 提供默认和自定义名称日志器，开箱即用
- Easy-to-use API with `std::format` style formatting / 提供 `std::format` 风格的简单易用 API
- Supports Minecraft-style color codes in console output / 支持类似 Minecraft 的彩色字符输出
- Supports multiple log levels: info, warn, error, fatal / 支持多种日志等级：info, warn, error, fatal
- Supports mixed usage of `std::string` and `std::wstring` for logging / 支持 `std::string` 与 `std::wstring` 混合使用
> ⚠️ Note: Using `std::string` with non-ASCII characters is **not recommended** to avoid decoding issues.
> 
> ⚠️ 注意：不建议在 `std::string` 中使用非 ASCII 字符，避免解码出现乱码