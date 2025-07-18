#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<string>
#include <termios.h> // 密码输入处理
#include <iomanip>
#include <nlohmann/json.hpp>
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#include<vector>
#include <map>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>

 
using json = nlohmann::json;
using namespace std;
class groupchat
{
private:
    string userName;
    int sock;
    bool running= true;
    thread recvThread;
    bool inChatSession = false;
    int activeid;
    mutex outputMutex;
    mutex inputMutex;
    string currentGroup;
    int groupid;
    string inputBuffer;
    json sendRequest(const json& request) {
        std::string requestStr = request.dump();
        send(sock, requestStr.c_str(), requestStr.size(), 0);
        char buffer[4096] = {0};
        recv(sock, buffer, 4096, 0);
        return json::parse(buffer);
    }
    // // 终端原始模式设置
    // struct termios orig_termios;

    // // 设置终端为原始模式
    // void setRawTerminalMode() {
    //     struct termios raw;
    //     tcgetattr(STDIN_FILENO, &orig_termios);
    //     raw = orig_termios;
        
    //     // 禁用回显和规范模式
    //     raw.c_lflag &= ~(ICANON | ECHO);
    //     raw.c_cc[VMIN] = 1;
    //     raw.c_cc[VTIME] = 0;
        
    //     tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    // }

    // // 恢复原始终端设置
    // void restoreTerminal() {
    //     tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    // }
public:
    
    void groupChat(int fd,string name)
    {
        userName=name;
        sock=fd;
        // 启动接收线程
       recvThread = std::thread([this]() { receivegroupMessages(); });
    
        // 运行主菜单系统
            mainMenu();
            // 清理资源
        running = false;
        if (recvThread.joinable()) {
            recvThread.join();
        }
    }
    // 发送消息给指定用户
    void sendMessage(const string& recipient, const string& text) 
    {
        time_t now = time(nullptr);
        
        json request = {
            {"type", "group_message"},
            {"sender", userName},
            {"groupID", groupid},
            {"message", text},
            {"timestamp", static_cast<uint64_t>(now)}
        };
        
        json response = sendRequest(request);
        
        if (response["success"]) {
            // 显示发送的消息
            lock_guard<mutex> lock(outputMutex);
            
            string timeStr = "Now";
            tm* localTime = localtime(&now);
            if (localTime) {
                char timeBuffer[80];
                strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
                timeStr = string(timeBuffer);
            }
            
            cout << "[" << timeStr << "] 你 -> " << groupid << ": " << text << endl;
        } else {
            lock_guard<mutex> lock(outputMutex);
            cerr << "消息发送失败: " 
                << response["message"] << endl;
        }
    }
    void mainMenu() 
    {
        while (running) {
            // 显示主菜单
            cout << "\n===== 聊天系统主菜单 =====" << endl;
            cout << "1. 开始聊天" << endl;
            cout << "2. 查看历史群聊天记录" << endl;
            cout << "3. 退出系统" << endl;
            cout << "==========================" << endl;
            cout << "请选择操作: ";
            
            string choice;
            getline(cin, choice);
            
            if (choice == "1") {
                // 开始聊天
                cout << "请输入群ID: ";
                
                cin>>groupid;
                //if (!currentGroup.empty()) {
                json req ;
        
                    startgroupChat(groupid);
                //}
            }
            else if (choice == "2") 
            {
                cout << "=====查找聊天记录=====: "<<endl;
                cout<<"请输入群名: ";
                string friendName;
                getline(cin, friendName);
                
                if (!friendName.empty()) {
                    //queryChatHistory(friendName);
                }
            } 
            else if (choice == "3") {
                // 退出系统
                cout << "感谢使用，再见!" << endl;
                running = false;
                break;
            } else {
                cout << "无效选择，请重新输入!" << endl;
            }
        }
    }
    string readLineNonBlocking() 
    {
   
        char buffer[1024];
        string input;
        bool lineComplete = false;
        
        while (!lineComplete) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);
            
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 10000; // 10ms
            
            int ready = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &tv);
            
            if (ready > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
                ssize_t bytesRead = read(STDIN_FILENO, buffer, 1024 - 1);
                
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    
                    for (int i = 0; i < bytesRead; i++) {
                        if (buffer[i] == '\n' || buffer[i] == '\r') {
                            lineComplete = true;
                            break;
                        } else {
                            input += buffer[i];
                        }
                    }
                }
            } else if (ready == 0) {
                // 超时，没有输入
                break;
            }
        }
        
        return input;
    }
    void startgroupChat(int id)
    {
        //displayUnreadMessagesFromFriend(friendName);
        // 设置活动聊天状态
        inChatSession = true;
        activeid = id;
        inputBuffer.clear();
        setvbuf(stdout, NULL, _IONBF, 0); // 禁用标准输出缓冲
        // 显示提示信息
        {
            lock_guard<mutex> lock(outputMutex);
            cout << "\n开始在群" << groupid << " 聊天 (输入 /exit 退出)" << endl;

            cout << "> " << flush;
        }
        while (inChatSession) 
        {
        // 读取整行输入
        string inputLine = readLineNonBlocking();
        
        if (!inputLine.empty()) {
            // 处理输入行
            if (inputLine == "/exit") {
                break;
            }
            cout<<"\033[A"<< flush;
            cout << "\r\033[2K" << "> " << flush;
            // 发送消息
            sendMessage(currentGroup, inputLine);
            
            // 更新输入提示
            cout << "\r\033[2K" << "> " << flush;
        }
        // 短暂休眠避免过度占用CPU
        this_thread::sleep_for(chrono::milliseconds(50));
    }
      
    
        // 退出聊天会话
        inChatSession = false;
        activeid = -1;
        inputBuffer.clear();
        //restoreTerminal();
        
        lock_guard<mutex> lock(outputMutex);
        cout << "\n退出与 " << currentGroup << " 的聊天" << endl;
    } 
    void receivegroupMessages()
    {
        char buffer[4096];
        
        while (running) {
            // 清空缓冲区
            memset(buffer, 0, sizeof(buffer));
            
            // 接收消息（非阻塞模式）
            ssize_t bytesRead = recv(sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
            
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0'; // 确保字符串正确终止
                
                try {
                    json message = json::parse(buffer);
                    // 根据消息类型直接处理
                    if (!message.contains("type")) {
                        lock_guard<mutex> lock(outputMutex);
                        cerr << "无效消息格式，缺少类型字段" << endl;
                        continue;
                    }
                    
                    string type = message["type"];
                    
                    if (type == "group_messages") {
                        
                        // 处理私聊消息
                        string sender = message.value("sender", "");
                        int id=message["group_id"];
                        string text = message.value("message", "");
                        time_t timestamp = message.value("timestamp", time(nullptr));
                        
                        // 格式化时间
                        string timeStr = "Unknown time";
                        tm* localTime = localtime(&timestamp);
                        if (localTime) {
                            char timeBuffer[80];
                            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
                            timeStr = string(timeBuffer);
                        }
                        
                        // 显示消息（使用互斥锁保护输出）
                        {
                            lock_guard<mutex> lock(outputMutex);
                            // 如果正在与发送者聊天，直接显示消息
                            if (inChatSession && activeid == id) {
                                cout << "\n[" << timeStr << "] " << sender << ": " << text << endl;
                                cout << "> " << inputBuffer << flush; // 重新显示输入提示
                            }
                            // 否则显示通知
                            else {
                                cout << "\n您收到来自 " << sender << " 的新消息 (" 
                                    << (text.length() > 20 ? text.substr(0, 20) + "..." : text) 
                                    << ") [" << timeStr << "]" << endl;
                                if (inChatSession) {
                                    cout << "> " << inputBuffer << flush; // 重新显示输入提示
                                }
                            }
                        }

                        // // 发送消息确认（如果提供了消息ID）                                                 
                        // if (message.contains("message_id")) {
                        //     json ack = {
                        //         {"type", "ack_private_message"},
                        //         //{"message_id", message["message_id"]},
                        //         {"user", currentUser},
                        //         {"friend",sender}
                        //     };
                        //     sendRequest(ack);
                        // }
                    }
                    
                } catch (json::parse_error& e) {
                    lock_guard<mutex> lock(outputMutex);
                    cerr << "消息解析错误: " << e.what() << endl;
                }
            } else if (bytesRead == 0) {
                // 连接关闭
                lock_guard<mutex> lock(outputMutex);
                cerr << "\n服务器关闭了连接" << endl;
                running = false;
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // 真正的错误（非非阻塞操作产生的错误）
                lock_guard<mutex> lock(outputMutex);
                perror("\n接收消息错误");
                running = false;
                break;
            }
            
            // 短暂休眠避免过度占用CPU
            this_thread::sleep_for(chrono::milliseconds(50));
        }
    }
   
    

};