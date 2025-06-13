#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>

using namespace std;

int main() {

    // Tạo socket TCP
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(client == -1) {
        cout << "Can't creat socket" << endl;
        return 1;
    } else {
        cout << "Create socket success" << endl;
    }

    // Khai báo địa chỉ của server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(9000);

    // Ket noi den server
    int res = connect(client, (struct sockaddr *)&addr, sizeof(addr));
    if(res == -1) {
        cout << "Can't connect with server" << endl;
        return 1;
    }

    // Gửi dữ liệu tới server
    char *msg = "Hello from client";
    send(client, msg, strlen(msg), 0);

    // Nhận tin nhắn từ server
    char buf[2048];
    int len = recv(client, buf, sizeof(buf), 0);
    buf[len] = 0;
    cout << "Data receive: " << buf << endl;

    // Đóng kết nối
    close(client);

    return 0;
}