#include "SerialCommunication.h"


SerialCommunication::SerialCommunication(const std::string& devicePath, Log& log) : fd_(-1), totalFileSize_(0), sentFileSize_(0),
	decompressedFileSize_(0), startSendingFile_(false),
	startDecompression_(false), m_log(&log)
{
	// 打开串口设备文件
	//fd_ = open(devicePath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_SYNC);  // 设置 O_NONBLOCK 标志位
	fd_ = open(devicePath.c_str(), O_RDWR | O_NOCTTY);
	if (fd_ == -1)
	{
		m_log->write("ERROR! 无法打开串口设备文件!");
	}
	
	struct termios tty;
	/*
	struct termios options;
	tcgetattr(fd_, &options);
	options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_cflag = CS8 | CREAD | CLOCAL;
    options.c_lflag = 0;
	struct termios options;
	cfsetispeed(&options,B115200);
	cfsetospeed(&options,B115200);
	//set data bit
	
	options.c_cflag |= CLOCAL | CREAD;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	//set parity
	options.c_cflag &= ~PARENB;
	//set stopbit
	options.c_cflag &= ~CSTOPB;
		
	tcsetattr(fd_, TCSANOW, &options);
	*/
	//配置串口参数
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "无法获取串口参数！" << std::endl;
        close(fd_);
        return ;
    }
    tty.c_cflag &= ~PARENB; // 禁用奇偶校验
    tty.c_cflag &= ~CSTOPB; // 设置停止位为1位
    tty.c_cflag &= ~CSIZE;  // 清除数据位的设置
    tty.c_cflag |= CS8;     // 设置数据位为8位
    tty.c_cflag |= CREAD;   // 启用读取
    tty.c_cflag |= CLOCAL;  // 忽略解调器线路状态
    //tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 禁用规范输入和回显
    tty.c_lflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_iflag &= ~(INLCR|IGNCR|ICRNL);
    tty.c_oflag &= ~(ONLCR|OCRNL);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // 禁用软件流控制
    tty.c_oflag &= ~OPOST;                           // 禁用输出处理
    tty.c_cc[VMIN] = 1;  // 读取至少0个字节
    tty.c_cc[VTIME] = 0; // 读取的超时时间为1秒



    speed_t baudRate = B115200;//B460800;
    cfsetospeed(&tty, baudRate); // 设置输出波特率
    cfsetispeed(&tty, baudRate); // 设置输入波特率

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "无法设置串口参数！" << std::endl;
        close(fd_);
        return ;
    }
}

SerialCommunication::~SerialCommunication() 
{
	if (fd_ != -1)
	{
		close(fd_);
	}
}

size_t SerialCommunication::getFileSize(const std::string& filename) 
{
	struct stat st;
	if (stat(filename.c_str(), &st) == 0)
	{
		return st.st_size;
	}
	return 0;
}

std::string SerialCommunication::getUpdateVersion(const std::string& filename)
{
	std::ifstream file(filename);
	if (file.is_open())
	{
		std::string line;
		if (std::getline(file, line))
		{
			return line;
		}
	}
	return "";
}

std::vector<std::string> SerialCommunication::parseInstruction(const std::string& instruction)
{
	std::vector<std::string> fields;

	// 检查指令是否以$BDAWS开头和\r\n结尾
	if (instruction.substr(0, 7) == "$BDAWS," && instruction.substr(instruction.length() - 2) == "\r\n") 
	{
		// 移除指令头和尾部的特殊字符
		std::string trimmedInstruction = instruction.substr(0,instruction.length() - 5);
		// 解析指令字段
		size_t pos = 0;
		std::string field;
		while ((pos = trimmedInstruction.find(',')) != std::string::npos) 
		{
			field = trimmedInstruction.substr(0, pos);
			fields.push_back(field);
			trimmedInstruction.erase(0, pos + 1);
		}
		// 添加最后一个字段
		fields.push_back(trimmedInstruction);
	}
	else if(instruction.substr(0, 7) == "$BDVER," && instruction.substr(instruction.length() - 2) == "\r\n")
	{
		std::string trimmedInstruction = instruction;
		// 解析指令字段
                size_t pos = 0;
                std::string field;
                while ((pos = trimmedInstruction.find(',')) != std::string::npos)
                {
                        field = trimmedInstruction.substr(0, pos);
                        fields.push_back(field);
                        trimmedInstruction.erase(0, pos + 1);
                }
                // 添加最后一个字段
                fields.push_back(trimmedInstruction);
	}

	return fields;
}

bool SerialCommunication::compareM2Version(std::string updateVersion)
{
	const char* command = "LOG VERSION\r\n";
	write(fd_, command, strlen(command));
	// 开始计时
	auto startTime = std::chrono::steady_clock::now();
	std::string receivedData;

	while (true)
	{
		// 如果超过计时，发送中止
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::minutes>(currentTime - startTime).count();
		if (elapsedTime >= 1)
		{
			m_log->write("超过计时，发送终止");
			break;
		}
		char recvBuf[256];
		int bytesRead = read(fd_, recvBuf, sizeof(recvBuf));
		if (bytesRead != -1)
		{
			// 将接收到的数据存储起来
			receivedData += std::string(recvBuf, bytesRead);
			std::memset(recvBuf, 0, sizeof(recvBuf));
			m_log->write("Received data:%s", receivedData.c_str());
			//检查是否接收到完整指令
			size_t end_pos = receivedData.find("\r\n");
			std::vector<std::string> fields;
			fields.clear();
			std::string current_version = "";
			if (end_pos != std::string::npos)
			{
				size_t start_pos_VER = receivedData.find("$BDVER");
				if (start_pos_VER != std::string::npos && start_pos_VER < end_pos)
				{
					std::string instruction = receivedData.substr(start_pos_VER, end_pos - start_pos_VER + 2);
					fields = parseInstruction(instruction);
					if (fields.size() >= 2)
					{
						current_version = fields[1];
						std::string update_version = getUpdateVersion("./M2_software_info.prototxt");
						m_log->write("当前M2固件版本:%s", current_version.c_str());
						m_log->write("升级固件版本:%s", update_version.c_str());
						if (current_version == update_version)
						{
							m_log->write("当前M2版本与配置文件版本相同!");
							return true;
						}
						else
						{
							m_log->write("当前M2版本与配置文件版本不相同!");
							return false;
						}
					}

				}
				receivedData.erase(0, end_pos + 2);
			}
		}
	}
	return false;
}

std::string SerialCommunication::getCurrentProgress()
{
	std::string ret = "";
	if (totalFileSize_ == 0)
	{
		ret = "未开始升级";
	}
	else if (totalFileSize_ != 0 && sentFileSize_ < totalFileSize_)
	{
		float sendProgress = static_cast<float>(sentFileSize_) / (totalFileSize_);
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2) << sendProgress*100;

		ret = "固件发送中,当前进度:" + oss.str();
	}
	else if (totalFileSize_ != 0 && decompressedFileSize_ != 0 && burnedFileSize_ < decompressedFileSize_)
	{
		float burnProgress = static_cast<float>(burnedFileSize_) / (decompressedFileSize_);
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(2) << burnProgress * 100;

		ret = "固件升级中,当前进度:" + oss.str();
	}
	else
	{
		ret = "未定义错误";
	}
	return ret;
}

// 发送数据的线程函数
void sendThread(int fd, size_t& sentFileSize, const std::string& filePath, bool& startSendingFile, size_t& totalFileSize)
{
    // 打开文件并发送数据
    int fileFd = open(filePath.c_str(), O_RDONLY);
    if (fileFd != -1)
    {
        startSendingFile = true;
        while (true)
       	{
            char sendBuf[512];
            int bytesToSend = read(fileFd, sendBuf, sizeof(sendBuf));
            if (bytesToSend > 0)
			{
                write(fd, sendBuf, bytesToSend);
                sentFileSize += bytesToSend;
	//			usleep(1000);
            }
			else
			{
                break;
            }
        }

        close(fileFd);
        
    }
}

bool SerialCommunication::updateM2Firmware(const std::string& filePath, size_t timeoutMinutes)
{
	if (fd_ == -1) 
	{
		std::cerr << "无法打开串口设备文件" << std::endl;
		return false;
	}
	
	// 发送指令 "$BDCTR,10,1,2.000*73\r\n"
	const char* command = "$BDCTR,10,1,2.000*73\r\n";
	write(fd_, command, strlen(command));
	m_log->write("Send command:%s", command);
	// 开始计时
	auto startTime = std::chrono::steady_clock::now();

	// 接收数据并计算发送进度
	std::string receivedData;
	totalFileSize_ = 0;
	sentFileSize_ = 0;
	decompressedFileSize_ = 0;
	burnedFileSize_ = 0;
	bool startSendingFile = false;
	bool startDecompression = false;

	while (true)
	{
		// 如果超过计时，发送中止
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::minutes>(currentTime - startTime).count();
		if (elapsedTime >= timeoutMinutes)
		{
			m_log->write("超过计时，发送终止");
			break;
		}
		char recvBuf[1024];
		int bytesRead = read(fd_, recvBuf, sizeof(recvBuf));
		if (bytesRead > 0)
		{
			recvBuf[bytesRead] = '\0';
			m_log->write("Received data size:%d; data:%s",bytesRead, std::string(recvBuf, bytesRead).c_str());
			std::cout << "Received data:" << std::string(recvBuf, bytesRead).c_str() << std::endl;
			// 将接收到的数据存储起来
			receivedData += std::string(recvBuf, bytesRead);
			std::memset(recvBuf,0,sizeof(recvBuf));
			//m_log->write("Received data:%s", receivedData.c_str());
			//检查是否接收到完整指令
			size_t end_pos = receivedData.find("\r\n");
			std::vector<std::string> fields;
			fields.clear();
			std::string current_version = "";

			if (end_pos != std::string::npos)
			{
				size_t start_pos_AWS = receivedData.find("$BDAWS");
				size_t start_pos_VER = receivedData.find("$BDVER");
				size_t start_pos_CRC = receivedData.find("$BDCRC");
				if (start_pos_AWS != std::string::npos && start_pos_AWS < end_pos)
				{
					std::string instruction = receivedData.substr(start_pos_AWS, end_pos - start_pos_AWS + 2);
					fields = parseInstruction(instruction);
					if (fields.size() >= 5 && fields[1] == "11")
					{
						// 当第三个字段为0时，开始发送文件
						m_log->write("fields[0]: %s; fields[1]: %s; fields[2]: %s; fields[3]: %s; fields[4]: %s",fields[0].c_str(),fields[1].c_str(),fields[2].c_str(),fields[3].c_str(),fields[4].c_str());
						if (fields[2] == "0" && !startSendingFile)
						{
							// 计算文件大小
							m_log->write("startSendingFile...");
							m_log->write("file path: %s", filePath.c_str());
							totalFileSize_ = getFileSize(filePath);
							m_log->write("Firmware file size:%d",totalFileSize_);
							// 启动发送线程
							std::thread sendT(sendThread, fd_, std::ref(sentFileSize_), filePath, std::ref(startSendingFile), std::ref(totalFileSize_));
							sendT.detach();
						}
						if (fields[3] == "0")
						{
							sentFileSize_ = std::stoi(fields[2]);
							float sendProgress = static_cast<float>(sentFileSize_) / (totalFileSize_);
							m_log->write("发送进度:%.2f%%",sendProgress * 100);
							std::cout << "发送进度:" << sendProgress * 100 << "%" << std::endl;
						}

						// 当第三个字段为1时，解压完成，计算解压后的文件总大小
						if (fields[3] == "1")
						{
							decompressedFileSize_ = std::stoi(fields[2]);
							m_log->write("解压后固件大小:%d",decompressedFileSize_);
							std::cout << "解压后固件大小:" << decompressedFileSize_ << std::endl;
						}

						// 当第三个字段为2时，开始烧写，计算烧写进度
						if (fields[3] == "2")
						{
							burnedFileSize_ = std::stoi(fields[2]);
							float burnProgress = static_cast<float>(burnedFileSize_) / (decompressedFileSize_);
							m_log->write("烧写进度:%.2f%%",burnProgress * 100);
							std::cout << "烧写进度:" << burnProgress * 100 << "%" << std::endl;
						}
						if (fields[4] == "1")
						{
							m_log->write("升级出现错误,请检查M2固件是否正确!");
							std::cout << "升级出现错误,请检查M2固件是否正确!" << std::endl;
							return false;
						}
					}
					else if (fields.size() >= 5 && fields[1] == "10")	//$BDAWS,10,3053039,1,1*7D
					{
						if (fields[3] == "1" && fields[4] == "1")
						{
							/*
							m_log->write("烧写完成!");

							const char* command1 = "LOG VERSION\r\n";							
							write(fd_, command1, strlen(command1));
							m_log->write("Send command:%s", command);
							*/
							std::cout << "固件烧写完成!" << std::endl;
							return true;
						}
					}
				}
				else if (start_pos_VER != std::string::npos && start_pos_VER < end_pos)
				{
					std::string instruction = receivedData.substr(start_pos_VER, end_pos - start_pos_VER + 2);
					fields = parseInstruction(instruction);
					if (fields.size() >= 2)
					{
						current_version = fields[1];
						std::string update_version = getUpdateVersion("./M2_software_info.prototxt");
						m_log->write("当前M2固件版本:%s",current_version.c_str());
						m_log->write("升级固件版本:%s",update_version.c_str());
						if (current_version == update_version)
						{
							m_log->write("升级完成!升级后版本与配置文件版本相同!");
							return true;
						}
						else
						{
							m_log->write("升级完成!升级后版本与配置文件版本不同!请检查是否有异常!");
							return false;
						}
					}
				}
				receivedData.erase(0, end_pos + 2);
				end_pos = receivedData.find("\r\n");
			}
		}
	}

	close(fd_);
	return false;
}





