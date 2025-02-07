//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <iostream>
//#include <string>
//#include <thread>
//#include <atomic>
//#include <vector>
//
//#pragma comment(lib, "ws2_32.lib")
//
//#define DEFAULT_PORT 65432
//#define DEFAULT_BUFFER_SIZE 1024
//
//std::atomic<bool> running = true;
//std::string currentChatMode = "group"; // Ĭ����Ⱥ��ģʽ
//std::string privateTargetUser = "";   // ��ǰ��˽��Ŀ��
//std::vector<std::string> privateChannels; // �洢˽��Ƶ������
//
//
//// **������Ϣ���̺߳���**
////void receiveMessages(SOCKET clientSocket) {
////    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
////
////    while (running) {
////        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
////        if (bytesReceived > 0) {
////            buffer[bytesReceived] = '\0';
////            std::string message(buffer);
////
////            if (message.rfind("USERS:", 0) == 0) {
////                std::cout << "[ϵͳ��Ϣ] �����û�: " << message.substr(6) << std::endl;
////            } else if (message.rfind("[˽��]", 0) == 0) {
////                std::cout << "[˽����Ϣ] " << message.substr(4) << std::endl;
////            } else if (message.rfind("[Ⱥ��]", 0) == 0) {
////                std::cout << "[Ⱥ����Ϣ] " << message.substr(4) << std::endl;
////            }
////        } else {
////            std::cout << "[ϵͳ] �������Ͽ����ӡ�\n";
////            running = false;
////            break;
////        }
////    }
////}
//
//
//// **������Ϣ���̺߳���**
//void receiveMessages(SOCKET clientSocket) {
//    char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
//
//    while (running) {
//        int bytesReceived = recv(clientSocket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
//        if (bytesReceived > 0) {
//            buffer[bytesReceived] = '\0';
//            std::string message(buffer);
//
//            // ����Ƿ���˽��Ƶ������֪ͨ
//            if (message.rfind("[ϵͳ]", 0) == 0 && message.find("˽��Ƶ��") != std::string::npos) {
//                // ��ȡƵ������
//                std::string newChannel = message.substr(message.find_last_of(' ') + 1);
//                std::cout << "[ϵͳ��Ϣ] ����˽��Ƶ��: " << newChannel << "\n";
//
//                // ��ӵ�˽��Ƶ���б�
//                privateChannels.push_back(newChannel);
//            }
//            else if (message.rfind("[˽��]", 0) == 0) {
//                std::cout << message << std::endl;
//            }
//            else if (message.rfind("[Ⱥ��]", 0) == 0) {
//                std::cout << message << std::endl;
//            }
//            else {
//                std::cout << message << std::endl;
//            }
//        }
//        else {
//            std::cout << "[ϵͳ] �������Ͽ����ӡ�\n";
//            running = false;
//            break;
//        }
//    }
//}
//
//
//
//
//// **������Ϣ�ĺ���**
//void sendMessage(SOCKET clientSocket, const std::string& message) {
//    if (send(clientSocket, message.c_str(), message.size(), 0) == SOCKET_ERROR) {
//        std::cerr << "[����] ����ʧ�ܣ�������: " << WSAGetLastError() << std::endl;
//    }
//    else {
//        std::cout << "[����] " << message << std::endl;
//    }
//}
//
//// **�ͻ�����������**
//void startClient() {
//    WSADATA wsaData;
//    WSAStartup(MAKEWORD(2, 2), &wsaData);
//
//    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    sockaddr_in serverAddr{};
//    serverAddr.sin_family = AF_INET;
//    serverAddr.sin_port = htons(DEFAULT_PORT);
//    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
//
//    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
//
//    std::string username;
//    std::cout << "�������û���: ";
//    std::getline(std::cin, username);
//    send(clientSocket, username.c_str(), username.size(), 0);
//
//    std::thread recvThread(receiveMessages, clientSocket);
//    recvThread.detach();
//
//    while (running) {
//        std::string message;
//        std::getline(std::cin, message);
//
//        if (message == "!bye") break;
//
//        // �л�˽��Ŀ��
//        if (message.rfind("/dm ", 0) == 0) {
//            privateTargetUser = message.substr(4);
//            currentChatMode = "private";
//            std::cout << "[ϵͳ] ���л����� " << privateTargetUser << " ��˽��ģʽ��\n";
//            continue;
//        }
//
//        // �л���Ⱥ��ģʽ
//        if (message == "/group") {
//            currentChatMode = "group";
//            privateTargetUser = "";
//            std::cout << "[ϵͳ] ���л���Ⱥ��ģʽ��\n";
//            continue;
//        }
//
//        // ����ģʽ������Ϣ
//        if (currentChatMode == "private") {
//            std::string privateMessage = "DM:" + privateTargetUser + ":" + message;
//            send(clientSocket, privateMessage.c_str(), privateMessage.size(), 0);
//        }
//        else {
//            send(clientSocket, message.c_str(), message.size(), 0);
//        }
//    }
//
//    closesocket(clientSocket);
//    WSACleanup();
//}
//
////int main() {
////    startClient();
////}
