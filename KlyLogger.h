#ifndef KLY_LOGGER_INCLUDED
#define KLY_LOGGER_INCLUDED
#include <filesystem>
#include <functional>
#include <iostream>
#include <codecvt>
#include <fstream>
#include <format>
#include <thread>
#include <queue>
using namespace std::chrono_literals;

#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#define isatty _isatty
#define fileno _fileno
#ifndef KLY_LOGGER_DISABLE_EXTERN_RTL_GET_VERSION
#pragma comment(lib, "ntdll.lib")
extern "C" int RtlGetVersion(PRTL_OSVERSIONINFOEXW) noexcept;
#endif
#else
#include <sys/stat.h>
#define FOREGROUND_RED 0
#define FOREGROUND_BLUE 0
#define FOREGROUND_GREEN 0
#define FOREGROUND_INTENSITY 0
#define COMMON_LVB_UNDERSCORE 0
#define COMMON_LVB_REVERSE_VIDEO 0
#endif

template <typename T, typename = void>
struct has_string : std::false_type {};
template <typename T>
struct has_string<T, std::void_t<decltype(std::declval<T>().string())>> : std::true_type {};
template <typename T, typename = void>
struct has_wstring : std::false_type {};
template <typename T>
struct has_wstring<T, std::void_t<decltype(std::declval<T>().wstring())>> : std::true_type {};

// KlyLogger: lightweight, visually and easy-to-use logger.
class KlyLogger {
private:
	struct LogTask {
		std::wstring name, message;
		std::string level, levelAnsiColor, textAnsiColor;
		unsigned short levelColor, textColor;
	};

	static inline std::queue<LogTask> logQueue;
	static inline bool isAtty = isatty(fileno(stderr));
	static inline std::function<void()> fOnLog = nullptr;
	static inline std::queue<std::wstring> convertArgsStorage;
	static inline std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

#ifdef _WIN32
	static inline HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);

	static inline bool isAnsiSupported() noexcept {
		if (!isAtty) return false;
		RTL_OSVERSIONINFOEXW osInfo = {};
		osInfo.dwOSVersionInfoSize = sizeof(osInfo);
		if (RtlGetVersion(&osInfo) || osInfo.dwMajorVersion < 10) return false;
		unsigned long mode = 0;
		if (GetConsoleMode(hStderr, &mode)) {
			SetConsoleMode(hStderr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
			return true;
		} else return false;
	}

	static inline bool ansiSupported = isAnsiSupported();
	static inline CONSOLE_SCREEN_BUFFER_INFO csbi;
#else
	static inline bool ansiSupported = true;
#endif

	static inline unsigned short getTextAttribute() noexcept {
#ifdef _WIN32
		return GetConsoleScreenBufferInfo(hStderr, &csbi) ? csbi.wAttributes : FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
#else
		return 0;
#endif
	}

	static bool iStartWorkerThread;

	std::wstring name, as_wstring;
	std::string as_string;

	static inline tm getLocalTime() noexcept {
		time_t now = time(nullptr);
		tm result {};
#ifdef _WIN32
		localtime_s(&result, &now);
#else
		localtime_r(&now, &result);
#endif
		return result;
	}

	static inline std::wstring convertToWString(const std::string& str) noexcept {
#ifdef _WIN32
		int len = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);
		if (len--) {
			std::wstring result(len, 0);
			MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), -1, result.data(), len);
			return result;
		}
#endif
		try {
			return converter.from_bytes(str);
		} catch (...) {
			return std::wstring(str.begin(), str.end()) + L"\2478\247o (decoder error)";
		}
	}

	static inline void setConsoleColor(unsigned color, const std::string& ansi) {
		if (!isAtty) return;
#ifdef _WIN32
		if (ansiSupported) std::cerr << ansi;
		else SetConsoleTextAttribute(hStderr, color);
#else
		std::cerr << ansi;
#endif
	}

#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
	static inline std::ofstream getLogFileHandle() noexcept {
		try {
#ifdef _WIN32
			wchar_t path[5120];
			GetModuleFileNameW(nullptr, path, 5120);
#else
			char path[5120];
			ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
			if (len == -1) return {};
			path[len] = 0;
#endif
			if (logsDirectory.empty() || latestLog.empty()) {
				logsDirectory = std::filesystem::path(path).parent_path() / "logs";
				latestLog = logsDirectory / "latest.log";
			}
			std::filesystem::create_directories(logsDirectory);
			tm time = getLocalTime();
			if (std::filesystem::exists(latestLog)) {
				tm fileTime {};
				struct stat fileStat {};
#ifdef _WIN32
				if (_wstat(latestLog.c_str(), (struct _stat64i32*)&fileStat)) fileTime = time;
				else localtime_s(&fileTime, &fileStat.st_mtime);
#else
				if (stat(latestLog.string().c_str(), &fileStat)) fileTime = time;
				else localtime_r(&fileStat.st_mtime, &fileTime);
#endif
				unsigned i = 0;
				while (std::filesystem::exists(std::format(L"{}/{:04}-{:02}-{:02}-{}.log", logsDirectory.wstring(), fileTime.tm_year + 1900, fileTime.tm_mon + 1, fileTime.tm_mday, ++i)));
				std::filesystem::rename(latestLog, std::format(L"{}/{:04}-{:02}-{:02}-{}.log", logsDirectory.wstring(), fileTime.tm_year + 1900, fileTime.tm_mon + 1, fileTime.tm_mday, i));
			}
			logFileCreateDate = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
			return std::ofstream(latestLog, std::ios::out | std::ios::trunc | std::ios::binary);
		} catch (...) {
			return {};
		}
	}

	static inline unsigned logFileCreateDate = 0;
	static inline std::ofstream logFile = getLogFileHandle();
	static inline std::filesystem::path logsDirectory, latestLog;

	static inline void updateLogFileHandle() noexcept {
		tm time = getLocalTime();
		unsigned date = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		if (date != logFileCreateDate) {
			if (logFile.is_open()) logFile.close();
			logFile = getLogFileHandle();
		}
	}
#endif

	static inline std::wstring legalizeLoggerName(const std::wstring& name) noexcept {
#ifdef max
		intptr_t lastPos = max((intptr_t)name.find_last_of(L'\r'), (intptr_t)name.find_last_of(L'\n'));
#else
		intptr_t lastPos = std::max((intptr_t)name.find_last_of(L'\r'), (intptr_t)name.find_last_of(L'\n'));
#endif
		return lastPos != std::wstring::npos ? name.substr(lastPos + 1) : name;
	}

	static inline bool startWorkerThread() noexcept {
		static std::shared_ptr<void> ptr(nullptr, [](void*) { wait(); });
		std::thread([]() {
			while (true) {
				if (finishedTasks()) continue;
				LogTask task = logQueue.front();
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
				updateLogFileHandle();
#endif
				logMessage(task.name, task.message, task.level, task.levelColor, task.levelAnsiColor, task.textColor, task.textAnsiColor);
				try {
					if (fOnLog) fOnLog();
				} catch (...) {}
				logQueue.pop();
			}
		}).detach();
		return true;
	}

	static inline void write(const std::string& msg) noexcept {
		if (isAtty) std::cerr << msg;
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		if (logFile.is_open()) logFile << msg;
#endif
	}

	static inline void write(std::wstring msg, unsigned short initialColor, const std::string& ansiColor) noexcept {
		while (msg.back() == L'\247') msg.pop_back();
		size_t pos;
		while ((pos = msg.find(L'\247')) != std::wstring::npos) {
			write(msg.substr(0, pos), initialColor, ansiColor);
			if (isAtty) {
				switch (msg[pos + 1]) {
					case L'0':
						setConsoleColor(0, "\33[30m");
						break;
					case L'1':
						setConsoleColor(FOREGROUND_BLUE, "\33[0;34m");
						break;
					case L'2':
						setConsoleColor(FOREGROUND_GREEN, "\33[0;32m");
						break;
					case L'3':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN, "\33[0;36m");
						break;
					case L'4':
						setConsoleColor(FOREGROUND_RED, "\33[0;31m");
						break;
					case L'5':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_RED, "\33[0;35m");
						break;
					case L'6':
						setConsoleColor(FOREGROUND_GREEN | FOREGROUND_RED, "\33[0;33m");
						break;
					case L'7':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, "\33[0;37m");
						break;
					case L'8':
						setConsoleColor(FOREGROUND_INTENSITY, "\33[0;90m");
						break;
					case L'9':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY, "\33[0;94m");
						break;
					case L'a':
						setConsoleColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY, "\33[0;92m");
						break;
					case L'b':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY, "\33[0;96m");
						break;
					case L'c':
						setConsoleColor(FOREGROUND_RED | FOREGROUND_INTENSITY, "\33[0;91m");
						break;
					case L'd':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY, "\33[0;95m");
						break;
					case L'e':
						setConsoleColor(FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "\33[0;93m");
						break;
					case L'f':
						setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "\33[0;97m");
						break;
					case L'r':
						setConsoleColor(initialColor, ansiColor);
						break;
					case L'k':
						setConsoleColor(getTextAttribute() | COMMON_LVB_REVERSE_VIDEO, "\33[5m");
						break;
					case L'l':
						if (ansiSupported) std::cerr << "\33[21m";
						break;
					case L'm':
						if (ansiSupported) std::cerr << "\33[9m";
						break;
					case L'n':
						setConsoleColor(getTextAttribute() | COMMON_LVB_UNDERSCORE, "\33[4m");
						break;
					case L'o':
						if (ansiSupported) std::cerr << "\33[3m";
				}
			}
			msg = msg.substr(pos + 2);
		}
		std::string converted;
		if (isAtty) {
#ifdef _WIN32
			std::cerr << std::flush;
			WriteConsoleW(hStderr, msg.c_str(), (unsigned)msg.length(), nullptr, nullptr);
#else
			try {
				converted = converter.to_bytes(msg);
			} catch (...) {
				converted = { msg.begin(), msg.end() };
			}
			std::cerr << converted;
#endif
		}
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		if (converted.empty()) converted = converter.to_bytes(msg);
		if (logFile.is_open()) logFile << converted;
#endif
	}

	static inline void printTime(const std::wstring& name, const std::string& level, unsigned short levelColor, const std::string& levelAnsiColor, unsigned short textColor, const std::string& textAnsiColor) noexcept {
		tm localTime = getLocalTime();
		if (isAtty) {
			std::cerr << '\r';
			setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN, "\33[0;36m");
		}
		write("[");
		setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN, "\33[0;36m");
		write(std::format("{:02}:{:02}:{:02} ", localTime.tm_hour, localTime.tm_min, localTime.tm_sec));
		setConsoleColor(levelColor , levelAnsiColor);
		write(level);
		setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN, "\33[0;36m");
		write("] ");
		if (!name.empty()) {
			write("[");
			write(name, FOREGROUND_BLUE | FOREGROUND_GREEN, "\33[0;36m");
			setConsoleColor(FOREGROUND_BLUE | FOREGROUND_GREEN, "\33[0;36m");
			write("] ");
		}
		setConsoleColor(textColor, textAnsiColor);
	}

	static inline void logMessage(const std::wstring& name, std::wstring message, const std::string& level, unsigned short levelColor, const std::string& levelAnsiColor, unsigned short textColor, const std::string& textAnsiColor) noexcept {
		size_t newlinePos;
#ifdef min
		while ((newlinePos = min(message.find(L'\r'), message.find(L'\n'))) != std::wstring::npos) {
#else
		while ((newlinePos = std::min(message.find(L'\r'), message.find(L'\n'))) != std::wstring::npos) {
#endif
			logMessage(name, message.substr(0, newlinePos), level, levelColor, levelAnsiColor, textColor, textAnsiColor);
			message = message.substr(newlinePos + 1);
		}
		if (message[0]) {
			printTime(name, level, levelColor, levelAnsiColor, textColor, textAnsiColor);
			write(message.substr(message.find_last_of(L'\r') + 1), textColor, textAnsiColor);
			if (isAtty) {
				if (ansiSupported) std::cerr << "\33[m\33[K";
#ifdef _WIN32
				else {
					SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
					GetConsoleScreenBufferInfo(hStderr, &csbi);
					unsigned long length = csbi.dwSize.X - csbi.dwCursorPosition.X;
					FillConsoleOutputCharacterA(hStderr, ' ', length, csbi.dwCursorPosition, &length);
					FillConsoleOutputAttribute(hStderr, csbi.wAttributes, length, csbi.dwCursorPosition, &length);
				}
#endif
			}
			write("\n");
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
			if (logFile.is_open()) logFile << std::flush;
#endif
		}
	}

	template<typename T>
	static inline auto& convertFormatting(const T& arg) {
		if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char*>) {
			convertArgsStorage.push(convertToWString(arg));
			return convertArgsStorage.back();
		} else if constexpr (has_wstring<T>::value) {
			convertArgsStorage.push(arg.wstring());
			return convertArgsStorage.back();
		} else if constexpr (has_string<T>::value) {
			convertArgsStorage.push(convertToWString(arg.string()));
			return convertArgsStorage.back();
		} else return arg;
	}

	template<typename... Args>
	inline void pushTask(const std::wstring& message, const std::string& level, unsigned short levelColor, const std::string& levelAnsiColor, unsigned short textColor, const std::string& textAnsiColor, const Args&... args) const noexcept {
		std::wstring formatted;
		try {
			formatted = std::vformat(message, std::make_wformat_args(convertFormatting(args)...));
			convertArgsStorage = std::queue<std::wstring>();
		} catch (const std::exception& e) {
			formatted = message + L"\2478\247o (" + convertToWString(e.what()) + L')';
		} catch (...) {}
		logQueue.push({ name, formatted, level, levelAnsiColor, textAnsiColor, levelColor, textColor });
	}

public:
	// Create a nameless logger.
	KlyLogger() noexcept : as_string("KlyLogger{name=<empty>}"), as_wstring(L"KlyLogger{name=<empty>}") {};

	// Get as std::string
	[[nodiscard]] inline std::string string() const noexcept { return as_string; }

	// Get as std::wstring
	[[nodiscard]] inline std::wstring wstring() const noexcept { return as_wstring; }

	// Create a logger with std::wstring as its name.
	explicit KlyLogger(const std::wstring& name) noexcept : name(legalizeLoggerName(name)), as_wstring(std::wstring(L"KlyLogger{name=") + (name.empty() ? L"<empty>" : name) + L'}'), as_string(converter.to_bytes(as_wstring)) {}

	// Create a logger with std::string as its name.
	explicit KlyLogger(const std::string& name) noexcept : name(legalizeLoggerName(convertToWString(name))), as_wstring(std::wstring(L"KlyLogger{name=") + (name.empty() ? L"<empty>" : this->name) + L'}'), as_string(converter.to_bytes(as_wstring)) {}

	// Log INFO with std::wstring.
	template<typename... Args>
	inline void info(const std::wstring& message, const Args&... args) const noexcept { pushTask(message, "INFO", FOREGROUND_GREEN | FOREGROUND_INTENSITY, "\33[0;92m", FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, "\33[0;37m", args...); }

	// Log INFO with std::string.
	template<typename... Args>
	inline void info(const std::string& message, const Args&... args) const noexcept { info(convertToWString(message), args...); }

	// Log WARN with std::wstring.
	template<typename... Args>
	inline void warn(const std::wstring& message, const Args&... args) const noexcept { pushTask(message, "WARN", FOREGROUND_GREEN | FOREGROUND_RED, "\33[0;33m", FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, "\33[0;93m", args...); }

	// Log WARN with std::string.
	template<typename... Args>
	inline void warn(const std::string& message, const Args&... args) const noexcept { warn(convertToWString(message), args...); }

	// Log ERROR with std::wstring.
	template<typename... Args>
	inline void error(const std::wstring& message, const Args&... args) const noexcept { pushTask(message, "ERROR", FOREGROUND_RED, "\33[0;31m", FOREGROUND_RED | FOREGROUND_INTENSITY, "\33[0;91m", args...); }

	// Log ERROR with std::string.
	template<typename... Args>
	inline void error(const std::string& message, const Args&... args) const noexcept { error(convertToWString(message), args...); }

	// Log FATAL with std::wstring.
	template<typename... Args>
	inline void fatal(const std::wstring& message, const Args&... args) const noexcept { pushTask(message, "FATAL", COMMON_LVB_UNDERSCORE | FOREGROUND_RED, "\33[2;31m", FOREGROUND_RED, "\33[0;31m", args...); }

	// Log FATAL with std::string.
	template<typename... Args>
	inline void fatal(const std::string& message, const Args&... args) const noexcept { fatal(convertToWString(message), args...); }

	// Determine whether all logging tasks have been completed.
	static inline bool finishedTasks() noexcept { return logQueue.empty(); }

	// Wait for log output to finish.
	static inline void wait() noexcept { while (!finishedTasks()) std::this_thread::sleep_for(1ms); }

	// Setting behavior on after an output.
	static inline void onLog(const std::function<void()>& func) noexcept { fOnLog = func; }
};

bool KlyLogger::iStartWorkerThread = startWorkerThread();
#endif