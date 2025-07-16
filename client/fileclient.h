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
#include <openssl/sha.h>  // 提供 SHA256_CTX 定义
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <openssl/buffer.h>
#include <sys/sendfile.h>
#include <fcntl.h> 
#include <sys/stat.h>
namespace fs = std::filesystem;  
using json = nlohmann::json;
using namespace std;
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 10
class FileTransferClient
{
private:

    int sock;
    string currentUser;
    string friendName;
    string filePath;
    json sendRequest(int sock, const json& request) {
        string requestStr = request.dump();
        send(sock, requestStr.c_str(), requestStr.size(), 0);
        
        char buffer[4096];
        ssize_t bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            // return {{"success",true}, {"message", "接收响应失败"}};
            return {{"success",true}};
        }
        
        // buffer[bytesRead] = '\0';
        // try {
        //     return json::parse(buffer);
        // } catch (...) {
        //     return {{"success", false}, {"message", "解析响应失败"}};
        // }
    }
    
public:
    void sendFile(int sockfd,string currentuser)
    {
        sock=sockfd;
        currentUser=currentuser;
        handleFileNotification(); 
        // cout << "请输入对方用户名: ";
        // getline(cin, friendName);
        // cout<<"请输入文件路径"<<endl;
        // getline(cin, filePath);
        // if (!fs::exists(filePath)) {
        //     cerr << "文件不存在: " << filePath << endl;
        //     return;
        // }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
        // 输入用户名
        cout << "请输入对方用户名: ";
        getline(cin, friendName);
        
        // 验证用户名
        if (friendName.empty()) {
            cerr << "用户名不能为空" << endl;
            return;
        }
        
        // 输入文件路径
        bool validPath = false;
        while (!validPath) {
            cout << "请输入文件路径: ";
            getline(cin, filePath);
            
            // 验证文件路径
            if (filePath.empty()) {
                cerr << "文件路径不能为空" << endl;
            } else {
                // 检查文件是否存在
                if (filesystem::exists(filePath)) {
                    validPath = true;
                } else {
                    cerr << "文件不存在，请重新输入" << endl;
                }
            }
        }
        //获取文件信息
        string fileName = fs::path(filePath).filename().string();
        uint64_t fileSize = fs::file_size(filePath);
        // 发送文件上传请求
        json request = {
            {"type", "file_upload_request"},
            {"user", currentUser},
            {"file_name", fileName},
            {"file_size", fileSize},
            {"filepath",filePath},
            {"recipient", friendName},
            {"timestamp", static_cast<uint64_t>(time(nullptr))}
        };
        json response = sendRequest(sock, request);
        if (!response.value("success", false)) {
            cerr << "文件上传请求失败: " 
                 << response.value("message", "未知错误") << endl;
            return;
        }

        
        // 上传文件到服务器
        uploadFileToServer(filePath,sockfd);

    }
    // bool uploadFileToServer(const string& filePath)
    // {
    //     ifstream file(filePath, ios::binary);
    //     if (!file) {
    //         cerr << "无法打开文件: " << filePath << endl;
    //         return false;
    //     }
    //     uint64_t fileSize = fs::file_size(filePath);
        
    //     // 分块上传文件
    //     const size_t chunkSize = 1024 * 1024; // 1MB
    //     vector<char> buffer(chunkSize);
    //     uint64_t totalSent = 0;
    //     while (totalSent < fileSize) 
    //     {
    //         size_t bytesToRead = min(chunkSize, static_cast<size_t>(fileSize - totalSent));
    //         file.read(buffer.data(), bytesToRead);
    //         size_t bytesRead = file.gcount();
            
    //         // 发送文件块
    //         json chunkRequest = {
    //             {"type", "file_upload_chunk"},
    //             {"filepath",filePath},
    //             {"offset", totalSent},
    //             {"size", bytesRead},
    //             {"data", vector<char>(buffer.begin(), buffer.begin() + bytesRead)}
    //         };
            
    //         json chunkResponse = sendRequest(sock, chunkRequest);
            
    //         if (!chunkResponse.value("success", false)) {
    //             cerr << "上传块失败: " 
    //                  << chunkResponse.value("message", "未知错误") << endl;
    //             return false;
    //         }
            
    //         totalSent += bytesRead;
            
    //         // 显示进度
    //         showProgress(totalSent, fileSize);
    //     }
    //     file.close();

    // }
    // bool uploadFileToServer(const string& filePath)
    // {
    //     std::ifstream file(filePath, std::ios::binary);
    //     if (!file) throw std::runtime_error("文件不存在");
    //     uint64_t fileSize = fs::file_size(filePath);
    //     char buffer[4096];
    //     ssize_t total = 0;
        
    //     // while (file) {
    //     //     file.read(buffer, sizeof(buffer));
    //     //     std::streamsize bytes_read = file.gcount();
            
    //     //     if (bytes_read > 0) {
    //     //         ssize_t total_sent = 0;
    //     //         while (total_sent < bytes_read) {
    //     //             ssize_t sent = send(sock, 
    //     //                             buffer + total_sent, 
    //     //                             bytes_read - total_sent, 
    //     //                             MSG_NOSIGNAL);
    //     //             if (sent <= 0) {
    //     //                 throw std::runtime_error("发送失败");
    //     //             }
    //     //             total_sent += sent;
    //     //         }
    //     //         total += total_sent;
    //     //     }
    //     //     showProgress(total, fileSize);
    //     // }
    //     return true;
    // }
    void showProgress(uint64_t current, uint64_t total) 
    {
        float progress = static_cast<float>(current) / total;
        int barWidth = 50;
        
        cout << "[";
        int pos = barWidth * progress;
        for (int i = 0; i < barWidth; i++) {
            if (i < pos) cout << "=";
            else if (i == pos) cout << ">";
            else cout << " ";
        }
        cout << "] " << int(progress * 100.0) << " %\r";
        cout.flush();
    }
    void handleFileNotification()
    {

    } 
    // Base64编码函数
    std::string base64_encode(const char* input, size_t length) 
    {
        // 创建OpenSSL BIO对象链
        BIO *bio, *b64;
        BUF_MEM *bufferPtr;  // 内存缓冲区指针

        // 创建Base64过滤器BIO
        b64 = BIO_new(BIO_f_base64());
        // 创建内存BIO
        bio = BIO_new(BIO_s_mem());
        // 将Base64过滤器链接到内存BIO
        bio = BIO_push(b64, bio);
        
        // 设置标志：不添加换行符
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        
        // 将原始数据写入BIO链
        BIO_write(bio, input, length);
        // 刷新BIO链，确保所有数据都被处理
        BIO_flush(bio);
        
        // 获取内存BIO中的缓冲区指针
        BIO_get_mem_ptr(bio, &bufferPtr);
        
        // 从缓冲区创建字符串
        std::string result(bufferPtr->data, bufferPtr->length);
        
        // 释放BIO链资源
        BIO_free_all(bio);
        
        return result;
    }

    // // 文件上传函数
    // void uploadFileToServer(const std::string& filePath)
    // {
    //     ifstream file(filePath,ios::binary);
    //     if (!file) {
    //         throw std::runtime_error("无法打开文件: " + filePath);
    //     }
    //     // 2. 获取文件大小
    //     uint64_t fileSize = fs::file_size(filePath);
    //     std::cout << "开始上传文件: " << filePath << " (" << fileSize << " 字节)" << std::endl;
    //     json startMsg = {
    //         {"type", "file_start"},
    //         {"file_path", filePath},
    //         {"file_size", fileSize}
    //     };
        
    //     std::string startStr = startMsg.dump();
    //     uint32_t startSize = htonl(static_cast<uint32_t>(startStr.size()));
    //     // if (send(sock, &startSize, sizeof(startSize), 0) >0) {
    //     //     cout<<"fasong"<<endl;
    //     // }
        
    //     // // 发送大小前缀
    //     // if (send(sock, &startSize, sizeof(startSize), 0) != sizeof(startSize)) {
    //     //     throw std::runtime_error("发送开始消息大小失败");
    //     // }
        
    //     // 发送开始消息
    //     // if (send(sock, startStr.c_str(), startStr.size(), 0) != static_cast<ssize_t>(startStr.size())) {
    //     //     throw std::runtime_error("发送开始消息失败");
    //     // }
    //     json respose =sendRequest(sock,startMsg);
    //     // 4. 读取并发送文件数据
    //     const size_t chunkSize = 8192; // 8KB块大小
    //     char buffer[chunkSize];
    //     uint64_t totalSent = 0;
    //     while(true)
    //     {cout<<"true"<<endl;
    //         file.read(buffer,sizeof(buffer));
    //         streamsize bytesRead = file.gcount();
    //         if (bytesRead <= 0) {
    //             cout<<"222"<<endl;
    //             break; // 文件结束
    //         }
    //         std::string base64Data = base64_encode(buffer, bytesRead);
    //         // 准备数据块消息
    //         json chunkMsg = {
    //             {"type", "file_chunk"},
    //             {"file_path", filePath},
    //             {"chunk_size", static_cast<uint64_t>(bytesRead)},
    //             {"data", base64Data}
    //         };
            
    //         std::string chunkStr = chunkMsg.dump();
    //         uint32_t chunkMsgSize = htonl(static_cast<uint32_t>(chunkStr.size()));
            
    //         // // 发送大小前缀
    //         // if (send(sock, &chunkMsgSize, sizeof(chunkMsgSize), 0) != sizeof(chunkMsgSize)) {
    //         //     throw std::runtime_error("发送数据块大小前缀失败");
    //         // }
            
    //         // // 发送数据块消息
    //         // if (send(sock, chunkStr.c_str(), chunkStr.size(), 0) != static_cast<ssize_t>(chunkStr.size())) {
    //         //     throw std::runtime_error("发送数据块消息失败");
    //         // }
    //         if(send(sock, chunkStr.c_str(), chunkStr.size(), 0))
    //         {
    //             cout<<"send"<<endl;
    //         }
    //         //json respose =sendRequest(sock,chunkMsg);
            
    //         totalSent += bytesRead;
    //         //showProgress(totalSent, fileSize);
           
    //     }
    //      // 5. 发送结束消息
    //         json endMsg = {
    //             {"type", "file_end"},
    //             {"file_path", filePath},
    //             {"file_size", fileSize},
    //             {"success", true}
    //         };
            
    //         std::string endStr = endMsg.dump();
    //         uint32_t endSize = htonl(static_cast<uint32_t>(endStr.size()));
            
    //         // // 发送大小前缀
    //         // if (send(sock, &endSize, sizeof(endSize), 0) != sizeof(endSize)) {
    //         //     throw std::runtime_error("发送结束消息大小失败");
    //         // }
            
    //         // 发送结束消息
    //         if (send(sock, endStr.c_str(), endStr.size(), 0) != static_cast<ssize_t>(endStr.size())) {
    //             throw std::runtime_error("发送结束消息失败");
    //         }
    //         //json respose =sendRequest(sock,endMsg);
            
    //         std::cout << "文件上传完成: " << filePath << std::endl;
                    

        

    // }
    void uploadFileToServer(const std::string& filePath, int sock) 
    {
        // 1. 打开文件
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error("无法打开文件: " + filePath + " - " + strerror(errno));
        }
        
        // 2. 获取文件大小
        struct stat fileStat;
        if (fstat(fd, &fileStat) == -1) {
            close(fd);
            throw std::runtime_error("无法获取文件信息: " + filePath + " - " + strerror(errno));
        }
        
        uint64_t fileSize = fileStat.st_size;
        std::cout << "开始上传文件: " << filePath << " (" << fileSize << " 字节)" << std::endl;
        
        // 3. 发送开始消息
        json startMsg = {
            {"type", "file_start"},
            {"file_path", filePath},
            {"file_size", fileSize}
        };
        std::string startStr = startMsg.dump();
        json response = sendRequest(sock, startMsg);
        
        // // 4. 检查服务器响应是否继续
        // if (!response.contains("continue") || !response["continue"].get<bool>()) {
        //     close(fd);
        //     throw std::runtime_error("服务器拒绝文件传输请求");
        // }
        json dataMsg = {
        {"type", "file_chunk"},
        {"file_path", filePath}
        };
        sendRequest(sock, dataMsg);
        
        // 5. 使用 sendfile 传输文件数据
        off_t offset = 0;             // 文件偏移量
        uint64_t totalSent = 0;        // 已发送字节数
        const size_t chunkSize = 4 * 1024 * 1024; // 每次发送4MB
        
        while (totalSent < fileSize) {
            // 计算本次传输的大小（不超过剩余数据量）
            // size_t sendSize = (fileSize - totalSent > chunkSize) ? 
            //                 chunkSize : fileSize - totalSent;
            size_t sendSize = static_cast<size_t>(
            min(static_cast<uint64_t>(chunkSize), fileSize - totalSent));
            // 使用 sendfile 直接传输文件数据
            ssize_t sent = sendfile(sock, fd, &offset, sendSize);
            
            if (sent == -1) {
                close(fd);
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    throw std::runtime_error("文件传输超时");
                } else {
                    throw std::runtime_error("文件传输失败: " + std::string(strerror(errno)));
                }
            } else if (sent == 0) {
                close(fd);
                throw std::runtime_error("文件传输中断: 连接已关闭");
            }
            
            totalSent += sent;
            // 每10MB显示一次进度
        // if (totalSent % (10 * 1024 * 1024) == 0 || totalSent == fileSize) {
        //     showProgress(totalSent, fileSize);
        // }
        }
        
        // 6. 检查完整性
        if (totalSent != fileSize) {
            close(fd);
            throw std::runtime_error("文件传输不完整: " + std::to_string(totalSent) + 
                                    "/" + std::to_string(fileSize));
        }
        
        // 7. 关闭文件
        close(fd);
        
        // 8. 发送结束消息
        json endMsg = {
            {"type", "file_end"},
            {"file_path", filePath},
            {"file_size", fileSize},
            {"success", true},
            {"bytes_sent", totalSent}
        };
        json endResponse = sendRequest(sock, endMsg);
        
        std::cout << "文件上传完成: " << filePath << std::endl;
    }
};