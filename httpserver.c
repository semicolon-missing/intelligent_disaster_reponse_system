#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include "httpserver.h"

#define BUFFER_SIZE 65536

void startHttpServer(int port, ApiHandler handler) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return;
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed on port %d.\n", port);
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    printf("Tactical Command Center Server listening on http://localhost:%d ...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == INVALID_SOCKET) continue;

        char *buffer = malloc(BUFFER_SIZE);
        if (!buffer) {
            closesocket(client_fd);
            continue;
        }
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            free(buffer);
            closesocket(client_fd);
            continue;
        }

        char method[16] = {0}, raw_path[512] = {0};
        sscanf(buffer, "%15s %511s", method, raw_path);

        if (strcmp(method, "OPTIONS") == 0) {
            const char *cors_res = "HTTP/1.1 204 No Content\r\n"
                                   "Access-Control-Allow-Origin: *\r\n"
                                   "Access-Control-Allow-Methods: GET, POST, OPTIONS, PUT, DELETE\r\n"
                                   "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
            send(client_fd, cors_res, strlen(cors_res), 0);
            free(buffer);
            closesocket(client_fd);
            continue;
        }

        char *body = strstr(buffer, "\r\n\r\n");
        if (body) body += 4;
        else body = "";

        // Serve static files
        if (strcmp(method, "GET") == 0 && (strcmp(raw_path, "/") == 0 || strncmp(raw_path, "/public/", 8) == 0 || strcmp(raw_path, "/index.html") == 0)) {
            char filepath[1024] = "public/index.html";
            if (strcmp(raw_path, "/") != 0 && strcmp(raw_path, "/index.html") != 0) {
                snprintf(filepath, sizeof(filepath), ".%s", raw_path);
            }
            FILE *f = fopen(filepath, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                char header[512];
                const char* contentType = "text/html; charset=utf-8";
                if (strstr(filepath, ".css")) contentType = "text/css";
                else if (strstr(filepath, ".js")) contentType = "application/javascript";
                else if (strstr(filepath, ".json")) contentType = "application/json";
                else if (strstr(filepath, ".svg")) contentType = "image/svg+xml";
                else if (strstr(filepath, ".png")) contentType = "image/png";
                
                snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nAccess-Control-Allow-Origin: *\r\n\r\n", contentType, fsize);
                send(client_fd, header, strlen(header), 0);
                
                char *filebuf = malloc(fsize);
                if (filebuf) {
                    fread(filebuf, 1, fsize, f);
                    send(client_fd, filebuf, fsize, 0);
                    free(filebuf);
                }
                fclose(f);
                free(buffer);
                closesocket(client_fd);
                continue;
            }
        }
        
        // API Handler
        if (strncmp(raw_path, "/api/", 5) == 0) {
            char *response_body = malloc(BUFFER_SIZE);
            if (response_body) {
                memset(response_body, 0, BUFFER_SIZE);
                handler(method, raw_path, body, response_body, BUFFER_SIZE);
                
                char *http_response = malloc(BUFFER_SIZE + 1024);
                if (http_response) {
                    snprintf(http_response, BUFFER_SIZE + 1024,
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json; charset=utf-8\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Headers: Content-Type\r\n"
                             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n\r\n%s", response_body);
                    send(client_fd, http_response, strlen(http_response), 0);
                    free(http_response);
                }
                free(response_body);
            }
        } else {
            const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found\r\n";
            send(client_fd, not_found, strlen(not_found), 0);
        }
        
        free(buffer);
        closesocket(client_fd);
    }
    closesocket(server_fd);
    WSACleanup();
}
