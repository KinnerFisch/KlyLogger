#define _CRT_SECURE_NO_WARNINGS
#include <functional>
#include <Windows.h>
#include <thread>
#include <queue>

// KlyLogger: 轻量、直观、易用的 logger
class KlyLogger {
private:
	struct LogTask { // 日志输出任务
		std::wstring name, message;
		std::string level;
		WORD levelColor, textColor;
	};
	static inline HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE); // stderr 输出流
	static inline std::function<void()> fOnLog = nullptr; // 输出日志后任务
	static inline std::queue<LogTask> logQueue; // 任务队列
	static int iStartWorkerThread; // 启动线程
	std::wstring _name; // logger 名称

	// 系统默认编码字符串转宽字符串
	static inline std::wstring wideString(const std::string& src) {
		int len = MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, nullptr, 0); // 获取原字符串在系统默认编码下的字符数
		std::wstring str(len - 1, 0); // 开辟长度为原字符串长度的宽字符串
		MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, str.data(), len); // 将原字符串转为宽字符串
		return str;
	}

	// 宽字符串转 UTF-8 字符串
	static inline std::string UTF8(const std::wstring& src) {
		int len = WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, nullptr, 0, nullptr, nullptr); // 获取原字符串在系统默认编码下的字符数
		std::string str(len - 1, 0); // 开辟长度为原字符串长度的宽字符串
		WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, str.data(), len, nullptr, nullptr); // 将原字符串转为宽字符串
		return str; // 返回新宽字符串
	}

	// 获取当前时间
	static inline std::tm getLocalTime() {
		time_t now = time(nullptr); // 获取当前时间戳
		return *localtime(&now); // 转换为结构体
	}

#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
	static inline unsigned logFileCreateDate = 0; // 日志文件创建时间
	static inline std::string logsDirectory, latestLog; // 日志文件目录 (最初工作目录下的 logs 文件夹) 及最新日志文件地址 (常用值)

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
			while (true) {
				char newFileName[1024];
				strcpy_s(newFileName, logsDirectory.c_str());
				_snprintf_s(newFileName + logsDirectory.length(), 100, _TRUNCATE, "%04d-%02d-%02d-%d.log", stLocal.wYear, stLocal.wMonth, stLocal.wDay, ++i);
				if (MoveFileA(latestLog.c_str(), newFileName)) break;
			}
		}
		std::tm time = getLocalTime();
		logFileCreateDate = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		return CreateFileA(latestLog.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	}

	static inline HANDLE hLogFile = getLogFileHandle(); // 日志文件句柄

	// 更新日志文件句柄
	static inline void updateLogFileHandle() {
		std::tm time = getLocalTime();
		unsigned date = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		if (date != logFileCreateDate) {
			CloseHandle(hLogFile);
			hLogFile = getLogFileHandle();
		}
	}
#endif

	// 使 logger 名称合理化
	static inline std::wstring legalizeLoggerName(const std::wstring& name) {
		// 查找最后一个回车符或换行符
#ifdef max
		long long lastPos = max((long long)name.find_last_of(L'\r'), (long long)name.find_last_of(L'\n'));
#else
		long long lastPos = std::max((long long)name.find_last_of(L'\r'), (long long)name.find_last_of(L'\n'));
#endif
		// 截取回车或换行符右侧的字符串
		return lastPos != std::wstring::npos ? name.substr(lastPos + 1) : name;
	}

	// 获取当前控制台文本属性
	static inline WORD getTextAttribute() {
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
	static inline void write(const std::string& msg) {
		WriteFile(hStderr, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		if (hLogFile != INVALID_HANDLE_VALUE) WriteFile(hLogFile, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#endif
	}

	// 输出信息 (宽字符串)
	static inline void write(std::wstring msg) {
		size_t pos;
		while ((pos = msg.find(L'§')) != std::wstring::npos) {
			write(msg.substr(0, pos));
			wchar_t code = msg[pos + 1];
			if (code == L'0') SetConsoleTextAttribute(hStderr, 0);
			else if (code == L'1') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE);
			else if (code == L'2') SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN);
			else if (code == L'3') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
			else if (code == L'4') SetConsoleTextAttribute(hStderr, FOREGROUND_RED);
			else if (code == L'5') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_RED);
			else if (code == L'6') SetConsoleTextAttribute(hStderr, FOREGROUND_GREEN | FOREGROUND_RED);
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
			else if (code == L'7' || code == L'r') SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
			msg = msg.substr(pos + 2);
		}
		WriteConsoleW(hStderr, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		std::string utf8 = UTF8(msg);
		if (hLogFile != INVALID_HANDLE_VALUE) WriteFile(hLogFile, utf8.c_str(), (DWORD)utf8.length(), nullptr, nullptr);
#endif
	}

	// 打印日志头
	static inline void printHead(const std::wstring& name, const std::string& level, WORD levelColor, WORD textColor) {
		std::tm localTime = getLocalTime();
		WriteFile(hStderr, "\r", 1, nullptr, nullptr);
		SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
		write("[");
		SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		char time[10] = "";
		_snprintf_s(time, 10, _TRUNCATE, "%02d:%02d:%02d ", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
		write(time);
		SetConsoleTextAttribute(hStderr, levelColor);
		write(level);
		SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
		write("] ");
		if (!name.empty()) {
			write("[");
			write(name);
			SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN);
			write("] ");
		}
		SetConsoleTextAttribute(hStderr, textColor);
	}

	// 预处理消息参数
	template<typename T>
	static inline auto processArg(const T& arg) {
		if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char*>) return wideString(arg);
		else if constexpr (std::is_same_v<T, bool>) return arg ? L"true" : L"false";
		else return arg;
	}

	// 获取参数可能使用长度
	template<typename T>
	static inline size_t getLength(const T& arg) {
		if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>) return arg.length();
		if constexpr (std::is_same_v<T, short> || std::is_same_v<T, unsigned short>) return 5;
		if constexpr (std::is_same_v<T, int> || std::is_same_v<T, unsigned> || std::is_same_v<T, long> || std::is_same_v<T, unsigned long>) return 10;
		if constexpr (std::is_same_v<T, long long> || std::is_same_v<T, unsigned long long>) return 20;
		if constexpr (std::is_same_v<T, float>) return 15;
		if constexpr (std::is_same_v<T, double>) return 25;
		if constexpr (std::is_same_v<T, long double>) return 35;
		if constexpr (std::is_same_v<T, bool>) return 5;
		return sizeof(arg);
	}

	// 格式化日志消息
	template<typename... Args>
	static inline std::wstring formatMessage(const std::wstring& message, const Args&... args) {
		size_t len = message.length() + (0 + ... + getLength(args)) + 1;
		auto* arr = new wchar_t[len];
		_snwprintf_s(arr, len, _TRUNCATE, message.c_str(), processArg(args)...);
		return arr;
	}

	// 输出日志信息
	static inline void logMessage(const std::wstring& name, std::wstring message, const std::string& level, WORD levelColor, WORD textColor) {
		size_t newlinePos;
#ifdef min
		while ((newlinePos = min(message.find(L'\n'), message.find(L"\r\n"))) != std::wstring::npos) { // \r\n: CRLF
#else
		while ((newlinePos = std::min(message.find(L'\n'), message.find(L"\r\n"))) != std::wstring::npos) {
#endif
			logMessage(name, message.substr(0, newlinePos), level, levelColor, textColor);
			message = message.substr(newlinePos + 1);
		}
		if (message[0]) { // 确保消息不是空字符串
			printHead(name, level, levelColor, textColor);
			write(message.substr(message.find_last_of(L'\r') + 1));
			SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
			write("\n");
		}
	}

	// 添加任务
	template<typename... Args>
	inline void pushTask(const std::wstring & message, const std::string & level, WORD levelColor, WORD textColor, Args... args) {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		updateLogFileHandle();
#endif
		std::wstring formatted = formatMessage(message.c_str(), args...);
		LogTask task = { _name, formatted, level, levelColor, textColor };
		logQueue.push(task);
	}

public:
	// 创建无名 logger
	KlyLogger() = default;

	// 以宽字符串类型名称创建 logger
	explicit KlyLogger(const std::wstring & name) : _name(legalizeLoggerName(name)) {}

	// 以普通字符串类型名称创建 logger
	explicit KlyLogger(const std::string & name) : _name(legalizeLoggerName(wideString(name))) {}

	// 输出普通信息 (宽字符串)
	template<typename... Args>
	inline void info(const std::wstring & message, const Args&... args) { pushTask(message, "INFO", FOREGROUND_GREEN | FOREGROUND_INTENSITY, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, args...); }

	// 输出普通信息 (字符串)
	template<typename... Args>
	inline void info(const std::string & message, const Args&... args) { info(wideString(message), args...); }

	// 输出警告信息 (宽字符串)
	template<typename... Args>
	inline void warn(const std::wstring & message, const Args&... args) { pushTask(message, "WARN", FOREGROUND_GREEN | FOREGROUND_RED, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, args...); }

	// 输出警告信息 (字符串)
	template<typename... Args>
	inline void warn(const std::string & message, const Args&... args) { warn(wideString(message), args...); }

	// 输出错误信息 (宽字符串)
	template<typename... Args>
	inline void error(const std::wstring & message, const Args&... args) { pushTask(message, "ERROR", FOREGROUND_RED, FOREGROUND_RED | FOREGROUND_INTENSITY, args...); }

	// 输出错误信息 (字符串)
	template<typename... Args>
	inline void error(const std::string & message, const Args&... args) { error(wideString(message), args...); }

	// 输出严重错误信息 (宽字符串)
	template<typename... Args>
	inline void fatal(const std::wstring & message, const Args&... args) { pushTask(message, "FATAL", COMMON_LVB_UNDERSCORE | FOREGROUND_RED, FOREGROUND_RED, args...); }

	// 输出严重错误信息 (字符串)
	template<typename... Args>
	inline void fatal(const std::string & message, const Args&... args) { fatal(wideString(message), args...); }

	// 判断是否完成所有日志输出任务
	static inline bool finishedTasks() { return logQueue.empty(); }

	// 等待日志输出完毕 (3s)
	static inline void wait() {
		Sleep(1500);
		while (!finishedTasks());
		Sleep(1500);
	}

	// 设置输出日志后行为
	static inline void onLog(const std::function<void()>&func) { fOnLog = func; }
};

int KlyLogger::iStartWorkerThread = startWorkerThread(); // 启动 logger 专用线程