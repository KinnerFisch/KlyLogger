#define KLY_LOGGER_INCLUDED
#include <functional>
#include <Windows.h>
#include <format>
#include <thread>
#include <queue>

// 检测转字符串成员函数
template <typename T, typename = void>
struct has_to_string : std::false_type {};
template <typename T>
struct has_to_string<T, std::void_t<decltype(std::declval<T>().to_string())>> : std::true_type {};
template <typename T, typename = void>
struct has_to_wstring : std::false_type {};
template <typename T>
struct has_to_wstring<T, std::void_t<decltype(std::declval<T>().to_wstring())>> : std::true_type {};

// KlyLogger: 轻量、直观、易用的 logger
class KlyLogger {
private:
	struct LogTask { // 日志输出任务
		std::wstring name, message;
		std::string level;
		WORD levelColor, textColor;
	};

	static inline HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE); // 标准错误输出句柄
	static inline std::function<void()> fOnLog = nullptr; // 输出日志后任务
	static inline std::queue<LogTask> logQueue; // 任务队列
	static int iStartWorkerThread; // 启动线程

	std::wstring _name, as_wstring; // 名称及转宽字符串缓存
	std::string as_string; // 转字符串缓存

	// 系统默认编码字符串转宽字符串
	static inline std::wstring wideString(const std::string& src) noexcept {
		int len = MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, nullptr, 0); // 获取原字符串在系统默认编码下的字符数
		std::wstring str(len - 1, 0); // 开辟长度为原字符串长度的宽字符串
		MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, str.data(), len); // 将原字符串转为宽字符串
		return str;
	}

	// 宽字符串转特定编码字符串
	static inline std::string simpleString(unsigned codepage, const std::wstring& src) noexcept {
		int len = WideCharToMultiByte(codepage, 0, src.c_str(), -1, nullptr, 0, nullptr, nullptr); // 获取原字符串在系统默认编码下的字符数
		std::string str(len - 1, 0); // 开辟长度为原字符串长度的宽字符串
		WideCharToMultiByte(codepage, 0, src.c_str(), -1, str.data(), len, nullptr, nullptr); // 将原字符串转为宽字符串
		return str; // 返回新宽字符串
	}

	// 获取当前时间
	static inline tm getLocalTime() noexcept {
		time_t now = time(nullptr);
		tm result;
		if (localtime_s(&result, &now)) return {};
		return result;
	}

#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
	// 获取当前日志文件句柄
	static inline HANDLE getLogFileHandle() {
		if (logsDirectory.empty() || latestLog.empty()) {
			char path[1024];
			GetCurrentDirectoryA(1024, path);
			logsDirectory = std::string(path) + "\\logs\\";
			latestLog = logsDirectory + "latest.log";
		}
		if (!CreateDirectoryA(logsDirectory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return INVALID_HANDLE_VALUE; // 无法创建文件夹
		DWORD attributes = GetFileAttributesA(latestLog.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES) {
			HANDLE hExistingFile = CreateFileA(latestLog.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			bool gotTime = false;
			FILETIME ftCreate, ftAccess, ftWrite;
			if (hExistingFile != INVALID_HANDLE_VALUE) {
				gotTime = GetFileTime(hExistingFile, &ftCreate, &ftAccess, &ftWrite);
				CloseHandle(hExistingFile);
			}
			SYSTEMTIME stUTC, stLocal;
			if (gotTime) {
				FileTimeToSystemTime(&ftWrite, &stUTC);
				SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
			} else GetLocalTime(&stLocal);
			unsigned i = 0;
			while (true) if (MoveFileA(latestLog.c_str(), std::format("{}{:04}-{:-2}-{:-2}-{}.log", logsDirectory, stLocal.wYear, stLocal.wMonth, stLocal.wDay, ++i).c_str())) break;
		}
		tm time = getLocalTime();
		logFileCreateDate = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		return CreateFileA(latestLog.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	}

	static inline unsigned logFileCreateDate = 0; // 日志文件创建时间
	static inline HANDLE hLogFile = getLogFileHandle(); // 日志文件句柄
	static inline std::string logsDirectory, latestLog; // 日志文件目录及最新日志文件地址

	// 更新日志文件句柄
	static inline void updateLogFileHandle() {
		tm time = getLocalTime();
		unsigned date = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		if (date != logFileCreateDate) {
			CloseHandle(hLogFile);
			hLogFile = getLogFileHandle();
		}
	}
#endif

	// 规范化 logger 名称
	static inline std::wstring legalizeLoggerName(const std::wstring& name) noexcept {
		// 查找最后一个回车符或换行符
#ifdef max
		intptr_t lastPos = max((intptr_t)name.find_last_of(L'\r'), (intptr_t)name.find_last_of(L'\n'));
#else
		intptr_t lastPos = std::max((intptr_t)name.find_last_of(L'\r'), (intptr_t)name.find_last_of(L'\n'));
#endif
		// 截取回车或换行符右侧的字符串
		return lastPos != std::wstring::npos ? name.substr(lastPos + 1) : name;
	}

	// 获取当前文字属性
	static inline WORD getTextAttribute() noexcept {
		CONSOLE_SCREEN_BUFFER_INFO buffer;
		if (!GetConsoleScreenBufferInfo(hStderr, &buffer)) return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
		return buffer.wAttributes;
	}

	// 启动 logger 专用线程
	static inline int startWorkerThread() {
		std::thread([]() {
			while (true) {
				while (finishedTasks());
				LogTask task = logQueue.front();
				logMessage(task.name, task.message, task.level, task.levelColor, task.textColor);
				try {
					if (fOnLog) fOnLog();
				} catch (...) {}
				logQueue.pop();
			}
		}).detach();
		return 0;
	}

	// 输出信息 (字符串)
	static inline void write(const std::string& msg) noexcept {
		WriteFile(hStderr, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		if (hLogFile != INVALID_HANDLE_VALUE) WriteFile(hLogFile, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#endif
	}

	// 输出信息 (宽字符串)
	static inline void write(std::wstring msg, WORD initialColor) {
		while (msg.back() == L'§') msg.pop_back();
		size_t pos;
		while ((pos = msg.find(L'§')) != std::string::npos) {
			write(msg.substr(0, pos), initialColor);
			wchar_t code = msg[pos + 1];
			if (code == L'0') SetConsoleTextAttribute(hStderr, 0);
			else if (code == L'1') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE);
			else if (code == L'2') SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN);
			else if (code == L'3') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
			else if (code == L'4') SetConsoleTextAttribute(hStderr, FOREGROUND_RED);
			else if (code == L'5') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_RED);
			else if (code == L'6') SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN | FOREGROUND_RED);
			else if (code == L'7') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
			else if (code == L'8') SetConsoleTextAttribute(hStderr, FOREGROUND_INTENSITY);
			else if (code == L'9') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
			else if (code == L'a') SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			else if (code == L'b') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
			else if (code == L'c') SetConsoleTextAttribute(hStderr, FOREGROUND_RED | FOREGROUND_INTENSITY);
			else if (code == L'd') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY);
			else if (code == L'e') SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
			else if (code == L'f') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
			else if (code == L'k') SetConsoleTextAttribute(hStderr, getTextAttribute() | COMMON_LVB_REVERSE_VIDEO);
			else if (code == L'n') SetConsoleTextAttribute(hStderr, getTextAttribute() | COMMON_LVB_UNDERSCORE);
			else if (code == L'r') SetConsoleTextAttribute(hStderr, initialColor);
			msg = msg.substr(pos + 2);
		}
		WriteConsoleW(hStderr, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		std::string utf8 = simpleString(CP_UTF8, msg);
		if (hLogFile != INVALID_HANDLE_VALUE) WriteFile(hLogFile, utf8.c_str(), (DWORD)utf8.length(), nullptr, nullptr);
#endif
	}

	// 输出时间
	static inline void printTime(const std::wstring& name, const std::string& level, WORD levelColor, WORD textColor) noexcept {
		tm localTime = getLocalTime();
		WriteFile(hStderr, "\r", 1, nullptr, nullptr);
		SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
		write("[");
		SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		write(std::format("{:02}:{:02}:{:02} ", localTime.tm_hour, localTime.tm_min, localTime.tm_sec));
		SetConsoleTextAttribute(hStderr, levelColor);
		write(level);
		SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
		write("] ");
		if (!name.empty()) {
			write("[");
			write(name, FOREGROUND_BLUE | FOREGROUND_GREEN);
			SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
			write("] ");
		}
		SetConsoleTextAttribute(hStderr, textColor);
	}

	// 输出日志信息
	static inline void logMessage(const std::wstring& name, std::wstring message, const std::string& level, WORD levelColor, WORD textColor) {
		size_t newlinePos;
#ifdef min
		while ((newlinePos = min(message.find(L'\r'), message.find(L'\n'))) != std::wstring::npos) {
#else
		while ((newlinePos = std::min(message.find(L'\r'), message.find(L'\n'))) != std::wstring::npos) {
#endif
			logMessage(name, message.substr(0, newlinePos), level, levelColor, textColor);
			message = message.substr(newlinePos + 1);
		}
		if (message[0]) { // 确保消息不是空字符串
			printTime(name, level, levelColor, textColor);
			write(message.substr(message.find_last_of(L'\r') + 1), textColor);
			SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
			write("\n");
		}
	}

	// 预处理消息参数: 将普通字符串转为宽字符串
	template<typename T>
	static inline auto& processFormattings(const T& arg) {
		if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char*>) {
			const static std::wstring& result = wideString(arg);
			return result;
		} else if constexpr (has_to_wstring<T>::value) {
			const static std::wstring& result = arg.to_wstring();
			return result;
		} else if constexpr (has_to_string<T>::value) {
			const static std::wstring& result = wideString(arg.to_string());
			return result;
		} else return arg;
	}

	// 添加日志任务
	template<typename... Args>
	inline void pushTask(const std::wstring& message, const std::string& level, WORD levelColor, WORD textColor, const Args&... args) const {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		updateLogFileHandle();
#endif
		std::wstring formatted;
		try {
			formatted = std::vformat(message, std::make_wformat_args(processFormattings(args)...));
		} catch (const std::exception& e) {
			formatted = wideString(e.what());
		} catch (...) {}
		logQueue.push({ _name, formatted, level, levelColor, textColor });
	}

public:
	// 创建无名 logger
	KlyLogger() noexcept : as_string("KlyLogger{name=<empty>}"), as_wstring(L"KlyLogger{name=<empty>}") {};

	// 以宽字符串类型的字符串作为名称创建 logger
	explicit KlyLogger(const std::wstring& name) noexcept : _name(legalizeLoggerName(name)), as_wstring(std::wstring(L"KlyLogger{name=") + (_name.empty() ? L"<empty>" : _name) + L'}'), as_string(simpleString(CP_ACP, as_wstring)) {}

	// 以普通字符串类型的字符串作为名称创建 logger
	explicit KlyLogger(const std::string& name) noexcept : _name(legalizeLoggerName(wideString(name))), as_wstring(std::wstring(L"KlyLogger{name=") + (_name.empty() ? L"<empty>" : _name) + L'}'), as_string(simpleString(CP_ACP, as_wstring)) {}

	// 转为字符串
	inline std::string to_string() const noexcept { return as_string; }
	
	// 转为宽字符串
	inline std::wstring to_wstring() const noexcept { return as_wstring; }

	// 输出普通信息 (宽字符串)
	template<typename... Args>
	inline void info(const std::wstring& message, const Args&... args) const { pushTask(message, "INFO", FOREGROUND_GREEN | FOREGROUND_INTENSITY, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, args...); }

	// 输出普通信息 (字符串)
	template<typename... Args>
	inline void info(const std::string& message, const Args&... args) const { info(wideString(message), args...); }

	// 输出警告信息 (宽字符串)
	template<typename... Args>
	inline void warn(const std::wstring& message, const Args&... args) const { pushTask(message, "WARN", FOREGROUND_GREEN | FOREGROUND_RED, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, args...); }

	// 输出警告信息 (字符串)
	template<typename... Args>
	inline void warn(const std::string& message, const Args&... args) const { warn(wideString(message), args...); }

	// 输出错误信息 (宽字符串)
	template<typename... Args>
	inline void error(const std::wstring& message, const Args&... args) const { pushTask(message, "ERROR", FOREGROUND_RED, FOREGROUND_RED | FOREGROUND_INTENSITY, args...); }

	// 输出错误信息 (字符串)
	template<typename... Args>
	inline void error(const std::string& message, const Args&... args) const { error(wideString(message), args...); }

	// 输出严重错误信息 (宽字符串)
	template<typename... Args>
	inline void fatal(const std::wstring& message, const Args&... args) const { pushTask(message, "FATAL", COMMON_LVB_UNDERSCORE | FOREGROUND_RED, FOREGROUND_RED, args...); }

	// 输出严重错误信息 (字符串)
	template<typename... Args>
	inline void fatal(const std::string& message, const Args&... args) const { fatal(wideString(message), args...); }

	// 判断是否完成所有日志输出任务
	static inline bool finishedTasks() noexcept { return logQueue.empty(); }

	// 等待日志输出完毕
	static inline void wait() noexcept { while (!finishedTasks()) Sleep(1); }

	// 设置输出后行为
	static inline void onLog(const std::function<void()>& func) { fOnLog = func; }
};

int KlyLogger::iStartWorkerThread = startWorkerThread(); // 启动 logger 专用线程