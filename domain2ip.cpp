#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

int main(int agrc, char* argv[]) {

    /**
     *  struct addrinfo {
                int ai_flags;       // Thường là AI_CANONNAME
                int ai_family;      // Thường là AF_INET
                int ai_socktype;    // Loại socket
                int ai_protocol;    // Giao thứ giao vận
                socklen_t ai_addrlen;       // Chiều dài của ai_addr
                char* ai_canonname;         // Tên miền
                struct sockaddr* ai_addr;   // Địa chỉ socket đã phân giải
                struct addrinfo* ai_next;   // Con trỏ tới cấu trúc sau
        };
     */
    
    struct addrinfo *res, *p;

    /**
     * int getaddrinfo( 
     *              const char* nodename,               // Tên cần phân giải
     *              const char* servname,               // Dịch vụ hoặc cổng
     *              const struct addrinfo* hints,       // Cấu trúc gợi ý
     *              struct addrinfo** res               // Kết quả
     * );
     */
    int ret = getaddrinfo(argv[1], "http", NULL, &res);

    if( ret != 0) {
        cout << "Failed to get IP" << endl;
        return 1;
    }

    /*
    struct sockaddr_in6 {
        SOCKADDR_COMMON (sin6_);
        in_port_t sin6_port;        // Transport layer port
        uint32_t sin6_flowinfo;     // IPv6 flow information
        struct in6_addr sin6_addr;  // IPv6 address 
        uint32_t sin6_scope_id;     // IPv6 scope-id
    };
    */

    /*
        struct in_addr {
            in_addr_t s_addr; // địa chỉ IPv4 32 bit
        };

        struct sockaddr_in {
            uint8_t sin_len;            // độ dài cấu trúc địa chỉ (16 bytes)
            sa_family_t sin_family;     // họ địa chỉ IPv4 - AF_INET
            in_port_t sin_port;         // giá trị cổng
            struct in_addr sin_addr;    // 32 bit địa chỉ
            char sin_zero[8];           // không sử dụng
        };
    */

    p = res;
    while(p != NULL) {
        if(p->ai_family == AF_INET) {
            cout << "IPv4" << endl;
            struct sockaddr_in addr;
            memcpy(&addr, p->ai_addr, p->ai_addrlen);
            cout << "IP: " << inet_ntoa(addr.sin_addr) << endl;
        } else {
            cout << "IPv6" << endl;
            char buf[64];
            struct sockaddr_in6 addr;
            memcpy(&addr, p->ai_addr, p->ai_addrlen);
            cout << "IP: " << inet_ntop(p->ai_family, &addr.sin6_addr, buf, sizeof(addr)) << endl;
        }
        p = p->ai_next;
    }

    freeaddrinfo(res);
    
    return 0;
}