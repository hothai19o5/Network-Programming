#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <malloc.h>
#include <dirent.h>

typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct in_addr IN_ADDR;

const char* rootFolder = "/";
char currentPath[8192] = { 0 };

// Passive mode configuration
const int PASSIVE_PORT_MIN = 20000;
const int PASSIVE_PORT_MAX = 21000;
static int current_passive_port = PASSIVE_PORT_MIN;

void AppendStr(char** output, const char* str)
{
    char* tmp = *output;
    int oldlen = tmp == NULL ? 0 : strlen(tmp);
    int newlen = oldlen + strlen(str) + 1;
    tmp = realloc(tmp, newlen);
    tmp[newlen - 1] = 0;
    sprintf(tmp + oldlen, "%s", str);
    *output = tmp;
}

void AppendInt(char** output, const int d)
{
    char str[1024] = { 0 };
    sprintf(str, "%d", d);
    char* tmp = *output;
    int oldlen = tmp == NULL ? 0 : strlen(tmp);
    int newlen = oldlen + strlen(str) + 1;
    tmp = realloc(tmp, newlen);
    tmp[newlen - 1] = 0;
    sprintf(tmp + oldlen, "%s", str);
    *output = tmp;
}

int dirsort(const struct dirent ** x, const struct dirent ** y)
{
    const struct dirent* a = *x;
    const struct dirent* b = *y;
    if (a->d_type == b->d_type)
    {
        return -strcmp(a->d_name, b->d_name);
    }else if (a->d_type == DT_DIR)
        return 1;
    else
        return -1;    
}

// Function to get next available passive port
int get_next_passive_port()
{
    int port = current_passive_port;
    current_passive_port++;
    if (current_passive_port > PASSIVE_PORT_MAX)
        current_passive_port = PASSIVE_PORT_MIN;
    return port;
}

// Function to setup passive mode data connection
int setup_passive_data_connection()
{
    int passive_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (passive_socket < 0) {
        return -1;
    }

    // Set SO_REUSEADDR to avoid "Address already in use" error
    int opt = 1;
    setsockopt(passive_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    SOCKADDR_IN passive_addr;
    passive_addr.sin_family = AF_INET;
    passive_addr.sin_addr.s_addr = INADDR_ANY;

    // Try to bind to a passive port
    int attempts = 0;
    int max_attempts = PASSIVE_PORT_MAX - PASSIVE_PORT_MIN;
    
    while (attempts < max_attempts) {
        int port = get_next_passive_port();
        passive_addr.sin_port = htons(port);
        
        if (bind(passive_socket, (SOCKADDR*)&passive_addr, sizeof(passive_addr)) == 0) {
            if (listen(passive_socket, 1) == 0) {
                return passive_socket;
            }
        }
        attempts++;
    }
    
    close(passive_socket);
    return -1;
}

void* ClientThread(void* param)
{
    int c = *((int*)param);
    free(param); // Free the allocated parameter
    
    int len = 0;
    int data_socket = -1;
    int passive_server_socket = -1;
    int is_passive_mode = 0;
    
    char* response = "220 LA THE VINH FTP SERVER READY\r\n";
    send(c, response, strlen(response), 0);
    
    do
    {
        char buffer[1024] = { 0 };
        int r = recv(c, buffer, sizeof(buffer) - 1, 0);
        if (r > 0)
        {
            printf("Received command: %s", buffer);
            
            if (strncmp(buffer, "USER", 4) == 0)
            {
                char* response = "331 User okay, please send password\r\n";
                send(c, response, strlen(response), 0);
            }
            else if (strncmp(buffer, "PASS", 4) == 0)
            {
                char* response = "230 User logged in\r\n";
                send(c, response, strlen(response), 0);
            }
            else if (strncmp(buffer, "SYST", 4) == 0)
            {
                char* response = "215 UNIX Type: L8\r\n";
                send(c, response, strlen(response), 0);
            }
            else if (strncmp(buffer, "FEAT", 4) == 0)
            {
                char* response = "211-Features:\r\n MDTM\r\n REST STREAM\r\n SIZE\r\n MLST type*;size*;modify*;perm*;\r\n MLSD\r\n PASV\r\n PORT\r\n UTF8\r\n211 End\r\n";
                send(c, response, strlen(response), 0);        
            }
            else if (strncmp(buffer, "OPTS", 4) == 0)
            {
                char* response = "200 OPTS UTF8 ON\r\n";
                send(c, response, strlen(response), 0);
            }
            else if (strncmp(buffer, "PWD", 3) == 0)
            {
                char response[65536] = { 0 };
                sprintf(response,"257 \"%s\" is current directory\r\n", currentPath);
                send(c, response, strlen(response), 0);
            }
            else if (strncmp(buffer, "TYPE", 4) == 0)
            {
                char* response = "200 Type set to I\r\n";
                send(c, response, strlen(response), 0);
            }
            else if (strncmp(buffer, "PORT", 4) == 0)
            {
                // Close any existing passive socket
                if (passive_server_socket > 0) {
                    close(passive_server_socket);
                    passive_server_socket = -1;
                }
                is_passive_mode = 0;
                
                for (int i = 0; i < strlen(buffer); i++)
                {
                    if (buffer[i] == ',')
                        buffer[i] = ' ';
                }
                char temp[1024] = { 0 };
                int ip1, ip2, ip3, ip4, prt1, prt2;
                sscanf(buffer,"%s%d%d%d%d%d%d", temp, &ip1, &ip2, &ip3, &ip4, &prt1, &prt2);
                memset(temp, 0, sizeof(temp));
                sprintf(temp, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
                short port = prt1 * 256 + prt2;
                
                data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                SOCKADDR_IN caddr;
                caddr.sin_family = AF_INET;
                caddr.sin_port = htons(port);
                caddr.sin_addr.s_addr = inet_addr(temp);
                
                int error = connect(data_socket, (SOCKADDR*)&caddr, sizeof(caddr));
                if (error != 0)
                {
                    close(data_socket);
                    data_socket = -1;
                    char* error = "425 Can't open data connection\r\n";
                    send(c, error, strlen(error), 0);
                }
                else
                {
                    char* response = "200 PORT command successful\r\n";
                    send(c, response, strlen(response), 0); 
                }            
            }
            else if (strncmp(buffer, "PASV", 4) == 0)
            {
                // Close any existing data socket from PORT mode
                if (data_socket > 0) {
                    close(data_socket);
                    data_socket = -1;
                }
                
                // Close any existing passive socket
                if (passive_server_socket > 0) {
                    close(passive_server_socket);
                    passive_server_socket = -1;
                }
                
                passive_server_socket = setup_passive_data_connection();
                if (passive_server_socket > 0) {
                    is_passive_mode = 1;
                    
                    // Get the port number that was bound
                    SOCKADDR_IN addr;
                    socklen_t addr_len = sizeof(addr);
                    getsockname(passive_server_socket, (SOCKADDR*)&addr, &addr_len);
                    int port = ntohs(addr.sin_port);
                    
                    // Get server IP (simplified - using 127.0.0.1 for localhost)
                    int ip1 = 127, ip2 = 0, ip3 = 0, ip4 = 1;
                    int port1 = port / 256;
                    int port2 = port % 256;
                    
                    char response[256] = { 0 };
                    sprintf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n", 
                            ip1, ip2, ip3, ip4, port1, port2);
                    send(c, response, strlen(response), 0);
                    
                    printf("Passive mode enabled on port %d\n", port);
                }
                else {
                    char* error = "425 Can't open passive connection\r\n";
                    send(c, error, strlen(error), 0);
                }
            }
            else if (strncmp(buffer, "MLSD", 4) == 0)
            {
                int actual_data_socket = -1;
                
                if (is_passive_mode && passive_server_socket > 0) {
                    // Accept connection in passive mode
                    SOCKADDR_IN client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    actual_data_socket = accept(passive_server_socket, (SOCKADDR*)&client_addr, &client_len);
                    printf("Accepted passive data connection\n");
                }
                else if (data_socket > 0) {
                    actual_data_socket = data_socket;
                }
                
                if (actual_data_socket > 0)
                {
                    char* response1 = "150 Opening ASCII mode data connection for directory listing\r\n";
                    send(c, response1, strlen(response1), 0); 

                    char* content = NULL;
                    struct dirent **namelist;
                    int n = scandir(currentPath, &namelist, NULL, dirsort);
                    if (n > 0)
                    {
                        while (n--) {
                            if (namelist[n]->d_type == DT_DIR)
                            {
                                AppendStr(&content, "type=dir;modify=20230808043247.034;perms=cplem; ");
                                AppendStr(&content, namelist[n]->d_name);
                                AppendStr(&content, "\r\n"); 
                            }
                            else
                            {
                                AppendStr(&content, "type=file;size=0;modify=20250531121709.494;perms=awr; ");
                                AppendStr(&content, namelist[n]->d_name);
                                AppendStr(&content, "\r\n"); 
                            }
                            free(namelist[n]);
                            namelist[n] = NULL;
                        }
                        free(namelist);
                        namelist = NULL;

                        if (content != NULL) {
                            int sent = 0;
                            int content_len = strlen(content);
                            while (sent < content_len)
                            {
                                int bytes_sent = send(actual_data_socket, content + sent, content_len - sent, 0);
                                if (bytes_sent <= 0) break;
                                sent += bytes_sent;
                            }
                            free(content);
                            content = NULL;
                        }
                    }
                    
                    close(actual_data_socket);
                    if (is_passive_mode) {
                        close(passive_server_socket);
                        passive_server_socket = -1;
                        is_passive_mode = 0;
                    } else {
                        data_socket = -1;
                    }
                    
                    char* response2 = "226 Directory send OK\r\n";
                    send(c, response2, strlen(response2), 0); 
                }
                else
                {
                    char* error = "425 Use PORT or PASV first\r\n";
                    send(c, error, strlen(error), 0);
                }                    
            }
            else if (strncmp(buffer,"CWD", 3) == 0)
            {
                char folder[1024] = { 0 };
                strcpy(folder, buffer + 4);
                while (folder[strlen(folder) - 1] == '\r' || folder[strlen(folder) - 1] == '\n')
                {
                    folder[strlen(folder) - 1] = 0;
                }

                if (currentPath[strlen(currentPath) - 1] == '/')
                {
                    sprintf(currentPath + strlen(currentPath), "%s", folder);
                }
                else
                {
                    sprintf(currentPath + strlen(currentPath), "/%s", folder);
                }
                char* response = "250 Directory successfully changed\r\n";
                send(c, response, strlen(response), 0); 
            }
            else if (strncmp(buffer,"STOR", 4) == 0)
            {
                char filename[1024] = { 0 };
                char path[65536] = { 0 };
                strcpy(filename, buffer + 5);
                while (filename[strlen(filename) - 1] == '\r' || filename[strlen(filename) - 1] == '\n')
                {
                    filename[strlen(filename) - 1] = 0;
                }

                if (currentPath[strlen(currentPath) - 1] == '/')
                {
                    sprintf(path, "%s%s", currentPath, filename);
                }
                else
                {
                    sprintf(path, "%s/%s", currentPath, filename);
                }
                
                int actual_data_socket = -1;
                
                if (is_passive_mode && passive_server_socket > 0) {
                    SOCKADDR_IN client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    actual_data_socket = accept(passive_server_socket, (SOCKADDR*)&client_addr, &client_len);
                }
                else if (data_socket > 0) {
                    actual_data_socket = data_socket;
                }
                
                FILE* f = fopen(path, "wb");
                if (f != NULL && actual_data_socket > 0)
                {
                    char* response1 = "150 Ok to send data\r\n";
                    send(c, response1, strlen(response1), 0); 
                    
                    int r = 0;
                    char temp[1024] = { 0 };
                    do
                    {
                        r = recv(actual_data_socket, temp, sizeof(temp), 0);
                        if (r > 0)
                        {
                            fwrite(temp, 1, r, f);
                        }
                    } while (r > 0);

                    fclose(f);
                    close(actual_data_socket);
                    
                    if (is_passive_mode) {
                        close(passive_server_socket);
                        passive_server_socket = -1;
                        is_passive_mode = 0;
                    } else {
                        data_socket = -1;
                    }

                    char* response2 = "226 Transfer complete\r\n";
                    send(c, response2, strlen(response2), 0); 
                }
                else
                {
                    if (f == NULL) {
                        char* error = "550 Cannot create file\r\n";
                        send(c, error, strlen(error), 0);
                    } else {
                        fclose(f);
                        char* error = "425 Use PORT or PASV first\r\n";
                        send(c, error, strlen(error), 0);
                    }
                }
            }
            else if (strncmp(buffer,"SIZE", 4) == 0)
            {
                char filename[1024] = { 0 };
                char path[65536] = { 0 };
                strcpy(filename, buffer + 5);
                while (filename[strlen(filename) - 1] == '\r' || filename[strlen(filename) - 1] == '\n')
                {
                    filename[strlen(filename) - 1] = 0;
                }

                if (currentPath[strlen(currentPath) - 1] == '/')
                {
                    sprintf(path, "%s%s", currentPath, filename);
                }
                else
                {
                    sprintf(path, "%s/%s", currentPath, filename);
                }
                
                FILE* f = fopen(path, "rb");
                if (f != NULL)
                {
                    fseek(f, 0, SEEK_END);
                    long fs = ftell(f);
                    fclose(f);
                    
                    char response[1024] = { 0 };
                    sprintf(response, "213 %ld\r\n", fs);
                    send(c, response, strlen(response), 0);
                }
                else
                {
                    char* error = "550 File not found\r\n";
                    send(c, error, strlen(error), 0);    
                }
            }
            else if (strncmp(buffer,"RETR", 4) == 0)
            {
                char filename[1024] = { 0 };
                char path[65536] = { 0 };
                strcpy(filename, buffer + 5);
                while (filename[strlen(filename) - 1] == '\r' || filename[strlen(filename) - 1] == '\n')
                {
                    filename[strlen(filename) - 1] = 0;
                }

                if (currentPath[strlen(currentPath) - 1] == '/')
                {
                    sprintf(path, "%s%s", currentPath, filename);
                }
                else
                {
                    sprintf(path, "%s/%s", currentPath, filename);
                }
                
                int actual_data_socket = -1;
                
                if (is_passive_mode && passive_server_socket > 0) {
                    SOCKADDR_IN client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    actual_data_socket = accept(passive_server_socket, (SOCKADDR*)&client_addr, &client_len);
                }
                else if (data_socket > 0) {
                    actual_data_socket = data_socket;
                }
                
                FILE* f = fopen(path, "rb");
                if (f != NULL && actual_data_socket > 0)
                {
                    char* response1 = "150 Opening BINARY mode data connection\r\n";
                    send(c, response1, strlen(response1), 0); 

                    char temp[1024] = { 0 };
                    int bytes_read;
                    while ((bytes_read = fread(temp, 1, sizeof(temp), f)) > 0)
                    {
                        send(actual_data_socket, temp, bytes_read, 0);
                    }
                    
                    fclose(f);
                    close(actual_data_socket);
                    
                    if (is_passive_mode) {
                        close(passive_server_socket);
                        passive_server_socket = -1;
                        is_passive_mode = 0;
                    } else {
                        data_socket = -1;
                    }

                    char* response2 = "226 Transfer complete\r\n";
                    send(c, response2, strlen(response2), 0); 
                }
                else
                {
                    if (f == NULL) {
                        char* error = "550 File not found\r\n";
                        send(c, error, strlen(error), 0);
                    } else {
                        fclose(f);
                        char* error = "425 Use PORT or PASV first\r\n";
                        send(c, error, strlen(error), 0);
                    }
                }
            }
            else if (strncmp(buffer,"CDUP", 4) == 0)
            {
                do
                {
                    currentPath[strlen(currentPath) - 1] = 0;
                } while (strlen(currentPath) > 0 && currentPath[strlen(currentPath) - 1] != '/');
                if (strlen(currentPath) == 0)
                {
                    strcpy(currentPath, "/");
                }
                char* response = "200 Directory successfully changed\r\n";
                send(c, response, strlen(response), 0); 
            }
            else if (strncmp(buffer,"QUIT", 4) == 0)
            {
                char* response = "221 Goodbye\r\n";
                send(c, response, strlen(response), 0);
                break;
            }
            else
            {
                char* error = "502 Command not implemented\r\n";
                send(c, error, strlen(error), 0);
            }
        }
        else
            break;
    } while (1);
    
    // Cleanup
    if (data_socket > 0) {
        close(data_socket);
    }
    if (passive_server_socket > 0) {
        close(passive_server_socket);
    }
    close(c);
    printf("Client disconnected\n");
    return NULL;
}

int main()
{
    sprintf(currentPath,"%s",rootFolder);
    printf("Starting FTP Server with PASSIVE mode support...\n");
    printf("Passive port range: %d-%d\n", PASSIVE_PORT_MIN, PASSIVE_PORT_MAX);
    
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        printf("Failed to create socket!\n");
        return 1;
    }
    
    // Set SO_REUSEADDR
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    SOCKADDR_IN saddr, caddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(8888);
    saddr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(s, (SOCKADDR*)&saddr, sizeof(saddr)) == 0)
    {
        listen(s, 10);
        printf("FTP Server listening on port 8888\n");
        
        while (1)
        {
            socklen_t clen = sizeof(caddr);
            printf("Waiting for connection...\n");
            int c = accept(s, (SOCKADDR*)&caddr, &clen);
            if (c >= 0)
            {
                printf("Client connected from %s:%d\n", 
                       inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
                       
                pthread_t pid;
                int* param = (int*)malloc(sizeof(int));
                *param = c;
                pthread_create(&pid, NULL, ClientThread, (void*)param);
                pthread_detach(pid); // Detach thread to avoid memory leaks
            }
            else
                break;
        }
    }
    else
        printf("Failed to bind to port 8888!\n");
        
    close(s);
    return 0;
}