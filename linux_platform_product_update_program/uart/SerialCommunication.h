#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <chrono>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <fstream>
#include <termios.h>
#include <thread>
#include "mylog.h"
class SerialCommunication {
public:
	SerialCommunication(const std::string& devicePath, Log& log);

	~SerialCommunication();

	bool updateM2Firmware(const std::string& filePath, size_t timeoutMinutes = 10);

	bool compareM2Version(std::string updateVersion);

	std::string getCurrentProgress();
private:
	size_t getFileSize(const std::string& filename);

	std::vector<std::string> parseInstruction(const std::string& instruction);

	std::string getUpdateVersion(const std::string& filename);
private:
	int fd_;
	size_t totalFileSize_;
	size_t sentFileSize_;
	size_t decompressedFileSize_;
	size_t burnedFileSize_;
	bool startSendingFile_;
	bool startDecompression_;
	bool startBurning_;
	std::string receivedData_;

	//log类
	Log *m_log;
};




