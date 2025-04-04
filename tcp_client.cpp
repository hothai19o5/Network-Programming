#include <iostream>
#include <string>
#include <cstring>          // Các hàm xử lý chuỗi ( memset, strerror )
#include <sys/socket.h>     // Các hàm socket ( socket, connect, send, recv )
#include <netdb.h>          // Các hàm liên quan đến tên miền ( gethostbyname )
#include <unistd.h>         // Các hàm liên quan đến socket ( close )
#include <arpa/inet.h>      // Chuyển đổi địa chỉ IP (inet_ntop, inet_pton)

// Chạy trên Linux, WSL
// g++ tcp_client.cpp -o tcp_client.exe
// ./tcp_client.exe <hostname> <port>

int main(int argc, char *argv[]) {
    if(argc != 3) { // 3 tham số đầu vào: tên chương trình, hostname, port
        std::cerr << "Usage: " << argv[0] << " <hostname> <port>" << std::endl;
        return 1;
    }

    const char *hostname = argv[1]; // Tên miền
    const char *port = argv[2]; // Cổng

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints); // Khởi tạo cấu trúc addrinfo
    hints.ai_family = AF_INET; // Chỉ định IPv4
    hints.ai_socktype = SOCK_STREAM; // Chỉ định kiểu socket là TCP

    int status; // Trạng thái trả về của hàm getaddrinfo

    status = getaddrinfo(hostname, port, &hints, &res); // Lấy thông tin địa chỉ từ tên miền và cổng

    if(status != 0) { // Kiểm tra lỗi
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return 1;
    }

    char ipstr[INET_ADDRSTRLEN]; // Chuỗi chứa địa chỉ IP ip string, INET_ADDRSTRLEN là kích thước tối đa của địa chỉ IPv4 dạng chuỗi

    inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipstr, sizeof ipstr); // Chuyển đổi địa chỉ IP từ nhị phân sang chuỗi

    std::cout << "IP Address: " << ipstr << std::endl;

    int socket_fd;  // Mô tả socket Socket File Descriptor
    socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Tạo socket
    if(socket_fd == -1) {
        std::cerr << "Socket error: " << strerror(errno) << std::endl; // Kiểm tra lỗi
        freeaddrinfo(res); // Giải phóng bộ nhớ
        return 1;
    }

    if(connect(socket_fd, res->ai_addr, res->ai_addrlen) == -1) { // Kết nối đến server
        std::cerr << "Connect error: " << strerror(errno) << std::endl; // Kiểm tra lỗi
        close(socket_fd); // Đóng socket
        freeaddrinfo(res); // Giải phóng bộ nhớ
        return 1;
    }

    std::string message = "GET / HTTP/1.1\r\nHost: ";
    message += hostname;
    message += "\r\nConnection: close\r\n\r\n";

    if(send(socket_fd, message.c_str(), message.length(), 0) == -1) {
        std::cerr << "Send error: " << strerror(errno) << std::endl; // Kiểm tra lỗi
        close(socket_fd); // Đóng socket
        freeaddrinfo(res); // Giải phóng bộ nhớ
        return 1;
    }

    std::cout << "Sent message: " << message << std::endl;

    char buffer[1024]; // Bộ đệm để nhận dữ liệu
    size_t bytes_received; // Số byte đã nhận

    bytes_received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0); // Nhận dữ liệu từ server
    if(bytes_received == -1) {  // Lỗi
        std::cerr << "Receive error: " << strerror(errno) << std::endl;
        close(socket_fd); // Đóng socket
        freeaddrinfo(res); // Giải phóng bộ nhớ
        return 1;
    } else if(bytes_received == 0) {
        std::cout << "Server closed connection" << std::endl; // Server đã đóng kết nối
    } else {
        buffer[bytes_received] = '\0'; // Kết thúc chuỗi
        std::cout << "Received data: " << buffer << std::endl; // In dữ liệu nhận được
    }

    close(socket_fd); // Đóng socket
    freeaddrinfo(res); // Giải phóng bộ nhớ

    return 0;
}