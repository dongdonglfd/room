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
// #include <fcntl.h>
// #include"threadpool.h"
// #include </usr/include/mysql_driver.h>       // MySQL驱动头文件
// #include <mysql_connection.h>  // 连接类头文件
// #include <cppconn/prepared_statement.h> // 预处理语句
// #include </usr/include/x86_64-linux-gnu/curl/curl.h>
// #include <time.h>
// #include <fstream>
// #include <openssl/sha.h>  // 提供 SHA256_CTX 定义
// #include <openssl/bio.h>
// #include <openssl/evp.h>
// #include <iomanip>
// #include <sstream>
// #include <openssl/buffer.h>
// #include <sys/stat.h>
#include "chatserver.h"



class groupchat:public Chat
{
    Chat group;
private:
    
    int sock;
    mutex groupMutex;
    mutex session;
    json sendRequest(const json& request) {
        // 序列化请求为JSON字符串
        string requestStr = request.dump();
        
        //发送请求到服务器
            
            if (send(sock, requestStr.c_str(), requestStr.size(), 0) < 0) {
                cerr << "发送请求失败" << endl;
                return {{"success", false}, {"message", "发送请求失败"}};
            }
        return {{"success", true}};
    }
public:
    void handleGroupMessage(int client_sock, const json& data)
    {   
        sock=client_sock;
       string sender = data["sender"];
       int groupid = data["groupID"];
       string message = data["message"]; 
       time_t timestamp = static_cast<time_t>(data["timestamp"]);
       unique_ptr<sql::Connection> con(getDBConnection());
       //判读sender是否在群内
       unique_ptr<sql::PreparedStatement> checkStmt(
            con->prepareStatement(
                "SELECT COUNT(*) AS count FROM group_members "
                // "SELECT * FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        checkStmt->setInt(1, groupid);
        checkStmt->setString(2, sender);

        unique_ptr<sql::ResultSet> res(checkStmt->executeQuery());
        
        if (res->next()) {
            int count = res->getInt("count");
            if (count == 0) {
                cout<<"5555"<<endl;
                // 发送者不在群组内
                json errorResponse = {
                    {"success", false},
                    {"message", "您不在该群组中，无法发送消息"}
                };
                sendRequest(errorResponse);
                return;
            }
        }
        
       unique_ptr<sql::PreparedStatement> msgStmt(
            con->prepareStatement(
                "INSERT INTO group_messages "
                "(group_id, sender, message, timestamp) "
                "VALUES (?, ?, ?, ?)"
            )
        );
        msgStmt->setInt(1, groupid);
        msgStmt->setString(2, sender);
        msgStmt->setString(3, message);
        msgStmt->setUInt64(4, timestamp);
        msgStmt->executeUpdate(); 
        
        json broadcast = {
            {"type", "group_messages"},
            {"group_id", groupid},
            {"sender", sender},
            {"message", message},
            {"timestamp", timestamp}
        };
        broadcastToGroup(groupid, broadcast, sender);
        json response = {
            {"type", "group_message_ack"},
            {"success", true},
            {"group_id", groupid},
            {"timestamp", timestamp}
        };
        sendRequest(response);


    }
    void broadcastToGroup(int group_id, const json& message, const string& excludeUser = "")
    {   
        lock_guard<mutex> lock(groupMutex);
        unique_ptr<sql::Connection> con(getDBConnection());
        
        // 准备查询群组成员的SQL语句
        unique_ptr<sql::PreparedStatement> membersStmt(
            con->prepareStatement(
                "SELECT user "
                "FROM group_members "
                "WHERE group_id = ?"
            )
        );
        membersStmt->setInt(1, group_id);
        
        // 执行查询
        unique_ptr<sql::ResultSet> membersRes(membersStmt->executeQuery());
        
        // 存储群组成员列表
        vector<string> members;
        while (membersRes->next()) {
            string username = membersRes->getString("user");
            members.push_back(username);
        }
            for (const auto& [username, id] : online_users) {
            cout << "Username: " << username << ", ID: " << id << endl;
        }
        for (const string& member : members) 
        {   
            if (member == excludeUser)
            {cout<<"34567"<<endl;
                continue;
            } 
            bool isOnline = false;
            int fd=-1;
            {   cout<<"876"<<endl;
                lock_guard<mutex> lock(session);
                // if (userSessions.find(member) != userSessions.end() && 
                //     userSessions[member].isOnline) {
                //     isOnline = true;
                //     sock = userSessions[member].sock;
                // }
                auto it = online_users.find(member);
                if (it != online_users.end())
                {
                    isOnline = true;
                    fd = it->second;
                    cout<<"qq"<<fd<<endl;
                }
                else{
                    cout<<"456"<<endl;
                }
                
            }
            
            if (isOnline) {
                cout<<"987"<<endl;
                // 用户在线，直接发送
                sendResponse(fd, message);
            } else {
                // 用户离线，存储消息
                storeOfflineMessage(member, group_id, message);
            }
        }


    }
    void storeOfflineMessage(const string& recipient, int group_id, const json& message)
    {
        cout<<"store"<<endl;
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> storeStmt(
            con->prepareStatement(
                "INSERT INTO offline_messages "
                "(recipient_username , group_id, sender_username , message, timestamp) "
                "VALUES (?, ?, ?, ?, ?)"
            )
        );
        storeStmt->setString(1, recipient); 
        storeStmt->setInt(2, group_id);
        storeStmt->setString(3, message["sender"].get<string>());
        storeStmt->setString(4, message["message"].get<string>());
        storeStmt->setUInt64(5, message["timestamp"].get<uint64_t>());
        storeStmt->executeUpdate();
        cout<<"456"<<endl;
    }
    void checkgroupUnreadMessages(int fd, const json& req)
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
        cout<<username<<endl;
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
                    "SELECT group_id, sender_username, message, timestamp "
                    "FROM offline_messages "
                    "WHERE recipient_username  = ? "
                    "ORDER BY timestamp ASC"
                )
            );
            msgStmt->setString(1, username);
            unique_ptr<sql::ResultSet> res(msgStmt->executeQuery());
            cout<<"group"<<endl;
            cout<<username<<endl;
            // 构建响应对象
            json response;
            response["type"] = "ungroupread_messages";
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
                    messageObj["type"]="group";
                    messageObj["groupid"]=res->getInt("group_id");
                    messageObj["sender"] = res->getString("sender_username");
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
    void handleGetGroupUnreadMessages(int client_sock, const json& data)
    {
        if (!data.contains("user") || !data["user"].is_string() ||
        !data.contains("groupid") || !data["groupid"].is_number()) {
        json errorResponse = {
            {"success", false},
            {"message", "无效请求: 缺少必要参数"}
        };
        sendResponse(client_sock, errorResponse);
        return;
        }

        string username = data["user"];
        int groupid = data["groupid"];
        unique_ptr<sql::Connection> con(getDBConnection());
        if (!con) {
            json errorResponse = {
                {"success", false},
                {"message", "数据库连接失败"}
            };
            sendResponse(client_sock, errorResponse);
            return;
        }
        try {
        // 查询该用户在指定群组的未读消息
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "SELECT id, sender_username, message, timestamp "
                "FROM offline_messages "
                "WHERE recipient_username = ? AND group_id = ? "
                "ORDER BY timestamp ASC"
            )
        );
        stmt->setString(1, username);
        stmt->setInt(2, groupid);
        
        unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        // 构建响应
        json response;
        response["type"] = "group_unread_messages";
        response["success"] = true;
        json messages = json::array();
        
        while (res->next()) {
            json message;
            message["id"] = res->getInt("id");
            message["sender"] = res->getString("sender_username");
            message["message"] = res->getString("message");
            
            // 将时间戳转换为字符串
            time_t timestamp = res->getUInt64("timestamp");
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&timestamp));
            message["timestamp"] = string(timeBuffer);
            
            messages.push_back(message);
        }
        
        response["messages"] = messages;
        sendResponse(client_sock, response);
        
        } catch (const sql::SQLException& e) {
            json errorResponse = {
                {"success", false},
                {"message", "数据库错误: " + string(e.what())}
            };
            sendResponse(client_sock, errorResponse);
        }
    }
    void handleGetGroupHistory(int client_sock, const json& data)
    {
        // 验证请求参数
        if (!data.contains("user") || !data["user"].is_string() ||
            !data.contains("groupid") || !data["groupid"].is_number()) {
            json errorResponse = {
                {"success", false},
                {"message", "无效请求: 缺少必要参数"}
            };
            sendResponse(client_sock, errorResponse);
            return;
        }

        string username = data["user"];
        int groupid = data["groupid"];
        
        unique_ptr<sql::Connection> con(getDBConnection());
        if (!con) {
            json errorResponse = {
                {"success", false},
                {"message", "数据库连接失败"}
            };
            sendResponse(client_sock, errorResponse);
            return;
        }
        try{
        unique_ptr<sql::PreparedStatement> checkStmt(
            con->prepareStatement(
                "SELECT COUNT(*) AS count FROM group_members "
                // "SELECT * FROM group_members "
                "WHERE group_id = ? AND user = ?"
            )
        );
        checkStmt->setInt(1, groupid);
        checkStmt->setString(2, username);

        unique_ptr<sql::ResultSet> checkres(checkStmt->executeQuery());
        
        if (checkres->next()) {
            int count = checkres->getInt("count");
            if (count == 0) {
                cout<<"5555"<<endl;
                // 发送者不在群组内
                json errorResponse = {
                    {"success", false},
                    {"message", "您不在该群组中，无法查询历史消息"}
                };
                sendRequest(errorResponse);
                return;
            }
        }
        // 2. 查询群组历史消息
        unique_ptr<sql::PreparedStatement> historyStmt(
            con->prepareStatement(
                "SELECT sender, message, timestamp "
                "FROM group_messages "
                "WHERE group_id = ? "
                "ORDER BY timestamp ASC"
            )
        );
        historyStmt->setInt(1, groupid);
        unique_ptr<sql::ResultSet> res(historyStmt->executeQuery());
        
        // 3. 构建响应
        json response;
        response["type"] = "group_history";
        response["success"] = true;
        response["groupid"] = groupid;
        json messagesArray = json::array();
        
        while (res->next()) {
            json message;
            message["sender"] = res->getString("sender");
            message["message"] = res->getString("message");
            message["timestamp"] = res->getUInt64("timestamp");
            messagesArray.push_back(message);
        }
        
        response["messages"] = messagesArray;
        sendResponse(client_sock, response);
        
    } catch (const sql::SQLException& e) {
        json errorResponse = {
            {"success", false},
            {"message", "数据库错误: " + string(e.what())}
        };
        sendResponse(client_sock, errorResponse);
    } catch (const exception& e) {
        json errorResponse = {
            {"success", false},
            {"message", "处理错误: " + string(e.what())}
        };
        sendResponse(client_sock, errorResponse);
    }


    }
    
};