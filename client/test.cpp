// #include<iostream>
// #include<stdio.h>
// #include<stdlib.h>
// #include<unistd.h>
// #include<string.h>
// #include<arpa/inet.h>
// #include<sys/epoll.h>
// #include<string>
// #include <termios.h> // 密码输入处理
// #include <iomanip>
// #include <nlohmann/json.hpp>
// #include </usr/include/x86_64-linux-gnu/curl/curl.h>
// #include<vector>
// #include <map>
// #include <algorithm>
// #include <fstream>
// #include <filesystem>
// #include <ctime>
// #include <iomanip>
// #include <atomic>
// #include <thread>
 
// using json = nlohmann::json;
// #define PORT 8080
// #define BUFFER_SIZE 1024
// #define MAX_EVENTS 10

// class Chat
// {
// private:
//     int sock;
//     std::string currentUser; 
//     json sendRequest(const json& req) {
//         std::string requestStr = req.dump();
//         send(sock, requestStr.c_str(), requestStr.size(), 0);

//         char buffer[4096] = {0};
//         recv(sock, buffer, 4096, 0);
//         return json::parse(buffer);
//     }
// public:

//     void privateChat(int sockfd,std::string current_user)
//     {
//         sock=sockfd;
//         currentUser=current_user;
//         std::cout << "\n=== 好友聊天 ===\n";
//         std::cout << "输入好友用户名: ";
//         std::string friendName;
//         std::cin >> friendName;
//         // 检查好友关系和黑名单
//         if (!isValidFriend(friendName)) {
//             std::cerr << "✘ 无法与 " << friendName << " 聊天，请先添加好友或解除屏蔽\n";
//             return;
//         }
        
        
//         std::cout << "开始与 " << friendName << " 聊天\n";
        
//         // 获取历史消息
//         //readChatHistoryFromFile(friendName);
//         // 启动消息接收线程
//         std::atomic<bool> stopChat(false);
//         std::thread receiver([&]() {
//             while (!stopChat) {
//                 processPrivateIncoming(friendName);
//                 std::this_thread::sleep_for(std::chrono::seconds(1));
//             }
//         });
        
//         // 聊天主循环
//         while (true) {
//             std::cout << "\n[" << friendName << "] 输入消息 (输入 /exit 退出, /file 发送文件): ";
//             std::string message;
//             std::cin.ignore();
//             std::getline(std::cin, message);
            
//             if (message == "/exit") {
//                 stopChat = true;
//                 break;
//             } else if (message == "/file") {
//                 //sendPrivateFile(friendName);
//                 continue;
//             }
            
//             // 发送消息
//             time_t timestamp = time(nullptr);
//             json req;
//             req["type"] = "private_message";
//             req["recipient"] = friendName;
//             req["sender"] = currentUser;
//             req["message"] = message;
//             req["timestamp"] = timestamp;
            
//             json res = sendRequest(req);
//             if (!res["success"]) {
//                 std::cerr << "✘ 消息发送失败: " << res["message"] << std::endl;
//             } else {
//             //     // 成功发送后保存到本地文件
//             //     //saveMessageToFile(friendName, currentUser, message, timestamp);
//             }
//         }
        
//         // 停止并加入接收线程
//         stopChat = true;
//         receiver.join();
//     }
//     void processPrivateIncoming(const std::string& friendName) 
//     {
//         json req;
//         req["type"] = "get_new_private_messages";
//         req["user"] = currentUser;
//         req["friend"] = friendName;
        
//         json res = sendRequest(req);
//         // if (!res["success"]) {
//         //     return;
//         // }
        
//         auto messages = res["messages"];
//         for (auto& msg : messages) {
//             std::time_t ts = msg["timestamp"];
//             std::string sender = msg["sender"];
//             std::string message = msg["message"];
            
//             // 显示消息
//             std::tm* tm = std::localtime(&ts);
//             std::cout << "\n[" << std::put_time(tm, "%H:%M") 
//                     << "] " << sender << ": " << message;
            
//             // 保存到本地文件
//             //saveMessageToFile(friendName, sender, message, ts);
            
//             // 标记消息为已接收
//             if (msg["id"].is_number_integer()) {
//                 json ack;
//                 ack["type"] = "ack_private_message";
//                 ack["message_id"] = msg["id"];
//                 ack["user"] = currentUser;
//                 sendRequest(ack);
//             }
//         }
        
//         // auto files = res["files"];
//         // for (auto& file : files) {
//         //     std::time_t ts = file["timestamp"];
//         //     std::tm* tm = std::localtime(&ts);
//         //     std::cout << "\n[" << std::put_time(tm, "%H:%M") 
//         //             << "] " << file["sender"] << " 发送文件: " 
//         //             << file["filename"] << " (" 
//         //             << (file["size"].get<int>() / 1024) << "KB)";
//         //     std::cout << "\n输入 /receive " << file["id"] << " 接收文件";
            
//         //     // 标记文件通知为已接收
//         //     json ack;
//         //     ack["type"] = "ack_private_file";
//         //     ack["file_id"] = file["id"];
//         //     ack["user"] = currentUser;
//         //     sendRequest(ack);
//         // }
//     }
    
//     // json sendRequest(const json& req)
//     // {
//     //     std::string requestStr = req.dump();
//     //     send(sock, requestStr.c_str(), requestStr.size(), 0);

//     //     char buffer[4096] = {0};
//     //     recv(sock, buffer, sizeof(buffer), 0);
//     //     return json::parse(buffer);
//     // }
//     bool isValidFriend(const std::string& friendName) 
//     {
//         if (friendName == currentUser) {
//             return false;
//         }
        
//         //向服务器查询
//         json req;
//         req["type"] = "check_friend_valid";
//         req["user"] = currentUser;
//         req["friend"] = friendName;
//         json res = sendRequest(req);
//         bool isValid = false;
//         if (res["success"]) {
//             isValid = res["valid"];
//         } else {
//             // 查询失败时保守处理
//             std::cerr << "✘ 好友验证失败: " << res["message"] << std::endl;
//             isValid = false;
//         }
//         return isValid;
//     }

// };
// #include <iostream>
// #include <string>
// #include <unordered_map>
// #include <queue>
// #include <mutex>
// #include <condition_variable>
// #include <thread>
// #include <mysql/mysql.h>
// #include <nlohmann/json.hpp>
// #include <sys/epoll.h>
// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <unistd.h>
// #include<fcntl.h>
// #include"threadpool.h"
// #include </usr/include/mysql_driver.h>       // MySQL驱动头文件
// #include <mysql_connection.h>  // 连接类头文件
// #include <cppconn/prepared_statement.h> // 预处理语句
// #include </usr/include/x86_64-linux-gnu/curl/curl.h>
// #include <time.h>


// using namespace std;
// using json = nlohmann::json;
// using namespace sql;

// // 数据库配置
// const string DB_HOST = "tcp://127.0.0.1:3306";
// const string DB_USER = "chatuser";   // 数据库账户名
// const string DB_PASS = "123";  // 数据库账户密码
// const string DB_NAME = "chat";
// class Chat
// {
//     public:
//     mutex online_mutex;
//     unordered_map<string, int> online_users; // 在线用户表
//     public:
//     Connection* getDBConnection() {
//         try {
//             sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
//             if(!driver) {
//                 cerr << "获取驱动实例失败" << endl;
//                 return nullptr;
//             }

//             sql::Connection *con = driver->connect(DB_HOST, DB_USER, DB_PASS);
//             if(!con) {
//                 cerr << "创建连接失败" << endl;
//                 return nullptr;
//             }

//             con->setSchema(DB_NAME);
//             cout << "数据库连接成功" << endl;
//             return con;
//         } catch (sql::SQLException &e) {
//             cerr << "MySQL错误[" << e.getErrorCode() << "]: " << e.what() << endl;
//         } catch (...) {
//             cerr << "未知错误" << endl;
//         }
//         return nullptr;
//     }
//     // 添加用户到在线列表
//     void userOnline(const std::string& username, int socket) {
//         std::lock_guard<std::mutex> lock(online_mutex);
//         online_users[username] = socket;
//     }
//     void handleCheckFriendValid(int fd, const json& req) 
//     {
//         string user = req["user"];
//         string friendName = req["friend"];
        
//         unique_ptr<sql::Connection> con(getDBConnection());
//         json response;
        
//         try {
            
//             unique_ptr<sql::PreparedStatement> blockStmt(
//                 con->prepareStatement(
//                     "SELECT id FROM blocks "
//                     "WHERE (user = ? AND blocked_user = ?) OR "
//                     "(user = ? AND blocked_user = ?)"
//                 )
//             );
//             blockStmt->setString(1, user);
//             blockStmt->setString(2, friendName);
//             blockStmt->setString(3, friendName);
//             blockStmt->setString(4, user);
            
//             unique_ptr<sql::ResultSet> blockRes(blockStmt->executeQuery());
//             if (blockRes->next()) {
//                 response["valid"] = false;
//                 response["reason"] = "存在屏蔽关系";
//                 response["success"] = true;
//                 send(fd, response.dump().c_str(), response.dump().size(), 0);
//                 return;
//             }
            
//             // 所有检查通过
//             response["valid"] = true;
//             response["reason"] = "有效好友";
//             response["success"] = true;
            
//         } catch (sql::SQLException &e) {
//             response["success"] = false;
//             response["message"] = "数据库错误: " + string(e.what());
//         }
        
//         send(fd, response.dump().c_str(), response.dump().size(), 0);
//     }
//     void handlePrivateMessage(int fd, const json& req)
//     {
//         string sender = req["sender"];
//         string recipient = req["recipient"];
//         string message = req["message"];
//         time_t timestamp = req["timestamp"];
//         unique_ptr<sql::Connection> con(getDBConnection());
//         // 3. 存储消息
//         unique_ptr<sql::PreparedStatement> msgStmt(
//             con->prepareStatement(
//                 "INSERT INTO private_messages "
//                 "(sender, recipient, message, timestamp) "
//                 "VALUES (?, ?, ?, ?)"
//             )
//         );
//         msgStmt->setString(1, sender);
//         msgStmt->setString(2, recipient);
//         msgStmt->setString(3, message);
//         msgStmt->setUInt64(4, timestamp);
//         msgStmt->executeUpdate();
//         unique_ptr<sql::Statement> idStmt(con->createStatement());
//         unique_ptr<sql::ResultSet> idRes(
//             idStmt->executeQuery("SELECT LAST_INSERT_ID() AS id")
//         );
        
//         uint32_t msgId = 0;
//         if (idRes->next()) {
//             msgId = idRes->getInt("id");
//         }
//         json response;
//         bool isOnline = false;
//         {
//             lock_guard<mutex> lock(online_mutex);
//             auto it = online_users.find(recipient);
//             if (it != online_users.end()) {
//                 isOnline = true;
                
//                 // 实时推送消息
//                 json notif = {
//                     {"type", "private_message"},
//                     {"sender", sender},
//                     {"message", message},
//                     {"timestamp", timestamp},
//                     {"message_id", msgId}
//                 };
//                 send(it->second, notif.dump().c_str(), notif.dump().size(), 0);
//             }
//         }
        
//         // 6. 构建响应
//         response["success"] = true;
//         response["message_id"] = msgId;
//         response["delivered"] = isOnline;
//         send(fd, response.dump().c_str(), response.dump().size(), 0);

//     }
//     void handleGetNewPrivateMessages(int fd, const json& req)
//     {
//         string user = req["user"];
//         string friendName = req["friend"];
        
//         unique_ptr<sql::Connection> con(getDBConnection());
//         json response;
//         //json messages = json::array();
//         vector<json> messages;
//         response["success"] = true;
//         try {
//         // 1. 获取未送达消息
//         unique_ptr<sql::PreparedStatement> stmt(
//             con->prepareStatement(
//                 "SELECT id, sender, message, timestamp "
//                 "FROM private_messages "
//                 "WHERE recipient = ? AND sender = ? AND delivered = 0 "
//                 "ORDER BY timestamp ASC"
//             )
//         );
//         stmt->setString(1, user);
//         stmt->setString(2, friendName);
        
//         unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        
//         while (res->next()) {
//             json msg = {
//                 {"id", res->getInt("id")},
//                 {"sender", res->getString("sender")},
//                 {"message", res->getString("message")},
//                 {"timestamp", res->getUInt64("timestamp")}
//             };
//             messages.push_back(msg);
//         }
        
//         // 2. 获取未通知的文件
//         // json files = json::array();
//         // unique_ptr<sql::PreparedStatement> fileStmt(
//         //     con->prepareStatement(
//         //         "SELECT f.id, f.sender, f.filename, f.size, f.timestamp "
//         //         "FROM files f "
//         //         "LEFT JOIN file_notifications n ON f.id = n.file_id AND n.user = ? "
//         //         "WHERE f.recipient = ? AND f.sender = ? AND n.notified IS NULL "
//         //         "AND f.status = 'completed'"
//         //     )
//         // );
//         // fileStmt->setString(1, user);
//         // fileStmt->setString(2, user);
//         // fileStmt->setString(3, friendName);
        
//         // unique_ptr<sql::ResultSet> fileRes(fileStmt->executeQuery());
//         // while (fileRes->next()) {
//         //     json file = {
//         //         {"id", fileRes->getInt("id")},
//         //         {"sender", fileRes->getString("sender")},
//         //         {"filename", fileRes->getString("filename")},
//         //         {"size", fileRes->getUInt64("size")},
//         //         {"timestamp", fileRes->getUInt64("timestamp")}
//         //     };
//         //     files.push_back(file);
//         // }
        
//         // 3. 构建响应
//         response["success"] = true;
//         response["messages"] = messages;
//         //response["files"] = files;
        
//         } catch (sql::SQLException &e) {
//             response["success"] = false;
//             response["message"] = "数据库错误: " + string(e.what());
//         }
//         send(fd, response.dump().c_str(), response.dump().size(), 0);
//     }
//     void handleAckPrivateMessage(int fd, const json& req)
//     {
//         uint32_t messageId = req["message_id"];
//         string user = req["user"];
        
//         unique_ptr<sql::Connection> con(getDBConnection());
//         if (!con) {
//             return; // 不需要响应
//         }
//         unique_ptr<sql::PreparedStatement> stmt(
//             con->prepareStatement(
//                 "UPDATE private_messages "
//                 "SET delivered = 1 "
//                 "WHERE id = ? AND recipient = ?"
//             )
//         );
//         stmt->setInt(1, messageId);
//         stmt->setString(2, user);
//         stmt->executeUpdate();
//     }
    
// };

 // else if (type == "history_response") 
                    // {
                    //     // 处理历史消息响应
                    //     lock_guard<mutex> lock(outputMutex); // 保护整个输出块
                        
                    //     if (message.value("success", false)) {
                    //         auto messages = message["messages"];
                    //         cout << "\n===== 聊天历史 =====" << endl;
                            
                    //         for (const auto& msg : messages) {
                    //             string sender = msg.value("sender", "");
                    //             string text = msg.value("message", "");
                    //             time_t timestamp = msg.value("timestamp", time(nullptr));
                                
                    //             string timeStr = "Unknown time";
                    //             tm* localTime = localtime(&timestamp);
                    //             if (localTime) {
                    //                 char timeBuffer[80];
                    //                 strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
                    //                 timeStr = string(timeBuffer);
                    //             }
                                
                    //             cout << "[" << timeStr << "] " << sender << ": " << text << endl;
                    //         }
                            
                    //         cout << "====================" << endl;
                    //     } else {
                    //         cerr << "获取历史记录失败: " 
                    //             << message.value("message", "未知错误") << endl;
                    //     }
                        
                    //     // 如果正在聊天会话中，重新显示输入提示
                    //     if (inChatSession) {
                    //         cout << "> " << inputBuffer << flush;
                    //     }
                    // }
                    // else if (type == "offline_notification") 
                    // {
                    //     // 处理离线消息通知
                    //     lock_guard<mutex> lock(outputMutex); // 保护输出
                        
                    //     string sender = message.value("sender", "");
                    //     int count = message.value("count", 0);
                        
                    //     cout << "\n您有 " << count << " 条来自 " << sender << " 的未读消息" << endl;
                    //     cout << "输入 /history " << sender << " 查看历史消息" << endl;
                        
                    //     // 如果正在聊天会话中，重新显示输入提示
                    //     if (inChatSession) {
                    //         cout << "> " << inputBuffer << flush;
                    //     }
                    // }
                    // else {
                    //     lock_guard<mutex> lock(outputMutex);
                    //     cerr << "未知消息类型: " << type << endl;
                    // }
// void checkUnreadMessages(int fd, const json& req)
    // {
    //     string username= req["user"];
    //     unique_ptr<sql::Connection> con(getDBConnection());
    //    // 3. 存储消息
    //     unique_ptr<sql::PreparedStatement> msgStmt(
    //         con->prepareStatement(
    //             "SELECT id, sender, message, timestamp "
    //             "FROM messages "
    //             "WHERE receiver = ? AND delivered = 0 "
    //         )
    //     );
    //     msgStmt->setString(1, username);
    //     unique_ptr<sql::ResultSet> res(msgStmt->executeQuery());
    //     // if(!res->next())
    //     // {
    //     //     std::cout << "没有未读消息" << std::endl;
    //     // }
    //     // else
    //     // {
    //     //     std::cout << "\n===== 未读消息 =====" << std::endl;
    //     //     do {
    //     //         std::string sender = res->getString("sender");
    //     //         sql::SQLString timestampStr = res->getString("timestamp");
                
    //     //         // 转换时间戳为可读格式
    //     //         std::tm tm = {};
    //     //         std::istringstream ss(timestampStr);
    //     //         ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    //     //         time_t timestamp = std::mktime(&tm);
                
    //     //         // 格式化时间
    //     //         char timeBuffer[80];
    //     //         std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", std::localtime(&timestamp));
                
    //     //         // 只显示发送者信息，不显示消息内容
    //     //         std::cout << "[" << timeBuffer << "] " << sender << " 给您发了消息" << std::endl;
                
    //     //     } while (res->next());
    //     // }
    //     json response;
    //     //response["type"] = "unread_messages";
    //     //response["user"] = username;
    //     if (!res->next()) 
    //     {
    //         // 没有未读消息
    //         response["success"] = false;
    //         // response["message"] = "没有未读消息";
    //         // response["count"] = 0;
            
    //         // response["messages"] = json::array(); // 空数组
            
    //         // 发送响应
    //         string responseStr = response.dump();
    //         send(fd, responseStr.c_str(), responseStr.size(), 0);
    //         return;
    //     }
    //     json messagesArray = json::array();
    //     int messageCount = 0;

    //     // 重置结果集指针到开头
    //     res->beforeFirst();

    //     // 遍历所有未读消息
    //     while (res->next()) {
    //         json messageObj;
    //         //messageObj["id"] = res->getInt("id");
    //         messageObj["sender"] = res->getString("sender");
    //         //messageObj["message"] = res->getString("message");
    //         messageObj["timestamp"] = res->getString("timestamp");
            
    //         messagesArray.push_back(messageObj);
    //         //messageCount++;
    //     }

    //     // 设置响应字段
    //     response["success"] = true;
    //     //response["count"] = messageCount;
    //     response["messages"] = messagesArray;

    //     // 序列化响应为JSON字符串
    //     string responseStr = response.dump();

    //     // 发送响应给客户端
    //     if (send(fd, responseStr.c_str(), responseStr.size(), 0) < 0) {
    //         cerr << "发送未读消息响应失败: " << strerror(errno) << endl;
    //     } else {
    //         cout << "已发送 " << messageCount << " 条未读消息给用户 " << username << endl;
    //     }
    //}
    // void queryChatHistory(const string& friendName)
    // {
    //     json request = {
    //         {"type", "get_chat_history"},
    //         {"user", currentUser},
    //         {"friend", friendName},
    //     };
    //     std::string requestStr = request.dump();
    //     send(sock, requestStr.c_str(), requestStr.size(), 0);
    //     char buffer[4096] = {0};
    //     recv(sock, buffer, 4096, 0);
    //     json response= json::parse(buffer);
    //     if(!response["success"])
    //     {
    //         cout<<"无历史聊天记录"<<endl;
    //     }
    //     else
    //     {
    //         auto messages = response["messages"];
    //         for (const auto& msg : messages) 
    //         {
    //             string timestampStr = msg.value("timestamp", "");
                
    //             // 转换时间戳为可读格式
    //             struct tm tm = {};
    //             strptime(timestampStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm);
    //             time_t timestamp = mktime(&tm);
                
    //             char timeBuffer[80];
    //             strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localtime(&timestamp));
                
    //             // 确定消息方向
    //             string sender = msg["sender"];
    //             string arrow = (sender == currentUser) ? "->" : "<-";
    //             string displayName = (sender == currentUser) ? "你" : sender;
                
    //             cout << "[" << timeBuffer << "] " 
    //                 << displayName << " " << arrow << " "
    //                 << msg["message"] << endl;
    //         }
            
    //         cout << "========================" << endl;
    //     }

    // }
    // void checkUnreadMessages(string currentUser,int sockfd) 
    // {
    //     json request = {
    //         {"type", "get_unread_messages"},
    //         {"user", currentUser}
    //     };

    //     int fd=sockfd;
    //     json req=sendReq(request,fd);
    //     if(req["success"])
    //     {
    //         cout<< "\n===== 未读消息 =====" << std::endl;
    //         for (const auto& msg : req["messages"]) 
    //         {
    //             time_t timestamp = msg["timestamp"];
    //             tm* localTime = localtime(&timestamp);
    //             char timeBuffer[80];
    //             strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M", localTime);
                
    //             cout << "[" << timeBuffer << "] " <<"有未读消息来自"
    //                 << msg["sender"] << endl;
    //                 //<< //msg["message"] << endl;
    //         }
            
    //         cout << "=======================" << endl;

    //     }
    //     else
    //     {
    //         cout<< "\n===== 无未读消息 =====" << std::endl;
    //     }
        
    // }


    // while (inChatSession && running) {
        //     // 读取输入字符
        //     if (read(STDIN_FILENO, &c, 1) > 0) {
        //         if (c == '\n' || c == '\r') {
        //             cout << "\r\033[2K" << "> " << flush;
        //             // 处理完整输入行
        //             if (!inputBuffer.empty()) {
        //                 if (inputBuffer == "/exit") {
        //                     break;
        //                 } 
        //                 else {
        //                     sendMessage(friendName, inputBuffer);
        //                 }
        //                 inputBuffer.clear();
                        
        //                 lock_guard<mutex> lock(outputMutex);
        //                 cout << "> " << flush;
        //             }
        //         } 
        //         // 处理退格
        //         else if (c == 127 || c == 8) { // 退格键
        //             if (!inputBuffer.empty()) {
        //                 inputBuffer.pop_back();
        //                 lock_guard<mutex> lock(outputMutex);
        //                 cout << "\b \b" << flush; // 擦除最后一个字符
        //             }
        //         } 
        //         // 处理普通字符
        //         else  
        //         //(c >= 32 && c <= 126)
        //          { // 可打印字符
        //             inputBuffer += c;
        //             lock_guard<mutex> lock(outputMutex);
        //             cout << c << flush;
        //         }
        //     }
            
        //     // 短暂休眠避免过度占用CPU
        //     this_thread::sleep_for(chrono::milliseconds(50));
        // }
        // vector<char> inputBuffer; // 输入缓冲区
        // // string str;
        // // str.resize(1024);
