#include"groupchatserver.h"
namespace fs = std::filesystem;
// 文件会话结构
struct FileSession {
    string filePath;          // 最终文件路径
    string filename;          // 临时文件路径
    uint64_t fileSize;        // 文件总大小
    uint64_t receivedSize;    // 已接收大小
    // ofstream fileStream;      // 文件输出流
     int fileFd;               // 文件描述符
    int clientFd;             // 客户端套接字
};
// 全局会话映射
mutex sessionMutex;
unordered_map<string, FileSession> fileSessions; // file_path -> session
class FileTransferServer
{
private:
    mutex uploadMutex;
    string storagePath = "server_files/";
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
    void sendResponse(int fd, const json& response) {
        string responseStr = response.dump();
        send(fd, responseStr.c_str(), responseStr.size(), 0);
    }
    // Base64解码函数
    vector<char> base64_decode(const string& encoded_string) {
        BIO *bio, *b64;
        vector<char> buffer(encoded_string.size());
        
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new_mem_buf(encoded_string.c_str(), encoded_string.size());
        bio = BIO_push(b64, bio);
        
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        
        int decoded_length = BIO_read(bio, buffer.data(), encoded_string.size());
        if (decoded_length < 0) {
            cerr << "Base64解码失败" << endl;
            return {};
        }
        
        BIO_free_all(bio);
        buffer.resize(decoded_length);
        return buffer;
    }
public:
    void handleFileUploadRequest(int fd, const json& request)
    {   
        string user = request["user"];
        string fileName = request["file_name"];
        uint64_t fileSize = request["file_size"];
        string recipient = request["recipient"];
        time_t timestamp = static_cast<time_t>(request["timestamp"]);
        string filePath = request["filepath"];
        // 存储文件信息
        storeFileInfo(user, recipient, fileName, fileSize, filePath,timestamp);
        json response = {
            {"type", "file_upload_response"},
            {"success", true},
            {"file_path", filePath}
        };
        
        sendResponse(fd, response);
        //handleFileDownloadChunk(fd);


    }
    void storeFileInfo(const string& sender, const string& recipient, 
                      const string& fileName, uint64_t fileSize, const string& filePath,time_t timestamp)
    {
        unique_ptr<sql::Connection> con(getDBConnection());
        unique_ptr<sql::PreparedStatement> stmt(
            con->prepareStatement(
                "INSERT INTO files (sender, recipient, file_name, file_size, file_path, created_at) "
                          "VALUES ( ?, ?, ?, ?, ?, ?)"
            )
        );
        stmt->setString(1, sender);
        stmt->setString(2, recipient);
        stmt->setString(3, fileName);
        stmt->setInt(4, fileSize);
        stmt->setString(5, filePath);
        stmt->setUInt64(6, timestamp);
        stmt->executeUpdate();

    }
    // void handleFileDownloadChunk(int fd)
    // {
    //     uint32_t jsonSize;
    //     // ssize_t bytes_read =recv(fd, &jsonSize, sizeof(jsonSize), MSG_WAITALL);
    //     ssize_t bytes_read =recv(fd, &jsonSize, sizeof(jsonSize), 0);
            

    //     if (bytes_read <0) {
    //         cout<<"1"<<endl;
    //     }
    //         jsonSize = ntohl(jsonSize);
    //     // while(true)
    //     // {
    //     //     cout<<"333333333"<<endl;
    //     //      // 1. 接收长度前缀
            
            
    //     //     // 2. 接收JSON数据
    //     //     std::vector<char> buffer(jsonSize + 1);
    //     //     recv(fd, buffer.data(), jsonSize, MSG_WAITALL);
    //     //     buffer[jsonSize] = '\0';
            
    //     //     // 3. 解析JSON
    //     //     json data = json::parse(buffer.data());
    //     //     std::string type = data["type"];
            
    //     //     // // 4. 根据类型处理消息
    //     //     // if (type == "file_start") {
    //     //     //     cout<<"77777777"<<endl;
    //     //     //     handleFileStart(data, fd);
    //     //     // } else if (type == "file_chunk") {
    //     //     //     handleFileChunk(data, fd);
    //     //     // } else if (type == "file_end") {
    //     //     //     handleFileEnd(data, fd);
    //     //     //     break;
    //     //     // }
        
    //     // }
    // }
    // void handleFileStart(json& data, int client_sock)
    // {cout<<"555555555"<<endl;
    //         string filePath = data["file_path"];
    //         uint64_t fileSize = data["file_size"];
            
    //         lock_guard<mutex> lock(sessionMutex);
    //         // 检查会话是否已存在
    //         if (fileSessions.find(filePath) != fileSessions.end()) {
    //             // 已有会话存在，清理旧会话
    //             FileSession& oldSession = fileSessions[filePath];
    //             if (oldSession.fileStream.is_open()) {
    //                 oldSession.fileStream.close();
    //             }
    //             // if (fs::exists(oldSession.tempPath)) {
    //             //     fs::remove(oldSession.tempPath);
    //             // }
    //             fileSessions.erase(filePath);
    //         }
            
    //         // 创建新会话
    //         FileSession newSession;
    //         newSession.filePath = filePath;
    //         string fileName = fs::path(filePath).filename().string();
    //         newSession.filename = fileName;
    //         newSession.fileSize = fileSize;
    //         newSession.receivedSize = 0;
    //         // 打开临时文件
    //         newSession.fileStream.open(newSession.filename, ios::binary | ios::out);
    //         if (!newSession.fileStream) {
    //             cerr << "无法创建临时文件: " << newSession.filename << endl;
                
    //             // 发送错误响应
    //             json error = {
    //                 {"type", "error"},
    //                 {"message", "无法创建临时文件"}
    //             };
    //             send(client_sock, error.dump().c_str(), error.dump().size(), 0);
    //             return;
    //         }
    //         cout << "开始接收文件: " << filePath << " (" << fileSize << " 字节)" << endl;
    
    //         // 存储会话
    //         //fileSessions[filePath] = newSession;
    //         fileSessions.emplace(filePath, std::move(newSession));
    //         json response = {
    //         {"success", true},
    //         {"file_path", filePath}
    //     };
        
    //     sendResponse(client_sock, response);

    // }
    void handleFileStart(json& data, int client_sock) {
    string filePath = data["file_path"];
    uint64_t fileSize = data["file_size"];
    
    lock_guard<mutex> lock(sessionMutex);
    
    // 检查会话是否已存在
    if (fileSessions.find(filePath) != fileSessions.end()) {
        // 已有会话存在，清理旧会话
        FileSession& oldSession = fileSessions[filePath];
        if (oldSession.fileFd != -1) {
            close(oldSession.fileFd);
        }
        
        fileSessions.erase(filePath);
    }
    
    // 创建新会话
    FileSession newSession;
    newSession.filePath = filePath;
    
    newSession.fileSize = fileSize;
    newSession.receivedSize = 0;
    string fileName = fs::path(filePath).filename().string();
    newSession.filename = fileName;
    newSession.clientFd = client_sock;
    
    // 创建临时文件
    newSession.fileFd = open(newSession.filename.c_str(), 
                           O_WRONLY | O_CREAT | O_TRUNC, 
                           S_IRUSR | S_IWUSR);
    setBlocking(newSession.fileFd);
    if (newSession.fileFd == -1) {
        cerr << "无法创建临时文件: " << newSession.filename
             << " - " << strerror(errno) << endl;
        
        // 发送错误响应
        json error = {
            {"type", "error"},
            {"message", "无法创建临时文件: " + string(strerror(errno))}
        };
        sendResponse(client_sock, error);
        return;
    }
    
    // 存储会话
    fileSessions[filePath] = newSession;
    
    // // 发送确认响应
    // json response = {
    //     {"type", "file_start_ack"},
    //     {"file_path", filePath},
    //     {"continue", true} // 告诉客户端可以继续传输
    // };
    
    // sendResponse(client_sock, response);
    json response = {
            {"success", true},
            {"file_path", filePath}
        };
        
        sendResponse(client_sock, response);
    
    cout << "开始接收文件: " << filePath << " (" << fileSize << " 字节)" << endl;
}

    // void handleFileChunk(json& data, int client_sock)
    // {cout<<"6666666666"<<endl;
    //     string filePath = data["file_path"];
    //     uint64_t chunkSize = data["chunk_size"];
    //     string base64Data = data["data"];
    //     vector<char> binaryData = base64_decode(base64Data);
    //     if (binaryData.size() != static_cast<size_t>(chunkSize)) {
    //         cerr << "解码后数据大小不匹配: " 
    //             << binaryData.size() << " vs " << chunkSize << endl;
            
    //         // 发送错误响应
    //         json error = {
    //             {"type", "error"},
    //             {"message", "数据大小不匹配"}
    //         };
    //         send(client_sock, error.dump().c_str(), error.dump().size(), 0);
    //         return;
    //     }
    //     lock_guard<mutex> lock(sessionMutex);
    
    //     // 检查会话是否存在
    //     if (fileSessions.find(filePath) == fileSessions.end()) {
    //         cerr << "找不到文件会话: " << filePath << endl;
            
    //         // 发送错误响应
    //         json error = {
    //             {"type", "error"},
    //             {"message", "会话不存在"}
    //         };
    //         send(client_sock, error.dump().c_str(), error.dump().size(), 0);
    //         return;
    //     }
    //      FileSession& session = fileSessions[filePath];
    
    //     // 写入文件
    //     session.fileStream.write(binaryData.data(), binaryData.size());
    //     if (!session.fileStream) {
    //         cerr << "写入文件失败"<< endl;
            
    //         // 发送错误响应
    //         json error = {
    //             {"type", "error"},
    //             {"message", "写入文件失败"}
    //         };
    //         send(client_sock, error.dump().c_str(), error.dump().size(), 0);
    //         return;
    //     }
        
    //     // 更新会话状态
    //     session.receivedSize += binaryData.size();
    //     json response = {
    //         {"success", true},
    //         {"file_path", filePath}
    //     };
        
    //     sendResponse(client_sock, response);
    // }
    //1
    void setBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);  // 获取当前标志
        if (flags == -1) {
            perror("fcntl(F_GETFL) failed");
            return;
        }
        flags &= ~O_NONBLOCK;  // 清除 O_NONBLOCK 标志（即设置为阻塞）
        if (fcntl(fd, F_SETFL, flags) == -1) {
            perror("fcntl(F_SETFL) failed");
        }
    }
    void handleFileData(int client_sock, const string& filePath) 
    {
        lock_guard<mutex> lock(sessionMutex);
        
        // 检查会话是否存在
        if (fileSessions.find(filePath) == fileSessions.end()) {
            cerr << "找不到文件会话: " << filePath << endl;
            
            // 发送错误响应
            json error = {
                {"type", "error"},
                {"message", "会话不存在"}
            };
            sendResponse(client_sock, error);
            return;
        }
        
        FileSession& session = fileSessions[filePath];
        
        // 计算剩余要接收的数据大小
        uint64_t remaining = session.fileSize - session.receivedSize;
        cout<<"remain="<<remaining<<endl;
        
        // 使用 splice 或 read/write 接收数据
        // 注意：sendfile 是单向的（客户端->服务器），所以服务器需要接收数据
        // 这里使用 read 循环接收数据
        
        //const size_t bufferSize = 64 * 1024; // 64KB 缓冲区
        const size_t bufferSize = 1 * 1024 * 1024*5;
        char buffer[bufferSize];
        
        while (remaining > 0) {
            if(remaining<20)
            {
                cout<<"rem="<<remaining<<endl;
            }
            // 计算本次读取的大小
            size_t toRead = min(static_cast<size_t>(remaining), bufferSize);
            
            // 从套接字读取数据
            ssize_t bytesRead = read(client_sock, buffer, toRead);
            
            if (bytesRead < 0) {
                // 读取错误
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 超时，可以重试
                    continue;
                }
                cerr << "读取文件数据失败: " << strerror(errno) << endl;
                break;
            } else if (bytesRead == 0) {
                // 连接关闭
                cerr << "连接已关闭，文件传输中断" << endl;
                break;
            }
            
            // 写入文件
            ssize_t bytesWritten = write(session.fileFd, buffer, bytesRead);
            if (bytesWritten != bytesRead) {
                cerr << "写入文件失败: " << strerror(errno) << endl;
                break;
            }
            
            // 更新进度
            session.receivedSize += bytesWritten;
            remaining -= bytesWritten;
            
            // 显示进度
            float progress = static_cast<float>(session.receivedSize) / session.fileSize * 100;
            cout << "\r接收进度: " << fixed << setprecision(1) << progress << "% ("
                << session.receivedSize << "/" << session.fileSize << " 字节)";
            cout.flush();
        }
        
        cout << endl;
        
        // 检查是否完整接收
        if (session.receivedSize != session.fileSize) {
            cerr << "文件接收不完整: " << session.receivedSize 
                << "/" << session.fileSize << " 字节" << endl;
        }
        
    }
    //2
    // void handleFileData(int client_sock, const string& filePath) 
    // {
    //     lock_guard<mutex> lock(sessionMutex);
        
    //     // 检查会话是否存在
    //     if (fileSessions.find(filePath) == fileSessions.end()) {
    //         cerr << "找不到文件会话: " << filePath << endl;
            
    //         // 发送错误响应
    //         json error = {
    //             {"type", "error"},
    //             {"message", "会话不存在"}
    //         };
    //         sendResponse(client_sock, error);
    //         return;
    //     }
        
    //     FileSession& session = fileSessions[filePath];
        
    //     // 计算剩余要接收的数据大小
    //     uint64_t remaining = session.fileSize - session.receivedSize;
        
    //     // 使用 splice 或 read/write 接收数据
    //     // 注意：sendfile 是单向的（客户端->服务器），所以服务器需要接收数据
    //     // 这里使用 read 循环接收数据
        
    //     //const size_t bufferSize = 64 * 1024; // 64KB 缓冲区
    //     const size_t bufferSize = 1 * 1024 * 1024*5;
    //     char buffer[bufferSize];
        
    //     while (remaining > 0) {
    // // 计算本次读取的大小
    // size_t toRead = min(static_cast<size_t>(remaining), bufferSize);
    // size_t totalRead = 0;
    
    // // 循环读取直到获取请求的全部数据
    // while (totalRead < toRead) {
    //     // 从套接字读取数据
    //     ssize_t bytesRead = read(client_sock, buffer + totalRead, toRead - totalRead);
        
    //     if (bytesRead < 0) {
    //         if (errno == EAGAIN || errno == EWOULDBLOCK) {
    //             // 等待数据到达
    //             fd_set read_fds;
    //             FD_ZERO(&read_fds);
    //             FD_SET(client_sock, &read_fds);
                
    //             struct timeval timeout;
    //             timeout.tv_sec = 5;
    //             timeout.tv_usec = 0;
                
    //             if (select(client_sock + 1, &read_fds, NULL, NULL, &timeout) <= 0) {
    //                 throw runtime_error("等待数据超时");
    //             }
    //             continue;
    //         }
    //         cerr << "读取文件数据失败: " << strerror(errno) << endl;
    //         break;
    //     } else if (bytesRead == 0) {
    //         // 连接关闭
    //         cerr << "连接已关闭，文件传输中断" << endl;
    //         break;
    //     }
        
    //     totalRead += bytesRead;
    // }
    
    // // 写入文件
    // size_t totalWritten = 0;
    // while (totalWritten < totalRead) {
    //     ssize_t bytesWritten = write(session.fileFd, buffer + totalWritten, totalRead - totalWritten);
        
    //     if (bytesWritten < 0) {
    //         if (errno == EAGAIN || errno == EWOULDBLOCK) {
    //             // 等待文件系统准备好
    //             usleep(10000); // 10ms
    //             continue;
    //         }
    //         cerr << "写入文件失败: " << strerror(errno) << endl;
    //         break;
    //     }
        
    //     totalWritten += bytesWritten;
    // }
            
    //         // 更新进度
    //         session.receivedSize += totalRead;
    //         remaining -= totalRead;
            
    //         // 显示进度
    //         float progress = static_cast<float>(session.receivedSize) / session.fileSize * 100;
    //         cout << "\r接收进度: " << fixed << setprecision(1) << progress << "% ("
    //             << session.receivedSize << "/" << session.fileSize << " 字节)";
    //         cout.flush();
    //     }
        
    //     cout << endl;
        
    //     // 检查是否完整接收
    //     if (session.receivedSize != session.fileSize) {
    //         cerr << "文件接收不完整: " << session.receivedSize 
    //             << "/" << session.fileSize << " 字节" << endl;
    //     }
        
    // }
    //3
    // void handleFileData(int client_sock, const string& filePath)
    // {
    //     lock_guard<mutex> lock(sessionMutex);
        
    //     // 检查会话是否存在
    //     if (fileSessions.find(filePath) == fileSessions.end()) {
    //         cerr << "找不到文件会话: " << filePath << endl;
            
    //         // 发送错误响应
    //         json error = {
    //             {"type", "error"},
    //             {"message", "会话不存在"}
    //         };
    //         sendResponse(client_sock, error);
    //         return;
    //     }
        
    //     FileSession& session = fileSessions[filePath];
    //     int read_bytes = 0;
    //     char buffers[1024];
    //     std::cout << "reading " << std::endl;
    //     while ((read_bytes = read(client_sock, buffers, sizeof(buffers))) > 0)
    //     {
    //         if (write(session.fileFd, buffers, read_bytes) != read_bytes)
    //         {
    //             perror("write");
    //             close(session.fileFd);
    //             // send_Response(550, "Write error during file transfer.", read_fd);
    //             // return -1;
    //         }
    //     }
    //     if (read_bytes == 0)
    //     {
    //         std::cout << "read end" << std::endl;
    //         close(session.fileFd);
    //         // send_Response(226, "Transfer complete.", read_fd);
    //         // return 0;
    //     }
    //     else
    //     {
    //         std::cout << "read fail" << std::endl;
    //         perror("read");
    //         close(session.fileFd);
    //         // send_Response(451, "Read error during file transfer.", read_fd);
    //         // return -1;
    //     }
    //     // 检查是否完整接收
    //     if (session.receivedSize != session.fileSize) {
    //         cerr << "文件接收不完整: " << session.receivedSize 
    //             << "/" << session.fileSize << " 字节" << endl;
    //     }
    // }
    //4
//     void handleFileData(int client_sock, const string& filePath)
// {
//     lock_guard<mutex> lock(sessionMutex);
    
//     // 检查会话是否存在
//     if (fileSessions.find(filePath) == fileSessions.end()) {
//         cerr << "找不到文件会话: " << filePath << endl;
        
//         // 发送错误响应
//         json error = {
//             {"type", "error"},
//             {"message", "会话不存在"}
//         };
//         sendResponse(client_sock, error);
//         return;
//     }
    
//     FileSession& session = fileSessions[filePath];
    
//     // // 1. 首先读取 file_chunk 消息
//     // uint32_t jsonSize;
//     // ssize_t bytesRead = recv(client_sock, &jsonSize, sizeof(jsonSize), MSG_WAITALL);
//     // if (bytesRead != sizeof(jsonSize)) {
//     //     cerr << "读取消息大小失败" << endl;
//     //     return;
//     // }
    
//     // jsonSize = ntohl(jsonSize);
//     // vector<char> jsonBuffer(jsonSize + 1);
//     // bytesRead = recv(client_sock, jsonBuffer.data(), jsonSize, MSG_WAITALL);
//     // if (bytesRead != static_cast<ssize_t>(jsonSize)) {
//     //     cerr << "读取消息内容失败" << endl;
//     //     return;
//     // }
    
//     // jsonBuffer[jsonSize] = '\0';
//     // json data = json::parse(jsonBuffer.data());
    
//     // // 验证消息类型
//     // if (data["type"] != "file_chunk") {
//     //     cerr << "无效消息类型: " << data["type"] << endl;
//     //     return;
//     // }
    
//     // 2. 现在开始接收文件数据
//     uint64_t remaining = session.fileSize - session.receivedSize;
//     char buffer[4096];
    
//     cout << "开始接收文件数据: " << filePath 
//          << " (" << remaining << "/" << session.fileSize << " 字节)" << endl;
    
//     while (remaining > 0) {
//         // 计算本次读取的大小
//         size_t toRead = min(static_cast<size_t>(remaining), sizeof(buffer));
        
//         // 从套接字读取数据
//         ssize_t bytesRead = read(client_sock, buffer, toRead);
        
//         if (bytesRead < 0) {
//             if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                 // 超时，可以重试
//                 continue;
//             }
//             cerr << "读取文件数据失败: " << strerror(errno) << endl;
//             break;
//         } else if (bytesRead == 0) {
//             // 连接关闭
//             cerr << "连接已关闭，文件传输中断" << endl;
//             break;
//         }
        
//         // 写入文件
//         ssize_t bytesWritten = write(session.fileFd, buffer, bytesRead);
//         if (bytesWritten != bytesRead) {
//             cerr << "写入文件失败: " << strerror(errno) << endl;
//             break;
//         }
        
//         // 更新进度
//         session.receivedSize += bytesWritten;
//         remaining -= bytesWritten;
        
//         // 显示进度
//         float progress = static_cast<float>(session.receivedSize) / session.fileSize * 100;
//         cout << "\r接收进度: " << fixed << setprecision(1) << progress << "% ("
//              << session.receivedSize << "/" << session.fileSize << " 字节)";
//         cout.flush();
//     }
    
//     cout << endl;
    
//     // 检查是否完整接收
//     if (session.receivedSize != session.fileSize) {
//         cerr << "文件接收不完整: " << session.receivedSize 
//              << "/" << session.fileSize << " 字节" << endl;
//     } else {
//         cout << "文件接收完成" << endl;
//     }
// }
    //5
//     void handleFileData(int client_sock, const string& filePath)
// {
//     lock_guard<mutex> lock(sessionMutex);
    
//     // 检查会话是否存在
//     if (fileSessions.find(filePath) == fileSessions.end()) {
//         cerr << "找不到文件会话: " << filePath << endl;
        
//         // 发送错误响应
//         json error = {
//             {"type", "error"},
//             {"message", "会话不存在"}
//         };
//         sendResponse(client_sock, error);
//         return;
//     }
    
//     FileSession& session = fileSessions[filePath];
    
//     // 设置套接字超时
//     struct timeval tv;
//     tv.tv_sec = 30; // 30秒超时
//     tv.tv_usec = 0;
//     setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
//     // 计算剩余要接收的数据大小
//     uint64_t remaining = session.fileSize - session.receivedSize;
//     const size_t bufferSize = 64 * 1024; // 64KB
//     char buffer[bufferSize];
    
//     cout << "开始接收文件数据: " << filePath 
//          << " (" << remaining << "/" << session.fileSize << " 字节)" << endl;
    
//     auto start_time = chrono::steady_clock::now();
//     auto last_progress_time = start_time;
    
//     try {
//         while (remaining > 0) {
//             // 计算本次读取的大小
//             size_t toRead = min(static_cast<size_t>(remaining), bufferSize);
            
//             // 从套接字读取数据
//             ssize_t bytesRead = read(client_sock, buffer, toRead);
            
//             if (bytesRead < 0) {
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // 检查是否超时
//                     auto now = chrono::steady_clock::now();
//                     auto elapsed = chrono::duration_cast<chrono::seconds>(now - start_time);
//                     if (elapsed.count() > 30) {
//                         throw runtime_error("接收超时");
//                     }
//                     continue;
//                 }
//                 throw runtime_error("读取失败: " + string(strerror(errno)));
//             } else if (bytesRead == 0) {
//                 throw runtime_error("连接已关闭");
//             }
            
//             // 写入文件
//             ssize_t bytesWritten = write(session.fileFd, buffer, bytesRead);
//             if (bytesWritten != bytesRead) {
//                 throw runtime_error("写入失败: " + string(strerror(errno)));
//             }
            
//             // 更新进度
//             session.receivedSize += bytesWritten;
//             remaining -= bytesWritten;
            
//             // 显示进度（限制频率）
//             auto now = chrono::steady_clock::now();
//             if (chrono::duration_cast<chrono::milliseconds>(now - last_progress_time).count() > 100 ||
//                 remaining == 0) {
//                 float progress = static_cast<float>(session.receivedSize) / session.fileSize * 100;
//                 cout << "\r接收进度: " << fixed << setprecision(1) << progress << "% ("
//                      << session.receivedSize << "/" << session.fileSize << " 字节)";
//                 cout.flush();
//                 last_progress_time = now;
//             }
//         }
        
//         cout << endl;
        
//         // 最终刷新文件
//         fsync(session.fileFd);
        
//         // 验证文件大小
//         struct stat file_stat;
//         if (fstat(session.fileFd, &file_stat) == 0) {
//             if (static_cast<uint64_t>(file_stat.st_size) != session.fileSize) {
//                 cerr << "文件大小不匹配: 实际=" << file_stat.st_size 
//                      << " 预期=" << session.fileSize << endl;
//             }
//         }
        
//         cout << "文件接收完成: " << filePath << endl;
        
//     } catch (const exception& e) {
//         cerr << "文件接收错误: " << e.what() << endl;
        
//         // 检查是否完整接收
//         if (session.receivedSize != session.fileSize) {
//             cerr << "文件接收不完整: " << session.receivedSize 
//                  << "/" << session.fileSize << " 字节" << endl;
//         }
//     }
// }
    void handleFileEnd(json& data, int client_sock)
    {cout<<"77777"<<endl;
        string filePath = data["file_path"];
        uint64_t fileSize = data["file_size"];
        bool success = data["success"];
        
        lock_guard<mutex> lock(sessionMutex);
        FileSession& session = fileSessions[filePath];
        
        // 关闭文件流
        //session.fileStream.close();
        close(session.fileFd);
        fileSessions.erase(filePath);
        json response = {
            {"success", true},
            {"file_path", filePath}
        };
        
        sendResponse(client_sock, response);

    }
   





};