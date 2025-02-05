//#pragma once
//#define WIN32_LEAN_AND_MEAN // �������ඨ��
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <string>
//#include <vector>
//#include <thread>
//#include <atomic>
//#include <iostream>
//#include <mutex>
//
//// ���� Ws2_32.lib
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
//            std::cerr << "[����] WSAStartup ��ʼ��ʧ�ܡ�\n";
//            return false;
//        }
//
//        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//        if (clientSocket == INVALID_SOCKET) {
//            std::cerr << "[����] �����׽���ʧ�ܡ�\n";
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
//            std::cerr << "[����] �޷����ӵ���������\n";
//            closesocket(clientSocket);
//            WSACleanup();
//            return false;
//        }
//
//        sendMessage(username); // ������������û���
//        running = true;
//
//        recvThread = std::thread(&ChatRoomClient::receiveMessages, this);
//        return true;
//    }
//
//    void sendMessage(const std::string& message) {
//        if (send(clientSocket, message.c_str(), message.size(), 0) == SOCKET_ERROR) {
//            std::cerr << "[����] ��Ϣ����ʧ�ܡ�\n";
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
//                    chatHistory.push_back(buffer); // ����Ϣ���浽��ʷ��¼��
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
