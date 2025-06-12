#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>

const int UDP_PORT = 5555;
const int CLIENT_RESPONSE_PORT = 6666;
const int MAX_CLIENTS = 32;
const int BUFFER_SIZE = 4096;

class UDPFileTransferServer {
private:
    int udp_socket;
    std::vector<std::thread> worker_threads;
    std::mutex threads_mutex;
    int active_threads;
    bool running;

public:
    UDPFileTransferServer() : udp_socket(-1), active_threads(0), running(false) {}

    ~UDPFileTransferServer() {
        cleanup();
    }

    bool initialize() {
        // Tạo UDP socket
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket == -1) {
            std::cerr << "Lỗi tạo UDP socket: " << strerror(errno) << std::endl;
            return false;
        }

        // Thiết lập SO_REUSEADDR
        int opt = 1;
        if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            std::cerr << "Lỗi thiết lập socket option: " << strerror(errno) << std::endl;
            return false;
        }

        // Thiết lập địa chỉ server
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(UDP_PORT);

        // Bind UDP socket
        if (bind(udp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Lỗi bind UDP socket: " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "UDP Server đang lắng nghe tại cổng " << UDP_PORT << std::endl;
        running = true;
        return true;
    }

    void run() {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        while (running) {
            memset(buffer, 0, BUFFER_SIZE);
            
            // Nhận gói tin UDP
            ssize_t bytes_received = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0,
                                             (struct sockaddr*)&client_addr, &client_len);
            
            if (bytes_received == -1) {
                if (running) {
                    std::cerr << "Lỗi nhận UDP packet: " << strerror(errno) << std::endl;
                }
                continue;
            }

            std::string command(buffer);
            command = trim(command);
            
            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            std::cout << "Nhận lệnh từ " << client_ip << ": " << command << std::endl;

            // Kiểm tra số lượng thread đang hoạt động
            {
                std::lock_guard<std::mutex> lock(threads_mutex);
                if (active_threads >= MAX_CLIENTS) {
                    std::cout << "Đã đạt số lượng client tối đa. Bỏ qua yêu cầu từ " 
                              << client_ip << std::endl;
                    continue;
                }
                active_threads++;
            }

            // Tạo thread để xử lý yêu cầu
            worker_threads.emplace_back(&UDPFileTransferServer::handleRequest, 
                                       this, command, client_ip);
        }
    }

    void stop() {
        running = false;
        if (udp_socket != -1) {
            close(udp_socket);
        }
    }

private:
    void handleRequest(const std::string& command, const std::string& client_ip) {
        if (command.substr(0, 3) == "GET") {
            handleGetCommand(command, client_ip);
        } else {
            sendResponseToClient(client_ip, "INVALID COMMAND");
        }

        // Giảm số lượng thread đang hoạt động
        {
            std::lock_guard<std::mutex> lock(threads_mutex);
            active_threads--;
        }
    }

    void handleGetCommand(const std::string& command, const std::string& client_ip) {
        // Parse lệnh GET FILENAME PORT
        std::istringstream iss(command);
        std::string get_cmd, filename, port_str;
        
        if (!(iss >> get_cmd >> filename >> port_str)) {
            sendResponseToClient(client_ip, "INVALID COMMAND");
            return;
        }

        int data_port;
        try {
            data_port = std::stoi(port_str);
            if (data_port <= 0 || data_port > 65535) {
                throw std::invalid_argument("Invalid port range");
            }
        } catch (const std::exception& e) {
            sendResponseToClient(client_ip, "INVALID COMMAND");
            return;
        }

        std::cout << "Xử lý yêu cầu file: " << filename << " qua cổng: " << data_port 
                  << " cho client: " << client_ip << std::endl;

        // Kiểm tra file có tồn tại không
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "File không tồn tại: " << filename << std::endl;
            sendResponseToClient(client_ip, "FILE NOT FOUND");
            return;
        }

        // Tạo TCP socket cho kênh dữ liệu
        int data_server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (data_server_socket == -1) {
            std::cerr << "Lỗi tạo data socket: " << strerror(errno) << std::endl;
            sendResponseToClient(client_ip, "SERVER ERROR");
            file.close();
            return;
        }

        // Thiết lập SO_REUSEADDR cho data socket
        int opt = 1;
        setsockopt(data_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Bind data socket
        struct sockaddr_in data_addr;
        memset(&data_addr, 0, sizeof(data_addr));
        data_addr.sin_family = AF_INET;
        data_addr.sin_addr.s_addr = INADDR_ANY;
        data_addr.sin_port = htons(data_port);

        if (bind(data_server_socket, (struct sockaddr*)&data_addr, sizeof(data_addr)) == -1) {
            std::cerr << "Lỗi bind data socket tại cổng " << data_port 
                      << ": " << strerror(errno) << std::endl;
            sendResponseToClient(client_ip, "PORT BIND ERROR");
            close(data_server_socket);
            file.close();
            return;
        }

        // Listen trên data socket
        if (listen(data_server_socket, 1) == -1) {
            std::cerr << "Lỗi listen data socket: " << strerror(errno) << std::endl;
            sendResponseToClient(client_ip, "PORT LISTEN ERROR");
            close(data_server_socket);
            file.close();
            return;
        }

        std::cout << "Đang chờ kết nối dữ liệu từ " << client_ip 
                  << " tại cổng " << data_port << std::endl;

        // Thiết lập timeout cho accept
        struct timeval timeout;
        timeout.tv_sec = 30;  // 30 giây timeout
        timeout.tv_usec = 0;
        setsockopt(data_server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Chấp nhận kết nối dữ liệu
        struct sockaddr_in client_data_addr;
        socklen_t client_data_len = sizeof(client_data_addr);
        int data_socket = accept(data_server_socket, (struct sockaddr*)&client_data_addr, 
                                &client_data_len);
        
        if (data_socket == -1) {
            std::cerr << "Lỗi accept data connection: " << strerror(errno) << std::endl;
            sendResponseToClient(client_ip, "DATA CONNECTION FAILED");
            close(data_server_socket);
            file.close();
            return;
        }

        std::cout << "Kết nối dữ liệu được thiết lập với " << client_ip 
                  << ", bắt đầu gửi file: " << filename << std::endl;

        // Gửi file qua kênh dữ liệu
        char file_buffer[BUFFER_SIZE];
        size_t total_bytes_sent = 0;
        
        while (file.read(file_buffer, BUFFER_SIZE) || file.gcount() > 0) {
            ssize_t bytes_to_send = file.gcount();
            ssize_t bytes_sent = send(data_socket, file_buffer, bytes_to_send, 0);
            
            if (bytes_sent == -1) {
                std::cerr << "Lỗi gửi dữ liệu: " << strerror(errno) << std::endl;
                break;
            }
            total_bytes_sent += bytes_sent;
        }

        file.close();
        close(data_socket);
        close(data_server_socket);

        std::cout << "Hoàn thành gửi file " << filename << " (" << total_bytes_sent 
                  << " bytes) cho " << client_ip << std::endl;

        // Gửi phản hồi DONE đến client qua cổng 6666
        sendResponseToClient(client_ip, "DONE");
    }

    void sendResponseToClient(const std::string& client_ip, const std::string& message) {
        // Tạo TCP socket để gửi phản hồi
        int response_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (response_socket == -1) {
            std::cerr << "Lỗi tạo response socket: " << strerror(errno) << std::endl;
            return;
        }

        // Thiết lập địa chỉ client
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(CLIENT_RESPONSE_PORT);
        
        if (inet_pton(AF_INET, client_ip.c_str(), &client_addr.sin_addr) <= 0) {
            std::cerr << "Địa chỉ IP không hợp lệ: " << client_ip << std::endl;
            close(response_socket);
            return;
        }

        // Thiết lập timeout cho connect
        struct timeval timeout;
        timeout.tv_sec = 10;  // 10 giây timeout
        timeout.tv_usec = 0;
        setsockopt(response_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // Kết nối đến client
        if (connect(response_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1) {
            std::cerr << "Lỗi kết nối đến client " << client_ip << ":" 
                      << CLIENT_RESPONSE_PORT << " - " << strerror(errno) << std::endl;
            close(response_socket);
            return;
        }

        // Gửi phản hồi
        if (send(response_socket, message.c_str(), message.length(), 0) == -1) {
            std::cerr << "Lỗi gửi phản hồi: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Đã gửi phản hồi '" << message << "' đến " << client_ip 
                      << ":" << CLIENT_RESPONSE_PORT << std::endl;
        }

        close(response_socket);
    }

    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    void cleanup() {
        running = false;
        
        if (udp_socket != -1) {
            close(udp_socket);
        }
        
        // Đợi tất cả thread hoàn thành
        for (auto& t : worker_threads) {
            if (t.joinable()) {
                t.detach(); // Detach để tránh blocking khi shutdown
            }
        }
    }
};

int main() {
    UDPFileTransferServer server;
    
    if (!server.initialize()) {
        std::cerr << "Khởi tạo server thất bại" << std::endl;
        return 1;
    }

    std::cout << "UDP-TCP File Transfer Server đã sẵn sàng!" << std::endl;
    std::cout << "Cấu hình:" << std::endl;
    std::cout << "  - Lắng nghe UDP tại cổng: " << UDP_PORT << std::endl;
    std::cout << "  - Gửi phản hồi đến client tại cổng: " << CLIENT_RESPONSE_PORT << std::endl;
    std::cout << "  - Hỗ trợ tối đa " << MAX_CLIENTS << " client đồng thời" << std::endl;
    std::cout << "Lệnh hỗ trợ:" << std::endl;
    std::cout << "  GET <filename> <port> - Lấy file qua cổng TCP chỉ định" << std::endl;
    std::cout << "\nNhấn Ctrl+C để dừng server..." << std::endl;

    try {
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Lỗi server: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}