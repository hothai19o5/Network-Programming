#include <iostream>
#include <string>
#include <cstring>          // Làm việc với chuỗi C-style (strlen, c_str, memset)
#include <sys/socket.h>     // Thư viện cho socket (socket, connect, send, recv)
#include <netinet/in.h>     // sockaddr_in
#include <unistd.h>         // close

// Tạo socket -> Cấu hình địa chỉ và cổng -> Bind socket vào địa chỉ cổng -> Lắng nghe kết nối -> Chấp nhận kết nối -> Gửi thông báo -> Nhận tin nhắn từ client -> Đóng socket

const int PORT = 8888;
const int BUFFER_SIZE = 1024;

int main() {
    // Tạo socket - socket()
    int server_fd = socket(AF_INET,         // Ipv4
                            SOCK_STREAM,    // Sử dụng TCP
                            0               // Sử dụng giao thức mặc định cho TCP
                        );
    if(server_fd == 0) {
        perror("Socket Failed");
        return 1;
    }
    // Cấu hình địa chỉ, cổng - sockaddr_in
    struct sockaddr_in address;
    address.sin_family = AF_INET;           // Ipv4
    address.sin_addr.s_addr = INADDR_ANY;   // Lắng nghe trên tất cả các interface mạng của máy chủ 
    address.sin_port = htons(PORT);         // Chuyển đổi số cổng từ host byte order sang network byte order
    // Gán địa chỉ và cổng vào socket - bind()
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind Failed");
        close(server_fd);
        return 1;
    }
    // Lắng nghe kết nối - listen()
    if(listen(server_fd, 3) < 0) {          // Số lượng kết nối tối đa có thể được xếp hàng đợi (backlog queue). Nếu có nhiều hơn 3 client cố gắng kết nối cùng một lúc, các kết nối bổ sung sẽ bị từ chối.
        perror("Listen Failed");
        close(server_fd);
        return 1;
    }
    std::cout << "Server listening on port: " << PORT << "..." << std::endl;
    // Chấp nhận kết nối - accept()
    // Hàm này sẽ chặn (block) cho đến khi một client kết nối.
    int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&address);
    if(new_socket < 0) {
        perror("Accept Failed");
        close(server_fd);
        return 1;
    }
    // Gửi thông báo kết nối thành công - send()
    const char* welcome_message = "Welcome to my TCP server";
    send(new_socket, welcome_message, strlen(welcome_message), 0);
    std::cout << "Send welcome message: " << welcome_message << std::endl;
    // Nhận tin nhắn từ client - recv()
    char buffer[BUFFER_SIZE] = {0};
    int val_read = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
    if(val_read < 0) {
        perror("Recv Failed");
        close(new_socket);
        close(server_fd);
        return 1;
    }
    buffer[val_read] = '\0';                // Thêm ký tự kết thúc
    std::cout << "Recived message from client: " << buffer << std::endl;
    // Đóng socket - close()
    close(new_socket);
    close(server_fd);
    
    return 0;
}   