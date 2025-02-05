//#pragma once
//#define WIN32_LEAN_AND_MEAN // 避免冗余定义
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <string>
//#include <vector>
//#include <thread>
//#include <atomic>
//#include <iostream>
//#include <mutex>
//
//// 链接 Ws2_32.lib
//#pragma comment(lib, "Ws2_32.lib")
//
//class ChatRoomClient {
//public:
//    ChatRoomClient() : running(false), clientSocket(INVALID_SOCKET) {}
//
//    ~ChatRoomClient() {
//        running = false;
//        if (recvThread.joinable()) {
//            recvThread.join();
//        }
//        if (clientSocket != INVALID_SOCKET) {
//            closesocket(clientSocket);
//            WSACleanup();
//        }
//    }
//
//    bool connectToServer(const std::string& username, const std::string& serverIP, int port) {
//        WSADATA wsaData;
//        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//            std::cerr << "[错误] WSAStartup 初始化失败。\n";
//            return false;
//        }
//
//        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//        if (clientSocket == INVALID_SOCKET) {
//            std::cerr << "[错误] 创建套接字失败。\n";
//            WSACleanup();
//            return false;
//        }
//
//        sockaddr_in serverAddr = {};
//        serverAddr.sin_family = AF_INET;
//        serverAddr.sin_port = htons(port);
//        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
//
//        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
//            std::cerr << "[错误] 无法连接到服务器。\n";
//            closesocket(clientSocket);
//            WSACleanup();
//            return false;
//        }
//
//        sendMessage(username); // 向服务器发送用户名
//        running = true;
//
//        recvThread = std::thread(&ChatRoomClient::receiveMessages, this);
//        return true;
//    }
//
//    void sendMessage(const std::string& message) {
//        if (send(clientSocket, message.c_str(), message.size(), 0) == SOCKET_ERROR) {
//            std::cerr << "[错误] 消息发送失败。\n";
//        }
//    }
//
//    void receiveMessages() {
//        char buffer[1024];
//        while (running) {
//            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
//            if (bytesReceived > 0) {
//                buffer[bytesReceived] = '\0';
//                {
//                    std::lock_guard<std::mutex> lock(historyMutex);
//                    chatHistory.push_back(buffer); // 将消息保存到历史记录中
//                }
//            }
//            else {
//                running = false;
//            }
//        }
//    }
//
//    std::vector<std::string> getChatHistory() {
//        std::lock_guard<std::mutex> lock(historyMutex);
//        return chatHistory;
//    }
//
//    std::atomic<bool> running;
//
//private:
//    SOCKET clientSocket;
//    std::thread recvThread;
//    std::vector<std::string> chatHistory;
//    std::mutex historyMutex;
//};
