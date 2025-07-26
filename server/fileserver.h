#include"groupchatserver.h"
namespace fs = std::filesystem;
struct FileSession {
    string filePath;          // 最终文件路径
    string filename;          // 临时文件路径
    uint64_t fileSize;        // 文件总大小
    uint64_t receivedSize;    // 已接收大小
    // ofstream fileStream;      // 文件输出流
     //int fileFd;               // 文件描述符
     FILE * fileFd;
    int clientFd;             // 客户端套接字
};
// 全局会话映射
mutex sessionMutex;
//unordered_map<string, FileSession> fileSessions; // file_path -> session
class FileTransferServer
{
private:
    mutex uploadMutex;
    mutex data_mutex;
    int data_sock=-1;
    int listen_sock;
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
    int setBlocking(int fd) {
    // 获取当前标志
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) failed");
        return -1;
    }
 
    // 清除 O_NONBLOCK 标志（设置为阻塞）
    flags &= ~O_NONBLOCK;
 
    // 重新设置标志
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL) failed");
        return -1;
    }
 
    return 0;
}
int setNonBlocking(int fd) {
    // 获取当前标志
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) failed");
        return -1;
    }
 
    // 添加 O_NONBLOCK 标志（设置为非阻塞）
    flags |= O_NONBLOCK;
 
    // 重新设置标志
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl(F_SETFL) failed");
        return -1;
    }
 
    return 0;
}
    uint16_t pasv()
    {
        std::lock_guard<std::mutex> lock(data_mutex);
        
        // 创建数据监听socket
        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(listen_sock < 0) {
            return 0;
        }

        
        sockaddr_in data_addr{};
        data_addr.sin_family = AF_INET;
        data_addr.sin_addr.s_addr = INADDR_ANY;
        data_addr.sin_port = htons(8090); 

        if(bind(listen_sock, (sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        
            close(listen_sock);
            listen_sock = -1;
            return 0;
        }

        // 开始监听
        if(listen(listen_sock, 1) < 0) {
          
            close(listen_sock);
            listen_sock = -1;
            return 0;
        }
        // 获取绑定的端口号
        socklen_t addr_len = sizeof(data_addr);
        getsockname(listen_sock, (sockaddr*)&data_addr, &addr_len);//getsockname 可以用于获取绑定到套接字的实际地址和端口。
        uint16_t port = ntohs(data_addr.sin_port);// 获取端口号（网络字节序转主机字节序）
        return port;
        
    }
    void handleFileStart(json& data, int client_sock) 
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        //setBlocking(client_sock);
        string filepath = data["file_path"];
        uint64_t fileSize=data["filesize"];
        string fileName = fs::path(filepath).filename().string();
        string filestore="/home/lfd/3/"+fileName;
        std::ofstream file(filestore, std::ios::binary);
        if(!file) {
            // send_response("550 Can't create file");
            // close(data_sock);
            cout<<"fail"<<endl;
            return;
        }
        //uint16_t port=pasv();
        pasv();
        json req;
        req["success"]=true;
        //req["port"]=port;
        string str=req.dump();
        send(client_sock,str.c_str(),str.size(),0);
        
        cout << "开始接收文件: " << filepath  << endl;
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        data_sock = accept(listen_sock, 
                      (sockaddr*)&client_addr, &addr_len);
        char buffer[4096];
        ssize_t total = 0;
        
        
        while (fileSize>total) {
            ssize_t bytes = recv(data_sock, buffer, sizeof(buffer), 0);
            if (bytes < 0) {
                if (errno == EINTR) continue; // 处理中断
                cout<<"1234"<<endl;
                perror("recv error");
                break;
            }
            if (bytes == 0) break; // 客户端关闭连接
            
            file.write(buffer, bytes);
            if (!file) {
                //send_response("451 本地文件写入错误");
                break;
            }
            total += bytes;
            
            file.flush(); // 确保写入磁盘
        }
        cout<<"传输完成"<<endl;
        //setNonBlocking(client_sock);
    //handleFileData(client_sock, filepath);
    //transfrom(client_sock,file);
    }

    void handleFileEnd(json& data, int client_sock)
    {
        json response = {
            {"success", true}
        };
            
        sendResponse(client_sock, response);

    }
    void getUndeliveredFiles(int fd,json& data)
    {
        string username= data["username"];
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
        unique_ptr<sql::PreparedStatement> Stmt(
                con->prepareStatement(
                    "SELECT id, sender, file_name, file_size,created_at "
                    "FROM files "
                    "WHERE recipient  = ? AND delivered = 0 "
                    "ORDER BY created_at ASC"
                )
            );
        Stmt->setString(1, username);
        unique_ptr<sql::ResultSet> res(Stmt->executeQuery());
        json response;
            response["type"] = "unread_files";
            response["user"] = username;
            // 检查是否有未读消息
            if (!res->next()) {
                
            json filesArray = json::array();
                // 没有未读消息
                response["success"] = true;
                response["messages"] = json::array(); // 空数组
            } else {
                // 有未读消息
                json filesArray = json::array();
                

                // 重置结果集指针到开头
                res->beforeFirst();

                // 遍历所有未读消息
                while (res->next()) {
                    json messageObj;
                    messageObj["id"] = res->getInt("id");
                    messageObj["sender"] = res->getString("sender");
                    messageObj["file_name"] = res->getString("file_name");
                    messageObj["file_size"] = res->getInt("file_size");
                    messageObj["timestamp"] = res->getInt64("created_at");
                    filesArray.push_back(messageObj);
                }

                response["success"] = true;
                response["messages"] = filesArray;
            }
            string responseStr = response.dump();
            // 添加调试输出
            cout << "发送响应: " << responseStr << endl;

            // 发送响应给客户端
            if (send(fd, responseStr.c_str(), responseStr.size(), 0) < 0) {
                cerr << "发送未读消息响应失败: " << strerror(errno) << endl;
            } else {
                cout << "已发送未读消息给用户 " << username << endl;
            }

    }
    void privatefilesend(json& data, int client_sock)
    {
        string filename=data["filename"];
        string path="/home/lfd/3/"+filename;
        pasv();
        json response = {
            {"success", true}
        };   
        sendResponse(client_sock, response);
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        data_sock = accept(listen_sock, 
                      (sockaddr*)&client_addr, &addr_len);
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("文件不存在");
        char buffer[4096];
        ssize_t total = 0;
        
        while (file) {
            file.read(buffer, sizeof(buffer));
            std::streamsize bytes_read = file.gcount();
            
            if (bytes_read > 0) {
                ssize_t total_sent = 0;
                while (total_sent < bytes_read) {
                    ssize_t sent = send(data_sock, 
                                    buffer + total_sent, 
                                    bytes_read - total_sent, 
                                    MSG_NOSIGNAL);
                    if (sent <= 0) {
                        throw std::runtime_error("发送失败");
                    }
                    total_sent += sent;
                }
                total += total_sent;
            }
        }
        std::cout << "发送完成: "<< " (" << total << " bytes)" << std::endl;
        
    }






};