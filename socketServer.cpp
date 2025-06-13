#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <iostream>

using namespace std;

int main() {

    // Tạo socket TCP
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listener == -1) {
        cout << "Can't creat socket" << endl;
        return 1;
    } else {
        cout << "Create socket success" << endl;
    }

    // Khai báo cấu trúc địa chỉ của server
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    // Gắn địa chỉ với socket
    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        cout << "Bind() failed" << endl;
        return 1;
    }

    // Chuyển socket sang trạng thái chờ kết nối
    if(listen(listener, 5)) {
        cout << "Listen() failed." << endl;
        return 1;
    }

    cout << "Waiting Client..." << endl;

    // Chấp nhận kết nối trong hàm đợi
    int client = accept(listener, NULL, NULL);
    if(client == -1) {
        cout << "Accept() failed." << endl;
        return 1;
    }

    cout << "New client connected: " << client << endl;

    // Nhận dữ liệu từ client
    char buf[256];
    int ret = recv(client, buf, sizeof(buf), 0);
    if(ret <= 0) {
        cout << "Recv() failed." << endl;
        return 1;
    }

    if(ret < sizeof(buf)) {
        buf[ret] = 0;
    }
    cout << "Data receive: ";
    puts(buf);

    // Gửi dữ liệu sang client
    char *msg = "Hello from server";
    send(client, msg, strlen(msg), 0);

    // Đóng kết nối
    close(client);
    close(listener);

    return 0;
}