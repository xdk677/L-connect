#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <thread>
#include <ncurses.h>
#include <signal.h>
#include <algorithm>
#include "mylog.h"

volatile sig_atomic_t flag = 0;
int serialPort;
Log m_log("run.log", 5 * 1024 * 1024, 1);


void handleSigint(int sig)
{
    flag = 1;
}

void receiveData(WINDOW* win)
{
    char buffer[4096];
    int bytesReceived;
    while (!flag)
    {
        memset(buffer, 0, sizeof(buffer));
        bytesReceived = read(serialPort, buffer, sizeof(buffer));
        if (bytesReceived == -1)
        {
            std::cerr << "Error receiving response from serial port" << std::endl;
	    m_log.write("Error receiving response from serial port");
            break;
        }
        else if (bytesReceived == 0)
        {
            std::cerr << "Serial port disconnected" << std::endl;
	    m_log.write("Serial port disconnected");
            flag = 1;
            break;
        }
        // 处理消息，例如去除特殊字符
        std::string message = buffer;
        message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
        message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());

        wprintw(win, "Serial port response: %s\n", message.c_str());
	m_log.write("Serial port response: %s\n",message.c_str());
        wrefresh(win);
        if (message == "exit")
        {
            flag = 1;
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <serial_port>" << std::endl;
        return 1;
    }

    const char* serialPortName = argv[1];

    initscr(); // 初始化ncurses
    cbreak(); // 立即响应输入
    noecho(); // 不回显输入
    signal(SIGINT, handleSigint); // 设置信号处理程序

    serialPort = open(serialPortName, O_RDWR | O_NOCTTY); 
    if (serialPort == -1)
    {
        endwin(); // 结束ncurses
        reset_shell_mode(); // 恢复终端状态
        std::cerr << "Error opening serial port" << std::endl;
	m_log.write("Error opening serial port");
        return 1;
    }

    struct termios serialConfig;
    if (tcgetattr(serialPort, &serialConfig) != 0) {
        std::cerr << "无法获取串口参数！" << std::endl;
	m_log.write("无法获取串口参数！");
        close(serialPort);
        return 1;
    }
    tcgetattr(serialPort, &serialConfig);

    serialConfig.c_cflag &= ~PARENB; // 禁用奇偶校验
    serialConfig.c_cflag &= ~CSTOPB; // 设置停止位为1位
    serialConfig.c_cflag &= ~CSIZE;  // 清除数据位的设置
    serialConfig.c_cflag |= CS8;     // 设置数据位为8位
    serialConfig.c_cflag |= CREAD;   // 启用读取
    serialConfig.c_cflag |= CLOCAL;  // 忽略解调器线路状态
    //tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // 禁用规范输入和回显
    serialConfig.c_lflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    serialConfig.c_iflag &= ~(INLCR|IGNCR|ICRNL);
    serialConfig.c_oflag &= ~(ONLCR|OCRNL);
    serialConfig.c_iflag &= ~(IXON | IXOFF | IXANY);         // 禁用软件流控制
    serialConfig.c_oflag &= ~OPOST;                           // 禁用输出处理
    serialConfig.c_cc[VMIN] = 1;  // 读取至少0个字节
    serialConfig.c_cc[VTIME] = 0; // 读取的超时时间为1秒



    speed_t baudRate = B115200;//B460800;
    cfsetospeed(&serialConfig, baudRate); // 设置输出波特率
    cfsetispeed(&serialConfig, baudRate); // 设置输入波特率

    if (tcsetattr(serialPort, TCSANOW, &serialConfig) != 0) {
        std::cerr << "无法设置串口参数！" << std::endl;
	m_log.write("无法设置串口参数！");
        close(serialPort);
        return 1;
    }

    WINDOW* win = newwin(LINES - 3, COLS, 1, 0);
    scrollok(win, TRUE);

    WINDOW* inputWin = newwin(1, COLS, LINES - 2, 0);
    mvwprintw(inputWin, 0, 0, "Enter a command: ");
    wrefresh(win);
    wrefresh(inputWin);

    std::thread receiver(receiveData, win);
    receiver.detach(); // 分离接收线程

    while (!flag)
    {
        echo(); // 回显输入
        char userInput[4096];
        mvwgetnstr(inputWin, 0, 16, userInput, sizeof(userInput)); // 获取用户输入
        noecho(); // 不回显输入

        wclear(inputWin);
        mvwprintw(inputWin, 0, 0, "Enter a command: ");
        wrefresh(inputWin);

        std::string commandWithNewline = userInput;
        commandWithNewline += "\r\n"; // 添加回车和换行符

        if (write(serialPort, commandWithNewline.c_str(), commandWithNewline.size()) == -1)
        {
            std::cerr << "Error sending data to serial port" << std::endl;
	    m_log.write("Error sending data to serial port");
            break;
        }
        wprintw(win, "Sent: %s\n", userInput);
	m_log.write("Sent: %s\n",userInput);
        wrefresh(win);

        if (strcmp(userInput, "exit") == 0)
        {
            flag = 1;
            break;
        }
    }

    close(serialPort); // 关闭串口

    endwin(); // 结束ncurses
    reset_shell_mode(); // 恢复终端状态

    return 0;
}

