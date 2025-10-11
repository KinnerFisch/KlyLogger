#pragma once
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#ifndef KLY_LOGGER_INCLUDED
#define KLY_LOGGER_INCLUDED

#include <codecvt>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>
#include <sys/stat.h>
#include <thread>

using namespace std::chrono_literals;

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno

// If RtlGetVersion is already defined elsewhere in the codebase,
// this macro can be enabled to avoid symbol conflicts.
#ifndef KLY_LOGGER_DISABLE_EXTERN_RTL_GET_VERSION
#pragma comment(lib, "ntdll.lib")
extern "C" int RtlGetVersion(PRTL_OSVERSIONINFOEXW) noexcept;
#endif

#else
#include <sys/resource.h>
#include <unistd.h>
#endif

// Type traits for string conversion.
template<typename T, typename = void>
struct has_string : std::false_type {};
template<typename T>
struct has_string<T, std::void_t<decltype(std::declval<T>().string())>> : std::true_type {};
template<typename T, typename = void>
struct has_wstring : std::false_type {};
template<typename T>
struct has_wstring<T, std::void_t<decltype(std::declval<T>().wstring())>> : std::true_type {};

#if defined(min) || defined(max)
#undef min
#undef max
#endif

// KlyLogger: lightweight, visually and easy-to-use logger.
class KlyLogger {
public:
	// String conversion utilities.
	class StringConverter {
	public:
		// Cross-platform string encoding converter.
		static inline std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

		// Cache used when converting arguments for log tasks (prevents loss of converted data or incorrect log output).
		static inline std::queue<std::wstring> converted;

		// Convert wide string to narrow string.
		static std::string toString(const std::wstring &str) { return converter.to_bytes(str); }

		// Convert narrow string to wide string safely.
		static std::wstring toWString(const std::string &str) {
#ifdef _WIN32
			// On Windows, try using the system code page (CP_ACP) first for conversion.
			// MB_ERR_INVALID_CHARS ensures invalid characters cause an error.
			int len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
			// If a valid length is returned, perform the conversion.
			if (len > 0) {
				// Create a wide string with the required length.
				std::wstring result(len - 1, 0);
				// Convert the narrow string to wide string using MultiByteToWideChar.
				MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), -1, result.data(), len);
				// Return the successfully converted wide string.
				return result;
			}
#endif
			try {
				// Fallback: use std::wstring_convert to convert UTF-8 to wstring.
				return converter.from_bytes(str);
			} catch (const std::exception &e) {
				// If conversion fails, handle exception:
				// Return a fallback wide string: original characters plus an error message.
				const auto &what = e.what();
				return toWString(str.begin(), str.end()) + L"\2478\247o (decoder error: " + toWString(what, what + strlen(what)) + L')';
			}
		}

		// Convert narrow string to wide string directly.
		static std::wstring toWString(const auto &from, const auto &to) { return std::wstring(from, to); }

		// Helper to normalize different argument types into wide strings.
		// Handles std::string, const char*, and custom types with string()/wstring().
		template<typename T>
		static auto &convertFormatting(const T &arg) {
			// If argument is a std::string or convertible to const char*,
			// convert it to std::wstring and store.
			if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char *>) {
				converted.push(toWString(arg));
				return converted.back();
			}
			// If type provides a wstring() method, use it directly.
			else if constexpr (has_wstring<T>::value) {
				converted.push(arg.wstring());
				return converted.back();
			}
			// If type provides a string() method, convert it to wstring.
			else if constexpr (has_string<T>::value) {
				converted.push(toWString(arg.string()));
				return converted.back();
			}
			// Otherwise, return the argument itself.
			else return arg;
		}

		// Convert any argument into std::wstring for formatting.
		// Returns the original value if already wide string, otherwise uses std::format.
		template<typename T>
		static auto &convertArgumentToWString(const T &arg) {
			// If already a std::wstring or convertible to const wchar_t*, return directly.
			if constexpr (std::is_same_v<T, std::wstring> || std::is_convertible_v<T, const wchar_t *>) return arg;
			// Otherwise, format the argument into a wide string using std::format.
			else {
				converted.push(std::format(L"{}", arg));
				return converted.back();
			}
		}

		// Clear temporary converted string cache.
		static void clearConverted() { converted = std::queue<std::wstring>(); }

		// Format a message with optional arguments, returning the formatted wide string.
		template<typename MessageType, typename... Args>
		static std::wstring formatMessage(const MessageType &message, const Args &...args) {
			// Convert message to wide string format.
			const auto convertedMessage = convertFormatting(message);
			const std::wstring msg = convertArgumentToWString(convertedMessage);
			std::wstring formatted = msg;

			// Format message with arguments if provided.
			if constexpr (sizeof...(args) > 0) {
				try {
					// Use std::vformat for argument substitution.
					formatted = std::vformat(msg, std::make_wformat_args(convertFormatting(args)...));
					// Clear conversion cache after successful formatting.
					clearConverted();
				} catch (const std::exception &e) {
					// Append error message if formatting fails.
					formatted = msg + L"\2478\247o (" + toWString(e.what()) + L')';
				}
			}

			return formatted;
		}
	};

private:
	// LogStyle encapsulates all visual and textual attributes for a log level,
	// including level name, Windows console colors, and ANSI escape sequences.
	static constexpr struct LogStyle {
		const std::string level, levelAnsiColor, textAnsiColor;
		const unsigned short levelColor, textColor;
	} INFO_STYLE{"INFO", "\33[0;92m", "\33[0;37m", 10, 7}, WARN_STYLE{"WARN", "\33[0;33m", "\33[0;93m", 6, 14},
	ERROR_STYLE{"ERROR", "\33[0;31m", "\33[0;91m", 4, 12},
	FATAL_STYLE{"FATAL", "\33[2;31m", "\33[0;31m", 32772, 4};

	// Lookup tables: convert Minecraft color codes to ANSI sequences.
	static constexpr const char *mcToAnsiEscape[]{
			"\33[30m",	 "\33[0;34m", "\33[0;32m", "\33[0;36m", "\33[0;31m", "\33[0;35m", "\33[0;33m", "\33[0;37m",
			"\33[0;90m", "\33[0;94m", "\33[0;92m", "\33[0;96m", "\33[0;91m", "\33[0;95m", "\33[0;93m", "\33[0;97m" };

	// Log task containing logger name, log message and log style.
	struct LogTask {
		const LogStyle style;
		const std::wstring name, message;
	};

	// Platform-specific console handling.
	class ConsoleHelper {
	public:
		static inline std::unordered_map<wchar_t, std::pair<unsigned short, std::string>> mappings = {
				{ L'k', { 7, "\33[5m" } }, { L'l', { 7, "\33[21m" } }, { L'm', { 7, "\33[9m" } },
				{ L'n', { 7, "\33[4m" } }, { L'o', { 7, "\33[3m" } },  { L'r', { 7, "\33[m"} } };

		// Initialize console and check if console supports ANSI escape sequences.
		static bool initialize() {
#ifdef _WIN32
			// If the output is not a terminal, then ANSI sequences are not supported.
			if (!isAtty) return false;

			RTL_OSVERSIONINFOEXW osInfo{};
			osInfo.dwOSVersionInfoSize = sizeof(osInfo);

			// Get OS version using RtlGetVersion.
			// If the call fails or the system is older than Windows 10,
			// then ANSI sequences are not supported.
			if (RtlGetVersion(&osInfo) || osInfo.dwMajorVersion < 10) return false;

			unsigned long mode;
			if (GetConsoleMode(getHandle(), &mode)) {
				// Enable "virtual terminal processing" in the console mode.
				// This makes the console recognize ANSI escape sequences.
				return SetConsoleMode(getHandle(), mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
			}

			// If we cannot get the console mode, assume ANSI is unsupported.
			return false;
#else
			return isAtty;
#endif
		}

		// Set the text color for current console output.
		static void setColor(unsigned short color, const std::string &ansi) {
			if (!isAtty) return;

			if (ansiSupported) lineBuffer += StringConverter::toWString(ansi.begin(), ansi.end());
#ifdef _WIN32
			else SetConsoleTextAttribute(getHandle(), color);
#endif
		}

		// Get the text attributes of the current console text.
		static unsigned short getTextAttribute() {
#ifdef _WIN32
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			return GetConsoleScreenBufferInfo(getHandle(), &csbi) ? csbi.wAttributes : 7;
#else
			return 0;
#endif
		}

		// Output string to console and log file.
		static void write(const std::string &msg) {
			if (isAtty) {
				lineBuffer += StringConverter::toWString(msg.begin(), msg.end());
#ifdef _WIN32
				if (!ansiSupported) WriteConsoleA(getHandle(), msg.c_str(), static_cast<unsigned>(msg.length()), nullptr, nullptr);
#endif
			}
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			if (logFile.is_open()) logFile << msg;
#endif
		}

		// Output wide string to console and log file.
		static void write(const std::wstring &msg) {
			if (isAtty) {
				if (ansiSupported) lineBuffer += msg;
#ifdef _WIN32
				else WriteConsoleW(getHandle(), msg.c_str(), static_cast<unsigned>(msg.length()), nullptr, nullptr);
#endif
			}
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			if (logFile.is_open()) logFile << StringConverter::toString(msg);
#endif
		}

		// Flush buffered line content to console and log file.
		static void flushLine() {
#ifdef _WIN32
			if (ansiSupported) {
				lineBuffer.push_back(L'\n');
				WriteConsoleW(getHandle(), lineBuffer.c_str(), static_cast<unsigned>(lineBuffer.length()), nullptr, nullptr);
			} else WriteConsoleA(getHandle(), "\n", 1, nullptr, nullptr);
#else
			std::cerr << StringConverter::toString(lineBuffer) << std::endl;
#endif
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			if (logFile.is_open()) logFile << std::endl;
#endif
			lineBuffer.clear();
		}

		// Clear remaining content in current line.
		static void clearLine() {
			if (!isAtty) return;

			// Clear any remaining text to the right of the cursor.
			if (ansiSupported) lineBuffer += L"\33[m\33[K";
#ifdef _WIN32
			else {
				// On Windows without ANSI, fill the rest of the line with spaces and reset attributes.
				SetConsoleTextAttribute(getHandle(), 7);
				CONSOLE_SCREEN_BUFFER_INFO csbi;
				if (GetConsoleScreenBufferInfo(getHandle(), &csbi)) {
					unsigned long length = csbi.dwSize.X - csbi.dwCursorPosition.X;
					FillConsoleOutputCharacterA(getHandle(), ' ', length, csbi.dwCursorPosition, &length);
					FillConsoleOutputAttribute(getHandle(), csbi.wAttributes, length, csbi.dwCursorPosition, &length);
				}
			}
#endif
		}

		// Update dynamic color mappings and find the specified color code.
		static auto updateMappingsAndFind(const wchar_t code, unsigned short initialColor, const std::string &ansiColor) {
			const unsigned short attribute = getTextAttribute();
			mappings.at(L'l').first = mappings.at(L'm').first = mappings.at(L'o').first = attribute;
			mappings.at(L'k').first = attribute | 0x4000;
			mappings.at(L'n').first = attribute | 0x8000;
			mappings.at(L'r') = { initialColor, ansiColor };
			return mappings.find(code);
		}

		// Process Minecraft color codes in message text.
		static std::wstring processColorCodes(std::wstring msg, unsigned short initialColor, const std::string &ansiColor, bool stripMsg) {
			std::wstring stripped;
			while (!msg.empty() && msg.back() == L'\247') msg.pop_back();

			size_t pos;
			while ((pos = msg.find(L'\247')) != std::wstring::npos) {
				const std::wstring part = msg.substr(0, pos);
				write(part);
				if (stripMsg) stripped += part;
				if (isAtty && pos + 1 < msg.length()) applyMinecraftColorCode(msg[pos + 1], initialColor, ansiColor);
				msg = msg.substr(pos + 2);
			}

			if (!msg.empty()) {
				write(msg);
				if (stripMsg) stripped += msg;
			}

			return stripped;
		}

		// Set console text color and style based on a Minecraft-style color code.
		static void applyMinecraftColorCode(wchar_t code, unsigned short initialColor, const std::string &ansiColor) {
			// For color codes that only change the text color, use the special mapping.
			if (L'0' <= code && code <= L'9') setColor(code - L'0', mcToAnsiEscape[code - L'0']);
			else if (L'a' <= code && code <= L'f') setColor(code - L'W', mcToAnsiEscape[code - L'W']);
			// For formatting codes, use a temporary specific mapping.
			else {
				auto it = updateMappingsAndFind(code, initialColor, ansiColor);
				if (it != mappings.end()) setColor(it->second.first, it->second.second);
			}
		}

#ifdef _WIN32
		// Get Windows stderr handle with optional caching.
		static HANDLE getHandle() {
#ifndef KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE
			// In cache mode, no need to retrieve stderr handle each time.
			static
#endif
			// In no-cache mode, retrieve the stderr handle on each output.
			const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
			return handle;
		}
#endif
	};

	// File logging helper.
	class FileLogger {
	public:
		// Initialize file logging system if enabled.
		static void initialize() {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			logFile = getLogFileHandle();
#endif
		}

		// Pack date components into a compact unsigned integer representation.
		static unsigned packDate(const tm &time) { return (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday; }

		// Update the log file handle for log rotation.
		static void updateIfNeeded() {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			if (packDate(TimeUtils::getLocalTime()) != logFileCreateDate) {
				if (logFile.is_open()) logFile.close();
				initialize();
			}
#endif
		}

		// Get the absolute path of the current executable.
		static std::filesystem::path getExecutablePath() {
#ifdef _WIN32
			// Get executable path on Windows.
			wchar_t path[5120];
			GetModuleFileNameW(nullptr, path, 5120);
#else
			// Get executable path on Linux.
			char path[5120];
			const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
			if (len == -1) return {};
			path[len] = 0;
#endif
			return path;
		}

		// Retrieve the current log file handle, creating directories and rotating logs if needed.
		static std::ofstream getLogFileHandle() {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			try {
				// Initialize logs directory and latest log file path if empty.
				if (logsDirectory.empty() || latestLog.empty()) {
					logsDirectory = getExecutablePath().parent_path() / "logs";
					latestLog = logsDirectory / "latest.log";
				}

				// Ensure log directory exists.
				std::filesystem::create_directories(logsDirectory);
				const tm time = TimeUtils::getLocalTime();
				// Rename existing log file if present.
				rotateLogFiles(time);
				logFileCreateDate = packDate(time);
				return std::ofstream(latestLog, std::ios::out | std::ios::trunc | std::ios::binary);
			} catch (...) {
				// Disable log file if any exception occurs.
				return {};
			}
#else
			return {};
#endif
		}

		// Rename the existing latest.log to a dated backup file with the format YYYY-MM-DD-N.log.
		static void rotateLogFiles(const tm &time) {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			if (!std::filesystem::exists(latestLog)) return;

			tm fileTime{};
			struct stat fileStat;
			if (stat(latestLog.string().c_str(), &fileStat)) fileTime = time;
#ifdef _WIN32
			else localtime_s(&fileTime, &fileStat.st_mtime);
#else
			else localtime_r(&fileStat.st_mtime, &fileTime);
#endif
			// Find the first available filename in the format YYYY-MM-DD-N.log
			// Increment 'i' until a non-existing filename is found.
			unsigned i = 1;
			std::wstring newFilename;
			do {
				newFilename = std::format(L"{}/{:04}-{:02}-{:02}-{}.log", logsDirectory.wstring(), fileTime.tm_year + 1900, fileTime.tm_mon + 1, fileTime.tm_mday, i++);
			} while (std::filesystem::exists(newFilename));

			std::filesystem::rename(latestLog, newFilename);
#endif
		}
	};

	// Time utilities.
	class TimeUtils {
	public:
		// Retrieve the current local time.
		static tm getLocalTime() {
			const time_t now = time(nullptr);
			tm result{};
#ifdef _WIN32
			localtime_s(&result, &now);
#else
			localtime_r(&now, &result);
#endif
			return result;
		}

		// Format time structure to %H:%M:%S string.
		static std::wstring formatTime(const tm &time) {
			return std::format(L"{:02}:{:02}:{:02} ", time.tm_hour, time.tm_min, time.tm_sec);
		}
	};

	// Log message processing
	class MessageProcessor {
	public:
		// Process complete log message including line splitting.
		static void processMessage(const std::wstring &name, std::wstring message, const LogStyle &style) {
			size_t newlinePos;
			while ((newlinePos = findNextNewline(message)) != std::wstring::npos) {
				processSingleLine(name, message.substr(0, newlinePos), style);
				message = message.substr(newlinePos + 1);
			}

			if (!message.empty()) processSingleLine(name, message, style);
		}

		// Find position of next newline character (CR or LF).
		static size_t findNextNewline(const std::wstring &str) {
			const size_t crPos = str.find(L'\r'), lfPos = str.find(L'\n');

			if (crPos == std::wstring::npos) return lfPos;
			if (lfPos == std::wstring::npos) return crPos;

			return std::min(crPos, lfPos);
		}

		// Process single line of log message with formatting.
		static void processSingleLine(const std::wstring &name, const std::wstring &message, const LogStyle &style) {
			if (message.empty()) return;

			printTimeStamp(name, style);
			const std::wstring stripped = ConsoleHelper::processColorCodes(message, style.textColor, style.textAnsiColor, onLog != nullptr);
			if (onLog) {
				try {
					onLog(message, stripped);
				} catch (...) {
				}
			}
			ConsoleHelper::clearLine();
			ConsoleHelper::flushLine();
		}

		// Print current time and logger name (if provided).
		static void printTimeStamp(const std::wstring &name, const LogStyle &style) {
			// Get current local time for timestamp.
			const tm localTime = TimeUtils::getLocalTime();

			// Set cyan color for timestamp bracket if output is terminal.
			if (isAtty) ConsoleHelper::setColor(3, "\33[0;36m");

			ConsoleHelper::write("[");
			ConsoleHelper::setColor(3, "\33[0;36m");
			// Write formatted time (HH:MM:SS).
			ConsoleHelper::write(TimeUtils::formatTime(localTime));
			// Set level-specific color for level text.
			ConsoleHelper::setColor(style.levelColor, style.levelAnsiColor);
			ConsoleHelper::write(style.level);
			// Reset to cyan for closing bracket.
			ConsoleHelper::setColor(3, "\33[0;36m");
			ConsoleHelper::write("] ");

			// Add logger name section if name is not empty.
			if (!name.empty()) {
				ConsoleHelper::write("[");
				// Process color codes in logger name.
				ConsoleHelper::processColorCodes(name, 3, "\33[0;36m", false);
				ConsoleHelper::setColor(3, "\33[0;36m");
				ConsoleHelper::write("] ");
			}

			// Set final text color for the actual log message.
			ConsoleHelper::setColor(style.textColor, style.textAnsiColor);
		}
	};

	// Yield execution and sleep briefly to reduce busy-wait CPU usage.
	static void pauseBriefly() {
		std::this_thread::yield();
		std::this_thread::sleep_for(1ms);
	}

	// Thread lock manager.
	class LockManager {
	public:
		// Spin until the lock is acquired.
		static void acquire() {
			for (bool expected = false; !lockFlag.compare_exchange_weak(expected, true, std::memory_order_acquire); expected = false) pauseBriefly();
		}

		// Release the custom lock.
		static void release() { lockFlag.store(false, std::memory_order_release); }

		// Execute function with automatic lock acquisition and release.
		template<typename Func>
		static void execute(const Func &&operation) {
			acquire();
			operation();
			release();
		}
	};

	// Thread lock flag (mutex was avoided because on some devices it caused unexpected crashes).
	static inline std::atomic_bool lockFlag;
	// Log task queue, stores log tasks to be processed by the logging thread.
	static inline std::queue<LogTask> logQueue;
	// Code to execute after a log message has been output.
	static inline std::function<void(const std::wstring &, const std::wstring &)> onLog;
	// Detect whether the process has a terminal.
	// If not (e.g., output redirected to a file), console output will be disabled.
	static inline const bool isAtty = isatty(fileno(stderr));
	// Determines whether the console supports ANSI escape sequences.
	static inline const bool ansiSupported = ConsoleHelper::initialize();

#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
	// Record the date when the log file was created.
	static inline unsigned logFileCreateDate;
	// Log file handle.
	static inline std::ofstream logFile;
	// Directory and file path of log files.
	static inline std::filesystem::path logsDirectory, latestLog;
#endif

	// Cache buffer when ANSI escape sequences are enabled.
	// Output only complete lines to reduce output frequency.
	static inline std::wstring lineBuffer;
	// Logger name as wide string.
	const std::wstring name{}, as_wstring{};
	// Logger name as simple string.
	const std::string as_string{};

	// Normalize logger name to avoid unexpected illegal characters in output.
	static std::wstring legalizeLoggerName(const std::wstring &name) {
		const size_t lastPos = std::max(name.find_last_of(L'\r'), name.find_last_of(L'\n'));
		return (lastPos == std::wstring::npos) ? name : name.substr(lastPos + 1);
	}

	// Submit a log output task to the logging thread.
	template<typename MessageType, typename... Args>
	void log(const MessageType &message, const LogStyle &style, const Args &...args) const {
		// Format message using formatMessage helper function.
		const std::wstring formatted = StringConverter::formatMessage(message, args...);

		// Push formatted log task to queue.
		LockManager::execute([&style, &formatted, this] { logQueue.push({style, name, formatted}); });
	}

public:
	// Construct a logger with no name.
	KlyLogger() noexcept : as_wstring(L"KlyLogger{name=<empty>}"), as_string("KlyLogger{name=<empty>}") {}

	// Construct a logger with a std::wstring name.
	explicit KlyLogger(const std::wstring &name) noexcept :
		name(legalizeLoggerName(name)),
		as_wstring(L"KlyLogger{name=" + (this->name.empty() ? L"<empty>" : this->name) + L'}'),
		as_string(StringConverter::toString(as_wstring)) {}

	// Construct a logger with a std::string name.
	explicit KlyLogger(const std::string &name) noexcept :
		name(legalizeLoggerName(StringConverter::toWString(name))),
		as_wstring(L"KlyLogger{name=" + (name.empty() ? L"<empty>" : this->name) + L'}'),
		as_string(StringConverter::toString(as_wstring)) {}

	// Retrieve logger name as std::string.
	[[nodiscard]] const std::string &string() const noexcept { return as_string; }

	// Retrieve logger name as std::wstring.
	[[nodiscard]] const std::wstring &wstring() const noexcept { return as_wstring; }

	// Log an INFO-level message.
	template<typename MessageType, typename... Args>
	void info(const MessageType &message, const Args &...args) const noexcept {
		log(message, INFO_STYLE, args...);
	}

	// Log an WARN-level message.
	template<typename MessageType, typename... Args>
	void warn(const MessageType &message, const Args &...args) const noexcept {
		log(message, WARN_STYLE, args...);
	}

	// Log an ERROR-level message.
	template<typename MessageType, typename... Args>
	void error(const MessageType &message, const Args &...args) const noexcept {
		log(message, ERROR_STYLE, args...);
	}

	// Log an FATAL-level message.
	template<typename MessageType, typename... Args>
	void fatal(const MessageType &message, const Args &...args) const noexcept {
		log(message, FATAL_STYLE, args...);
	}

	// Check if all pending log tasks have been processed.
	static bool finishedTasks() noexcept { return logQueue.empty(); }

	// Block the current thread until all log output is completed.
	static void wait() noexcept {
		while (!finishedTasks()) pauseBriefly();
	}

	// Register a callback function to execute after each log output.
	// The callback receives two parameters:
	//   1. The original log message (may include formatting codes).
	//   2. The plain text version of the message (with formatting removed).
	static void setOnLog(const std::function<void(const std::wstring &, const std::wstring &)> &func) noexcept {
		onLog = func;
	}

private:
	// Initialize a background thread to handle log queue processing and callbacks, ensuring it stays alive until
	// program exit.
	static inline std::shared_ptr<void> waiter = [] {
		auto threadFunc = [] [[noreturn]] {
		// Set the current thread to the lowest priority.
#ifdef _WIN32
			const HANDLE hThread = GetCurrentThread();
			SetThreadPriority(hThread, THREAD_PRIORITY_IDLE);
#else
			setpriority(PRIO_PROCESS, gettid(), 19);
#endif

			FileLogger::initialize();
			while (true) {
				while (logQueue.empty()) pauseBriefly();

				LockManager::execute([] {
					auto &[style, name, message] = logQueue.front();
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
					FileLogger::updateIfNeeded();
#endif
					MessageProcessor::processMessage(name, message, style);

					logQueue.pop();
				});
			}
		};

		std::thread(threadFunc).detach();

		return std::shared_ptr<void>(nullptr, [](void *) { wait(); });
	}();
};

#endif