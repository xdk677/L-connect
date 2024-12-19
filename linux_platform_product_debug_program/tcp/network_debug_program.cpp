#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <ncurses.h>
#include <signal.h>
#include <algorithm>  
#include "mylog.h"

Log m_log("run.log", 5 * 1024 * 1024, 1);
volatile sig_atomic_t flag = 0;

void handleSigint(int sig)
{
    flag = 1;
}

void receiveData(int clientSocket, WINDOW* win) 
{
    char buffer[4096];
    int bytesReceived;
    while (!flag) 
    {
        memset(buffer, 0, sizeof(buffer));
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived == -1) 
        {
            std::cerr << "Error receiving response from server" << std::endl;
	    m_log.write("Error receiving response from server");
            break;
        } 
        else if (bytesReceived == 0)
        {
            std::cerr << "Server disconnected" << std::endl;
	    m_log.write("Server disconnected");
            flag = 1;
            break;
        }
        // 处理消息，例如去除特殊字符
        std::string message = buffer;
        message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
        message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());

        wprintw(win, "Server response: %s\n", message.c_str());
	m_log.write("Server response: %s",message.c_str());
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
    if (argc < 3) 
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port>" << std::endl;
        return 1;
    }

    const char* serverIp = argv[1];
    int port = std::stoi(argv[2]);

    initscr(); // 初始化ncurses
    cbreak(); // 立即响应输入
    noecho(); // 不回显输入
    signal(SIGINT, handleSigint); // 设置信号处理程序
    int clientSocket = socket(AF_INET, SOCK_STREAM,0);
    if (clientSocket == -1)
    {
        endwin(); // 结束ncurses
        reset_shell_mode(); // 恢复终端状态
        std::cerr << "Error creating socket" << std::endl;
	m_log.write("Error creating socket");
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port); // 服务端的端口号
    inet_pton(AF_INET, serverIp, &serverAddr.sin_addr); // 服务端的IP地址

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) 
    {
        endwin(); // 结束ncurses
        reset_shell_mode(); // 恢复终端状态
        std::cerr << "Error connecting to server" << std::endl;
	m_log.write("Error connecting to server");
        return 1;
    }

    WINDOW* win = newwin(LINES - 3, COLS, 1, 0);
    scrollok(win, TRUE);

    WINDOW* inputWin = newwin(1, COLS, LINES - 2, 0);
    mvwprintw(inputWin, 0, 0, "Enter a command: ");
    wrefresh(win);
    wrefresh(inputWin);

    std::thread receiver(receiveData, clientSocket, win);
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

        if (send(clientSocket, commandWithNewline.c_str(), commandWithNewline.size(), 0) == -1)
        {
            std::cerr << "Error sending data to server" << std::endl;
	    m_log.write("Error sending data to server");
            break;
        }
        wprintw(win, "Sent: %s\n", userInput);
	m_log.write("Sent: %s",userInput);
        wrefresh(win);

        if (strcmp(userInput, "exit") == 0) 
        {
            flag = 1;
            break;
        }
    }

    shutdown(clientSocket, SHUT_WR); // 断开发送数据的连接

    while (!flag) {} // 等待接收线程结束

    close(clientSocket); // 关闭套接字

    endwin(); // 结束ncurses
    reset_shell_mode(); // 恢复终端状态

    return 0;
}

