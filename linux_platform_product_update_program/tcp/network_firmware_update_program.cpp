#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <string>
#include <chrono>
#include <vector>
#include <sys/stat.h>
#include <fstream>
#include <termios.h>
#include <thread>
#include <algorithm>
#include "mylog.h"

Log m_log("run.log", 5 * 1024 * 1024, 1);

void sendThread(int clientSocket, size_t& sentFileSize, std::string filePath, bool& startSendingFile, size_t& totalFileSize)
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
		send(clientSocket, sendBuf, bytesToSend, 0);
                sentFileSize += bytesToSend;
                usleep(50000);
            }
                        else
                        {
                break;
            }
        }

        close(fileFd);

    }
}

std::vector<std::string> parseInstruction(const std::string& instruction)
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

size_t getFileSize(const std::string& filename)
{
        struct stat st;
        if (stat(filename.c_str(), &st) == 0)
        {
                return st.st_size;
        }
        return 0;
}

bool updateM2Firmware(const std::string& server_ip, int port, const std::string& filePath)
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating socket" << std::endl;
	m_log.write("Error creating socket");
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port); // 服务端的端口号
    inet_pton(AF_INET, server_ip.c_str(), &serverAddr.sin_addr); // 服务端的IP地址

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
        {
        std::cerr << "Error connecting to server" << std::endl;
	m_log.write("Error connecting to server");
        return false;
    }
    usleep(2000000);
    const char* message = "$BDCTR,10,1,2.000*73\r\n";
    //std::string message = "log version\r\n";
    send(clientSocket, message, strlen(message), 0);

    std::cout << "send:" << message << std::endl;
    m_log.write("send:%s",message);

    // 开始计时
	auto startTime = std::chrono::steady_clock::now();

	// 接收数据并计算发送进度
	std::string receivedData;
	size_t totalFileSize_ = 0;
	size_t sentFileSize_ = 0;
	size_t decompressedFileSize_ = 0;
	size_t burnedFileSize_ = 0;
	bool startSendingFile = false;
	bool startDecompression = false;
	size_t timeoutMinutes = 10;

	while (true)
	{
		// 如果超过计时，发送中止
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::minutes>(currentTime - startTime).count();
		if (elapsedTime >= timeoutMinutes)
		{
			m_log.write("超过计时，发送终止");
			break;
		}
		char recvBuf[4096];
		memset(recvBuf, 0, sizeof(recvBuf));
		int bytesRead = recv(clientSocket, recvBuf, sizeof(recvBuf), 0);										

		if (bytesRead > 0)
		{
			std::cout << "recvBuf:" << recvBuf << std::endl;
			m_log.write("recvBuf:%s",recvBuf);
			recvBuf[bytesRead] = '\0';
			// 将接收到的数据存储起来
			receivedData += std::string(recvBuf, bytesRead);
			std::memset(recvBuf,0,sizeof(recvBuf));
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
						if (fields[2] == "0" && !startSendingFile)
						{
							// 计算文件大小
							totalFileSize_ = getFileSize(filePath);
							// 启动发送线程
							std::thread sendT(sendThread, clientSocket, std::ref(sentFileSize_), filePath, std::ref(startSendingFile), std::ref(totalFileSize_));
                                                        sendT.detach();

						}
						if (fields[3] == "0")
						{
							sentFileSize_ = std::stoi(fields[2]);
							float sendProgress = static_cast<float>(sentFileSize_) / (totalFileSize_);
							std::cout << "发送进度:" << sendProgress*100 << "%" << std::endl;
							m_log.write("发送进度:%f%",sendProgress*100);
						}

						// 当第三个字段为1时，解压完成，计算解压后的文件总大小
						if (fields[3] == "1")
						{
							decompressedFileSize_ = std::stoi(fields[2]);
							std::cout << "解压后固件大小:" << decompressedFileSize_ << std::endl;
							m_log.write("解压后固件大小:%d",decompressedFileSize_);
						}

						// 当第三个字段为2时，开始烧写，计算烧写进度
						if (fields[3] == "2")
						{
							burnedFileSize_ = std::stoi(fields[2]);
							float burnProgress = static_cast<float>(burnedFileSize_) / (decompressedFileSize_);
							std::cout << "烧写进度:" << burnProgress*100 << "%" << std::endl;
							m_log.write("烧写进度:%f%",burnProgress*100);

						}
						if (fields[4] == "1")
						{
							m_log.write("升级出现错误,请检查M2固件是否正确!");
							return false;
						}
					}
					else if (fields.size() >= 5 && fields[1] == "10")	//$BDAWS,10,3053039,1,1*7D
					{
						if (fields[3] == "1" && fields[4] == "1")
						{
							std::cout << "固件烧写成功！" << std::endl;
							m_log.write("固件烧写成功！");
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
						/*
						current_version = fields[1];
						std::string update_version = getUpdateVersion("./M2_software_info.prototxt");
						m_log->write("当前M2固件版本:%s",current_version.c_str());
						m_log->write("升级固件版本:%s",update_version.c_str());
						if (current_version == update_version)
						{
							//m_log->write("升级完成!升级后版本与配置文件版本相同!");
							return true;
						}
						else
						{
							//m_log->write("升级完成!升级后版本与配置文件版本不同!请检查是否有异常!");
							return false;
						}
						*/
					}
				}
				receivedData.erase(0, end_pos + 2);
			}
		}
	}

    close(clientSocket);
    return true;
}

int main(int argc, char* argv[])
{
	if (argc < 4)
        {
                std::cerr << "Usage: " << argv[0] << " <server_ip> <port> <firmware_filename>" << std::endl;
                return 1;
        }
        updateM2Firmware(argv[1],atoi(argv[2]),argv[3]);
        return 0;
}

