#include"chatserver.h"
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
        
        // 使用 splice 或 read/write 接收数据
        // 注意：sendfile 是单向的（客户端->服务器），所以服务器需要接收数据
        // 这里使用 read 循环接收数据
        
        const size_t bufferSize = 64 * 1024; // 64KB 缓冲区
        char buffer[bufferSize];
        
        while (remaining > 0) {
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