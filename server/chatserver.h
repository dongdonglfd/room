#include <iostream>
#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <mysql/mysql.h>
#include <nlohmann/json.hpp>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include"threadpool.h"
#include </usr/include/mysql_driver.h>       // MySQL驱动头文件
#include <mysql_connection.h>  // 连接类头文件
#include <cppconn/prepared_statement.h> // 预处理语句
#include </usr/include/x86_64-linux-gnu/curl/curl.h>
#include <time.h>
#include <fstream>
#include <openssl/sha.h>  // 提供 SHA256_CTX 定义
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <openssl/buffer.h>
#include <sys/stat.h>


using namespace std;
using json = nlohmann::json;
using namespace sql;

// 数据库配置
const string DB_HOST = "tcp://127.0.0.1:3306";
const string DB_USER = "chatuser";   // 数据库账户名
const string DB_PASS = "123";  // 数据库账户密码
const string DB_NAME = "chat";
struct Message {
    std::string sender;
    std::string receiver;
    std::string content;
    time_t timestamp;
    uint64_t message_id;
    bool delivered = false;
};
 unordered_map<string, int> online_users; // 在线用户表
class Chat
{
    public:
     mutex online_mutex;
    //  unordered_map<string, int> online_users; // 在线用户表
     //std::unordered_map<std::string, UserSession> online_users;   // 在线用户
    std::mutex online_users_mutex;
    
    std::map<std::string, std::deque<Message>> messages_store;   // 按会话ID存储的消息
    std::map<std::string, uint64_t> unread_counts;               // 用户未读消息计数
    std::mutex data_mutex;

    // 连接管理
    std::unordered_map<int, std::string> connection_user_map;    // socket->username
    std::mutex connection_mutex;
    public:
    void sendResponse(int fd, const json& response) {
    string responseStr = response.dump();
    send(fd, responseStr.c_str(), responseStr.size(), 0);
}
    Connection* getDBConnection() {
        try {
            sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
            if(!driver) {
                cerr << "获取驱动实例失败" << endl;
                return nullptr;
            }

            sql::Connection *con = driver->connect(DB_HOST, DB_USER, DB_PASS);
            if(!con) {
                cerr << "创建连接失败" << endl;
                return nullptr;
            }

            con->setSchema(DB_NAME);
            cout << "数据库连接成功" << endl;
            return con;
        } catch (sql::SQLException &e) {
            cerr << "MySQL错误[" << e.getErrorCode() << "]: " << e.what() << endl;
        } catch (...) {
            cerr << "未知错误" << endl;
        }
        return nullptr;
    }
    // 添加用户到在线列表
    void userOnline(const std::string& username, int socket) {
        std::lock_guard<std::mutex> lock(online_mutex);
        online_users[username] = socket;
    }
    void handleCheckFriendValid(int fd, const json& req) 
    {
        // string user = req["user"];
        // string friendName = req["friend"];
        
        // unique_ptr<sql::Connection> con(getDBConnection());
        // json response;
        
        // try {
            
        //     unique_ptr<sql::PreparedStatement> blockStmt(
        //         con->prepareStatement(
        //             "SELECT id FROM blocks "
        //             "WHERE (user = ? AND blocked_user = ?) OR "
        //             "(user = ? AND blocked_user = ?)"
        //         )
        //     );
        //     blockStmt->setString(1, user);
        //     blockStmt->setString(2, friendName);
        //     blockStmt->setString(3, friendName);
        //     blockStmt->setString(4, user);
            
        //     unique_ptr<sql::ResultSet> blockRes(blockStmt->executeQuery());
        //     if (blockRes->next()) {
        //         response["valid"] = false;
        //         response["reason"] = "存在屏蔽关系";
        //         response["success"] = true;
        //         send(fd, response.dump().c_str(), response.dump().size(), 0);
        //         return;
        //     }
            
        //     // 所有检查通过
        //     response["valid"] = true;
        //     response["reason"] = "有效好友";
        //     response["success"] = true;
            
        // } catch (sql::SQLException &e) {
        //     response["success"] = false;
        //     response["message"] = "数据库错误: " + string(e.what());
        // }
        
        // send(fd, response.dump().c_str(), response.dump().size(), 0);
    }
    void handlePrivateMessage(int fd,const json& request)
    {
        Message msg;
        msg.sender = request["sender"];
        msg.receiver = request["recipient"];
        msg.content = request["message"];
        msg.timestamp = static_cast<time_t>(request["timestamp"]);
        msg.message_id = time(nullptr) * 1000 + rand() % 1000; // 简单实现的消息ID
        
        unique_ptr<sql::Connection> con(getDBConnection());
       // 3. 存储消息
        unique_ptr<sql::PreparedStatement> msgStmt(
            con->prepareStatement(
                "INSERT INTO private_messages "
                "(sender, recipient, message, timestamp) "
                "VALUES (?, ?, ?, ?)"
            )
        );
        msgStmt->setString(1, msg.sender);
        msgStmt->setString(2, msg.receiver);
        msgStmt->setString(3, msg.content);
        msgStmt->setUInt64(4, msg.timestamp);
        msgStmt->executeUpdate();
        unique_ptr<sql::Statement> idStmt(con->createStatement());
        unique_ptr<sql::ResultSet> idRes(
            idStmt->executeQuery("SELECT LAST_INSERT_ID() AS id")
        );
        
        uint32_t msgId = 0;
        if (idRes->next()) {
            msg.message_id = idRes->getInt("id");
        }
        // 尝试直接发送给接收者
        bool delivered = false;
        {
            std::lock_guard<std::mutex> lock(online_users_mutex);
            auto it = online_users.find(msg.receiver);
            if (it != online_users.end()) {
                json realtime_msg;
                realtime_msg["type"] = "private_message";
                realtime_msg["message_id"] = msg.message_id;
                realtime_msg["sender"] = msg.sender;
                realtime_msg["message"] = msg.content;
                realtime_msg["timestamp"] = msg.timestamp;
                
                sendResponse(it->second, realtime_msg);
                delivered = true;
            }
        }
    }
    void handleAckPrivateMessage(int fd, const json& req)
    {   
        //uint32_t messageId = req["message_id"];
        string user = req["user"];
        string sender = req["friend"];

        unique_ptr<sql::Connection> con(getDBConnection());
        if (!con) {
            return; // 不需要响应
        }
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "UPDATE private_messages "
                "SET delivered = 1 "
                //"WHERE id = ? AND recipient = ?"
                "WHERE sender = ? AND recipient = ?"
            )
        );
        //stmt->setInt(1, messageId);
        stmt->setString(1, sender);
        stmt->setString(2, user);
        stmt->executeUpdate();
    }   
    void checkUnreadMessages(int fd, const json& req)
    {

        // 确保请求包含用户字段
        if (!req.contains("user") || !req["user"].is_string()) {
            json errorResponse = {
                {"type", "error"},
                {"message", "无效请求: 缺少用户名字段"}
            };
            string responseStr = errorResponse.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
            return;
        }

        string username = req["user"];
        unique_ptr<sql::Connection> con(getDBConnection());
        
        // 检查数据库连接
        if (!con) {
            json errorResponse = {
                {"type", "error"},
                {"message", "数据库连接失败"}
            };
            string responseStr = errorResponse.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
            return;
        }

        try {
            // 查询未读消息
            unique_ptr<sql::PreparedStatement> msgStmt(
                con->prepareStatement(
                    "SELECT id, sender, message, timestamp "
                    "FROM private_messages "
                    "WHERE recipient  = ? AND delivered = 0 "
                    "ORDER BY timestamp ASC"
                )
            );
            msgStmt->setString(1, username);
            unique_ptr<sql::ResultSet> res(msgStmt->executeQuery());
            
            // 构建响应对象
            json response;
            response["type"] = "unread_messages";
            response["user"] = username;
            int messageCount = 0;
            // 检查是否有未读消息
            if (!res->next()) {
                
            json messagesArray = json::array();
                // 没有未读消息
                response["success"] = true;
                response["message"] = "没有未读消息";
                response["count"] = 0;
                response["messages"] = json::array(); // 空数组
            } else {
                // 有未读消息
                json messagesArray = json::array();
                

                // 重置结果集指针到开头
                res->beforeFirst();

                // 遍历所有未读消息
                while (res->next()) {
                    json messageObj;
                    messageObj["type"]="private";
                    messageObj["sender"] = res->getString("sender");
                    messageObj["timestamp"] = res->getInt64("timestamp");
                    
                    messagesArray.push_back(messageObj);
                    messageCount++;
                }

                response["success"] = true;
                response["count"] = messageCount;
                response["messages"] = messagesArray;
            }
             
            // response["success"] = true;
            // response["messages"] = messagesArray; 
            // 序列化响应为JSON字符串
            string responseStr = response.dump();
            
            // 添加调试输出
            cout << "发送响应: " << responseStr << endl;

            // 发送响应给客户端
            if (send(fd, responseStr.c_str(), responseStr.size(), 0) < 0) {
                cerr << "发送未读消息响应失败: " << strerror(errno) << endl;
            } else {
                cout << "已发送未读消息给用户 " << username << endl;
            }
            
        } catch (sql::SQLException &e) {
            // 数据库错误处理
            json errorResponse = {
                {"type", "error"},
                {"message", "数据库查询失败: " + string(e.what())}
            };
            string responseStr = errorResponse.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
        } catch (const exception& e) {
            // 其他异常处理
            json errorResponse = {
                {"type", "error"},
                {"message", "处理请求失败: " + string(e.what())}
            };
            string responseStr = errorResponse.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
        }
    }
    
    void handleGetFriendUnreadMessages(int fd, const json& req)
    {
        string username = req["user"];
        string friendName = req["friend"];
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> msgStmt(
            con->prepareStatement(
                "SELECT id, message, timestamp "
                "FROM private_messages "
                "WHERE recipient = ? AND sender = ? AND delivered = 0 "
                "ORDER BY timestamp ASC"
            )
        );
        msgStmt->setString(1, username);
        msgStmt->setString(2, friendName);
        unique_ptr<sql::ResultSet> res(msgStmt->executeQuery());
        json response;
        response["type"] = "friend_unread_messages";
        response["user"] = username;
        response["friend"] = friendName;
        if (!res->next()) {
            // 没有未读消息
            response["success"] = false;
            response["message"] = "没有未读消息";
            response["count"] = 0;
            response["messages"] = json::array();
            
        }
        else
        {
            json messagesArray = json::array();
            // 重置结果集指针到开头
            res->beforeFirst();
            // 遍历所有未读消息
            while (res->next()) {
                json messageObj;
                messageObj["id"] = res->getInt("id");
                messageObj["message"] = res->getString("message");
                messageObj["timestamp"] = res->getInt64("timestamp");
                
                messagesArray.push_back(messageObj);
            }

            response["success"] = true;
            response["messages"] = messagesArray;
        }

        // 发送响应
        string responseStr = response.dump();
        send(fd, responseStr.c_str(), responseStr.size(), 0);
    }
    // void handleChatHistoryRequest(int fd, const json& request)
    // {
    //     string user = request["user"];
    //     string friendName = request["friend"];
    //     unique_ptr<sql::Connection> con(getDBConnection());
    //     unique_ptr<sql::PreparedStatement> msgStmt(
    //         con->prepareStatement(
    //             "SELECT sender, recipient, message, timestamp "
    //             "FROM private_messages "
    //             "WHERE (sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?) "
    //             "ORDER BY timestamp ASC"// 按时间升序（从旧到新）
    //         )
    //     );
    //     msgStmt->setString(1, user);
    //     msgStmt->setString(2, friendName);
    //     msgStmt->setString(3, friendName);
    //     msgStmt->setString(4, user);
    //     unique_ptr<sql::ResultSet> res(msgStmt->executeQuery());
    //     json response;
    //     if (!res->next()) {
    //         // 没有未读消息
    //         response["success"] = false;
    //         response["messages"] = json::array();
    //     }
    //     else
    //     {
    //         json messagesArray = json::array();
    //         // 重置结果集指针到开头
    //         res->beforeFirst();
    //         // 遍历所有未读消息
    //         while (res->next()) {
    //             json messageObj;
    //             messageObj["sender"] = res->getString("sender");
    //             messageObj["message"] = res->getString("message");
    //             messageObj["timestamp"] = res->getString("timestamp");
    //             messagesArray.push_back(messageObj);
    //         }

    //         response["success"] = true;
    //         response["messages"] = messagesArray;
    //     }
    //     // 发送响应
    //     string responseStr = response.dump();
    //     send(fd, responseStr.c_str(), responseStr.size(), 0);
    // }
    void handleChatHistoryRequest(int fd, const json& request)
    {
        // 验证请求参数
        if (!request.contains("user") || !request["user"].is_string() ||
            !request.contains("friend") || !request["friend"].is_string()) {
            json error = {
                {"type", "error"},
                {"message", "无效请求: 缺少必要字段"}
            };
            send(fd, error.dump().c_str(), error.dump().size(), 0);
            return;
        }
        
        string user = request["user"];
        string friendName = request["friend"];
        
        unique_ptr<sql::Connection> con(getDBConnection());
        if (!con) {
            json error = {
                {"type", "error"},
                {"message", "数据库连接失败"}
            };
            send(fd, error.dump().c_str(), error.dump().size(), 0);
            return;
        }
        
        try {
            // 准备SQL查询
            unique_ptr<sql::PreparedStatement> msgStmt(
                con->prepareStatement(
                    "SELECT sender, recipient, message, timestamp "
                    "FROM private_messages "
                    "WHERE (sender = ? AND recipient = ?) OR (sender = ? AND recipient = ?) "
                    "ORDER BY timestamp ASC" // 按时间升序（从旧到新）
                )
            );
            msgStmt->setString(1, user);
            msgStmt->setString(2, friendName);
            msgStmt->setString(3, friendName);
            msgStmt->setString(4, user);
            
            // 执行查询
            unique_ptr<sql::ResultSet> res(msgStmt->executeQuery());
            
            // 构建响应
            json response;
            response["user"] = user;
            response["friend"] = friendName;
            
            // 检查是否有消息
            if (!res->next()) {
                // 没有消息
                response["success"] = true;
                response["count"] = 0;
                response["messages"] = json::array();
            } else {
                // 有消息
                json messagesArray = json::array();
                int messageCount = 0;
                
                // 重置结果集指针到开头
                res->beforeFirst();
                
                // 遍历所有消息
                while (res->next()) {
                    json messageObj;
                    
                    // 获取字段值
                    string sender = res->getString("sender");
                    string recipient = res->getString("recipient");
                    string message = res->getString("message");
                    time_t timestamp = res->getInt64("timestamp"); // 关键修复：使用getInt64获取时间戳
                    
                    // 构建消息对象
                    messageObj["sender"] = sender;
                    messageObj["recipient"] = recipient;
                    messageObj["message"] = message;
                    messageObj["timestamp"] = timestamp; // 存储为整数
                    
                    messagesArray.push_back(messageObj);
                    
                }
                
                response["success"] = true;
                response["messages"] = messagesArray;
            }
            
            // 发送响应
            string responseStr = response.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
            
        } catch (sql::SQLException &e) {
            // 数据库错误处理
            json error = {
                {"type", "error"},
                {"message", "数据库查询失败: " + string(e.what())}
            };
            string responseStr = error.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
        } catch (const exception& e) {
            // 其他异常处理
            json error = {
                {"type", "error"},
                {"message", "处理请求失败: " + string(e.what())}
            };
            string responseStr = error.dump();
            send(fd, responseStr.c_str(), responseStr.size(), 0);
        }
    }
};
