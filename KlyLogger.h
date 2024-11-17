#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <thread>
#include <queue>

// KlyLogger: ������ֱ�ۡ����õ� logger
class KlyLogger {
private:
	// ϵͳĬ�ϱ����ַ���ת���ַ���
	static inline std::wstring wideString(const std::string& src) {
		int len = MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, nullptr, 0); // ��ȡԭ�ַ�����ϵͳĬ�ϱ����µ��ַ���
		std::wstring str(len - 1, 0); // ���ٳ���Ϊԭ�ַ������ȵĿ��ַ���
		MultiByteToWideChar(CP_ACP, 0, src.c_str(), -1, str.data(), len); // ��ԭ�ַ���תΪ���ַ���
		return str;
	}

	// ���ַ���ת UTF8 �ַ���
	static inline std::string UTF8(const std::wstring& src) {
		int len = WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, nullptr, 0, nullptr, nullptr); // ��ȡԭ�ַ�����ϵͳĬ�ϱ����µ��ַ���
		std::string str(len - 1, 0); // ���ٳ���Ϊԭ�ַ������ȵĿ��ַ���
		WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, str.data(), len, nullptr, nullptr); // ��ԭ�ַ���תΪ���ַ���
		return str; // �����¿��ַ���
	}

	// ��ȡ��ǰʱ��
	static inline std::tm getLocalTime() {
		time_t now = time(nullptr); // ��ȡ��ǰʱ���
		return *localtime(&now); // ת��Ϊ�ṹ��
	}

#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
	static inline unsigned logFileCreateDate = 0; // ��־�ļ�����ʱ��

	// ��ȡ��ǰ��־�ļ����
	static inline HANDLE getLogFileHandle() {
		if (!CreateDirectoryA("logs", nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return INVALID_HANDLE_VALUE; // �޷������ļ���

		// ����ļ��Ƿ����
		DWORD attributes = GetFileAttributesA("logs\\latest.log");
		if (attributes != INVALID_FILE_ATTRIBUTES) {
			HANDLE hExistingFile = CreateFileA("logs\\latest.log", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
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
				char newFileName[260];
				_snprintf_s(newFileName, 260, _TRUNCATE, "logs\\%04d-%02d-%02d-%d.log", stLocal.wYear, stLocal.wMonth, stLocal.wDay, ++i);

				// �������ļ�
				if (MoveFileA("logs\\latest.log", newFileName)) break;
			}
		}


		std::tm time = getLocalTime();
		logFileCreateDate = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		return CreateFileA("logs\\latest.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	}

	static inline HANDLE hLogFile = getLogFileHandle(); // ��־�ļ����

	// ������־�ļ����
	static inline void updateLogFileHandle() {
		std::tm time = getLocalTime();
		unsigned date = (time.tm_year << 16) + (time.tm_mon << 8) + time.tm_mday;
		if (date != logFileCreateDate) {
			CloseHandle(hLogFile);
			hLogFile = getLogFileHandle();
		}
	}
#endif

	static inline HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE); // stderr �����

	// ʹ logger ���ƺ���
	static inline std::wstring legalizeLoggerName(const std::wstring& name) {
		// �������һ���س������з�
#ifdef max
		long long lastPos = max((long long)name.find_last_of(L'\r'), (long long)name.find_last_of(L'\n'));
#else
		long long lastPos = std::max((long long)name.find_last_of(L'\r'), (long long)name.find_last_of(L'\n'));
#endif

		// ��ȡ�س����з��Ҳ���ַ���
		return lastPos != std::string::npos ? name.substr(lastPos + 1) : name;
	}

	// ��ȡ��ǰ����̨�ı�����
	static inline WORD getTextAttribute() {
		CONSOLE_SCREEN_BUFFER_INFO buffer;
		if (!GetConsoleScreenBufferInfo(hStderr, &buffer)) return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
		return buffer.wAttributes;
	}

	struct LogTask {
		std::wstring name, message;
		std::string level;
		WORD levelColor, textColor;
	};

	static inline std::queue<LogTask> logQueue; // �������

	// ���� logger ר���߳�
	static inline int startWorkerThread() {
		std::thread([]() {
			while (true) {
				while (logQueue.empty());
				LogTask task = logQueue.front();
				logMessage(task.name, task.message, task.level, task.levelColor, task.textColor);
				logQueue.pop();
			}
		}).detach();
		return 0;
	}

	static int iStartWorkerThread; // �����߳�ռ�÷�

	// �����Ϣ (�ַ���)
	static inline void write(const std::string& msg) {
		WriteFile(hStderr, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		if (hLogFile != INVALID_HANDLE_VALUE) WriteFile(hLogFile, msg.c_str(), (DWORD)msg.length(), nullptr, nullptr);
#endif
	}

	// �����Ϣ (���ַ���)
	static inline void write(std::wstring msg) {
		size_t pos;
		while ((pos = msg.find(L'��')) != std::string::npos) {
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

	// ��ӡ��־ͷ��Ϣ
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

	// Ԥ������Ϣ����
	template<typename T>
	static inline auto processArg(const T& arg) {
		if constexpr (std::is_same_v<T, std::string> || std::is_convertible_v<T, const char*>) return wideString(arg);
		else if constexpr (std::is_same_v<T, bool>) return arg ? L"true" : L"false";
		else return arg;
	}

	// ��ȡ��������ʹ�ó���
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

	// ��ʽ����־��Ϣ
	template<typename... Args>
	static inline std::wstring formatMessage(const std::wstring& message, const Args&... args) {
		size_t len = message.length() + (0 + ... + getLength(args)) + 1;
		auto* arr = new wchar_t[len];
		_snwprintf_s(arr, len, _TRUNCATE, message.c_str(), processArg(args)...);
		return arr;
	}

	// �����־��Ϣ
	static inline void logMessage(const std::wstring& name, std::wstring message, const std::string& level, WORD levelColor, WORD textColor) {
		size_t newlinePos;
#ifdef min
		while ((newlinePos = min(message.find(L'\n'), message.find(L"\r\n"))) != std::string::npos) { // \r\n: CRLF
#else
		while ((newlinePos = std::min(message.find(L'\n'), message.find(L"\r\n"))) != std::string::npos) {
#endif
			logMessage(name, message.substr(0, newlinePos), level, levelColor, textColor);
			message = message.substr(newlinePos + 1);
		}
		if (message[0]) { // ȷ����Ϣ���ǿ��ַ���
			printHead(name, level, levelColor, textColor);
			write(message.substr(message.find_last_of(L'\r') + 1));
			SetConsoleTextAttribute(hStderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
			write("\n");
		}
	}

	std::wstring _name; // logger ����

	// �������
	template<typename... Args>
	inline void pushTask(const std::wstring& message, const std::string& level, WORD levelColor, WORD textColor, Args... args) {
#ifndef KLY_LOGGER_OPTION_NO_LOG_FILE
		updateLogFileHandle();
#endif

		std::wstring formatted = formatMessage(message.c_str(), args...);

		LogTask task = { _name, formatted, level, levelColor, textColor };
		logQueue.push(task);
	}

public:
	// �������� logger
	KlyLogger() = default;

	// �Կ��ַ����������ƴ��� logger
	explicit KlyLogger(const std::wstring& name) : _name(legalizeLoggerName(name)) {}

	// ����ͨ�ַ����������ƴ��� logger
	explicit KlyLogger(const std::string& name) : _name(legalizeLoggerName(wideString(name))) {}

	// �����ͨ��Ϣ (���ַ���)
	template<typename... Args>
	inline void info(const std::wstring& message, const Args&... args) { pushTask(message, "INFO", FOREGROUND_GREEN | FOREGROUND_INTENSITY, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED, args...); }

	// �����ͨ��Ϣ (�ַ���)
	template<typename... Args>
	inline void info(const std::string& message, const Args&... args) { info(wideString(message), args...); }

	// ���������Ϣ (���ַ���)
	template<typename... Args>
	inline void warn(const std::wstring& message, const Args&... args) { pushTask(message, "WARN", FOREGROUND_GREEN | FOREGROUND_RED, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY, args...); }

	// ���������Ϣ (�ַ���)
	template<typename... Args>
	inline void warn(const std::string& message, const Args&... args) { warn(wideString(message), args...); }

	// ���������Ϣ (���ַ���)
	template<typename... Args>
	inline void error(const std::wstring& message, const Args&... args) { pushTask(message, "ERROR", FOREGROUND_RED, FOREGROUND_RED | FOREGROUND_INTENSITY, args...); }

	// ���������Ϣ (�ַ���)
	template<typename... Args>
	inline void error(const std::string& message, const Args&... args) { error(wideString(message), args...); }

	// ������ش�����Ϣ (���ַ���)
	template<typename... Args>
	inline void fatal(const std::wstring& message, const Args&... args) { pushTask(message, "FATAL", COMMON_LVB_UNDERSCORE | FOREGROUND_RED, FOREGROUND_RED, args...); }

	// ������ش�����Ϣ (�ַ���)
	template<typename... Args>
	inline void fatal(const std::string& message, const Args&... args) { fatal(wideString(message), args...); }

	// �ȴ���־������ (3s)
	static inline void wait() {
		Sleep(1500);
		while (!logQueue.empty());
		Sleep(1500);
	}
};

int KlyLogger::iStartWorkerThread = startWorkerThread(); // ���� logger ר���߳�