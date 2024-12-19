#ifndef _MYLOG_H_
#define _MYLOG_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <mutex>
#include <thread>

class Log {
public:
    Log(const std::string& filename, int maxSize, int backupCount) :
        file_(filename), maxSize_(maxSize), backupCount_(backupCount), size_(0) {
        // 检查备份文件数量是否合法
        backupCount_ = std::max(0, backupCount_);
        backupCount_ = std::min(99, backupCount_);

        // 初始化备份文件名格式化字符串
        char buf[16];
        std::snprintf(buf, 16, "%%0%dd", 2);
        std::string format(buf);
        backupNameFormat_ = file_ + ".%s";

        for (int i = 1; i <= backupCount_; ++i) {
            std::stringstream ss;
            ss << std::setw(2) << std::setfill('0') << i;
            std::string index = ss.str();
            std::string backupFile = file_ + "." + format + ".bak";
            backupFiles_.push_back(backupFile);
        }
    }

    void write(const std::string& message) {
        std::unique_lock<std::mutex> lock(mutex_);
        std::ofstream fout(file_, std::ios::app);

        // 如果当前文件大小超过了最大值，则备份该文件，并创建新的日志文件
        if (size_ >= maxSize_) {
            if (!backupFiles_.empty()) {
                // 删除最老的备份文件
                std::remove(backupFiles_.back().c_str());
                // 将备份文件往后移动一个位置
                for (int i = backupCount_ - 1; i >= 1; --i) {
                    std::string oldFile = backupNameFormat_;
                    std::snprintf(&oldFile[0], oldFile.size(), backupNameFormat_.c_str(), i);
                    std::string newFile = backupNameFormat_;
                    std::snprintf(&newFile[0], newFile.size(), backupNameFormat_.c_str(), i + 1);
                    std::rename(oldFile.c_str(), newFile.c_str());
                }
            }

            // 将当前日志文件变成第一个备份文件
            std::string newBackupFile = backupNameFormat_;
            std::snprintf(&newBackupFile[0], newBackupFile.size(), backupNameFormat_.c_str(), 1);
            std::rename(file_.c_str(), newBackupFile.c_str());

            // 创建新的日志文件
            fout.close();
            fout.open(file_, std::ios::out | std::ios::trunc);
            size_ = 0;
        }
	std::time_t t = std::time(nullptr);
	char timeStr[128];
	std::strftime(timeStr, 128, "%Y-%m-%d %H:%M:%S", std::localtime(&t));

	fout << "[" << timeStr << "] " << message << std::endl;
        size_ += message.size() + std::strlen(timeStr) + 4; // 加上换行符的长度
    }

    template<typename... Args>
    void write(const char* format, const Args&... args)
    {
	char message[1024];
	std::snprintf(message, sizeof(message), format, args...);
	write(std::string(message));
    }

private:
    std::string file_;
    int maxSize_;
    int backupCount_;
    std::vector<std::string> backupFiles_;
    std::string backupNameFormat_;
    int size_;
    std::mutex mutex_;
};

#endif	//_MYLOG_H_

