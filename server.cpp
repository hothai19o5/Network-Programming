#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

// Định nghĩa hằng số
const int PORT = 12345; // Chọn một số cổng thích hợp
const int MAX_BUFFER_SIZE = 1024;

// Cấu trúc để lưu thông tin client
struct ClientInfo {
    sockaddr_in address;
    socklen_t addressLength;
};

int main() {
    // 1. Tạo socket
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Không thể tạo socket." << std::endl;
        return 1;
    }

    // 2. Cấu hình địa chỉ server
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Lắng nghe trên tất cả các giao diện

    // 3. Gán địa chỉ cho socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Không thể gán địa chỉ cho socket." << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Server đang lắng nghe trên cổng " << PORT << std::endl;

    // Danh sách các client đang kết nối
    std::vector<ClientInfo> clients;

    // Vòng lặp chính của server
    while (true) {
        // 4. Nhận dữ liệu từ client
        char buffer[MAX_BUFFER_SIZE];
        ClientInfo client;
        client.addressLength = sizeof(client.address);

        ssize_t bytesReceived = recvfrom(
            serverSocket,
            buffer,
            MAX_BUFFER_SIZE - 1,
            0,
            (struct sockaddr*)&client.address,
            &client.addressLength
        );

        if (bytesReceived == -1) {
            std::cerr << "Lỗi khi nhận dữ liệu." << std::endl;
            continue; // Tiếp tục vòng lặp để xử lý các client khác
        }

        buffer[bytesReceived] = '\0'; // Kết thúc chuỗi để dễ xử lý

        std::string message(buffer);
        std::cout << "Nhận từ client " << inet_ntoa(client.address.sin_addr) << ":" << ntohs(client.address.sin_port) << ": " << message << std::endl;

        // 5. Xử lý kết nối mới hoặc tin nhắn từ client hiện tại
        bool isNewClient = true;
        for (const auto& existingClient : clients) {
            if (memcmp(&existingClient.address, &client.address, sizeof(client.address)) == 0) {
                isNewClient = false;
                break;
            }
        }

        if (isNewClient) {
            std::cout << "Client mới kết nối: " << inet_ntoa(client.address.sin_addr) << ":" << ntohs(client.address.sin_port) << std::endl;
            clients.push_back(client);
        }

        // 6. Chuyển tiếp tin nhắn đến các client khác (trừ client gửi)
        for (const auto& otherClient : clients) {
            if (memcmp(&otherClient.address, &client.address, sizeof(client.address)) != 0) {
                ssize_t bytesSent = sendto(
                    serverSocket,
                    buffer,
                    bytesReceived,
                    0,
                    (struct sockaddr*)&otherClient.address,
                    otherClient.addressLength
                );

                if (bytesSent == -1) {
                    std::cerr << "Lỗi khi gửi dữ liệu đến client " << inet_ntoa(otherClient.address.sin_addr) << ":" << ntohs(otherClient.address.sin_port) << std::endl;
                }
            }
        }
    }

    // 7. Đóng socket (sẽ không bao giờ đến đây trong vòng lặp vô hạn)
    close(serverSocket);

    return 0;
}