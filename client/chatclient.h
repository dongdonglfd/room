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
#include <fstream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>  
using json = nlohmann::json;
using namespace std;
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 10

class Chat
{
private:
    
    // 全局变量
    int sock = -1;           // 已连接的套接字描述符
    string currentUser = "";    // 当前登录的用户名
    bool running= true; // 控制程序运行的标志
    thread recvThread;          // 接收消息的线程
    mutex outputMutex;          // 保护输出操作的互斥锁

    // 用于聊天会话的状态变量
    bool inChatSession = false;
    string activeRecipient;
    string inputBuffer;
        
    
public:

    // 保存原始终端设置
    struct termios origTermios;

    // 设置终端为非阻塞模式
    void setNonBlockingTerminal() {
        struct termios newTermios;
        tcgetattr(STDIN_FILENO, &origTermios);
        newTermios = origTermios;
        newTermios.c_lflag &= ~(ICANON | ECHO); // 禁用规范模式和回显
        newTermios.c_cc[VMIN] = 0;  // 非阻塞读取
        newTermios.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newTermios);
    }

    // 恢复终端设置
    void restoreTerminal() {
        tcsetattr(STDIN_FILENO, TCSANOW, &origTermios);
    }

    // 发送请求到服务器
    json sendRequest(const json& request) {
        // 序列化请求为JSON字符串
        string requestStr = request.dump();
        
        // 发送请求到服务器
        {
            lock_guard<mutex> lock(outputMutex);
            if (send(sock, requestStr.c_str(), requestStr.size(), 0) < 0) {
                cerr << "发送请求失败" << endl;
                return {{"success", false}, {"message", "发送请求失败"}};
            }
        }
        
        // 对于不需要即时响应的请求直接返回
        return {{"success", true}};
    }
    void mainMenu() 
    {
        while (running) {
            // 显示主菜单
            cout << "\n===== 聊天系统主菜单 =====" << endl;
            cout << "1. 开始聊天" << endl;
            cout << "2. 查看历史聊天记录" << endl;
            cout << "3.发送文件" << endl;
            cout << "4. 退出系统" << endl;
            cout << "==========================" << endl;
            cout << "请选择操作: ";
            
            string choice;
            getline(cin, choice);
            
            if (choice == "1") {
                // 开始聊天
                cout << "请输入对方用户名: ";
                string friendName;
                getline(cin, friendName);
                if (!friendName.empty()) {
                    startChatSession(friendName);
                }
            }
            else if (choice == "2") 
            {
                cout << "=====查找聊天记录=====: "<<endl;
                cout<<"请输入对方用户名: ";
                string friendName;
                getline(cin, friendName);
                
                if (!friendName.empty()) {
                    queryChatHistory(friendName);
                }
            } 
            if (choice == "3") {
                // 开始聊天
                cout << "请输入对方用户名: ";
                string friendName;
                getline(cin, friendName);
                if (!friendName.empty()) {
                    
                }
            }
            else if (choice == "4") {
                // 退出系统
                cout << "感谢使用，再见!" << endl;
                running = false;
                break;
            } else {
                cout << "无效选择，请重新输入!" << endl;
            }
        }
    }
    void startChatSession(const string& friendName) 
    {
        // //检查好友关系和黑名单
        // if (!isValidFriend(friendName)) {
        //     std::cerr << "✘ 无法与 " << friendName << " 聊天，请先添加好友或解除屏蔽\n";
        //     return;
        // }
        // 设置终端为非阻塞模式
        displayUnreadMessagesFromFriend(friendName);
        setNonBlockingTerminal();
        
        // 设置活动聊天状态
        inChatSession = true;
        activeRecipient = friendName;
        inputBuffer.clear();
        
        // 显示提示信息
        {
            lock_guard<mutex> lock(outputMutex);
            cout << "\n开始与 " << friendName << " 聊天 (输入 /exit 退出)" << endl;

            cout << "> " << flush;
        }
        
        char c;
        
        while (inChatSession && running) {
            // 读取输入字符
            if (read(STDIN_FILENO, &c, 1) > 0) {
                if (c == '\n' || c == '\r') {
                    cout << "\r\033[2K" << "> " << flush;
                    // 处理完整输入行
                    if (!inputBuffer.empty()) {
                        if (inputBuffer == "/exit") {
                            break;
                        } 
                        else {
                            sendMessage(friendName, inputBuffer);
                        }
                        inputBuffer.clear();
                        
                        lock_guard<mutex> lock(outputMutex);
                        cout << "> " << flush;
                    }
                } 
                // 处理退格
                else if (c == 127 || c == 8) { // 退格键
                    if (!inputBuffer.empty()) {
                        inputBuffer.pop_back();
                        lock_guard<mutex> lock(outputMutex);
                        cout << "\b \b" << flush; // 擦除最后一个字符
                    }
                } 
                // 处理普通字符
                else if (c >= 32 && c <= 126) { // 可打印字符
                    inputBuffer += c;
                    lock_guard<mutex> lock(outputMutex);
                    cout << c << flush;
                }
            }
            
            // 短暂休眠避免过度占用CPU
            this_thread::sleep_for(chrono::milliseconds(50));
        }
        
        // 退出聊天会话
        inChatSession = false;
        activeRecipient = "";
        inputBuffer.clear();
        restoreTerminal();
        
        lock_guard<mutex> lock(outputMutex);
        cout << "\n退出与 " << friendName << " 的聊天" << endl;
    }
    // 发送消息给指定用户
    void sendMessage(const string& recipient, const string& text) 
    {
        time_t now = time(nullptr);
        
        json request = {
            {"type", "send_message"},
            {"sender", currentUser},
            {"recipient", recipient},
            {"message", text},
            {"timestamp", static_cast<uint64_t>(now)}
        };
        
        json response = sendRequest(request);
        
        if (response.value("success", false)) {
            // 显示发送的消息
            lock_guard<mutex> lock(outputMutex);
            
            string timeStr = "Now";
            tm* localTime = localtime(&now);
            if (localTime) {
                char timeBuffer[80];
                strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
                timeStr = string(timeBuffer);
            }
            
            cout << "[" << timeStr << "] 你 -> " << recipient << ": " << text << endl;
        } else {
            lock_guard<mutex> lock(outputMutex);
            cerr << "消息发送失败: " 
                << response.value("message", "未知错误") << endl;
        }
    }
    void receiveMessages() 
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
                    
                    if (type == "private_message") {
                        
                        // 处理私聊消息
                        string sender = message.value("sender", "");
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
                            if (inChatSession && activeRecipient == sender) {
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
                        
                        // 发送消息确认（如果提供了消息ID）
                        if (message.contains("message_id")) {
                            json ack = {
                                {"type", "ack_private_message"},
                                //{"message_id", message["message_id"]},
                                {"user", currentUser},
                                {"friend",sender}
                            };
                            sendRequest(ack);
                        }
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
    void privateChat(int sockfd,std::string current_user)
    {   running=true;
        sock=sockfd;
        currentUser=current_user;
        // 启动接收线程
        recvThread = std::thread([this]() { receiveMessages(); });
    
        // 运行主菜单系统
            mainMenu();
            // 清理资源
        running = false;
        if (recvThread.joinable()) {
            recvThread.join();
        }
        
    }
    bool isValidFriend(const std::string& friendName) 
    {
        // if (friendName == currentUser) {
        //     return false;
        // }
        
        // //向服务器查询
        // json req;
        // req["type"] = "check_friend_valid";
        // req["user"] = currentUser;
        // req["friend"] = friendName;
        // json res = sendRequest(req);
        // bool isValid = false;
        // if (res["success"]) {
        //     isValid = res["valid"];
        // } else {
        //     // 查询失败时保守处理
        //     std::cerr << "✘ 好友验证失败: " << res["message"] << std::endl;
        //     isValid = false;
        // }
        // return isValid;
    }
    void displayUnreadMessagesFromFriend(const string& friendName)
    {
        
        json request = {
            {"type", "get_friend_unread_messages"},
            {"user", currentUser},
            {"friend", friendName}
        };
        
        // 发送请求并获取响应
        //json response = sendRequest(request);
        std::string requestStr = request.dump();
        send(sock, requestStr.c_str(), requestStr.size(), 0);
        char buffer[4096] = {0};
        recv(sock, buffer, 4096, 0);
        json response= json::parse(buffer);
        if (!response["success"]) 
        {
            
            return ;

        }
        auto messages = response["messages"];
        for (const auto& msg : messages) {
                string text = msg.value("message", "");
                string timestampStr = msg.value("timestamp", "");
                
                // 转换时间戳为可读格式
                struct tm tm = {};
                strptime(timestampStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
                time_t timestamp = mktime(&tm);
                
                char timeBuffer[80];
                strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localtime(&timestamp));
                
                cout << "[" << timeBuffer << "] " << friendName << ": " << text << endl;
            }
            
            cout << "=============================" << endl;
            
            // 发送确认请求，标记消息为已读
            json ack = {
                {"type", "ack_private_message"},
                {"user", currentUser},
                {"friend", friendName}
            };
            sendRequest(ack);
    }
    
    void queryChatHistory(const string& friendName)
    {
        json request = {
            {"type", "get_chat_history"},
            {"user", currentUser},
            {"friend", friendName},
        };
        
        std::string requestStr = request.dump();
        
        // 发送请求
        if (send(sock, requestStr.c_str(), requestStr.size(), 0) < 0) {
            perror("发送请求失败");
            return;
        }
        
        // 接收响应
        const int initialBufferSize = 4096;
        vector<char> buffer(initialBufferSize);
        ssize_t totalReceived = 0;
        
        while (true) {
            // 检查是否需要扩展缓冲区
            if (totalReceived >= buffer.size() - 1) {
                buffer.resize(buffer.size() * 2);
            }
            
            // 接收数据
            ssize_t bytesRead = recv(sock, buffer.data() + totalReceived, 
                                    buffer.size() - totalReceived - 1, 0);
            
            if (bytesRead < 0) {
                perror("接收响应失败");
                return;
            }
            
            if (bytesRead == 0) {
                // 连接关闭
                break;
            }
            
            totalReceived += bytesRead;
            
            // 尝试解析 JSON
            try {
                // 添加终止符
                buffer[totalReceived] = '\0';
                
                // 尝试解析
                json response = json::parse(buffer.data());
                
                // 如果解析成功，处理响应
                processChatHistory(response);
                return;
                
            } catch (json::parse_error& e) {
                // 如果错误不是"unexpected end of input"，继续接收更多数据
                if (e.id != 101) {
                    cerr << "JSON解析错误: " << e.what() << endl;
                    cerr << "已接收数据: " << totalReceived << "字节" << endl;
                    return;
                }
            }
        }
        
        // 如果循环结束但未解析成功
        cerr << "无法解析服务器响应" << endl;
    }

    void processChatHistory(const json& response) 
    {
        if (!response.value("success", false)) {
            cout << "无历史聊天记录" << endl;
            return;
        }
        
        if (!response.contains("messages") || !response["messages"].is_array()) {
            cerr << "无效响应格式" << endl;
            return;
        }
        
        auto messages = response["messages"];
        for (const auto& msg : messages) {
            //验证消息格式
            if (!msg.is_object() || 
                !msg.contains("sender") || !msg["sender"].is_string() ||
                !msg.contains("message") || !msg["message"].is_string() ||
                !msg.contains("timestamp") || !msg["timestamp"].is_number()) {
                cerr << "无效消息格式" << endl;
                continue;
            }
            
            // 解析时间戳
            time_t timestamp = msg["timestamp"];
            tm* localTime = localtime(&timestamp);
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
            
            // 确定消息方向
            string sender = msg["sender"];
            string arrow = (sender == currentUser) ? "->" : "<-";
            string displayName = (sender == currentUser) ? "你" : sender;
            
            cout << "[" << timeBuffer << "] " 
                << displayName << " " << arrow << " "
                << msg["message"] << endl;
        }
        
        cout << "========================" << endl;
    }
    

};