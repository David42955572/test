#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.h"

#define MAIN_PORT 8080
#define PORT_RANGE_START 50000
#define PORT_RANGE_END 51000
#define MAX_CLIENTS (PORT_RANGE_END - PORT_RANGE_START)
#define back_server "192.168.56.103"

typedef struct {
    int port;
    int in_use;
} PortEntry;

PortEntry port_table[MAX_CLIENTS];

void init_port_table() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        port_table[i].port = PORT_RANGE_START + i;
        port_table[i].in_use = 0;
    }
}

int allocate_port() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!port_table[i].in_use) {
            port_table[i].in_use = 1;
            return port_table[i].port;
        }
    }
    return -1;
}

void release_port(int port) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_table[i].port == port) {
            port_table[i].in_use = 0;
            break;
        }
    }
}

int connect_to_backend() {
    int backend_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_socket < 0) {
        perror("建立後端 socket 失敗");
        return -1;
    }

    struct sockaddr_in backend_addr;
    backend_addr.sin_family = AF_INET;
    backend_addr.sin_port = htons(MAIN_PORT);  // 替換成後端伺服器的 port
    inet_pton(AF_INET, back_server, &backend_addr.sin_addr);  // 替換成後端伺服器的 IP

    if (connect(backend_socket, (struct sockaddr *)&backend_addr, sizeof(backend_addr)) < 0) {
        perror("連接後端伺服器失敗");
        close(backend_socket);
        return -1;
    }

    return backend_socket;
}
void transfer_data(int src_socket, int dest_socket, int face) {
    int sequence_counter = 1;
    int received_final_status = 0;

    #define RECV_BUF_SIZE 8192
    uint8_t recv_buffer[RECV_BUF_SIZE];
    int buffer_len = 0;

    while (received_final_status == 0) {
        if (buffer_len < RECV_BUF_SIZE) {
            int bytes = recv(src_socket, recv_buffer + buffer_len, RECV_BUF_SIZE - buffer_len, 0);
             if (bytes < 0) {
                perror("接收資料失敗 1");
                break;
            } else if (bytes == 0) {
                printf("對端關閉連接 2\n");
                break;
            }
            buffer_len += bytes;
        }

        // 嘗試解析完整封包
        while (1) {
            printf ("%d\n",buffer_len);
            if (buffer_len < 11) break;

            printf ("%d\n",buffer_len);
            uint8_t username_len = recv_buffer[3];
            int header_len = 3 + username_len + 8;
            if (buffer_len < header_len) break;

            printf ("%d\n",buffer_len);
            uint32_t data_len;
            memcpy(&data_len, recv_buffer + 3 + username_len + 4, 4);
            data_len = ntohl(data_len);
            
            int total_packet_len = header_len + data_len;
            if (buffer_len < total_packet_len) break;

            printf ("%d\n",buffer_len);
            ProtocolHeader header;
            if (parse_header(recv_buffer, &header) == -1) {
                fprintf(stderr, "協議解析失敗 3\n");
                // 無效封包也要跳過，不然卡死
                buffer_len = 0;
                break;
            }
            printf ("%d\n",buffer_len);
            
            uint8_t data[MAX_DATA_SIZE + 1];
            parse_data(recv_buffer + 3 + header.username_len + 8, header.length, data);
            printf("接收到數據 - Operation: %d, Status: %d, Sequence: %u, Data: %s\n",
               header.operation, header.status, header.sequence, data);

            int send_bytes = send(dest_socket, recv_buffer, total_packet_len, 0);
            if (send_bytes != total_packet_len) {
                perror("轉發資料失敗");
                break;
            }

            if ((header.status == 1 && header.sequence == sequence_counter) ||
                header.operation == 1 ||
                (header.operation == 3 && face == 1) ||
                (header.operation == 4 && face == 0) ||
                (header.operation == 5 && face == 0)) {
                received_final_status = 1;
            }

            sequence_counter++;

            memmove(recv_buffer, recv_buffer + total_packet_len, buffer_len - total_packet_len);
            buffer_len -= total_packet_len;
        }
    }
}


void *handle_dynamic_port(void *arg) {
    int dynamic_socket = ((int *)arg)[0];
    int allocated_port = ((int *)arg)[1];
    int port_to_release = allocated_port;  // 獨立儲存 port
    free(arg);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_socket = accept(dynamic_socket, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket < 0) {
        perror("accept 失敗");
        close(dynamic_socket);
        return NULL;
    }

    int backend_socket = connect_to_backend();
    if (backend_socket < 0) {
        close(client_socket);
        close(dynamic_socket);
        return NULL;
    }
    
    //uint8_t buf[MAX_DATA_SIZE];
    //int n;
   // while ((n = recv(client_socket, buf, sizeof(buf), 0)) > 0) {
     //   write(1, buf, n);  // 直接印出
    //}

    transfer_data(client_socket, backend_socket, 0);
    transfer_data(backend_socket, client_socket, 1);
    
    close(backend_socket);
    close(client_socket);
    close(dynamic_socket);

    //釋放port
    release_port( port_to_release);

    return NULL;
}


void *handle_main_port(void *arg) {
    int server_fd = *(int *)arg;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            perror("接受連接失敗");
            continue;
        }

        uint8_t buffer[MAX_DATA_SIZE];
        int bytes_received = recv(client_socket, buffer, MAX_DATA_SIZE, 0);
        if (bytes_received <= 0) {
            close(client_socket);
            continue;
        }

        ProtocolHeader header;
        if (parse_header(buffer, &header) == -1 || header.operation != 0) {
            fprintf(stderr, "協議解析失敗或操作碼錯誤\n");
            close(client_socket);
            continue;
        }

        int allocated_port = allocate_port();
        if (allocated_port == -1) {
            fprintf(stderr, "無可用 port\n");
            close(client_socket);
            continue;
        }

        printf("分配 port %d 給新的客戶端\n", allocated_port);

        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", allocated_port);

        uint8_t send_buffer[MAX_DATA_SIZE];
        int send_len = pack_message(0, 0, header.username, 0, (const uint8_t *)port_str, strlen(port_str), send_buffer);
        send(client_socket, send_buffer, send_len, 0);
        close(client_socket);

        // 建立新的 socket
        int dynamic_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (dynamic_socket < 0) {
            perror("建立動態 socket 失敗");
            // 釋放 port
            release_port(allocated_port);
            close(client_socket);
            continue;
        }

        // 設定動態 port 的 sockaddr
        struct sockaddr_in dynamic_addr;
        dynamic_addr.sin_family = AF_INET;
        dynamic_addr.sin_addr.s_addr = INADDR_ANY;
        dynamic_addr.sin_port = htons(allocated_port);

        if (bind(dynamic_socket, (struct sockaddr *)&dynamic_addr, sizeof(dynamic_addr)) < 0) {
            perror("bind 動態 port 失敗");
            release_port(allocated_port);
            close(dynamic_socket);
            close(client_socket);
            continue;
        }

        if (listen(dynamic_socket, 3) < 0) {
            perror("listen 動態 port 失敗");
            release_port(allocated_port);
            close(dynamic_socket);
            close(client_socket);
            continue;
        }

        // 把動態 socket 傳給執行緒
        pthread_t tid;
        int *new_socket = malloc(2 * sizeof(int));
        new_socket[0] = dynamic_socket;
        new_socket[1] = allocated_port;
        if (pthread_create(&tid, NULL, handle_dynamic_port, new_socket) != 0) {
            perror("pthread_create 失敗");
            release_port(allocated_port);
            close(dynamic_socket);
            close(client_socket);
            free(new_socket);
        }else{
            pthread_detach(tid);
        }

    }

    return NULL;
}

int main() {
    init_port_table();

    int main_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket == 0) {
        perror("建立 socket 失敗");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MAIN_PORT);

    if (bind(main_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("綁定失敗");
        exit(EXIT_FAILURE);
    }

    if (listen(main_socket, 3) < 0) {
        perror("監聽失敗");
        exit(EXIT_FAILURE);
    }

    printf("主執行序啟動於 port %d\n", MAIN_PORT);

    pthread_t main_thread;
    pthread_create(&main_thread, NULL, handle_main_port, &main_socket);
    pthread_join(main_thread, NULL);

    return 0;
}
