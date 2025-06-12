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

const int COMMAND_PORT = 5555;
const int MAX_CLIENTS = 32;
const int BUFFER_SIZE = 4096;

class FileTransferServer {
private:
    int server_socket;
    std::vector<std::thread> client_threads;
    std::mutex clients_mutex;
    int active_clients;

public:
    FileTransferServer() : server_socket(-1), active_clients(0) {}

    ~FileTransferServer() {
        cleanup();
    }

    bool initialize() {
        // Tạo socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            std::cerr << "Lỗi tạo socket: " << strerror(errno) << std::endl;
            return false;
        }

        // Thiết lập SO_REUSEADDR để tránh lỗi "Address already in use"
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            std::cerr << "Lỗi thiết lập socket option: " << strerror(errno) << std::endl;
            return false;
        }

        // Thiết lập địa chỉ server
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(COMMAND_PORT);

        // Bind socket
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Lỗi bind socket: " << strerror(errno) << std::endl;
            return false;
        }

        // Lắng nghe kết nối
        if (listen(server_socket, MAX_CLIENTS) == -1) {
            std::cerr << "Lỗi listen socket: " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "Server đang lắng nghe tại cổng " << COMMAND_PORT << std::endl;
        return true;
    }

    void run() {
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            // Chấp nhận kết nối
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket == -1) {
                std::cerr << "Lỗi accept: " << strerror(errno) << std::endl;
                continue;
            }

            // Kiểm tra số lượng client
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (active_clients >= MAX_CLIENTS) {
                    std::cout << "Đã đạt số lượng client tối đa. Từ chối kết nối." << std::endl;
                    close(client_socket);
                    continue;
                }
                active_clients++;
            }

            std::cout << "Client mới kết nối từ " << inet_ntoa(client_addr.sin_addr) 
                      << ":" << ntohs(client_addr.sin_port) << std::endl;

            // Tạo thread để xử lý client
            client_threads.emplace_back(&FileTransferServer::handleClient, this, client_socket);
        }
    }

private:
    void handleClient(int client_socket) {
        char buffer[BUFFER_SIZE];
        
        while (true) {
            memset(buffer, 0, BUFFER_SIZE);
            
            // Nhận lệnh từ client
            ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                std::cout << "Client ngắt kết nối" << std::endl;
                break;
            }

            std::string command(buffer);
            command = trim(command);
            
            std::cout << "Nhận lệnh: " << command << std::endl;

            if (command == "QUIT") {
                std::cout << "Client yêu cầu ngắt kết nối" << std::endl;
                break;
            }
            else if (command.substr(0, 3) == "GET") {
                handleGetCommand(client_socket, command);
            }
            else {
                // Lệnh không hợp lệ
                std::string response = "INVALID COMMAND";
                send(client_socket, response.c_str(), response.length(), 0);
            }
        }

        close(client_socket);
        
        // Giảm số lượng client đang hoạt động
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            active_clients--;
        }
        
        std::cout << "Đã đóng kết nối với client" << std::endl;
    }

    void handleGetCommand(int command_socket, const std::string& command) {
        // Parse lệnh GET FILENAME PORT
        std::istringstream iss(command);
        std::string get_cmd, filename, port_str;
        
        if (!(iss >> get_cmd >> filename >> port_str)) {
            std::string response = "INVALID COMMAND";
            send(command_socket, response.c_str(), response.length(), 0);
            return;
        }

        int data_port;
        try {
            data_port = std::stoi(port_str);
        } catch (const std::exception& e) {
            std::string response = "INVALID COMMAND";
            send(command_socket, response.c_str(), response.length(), 0);
            return;
        }

        std::cout << "Yêu cầu file: " << filename << " qua cổng: " << data_port << std::endl;

        // Kiểm tra file có tồn tại không
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::string response = "FILE NOT FOUND";
            send(command_socket, response.c_str(), response.length(), 0);
            return;
        }

        // Tạo socket cho kênh dữ liệu
        int data_server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (data_server_socket == -1) {
            std::string response = "SERVER ERROR";
            send(command_socket, response.c_str(), response.length(), 0);
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
            std::string response = "PORT BIND ERROR";
            send(command_socket, response.c_str(), response.length(), 0);
            close(data_server_socket);
            file.close();
            return;
        }

        // Listen trên data socket
        if (listen(data_server_socket, 1) == -1) {
            std::string response = "PORT LISTEN ERROR";
            send(command_socket, response.c_str(), response.length(), 0);
            close(data_server_socket);
            file.close();
            return;
        }

        std::cout << "Đang chờ kết nối dữ liệu tại cổng " << data_port << std::endl;

        // Chấp nhận kết nối dữ liệu
        struct sockaddr_in client_data_addr;
        socklen_t client_data_len = sizeof(client_data_addr);
        int data_socket = accept(data_server_socket, (struct sockaddr*)&client_data_addr, &client_data_len);
        
        if (data_socket == -1) {
            std::string response = "DATA CONNECTION FAILED";
            send(command_socket, response.c_str(), response.length(), 0);
            close(data_server_socket);
            file.close();
            return;
        }

        std::cout << "Kết nối dữ liệu được thiết lập, bắt đầu gửi file" << std::endl;

        // Gửi file qua kênh dữ liệu
        char file_buffer[BUFFER_SIZE];
        while (file.read(file_buffer, BUFFER_SIZE) || file.gcount() > 0) {
            ssize_t bytes_to_send = file.gcount();
            ssize_t bytes_sent = send(data_socket, file_buffer, bytes_to_send, 0);
            if (bytes_sent == -1) {
                std::cerr << "Lỗi gửi dữ liệu: " << strerror(errno) << std::endl;
                break;
            }
        }

        file.close();
        close(data_socket);
        close(data_server_socket);

        std::cout << "Hoàn thành gửi file" << std::endl;

        // Gửi phản hồi DONE qua kênh lệnh
        std::string response = "DONE";
        send(command_socket, response.c_str(), response.length(), 0);
    }

    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    void cleanup() {
        if (server_socket != -1) {
            close(server_socket);
        }
        
        // Đợi tất cả thread hoàn thành
        for (auto& t : client_threads) {
            if (t.joinable()) {
                t.detach(); // Detach thay vì join để tránh blocking
            }
        }
    }
};

int main() {
    FileTransferServer server;
    
    if (!server.initialize()) {
        std::cerr << "Khởi tạo server thất bại" << std::endl;
        return 1;
    }

    std::cout << "Server File Transfer đã sẵn sàng!" << std::endl;
    std::cout << "Hỗ trợ tối đa " << MAX_CLIENTS << " client đồng thời" << std::endl;
    std::cout << "Lệnh hỗ trợ:" << std::endl;
    std::cout << "  GET <filename> <port> - Lấy file qua cổng chỉ định" << std::endl;
    std::cout << "  QUIT - Ngắt kết nối" << std::endl;

    try {
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Lỗi server: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}