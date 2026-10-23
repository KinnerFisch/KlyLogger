# KlyLogger

![License](https://img.shields.io/badge/License-BSL%201.0-blue) ![C++20](https://img.shields.io/badge/C++20-Standard-purple) ![C11](https://img.shields.io/badge/C11-Standard-yellow)

**A lightweight, color console and file logging library for C and C++.**

**一个支持彩色控制台输出和日志文件写入的轻量级 C/C++ 日志库。**

![Demo](https://raw.githubusercontent.com/KinnerFisch/KlyLogger/refs/heads/main/demo.jpg)

---

## Platforms / 平台支持

- Windows (tested on Windows 7, Windows 10 and Windows 11)
- Linux (tested on Ubuntu and Kali)

Windows builds support both the MSVC toolchain and clang-cl. GCC/MinGW is not supported on Windows.
Linux builds support both GCC and Clang.

Windows 版本支持 MSVC 工具链和 clang-cl，不支持 GCC/MinGW。
Linux 版本同时支持 GCC 和 Clang。

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

### Log level / 日志等级

The minimum severity is a single process-wide setting shared by every logger object.
Messages below it are discarded before formatting, and only `debug` is hidden by default.

最低等级是一个进程级设置，由所有日志器对象共享；低于它的消息在格式化之前就被丢弃，默认只隐藏 `debug`。

```cpp
KlyLogger::set_log_level(KlyLogger::LogLevel::Warn); // keep warn, error and fatal only / 只保留 warn、error、fatal
KlyLogger::LogLevel level = KlyLogger::log_level();  // read the active level / 读取当前等级
```

The same process-wide setting is exposed to C through `kly_logger_set_log_level` and `kly_logger_log_level`.

同一个进程级设置也通过 `kly_logger_set_log_level` 与 `kly_logger_log_level` 暴露给 C。

```c
kly_logger_set_log_level(KLY_LOGGER_LOG_LEVEL_WARN); // keep warn, error and fatal only / 只保留 warn、error、fatal
KlyLoggerLogLevel level = kly_logger_log_level();    // read the active level / 读取当前等级
```

### C API / C 接口

The C header keeps the object-oriented logger through an opaque handle. C11 `_Generic` lets the static library retain argument types and apply fmt-style `{}` formatting.
The default C APIs use wide strings; narrow-string APIs have the `_narrow` suffix.

C 头文件通过 opaque handle 保留日志器对象。C11 `_Generic` 让静态库能够保留参数类型，并应用 fmt 风格的 `{}` 格式化。
默认 C API 使用宽字符串，窄字符串 API 使用 `_narrow` 后缀。

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

The same `debug`, `info`, `warn`, `error`, and `fatal` names accept either a plain message or a format string followed by up to 16 arguments.
C11 `_Generic` retains each argument's type and the macro derives the argument count,
so users do not need to construct tagged arrays, provide a count, or call a separate `_format` function.
Explicit constructors such as `KLY_INT_ARG` remain available for uncommon types or exact conversion control.

同一组 `debug`、`info`、`warn`、`error`、`fatal` 名称既可接收普通消息，也可在格式串后直接接收最多 16 个参数。
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

## API Reference / 接口参考

The complete public surface: the C++ API is declared in `KlyLogger.hpp`, the C API in `KlyLogger.h`.
Everything is thread-safe, and logging is asynchronous: calls enqueue a message that a background thread writes out.

完整的公开接口：C++ 接口声明于 `KlyLogger.hpp`，C 接口声明于 `KlyLogger.h`。
所有接口都是线程安全的，且日志是异步的：每次调用只是把消息排入队列，由后台线程写出。

### C++ API

#### Construction and naming / 构造与命名

```cpp
#include "KlyLogger.hpp"

KlyLogger defaultLogger;                  // no name / 无名称
KlyLogger wideName(L"Network");           // std::wstring name / 宽字符串名称
KlyLogger narrowName(std::string("I/O")); // std::string name, decoded as UTF-8 / 窄字符串名称，按 UTF-8 解码

KlyLogger copied(wideName);               // copy ctor: also copies callbacks / 拷贝构造：同时拷贝回调
KlyLogger moved(std::move(narrowName));   // move ctor: transfers name and callbacks / 移动构造：转移名称与回调

defaultLogger.wstring(); // L"KlyLogger{name=<empty>}"
wideName.wstring();      // L"KlyLogger{name=Network}"
wideName.string();       //  "KlyLogger{name=Network}"
```

Logger names are sanitized: any text before the final CR/LF is discarded, so a name cannot forge extra log lines.

日志器名称会被净化：最终换行符之前的内容会被丢弃，因此名称无法伪造额外日志行。

#### Logging / 输出日志

```cpp
logger.debug(L"cache hit: {} entries", count);
logger.info("User {} {}", user, action);
logger.warn(L"Low memory: {} MB left", 100);
logger.error(L"File not found: {}", "config.txt");
logger.fatal(L"Unexpected crash!");
```

Rules that apply to all five levels / 五个等级共同的规则：

- The message may be `const wchar_t *`, `std::wstring`, `const char *`, `std::string`, or a string literal. Narrow input is decoded as UTF-8.
- 消息可以是 `const wchar_t *`、`std::wstring`、`const char *`、`std::string` 或字符串字面量；窄字符串按 UTF-8 解码。
- With no arguments the message is emitted verbatim (no `{}` parsing). With arguments, [{fmt}](https://fmt.dev/) `{}` formatting is applied.
- 无参数时消息原样输出（不解析 `{}`）；有参数时按 [{fmt}](https://fmt.dev/) 的 `{}` 语法格式化。
- Arguments are converted automatically: `std::string`/`char *` are decoded as UTF-8, `nullptr` prints `nullptr`, and any type exposing `.string()`/`.wstring()` (for example `std::filesystem::path`) is accepted.
- 参数会自动转换：`std::string`/`char *` 按 UTF-8 解码，`nullptr` 输出 `nullptr`，任何提供 `.string()`/`.wstring()` 的类型（例如 `std::filesystem::path`）都可直接传入。
- A format error never throws: the error text is appended to the message instead.
- 格式化出错不会抛异常：错误信息会附加到消息末尾。
- The methods are `const` and may be called from any thread. Messages are serialized by the background thread, so they never interleave with each other.
- 这些方法都是 `const`，可从任意线程调用；消息由后台线程串行写出，因此彼此之间不会交错。

#### Log level / 日志等级

```cpp
KlyLogger::LogLevel level = KlyLogger::log_level();   // current filter / 当前过滤等级
KlyLogger::set_log_level(KlyLogger::LogLevel::Warn);  // keep warn, error, fatal / 只保留 warn、error、fatal
```

| Enumerator | Default | Meaning |
|---|---|---|
| `LogLevel::Debug` | hidden | Detailed diagnostics / 详细诊断 |
| `LogLevel::Info` | shown | Normal events / 常规信息 |
| `LogLevel::Warn` | shown | Recoverable problems / 可恢复的问题 |
| `LogLevel::Error` | shown | Failed operations / 失败的操作 |
| `LogLevel::Fatal` | shown | Unrecoverable failures / 不可恢复的失败 |

The filter is process-wide and shared by every logger object. Calls below it are dropped *before* formatting, so filtered messages cost almost nothing.

该过滤等级是进程级的，被所有日志器对象共享。低于它的调用会在**格式化之前**被丢弃，因此被过滤的消息几乎没有开销。

#### Callbacks / 回调

```cpp
logger.set_before_log_callback([]() {
	// Runs before each output line from specific logger / 特定日志器的每行输出前调用
});

logger.set_after_log_callback([](const std::wstring &original, const std::wstring &plain) {
	// original: raw message including color codes / 原始消息（含颜色代码）
	// plain:    the message with color codes removed / 去掉颜色代码的纯文本消息
});

KlyLogger::set_global_before_log_callback([]() {
	// Runs before each output line from every logger / 所有日志器的每行输出前调用
});

KlyLogger::set_global_after_log_callback([](const std::wstring &original, const std::wstring &plain) {
	// Same arguments as the instance callback / 参数与实例回调相同
});

logger.set_before_log_callback({});        // clear / 清除
logger.set_after_log_callback(nullptr);    // clear / 清除
KlyLogger::set_global_before_log_callback({});     // clear / 清除
KlyLogger::set_global_after_log_callback(nullptr); // clear / 清除
```

- Instance callbacks apply to one logger; global callbacks apply to every logger. Both are snapshotted when a message is submitted, so a change affects later messages only. Pass an empty function or `nullptr` to clear either kind.
- 实例回调只作用于一个日志器，全局回调作用于所有日志器。两者都在提交消息时进行快照，因此修改只影响之后的消息。传入空函数或 `nullptr` 可清除任一种回调。
- Each non-empty output line runs in this order: `global_before -> instance_before -> log -> instance_after -> global_after`.
- 每个非空输出行按以下顺序执行：`global_before -> instance_before -> log -> instance_after -> global_after`。
- Callbacks are invoked on the background logging thread, each in its own try-catch block. Exceptions are swallowed without skipping the output or remaining callbacks.
- 回调在后台日志线程上调用，每个回调都有独立的 try-catch。异常会被忽略，不会跳过日志输出或后续回调。
- Calling `KlyLogger::wait()` inside a callback is a no-op, because the calling thread is the writer itself.
- 在回调里调用 `KlyLogger::wait()` 是空操作，因为调用线程本身就是写出线程。

#### Waiting and shutdown / 等待与退出

```cpp
KlyLogger::wait();                          // block until all queued output is written / 阻塞直到队列全部写出
bool idle = KlyLogger::is_finished_tasks(); // true when nothing is queued or in flight / 队列为空且无进行中的任务
```

The background runtime is drained and joined automatically at process exit, but calling `wait()` explicitly before returning from `main` is recommended when the log file must be complete.

后台运行时会在进程退出时自动排空并 join；如果要求日志文件完整，建议在 `main` 返回前显式调用 `wait()`。

#### StringConverter / 字符串转换工具

```cpp
std::string utf8 = KlyLogger::StringConverter::to_string(L"wide to UTF-8"); // wide -> UTF-8 narrow
std::wstring wide = KlyLogger::StringConverter::to_wstring("UTF-8 narrow"); // UTF-8 narrow -> wide

// Same conversion rules the log methods use / 与日志方法相同的转换规则
auto converted = KlyLogger::StringConverter::convert_formatting_value(42);

// Format a message exactly like a log call, without logging it / 按日志调用的规则格式化，但不输出
std::wstring text = KlyLogger::StringConverter::format_message(L"{} of {}", 1, 2);
```

- `to_string`/`to_wstring` are UTF-8 aware, handle surrogate pairs, and replace malformed input with U+FFFD.
- `to_string`/`to_wstring` 支持 UTF-8、代理对，并把非法输入替换为 U+FFFD。
- These helpers are handy inside an `after` callback when you need to normalize text yourself.
- 在 `after` 回调中需要自行处理文本时，这些工具很有用。

#### Predefined styles / 预定义样式

```cpp
struct LogStyle {
	std::string level, level_color, text_color;   // level name, ANSI level/text colors
	uint16_t level_color_win, text_color_win;     // legacy Windows console attributes
};

KlyLogger::DEBUG_STYLE; KlyLogger::INFO_STYLE; KlyLogger::WARN_STYLE;
KlyLogger::ERROR_STYLE; KlyLogger::FATAL_STYLE;
```

These are the read-only descriptions of the five built-in styles (level label plus console colors) that the logger uses for its own output.

它们描述了库自身输出所用的五种内置样式（等级名称与控制台配色），仅供读取。

### C API / C 接口

The C API mirrors the C++ API through an opaque `KlyLoggerHandle`. In C, `KlyLogger` is a typedef for that handle. Every function tolerates a `NULL` handle by doing nothing.

C 接口通过不透明的 `KlyLoggerHandle` 对应 C++ 接口；在 C 中 `KlyLogger` 是该句柄的类型别名。所有函数对 `NULL` 句柄都安全（不做事）。

#### Handle lifecycle / 句柄生命周期

```c
#include "KlyLogger.h"

KlyLogger a = kly_logger_create();                       // unnamed / 无名称
KlyLogger b = kly_logger_create_named(L"Network");       // wide name / 宽字符串名称
KlyLogger c = kly_logger_create_named_narrow("Network"); // narrow name / 窄字符串名称

if (!a) return 1; // NULL when allocation fails / 分配失败时为 NULL

const char *narrow = kly_logger_string(a);    // owned by the logger / 由日志器持有
const wchar_t *wide = kly_logger_wstring(a);  // owned by the logger / 由日志器持有

kly_logger_destroy(a); // free the handle / 释放句柄
```

Returned strings stay valid until that logger is destroyed. Pending log tasks keep their own callback snapshots, so destroying a logger is safe while output is still queued.

返回的字符串在该日志器销毁前一直有效。尚未处理的日志任务持有各自的回调快照，因此即使仍有输出排队，销毁日志器也是安全的。

#### Logging functions / 日志函数

```c
kly_logger_debug(logger, L"a plain message"); // no arguments, braces stay literal / 无参数，花括号原样输出
kly_logger_info(logger, L"User {} has {:04} points; pi={:.2f}", L"Lucy", 42, 3.14159);
kly_logger_warn(logger, L"Low memory: {} MB", 100);
kly_logger_error(logger, L"File not found: {}", KLY_STRING_ARG("config.txt"));
kly_logger_fatal(logger, L"Unexpected crash!");

// Narrow-string counterparts / 窄字符串版本
kly_logger_info_narrow(logger, "narrow {} {}", KLY_STRING_ARG("text"), 7);
```

- Wide-string functions take `const wchar_t *`; the `_narrow` variants take `const char *`.
- 宽字符串函数接收 `const wchar_t *`；`_narrow` 版本接收 `const char *`。
- Each name accepts either a plain message or a format string plus up to 16 arguments; the argument count is derived at compile time.
- 每个名称既可接收普通消息，也可接收格式串加最多 16 个参数；参数数量在编译期推导。
- A `char *` used as a formatting argument must be wrapped with `KLY_STRING_ARG` on platforms where `wchar_t` and `char` are compatible types.
- 在 `wchar_t` 与 `char` 为兼容类型的平台上，作为格式参数的 `char *` 需要用 `KLY_STRING_ARG` 包装。

#### Explicit formatting arguments / 显式格式化参数

The `_Generic` macros infer types automatically. When you need exact control, or a type the selector cannot see, build a tagged `KlyLoggerFormatArg` yourself:

`_Generic` 宏会自动推断类型。需要精确控制、或者类型无法被选择器识别时，可以自行构造带标记的 `KlyLoggerFormatArg`：

```c
kly_logger_info(logger, L"explicit {}", KLY_UINT_ARG(0xFFFFFFFFFFFFFFFFull));

// Advanced: build the tagged array yourself, then call the bridge directly /
// 进阶：自行构造带标记数组，再直接调用桥接函数
const KlyLoggerFormatArg args[] = { kly_logger_arg_long_double(3.5L) };
kly_logger_detail_info_args(logger, L"value {}", args, 1);
```

| Macro | Tagged type | Underlying value |
|---|---|---|
| `KLY_INT_ARG(v)` | `KLY_LOGGER_FORMAT_SIGNED_INTEGER` | `int64_t` |
| `KLY_UINT_ARG(v)` | `KLY_LOGGER_FORMAT_UNSIGNED_INTEGER` | `uint64_t` |
| `KLY_DOUBLE_ARG(v)` | `KLY_LOGGER_FORMAT_FLOATING_POINT` | `double` |
| `KLY_LONG_DOUBLE_ARG(v)` | `KLY_LOGGER_FORMAT_LONG_DOUBLE` | `long double` |
| `KLY_BOOL_ARG(v)` | `KLY_LOGGER_FORMAT_BOOLEAN` | `bool` |
| `KLY_CHAR_ARG(v)` | `KLY_LOGGER_FORMAT_CHARACTER` | `char` |
| `KLY_WCHAR_ARG(v)` | `KLY_LOGGER_FORMAT_WIDE_CHARACTER` | `wchar_t` |
| `KLY_STRING_ARG(v)` | `KLY_LOGGER_FORMAT_STRING` | `const char *` (UTF-8) |
| `KLY_WSTRING_ARG(v)` | `KLY_LOGGER_FORMAT_WIDE_STRING` | `const wchar_t *` |
| `KLY_POINTER_ARG(v)` | `KLY_LOGGER_FORMAT_POINTER` | `const void *` |

The equivalent low-level functions are `kly_logger_arg_signed`, `kly_logger_arg_unsigned`, `kly_logger_arg_double`, `kly_logger_arg_long_double`, `kly_logger_arg_bool`, `kly_logger_arg_char`, `kly_logger_arg_wchar`, `kly_logger_arg_string`, `kly_logger_arg_wstring`, `kly_logger_arg_pointer`, and the pass-through `kly_logger_arg_identity`. String and pointer values are borrowed only for the duration of the call.

对应的底层函数为 `kly_logger_arg_signed`、`kly_logger_arg_unsigned`、`kly_logger_arg_double`、`kly_logger_arg_long_double`、`kly_logger_arg_bool`、`kly_logger_arg_char`、`kly_logger_arg_wchar`、`kly_logger_arg_string`、`kly_logger_arg_wstring`、`kly_logger_arg_pointer`，以及直通的 `kly_logger_arg_identity`。字符串与指针只在调用期间被借用。

#### Callbacks, log level and waiting / 回调、日志等级与等待

```c
static void on_before(void) { /* ... */ }
static void on_after(const wchar_t *original, const wchar_t *plain) { /* ... */ }

kly_logger_set_before_log_callback(logger, on_before);
kly_logger_set_after_log_callback(logger, on_after);
kly_logger_set_before_log_callback(logger, NULL); // clear / 清除
kly_logger_set_after_log_callback(logger, NULL);  // clear / 清除

kly_logger_set_global_before_log_callback(on_before);
kly_logger_set_global_after_log_callback(on_after);
kly_logger_set_global_before_log_callback(NULL); // clear / 清除
kly_logger_set_global_after_log_callback(NULL);  // clear / 清除

kly_logger_set_log_level(KLY_LOGGER_LOG_LEVEL_DEBUG);
KlyLoggerLogLevel level = kly_logger_log_level();

kly_logger_wait();                        // drain queued output / 等待队列写出
int idle = kly_logger_finished_tasks();   // nonzero when idle / 空闲时非零
```

Global callbacks use the same callback types as instance callbacks, without a logger handle. Snapshot timing, per-line execution order, and exception handling are the same as in C++.

全局回调使用与实例回调相同的回调类型，无需传入日志器句柄。快照时机、逐行执行顺序和异常处理方式与 C++ 相同。

`KlyLoggerLogLevel` values are `KLY_LOGGER_LOG_LEVEL_DEBUG`, `KLY_LOGGER_LOG_LEVEL__INFO`, `KLY_LOGGER_LOG_LEVEL__WARN`, `KLY_LOGGER_LOG_LEVEL__ERROR` and `KLY_LOGGER_LOG_LEVEL__FATAL`, ordered from least to most severe and numerically identical to their C++ counterparts.

`KlyLoggerLogLevel` 的取值为 `KLY_LOGGER_LOG_LEVEL_DEBUG`、`KLY_LOGGER_LOG_LEVEL__INFO`、`KLY_LOGGER_LOG_LEVEL__WARN`、`KLY_LOGGER_LOG_LEVEL__ERROR`、`KLY_LOGGER_LOG_LEVEL__FATAL`，由低到高排列，且数值与 C++ 对应值一致。

The `kly_logger_detail_*_args` functions are internal bridges used by the convenience macros; applications should call `kly_logger_debug`/`kly_logger_info`/`kly_logger_warn`/`kly_logger_error`/`kly_logger_fatal` instead.

`kly_logger_detail_*_args` 是便捷宏内部使用的桥接函数，应用应改用 `kly_logger_debug`/`kly_logger_info`/`kly_logger_warn`/`kly_logger_error`/`kly_logger_fatal`。

### Color codes / 颜色代码

Messages may embed Minecraft-style codes: a `§` (U+00A7) followed by one character. Colors apply to the rest of the line, and `§r` restores the level's text color.

消息中可以嵌入 Minecraft 风格代码：`§`（U+00A7）后跟一个字符。颜色会作用到该行剩余部分，`§r` 恢复该等级的默认文字颜色。

| Code | Color | Code | Color |
|---|---|---|---|
| `§0` | black / 黑 | `§8` | dark gray / 深灰 |
| `§1` | dark blue / 深蓝 | `§9` | blue / 蓝 |
| `§2` | dark green / 深绿 | `§a` | green / 绿 |
| `§3` | dark aqua / 深青 | `§b` | aqua / 青 |
| `§4` | dark red / 深红 | `§c` | red / 红 |
| `§5` | dark purple / 深紫 | `§d` | light purple / 亮紫 |
| `§6` | gold / 金 | `§e` | yellow / 黄 |
| `§7` | gray / 灰 | `§f` | white / 白 |

Formatting codes / 样式代码：

| Code | Emitted / 发出的序列 | Effect / 效果 |
|---|---|---|
| `§k` | `ESC[5m` | Blink; a terminal cannot obfuscate text the way Minecraft does / 闪烁；终端无法像 MC 那样真正混淆文字 |
| `§l` | `ESC[21m` | Bold / 加粗 |
| `§m` | `ESC[9m` | Strikethrough / 删除线 |
| `§n` | `ESC[4m` | Underline / 下划线 |
| `§o` | `ESC[3m` | Italic / 斜体 |
| `§r` | the level's own text color / 该等级自身的文字颜色 | Restore the level's text color / 恢复该等级的文字颜色 |

These are the sequences used by the Minecraft server-side color converter this library's output is modeled on; the only deliberate difference is `§r`, which restores the level's text color instead of emitting a plain reset. `ESC` is the escape character (U+001B).

以上序列与该库输出所参考的 Minecraft 服务端颜色转换器一致；唯一有意的差异是 `§r`，它恢复该等级自身的文字颜色，而不是发出普通的重置序列。`ESC` 即转义字符 U+001B。

The 16 colors and `§r` work on both output paths, but `§k`, `§l`, `§m`, `§n` and `§o` are ANSI escape sequences, so they need a terminal that understands them. KlyLogger enables virtual terminal processing automatically on Windows 10 and later; on legacy consoles such as Windows 7, 8 or XP, or whenever VT processing cannot be enabled, it falls back to the legacy console API, where:

16 色与 `§r` 在两条输出路径上都有效；而 `§k`、`§l`、`§m`、`§n`、`§o` 是 ANSI 转义序列，需要能识别它们的终端。KlyLogger 会在 Windows 10 及更高版本上自动开启虚拟终端处理；在 Windows 7、8、XP 等旧式控制台，或无法开启 VT 处理时，会回退到旧式控制台 API，此时：

- `§n` maps to the console underscore attribute and still underlines / `§n` 映射为控制台下划线属性，仍可加下划线；
- `§k` becomes reverse video, because the Win32 console text attributes have no blink flag / `§k` 变为反显，因为 Win32 控制台文本属性没有闪烁位；
- `§l`, `§m` and `§o` have no effect / `§l`、`§m`、`§o` 完全无效。

Color codes are interpreted only when standard error is a terminal. When output is redirected they are dropped, and they are never written to the log file. Trailing lone `§` characters are removed.

只有标准错误是终端时才会解释颜色代码；输出被重定向时会被丢弃，且绝不会写入日志文件。行尾孤立的 `§` 会被移除。

### Log files / 日志文件

- Every line written to the console is also written, in plain color-free text without `\r`, to `logs/latest.log` beside the executable (not the working directory).
- 写入控制台的每一行，也会以无颜色的纯文本（不含 `\r`）写入**程序所在目录**下的 `logs/latest.log`（不是工作目录）。
- When the local date changes, `latest.log` is renamed to `logs/YYYY-MM-DD-N.log` (first free `N`) and a new `latest.log` is started.
- 当本地日期变化时，`latest.log` 会被重命名为 `logs/YYYY-MM-DD-N.log`（取第一个可用的 `N`），并新建 `latest.log`。
- The whole file backend is compiled out by the `NoLogFile` targets, which define `KLY_LOGGER_OPTION_NO_LOG_FILE`.
- 使用 `NoLogFile` 系列目标时会整体编译掉文件后端，它们定义了 `KLY_LOGGER_OPTION_NO_LOG_FILE`。

### Compile-time options / 编译期选项

| Option | Effect |
|---|---|
| `KLY_LOGGER_OPTION_NO_LOG_FILE` | Compile out the file backend / 编译掉文件后端 |
| `KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE` | Windows: re-fetch the stderr handle on every write instead of caching it / Windows：每次写出都重新获取 stderr 句柄，而不缓存 |
| `KLY_LOGGER_NATIVE_ARCH` | CMake option; tune for the build machine (`-march=native` / `/arch:AVX2`) / CMake 选项：针对构建机优化 |

## Static build / 静态库构建

```sh
git clone --recursive https://github.com/KinnerFisch/KlyLogger
cd KlyLogger
cmake -S . -B build
cmake --build build --config Release
```

With CMake, link the selected target directly.
On Windows, select the target whose suffix matches the application's runtime library (`/MD` or `/MT`).

使用 CMake 时直接链接所选目标。
Windows 下请选择后缀与应用程序运行库 (`/MD` 或 `/MT`) 匹配的目标。

### Choosing a compiler / 选择编译器

On Windows, both MSVC and clang-cl are supported:

- **MSVC** archives are built with `/GL` and must be linked with MSVC's `link.exe`.
  A clang-cl application can still consume them by passing `/fuse-ld=link` at link time.
- **clang-cl** archives are built with `-flto` and must be linked with `lld-link` (or the clang driver).
  Do not link them with MSVC's `link.exe`.

LTO objects produced by different toolchains are not interchangeable, so an archive built by one toolchain must be linked by the matching linker.

On Linux, both GCC and Clang are supported and follow the standard `-flto` flow.

Windows 下同时支持 MSVC 和 clang-cl：

- **MSVC** 产物使用 `/GL` 编译，必须用 MSVC 的 `link.exe` 链接。
  clang-cl 程序也可以在链接时加 `/fuse-ld=link` 来使用它。
- **clang-cl** 产物使用 `-flto` 编译，必须用 `lld-link`（或 clang 驱动）链接，不能用 MSVC 的 `link.exe`。

不同工具链产生的 LTO 目标文件不能互换，因此用哪种工具链编译的静态库，就必须用与之匹配的链接器来链接。

Linux 下同时支持 GCC 和 Clang，均使用标准的 `-flto` 流程。

```cmake
add_subdirectory(path/to/KlyLogger)
if(MSVC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
	target_link_libraries(your_target PRIVATE KlyLogger::KlyLogger_MD) # /MD
	# target_link_libraries(your_target PRIVATE KlyLogger::KlyLogger_MT) # /MT
else()
	target_link_libraries(your_target PRIVATE KlyLogger::KlyLogger)
endif()
```

All applicable combinations are built together. Each Windows row produces both suffixes, and this applies to both MSVC and clang-cl builds.
Because output-handle caching is Windows-specific, Linux builds only the default and `NoLogFile` targets.

所有适用的组合会同时生成。Windows 下每行都会生成 MD、MT 两份，MSVC 与 clang-cl 构建均适用。
由于输出句柄缓存是 Windows 特有行为，Linux 仅构建默认目标和 `NoLogFile` 目标。

| Log file | Handle cache | Windows CMake targets | Windows libraries | Linux target / archive |
|---|---|---|---|---|
| Enabled | Enabled | `KlyLogger::KlyLogger_MD`, `KlyLogger::KlyLogger_MT` | `KlyLogger_MD.lib`, `KlyLogger_MT.lib` | `KlyLogger::KlyLogger` / `libKlyLogger.a` |
| Disabled | Enabled | `KlyLogger::NoLogFile_MD`, `KlyLogger::NoLogFile_MT` | `KlyLoggerNoLogFile_MD.lib`, `KlyLoggerNoLogFile_MT.lib` | `KlyLogger::NoLogFile` / `libKlyLoggerNoLogFile.a` |
| Enabled | Disabled | `KlyLogger::NoOutputHandleCache_MD`, `KlyLogger::NoOutputHandleCache_MT` | `KlyLoggerNoOutputHandleCache_MD.lib`, `KlyLoggerNoOutputHandleCache_MT.lib` | Not built |
| Disabled | Disabled | `KlyLogger::NoLogFileNoOutputHandleCache_MD`, `KlyLogger::NoLogFileNoOutputHandleCache_MT` | `KlyLoggerNoLogFileNoOutputHandleCache_MD.lib`, `KlyLoggerNoLogFileNoOutputHandleCache_MT.lib` | Not built |

Output-handle caching is a Windows-specific behavior, so variants that disable it are not generated on Linux.

输出句柄缓存是 Windows 特有行为，因此 Linux 不生成用于禁用该行为的变体。

When linking the C API manually, use the C++ linker driver (or explicitly add the platform C++ runtime), because the static implementation is written in C++.

手动链接 C API 时请使用 C++ 链接器驱动，或显式加入平台的 C++ 运行库，因为静态库的内部实现使用 C++。

## Key Features / 主要功能

- Thread-safe operations / 多线程安全操作
- Color-coded console output / 控制台彩色输出
- Supports logging to both console and file / 同时支持控制台和文件日志
- Default and named logger objects ready-to-use / 提供默认和自定义名称日志器，开箱即用
- Easy-to-use [{fmt}](https://fmt.dev/) formatting, including dynamic width and precision / 提供简单易用的 [{fmt}](https://fmt.dev/) 格式化，支持动态宽度与精度
- Supports Minecraft-style color codes in console output / 支持类似 Minecraft 的彩色字符输出
- Supports multiple log levels: debug, info, warn, error, fatal, with a process-wide runtime filter / 支持多种日志等级：debug、info、warn、error、fatal，并提供进程级运行时过滤
- Supports mixed usage of `std::string` and `std::wstring` for logging / 支持 `std::string` 与 `std::wstring` 混合使用
- All log files are automatically stored under the `logs` folder located beside the executable, not in the working directory / 所有日志文件会自动保存到**程序所在位置**（非工作目录）下的 `logs` 文件夹中
> ⚠️ Note: Narrow strings must be UTF-8 encoded.
>
> ⚠️ 注意：窄字符串必须使用 UTF-8 编码。

## Licenses and third-party software / 许可证与第三方软件

KlyLogger is distributed under the [Boost Software License 1.0](LICENSE).
Copyright © 2024–2026 KinnerFisch.
Its formatting implementation uses [{fmt}](https://github.com/fmtlib/fmt),
Copyright © 2012–present Victor Zverovich and {fmt} contributors, which is distributed under the MIT License.
The complete fmt license text is retained in [`fmt/LICENSE`](fmt/LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md),
and is installed alongside KlyLogger binaries by CMake.

KlyLogger 使用 [Boost Software License 1.0](LICENSE) 发布。
版权所有 © 2024–2026 KinnerFisch。
格式化实现使用 [{fmt}](https://github.com/fmtlib/fmt)，版权归 Victor Zverovich 及 {fmt} 贡献者所有，并使用 MIT 许可证。
fmt 的完整许可文本保留于 [`fmt/LICENSE`](fmt/LICENSE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，
CMake 安装静态库时也会一并安装这些许可证文件。

## Inspiration / 灵感来源

The log output format of **KlyLogger** was inspired by [PaperMC](https://github.com/PaperMC/Paper), a well-known Minecraft server project.
The color scheme and visual style were then customized to my own preference.
This project also marks my first step into modern C++ development.

KlyLogger 的日志输出格式灵感来自知名的 Minecraft 服务器项目 [PaperMC](https://github.com/PaperMC/Paper)，并在此基础上根据个人喜好进行了颜色与视觉样式的改进。
同时，这也是本人踏入现代化 C++ 开发的入门作品。
