/*
   httpserver.c - minimal HTTP/1.1 server for the IDRS web command center.

   Cross-platform: Winsock2 on Windows (link with -lws2_32), BSD/POSIX
   sockets on Linux and macOS.

   Design: single-threaded, iterative. Connections are accepted and
   served one at a time (the API handler mutates global graph/heap
   state, so serialising requests keeps that state consistent without
   locks). Each response is sent with "Connection: close".
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "httpserver.h"

#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET sock_t;
  typedef int socklen_compat;
  #define CLOSESOCK(s) closesocket(s)
  #define SEND_FLAGS 0
#else
  #include <unistd.h>
  #include <signal.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  typedef int sock_t;
  typedef socklen_t socklen_compat;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR   (-1)
  #define CLOSESOCK(s) close(s)
  #ifdef MSG_NOSIGNAL
    #define SEND_FLAGS MSG_NOSIGNAL
  #else
    #define SEND_FLAGS 0
  #endif
#endif

#define BUFFER_SIZE 131072   /* max request (and response) size */

/* send() may write fewer bytes than asked; loop until everything is out. */
static void send_all(sock_t fd, const char *data, size_t len) {
    while (len > 0) {
        int n = send(fd, data, (int)len, SEND_FLAGS);
        if (n <= 0) return;
        data += n;
        len -= (size_t)n;
    }
}

/* Read a full request: headers, then Content-Length bytes of body.
   Returns total bytes read (buffer is NUL-terminated), or -1 on error. */
static int read_request(sock_t fd, char *buf, int cap) {
    int total = 0;
    char *hdr_end = NULL;
    while (total < cap - 1) {
        int n = recv(fd, buf + total, cap - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';
        if (!hdr_end) hdr_end = strstr(buf, "\r\n\r\n");
        if (hdr_end) {
            long content_len = 0;
            const char *cl = strstr(buf, "Content-Length:");
            if (!cl) cl = strstr(buf, "content-length:");
            if (cl && cl < hdr_end) content_len = strtol(cl + 15, NULL, 10);
            int body_have = total - (int)((hdr_end + 4) - buf);
            if (body_have >= content_len) break;
        }
    }
    if (total <= 0) return -1;
    buf[total] = '\0';
    return total;
}

static const char *content_type_for(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".css") == 0)  return "text/css";
    if (strcmp(dot, ".js") == 0)   return "application/javascript";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".svg") == 0)  return "image/svg+xml";
    if (strcmp(dot, ".png") == 0)  return "image/png";
    if (strcmp(dot, ".ico") == 0)  return "image/x-icon";
    return "application/octet-stream";
}

/* Returns 1 if the file was found and sent. */
static int serve_static(sock_t fd, const char *raw_path) {
    char filepath[1024] = "public/index.html";
    if (strcmp(raw_path, "/") != 0 && strcmp(raw_path, "/index.html") != 0) {
        if (strstr(raw_path, "..")) return 0;          /* no directory traversal */
        snprintf(filepath, sizeof(filepath), ".%s", raw_path);
        char *q = strchr(filepath, '?');
        if (q) *q = '\0';
    }
    FILE *f = fopen(filepath, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char header[512];
    int hl = snprintf(header, sizeof(header),
                      "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n"
                      "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
                      content_type_for(filepath), fsize);
    send_all(fd, header, (size_t)hl);

    char chunk[8192];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) send_all(fd, chunk, n);
    fclose(f);
    return 1;
}

void startHttpServer(int port, ApiHandler handler) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return;
    }
#else
    signal(SIGPIPE, SIG_IGN);   /* a client closing early must not kill the server */
#endif

    sock_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        goto cleanup_wsa;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed on port %d (is another server already running?).\n", port);
        CLOSESOCK(server_fd);
        goto cleanup_wsa;
    }
    if (listen(server_fd, 10) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        CLOSESOCK(server_fd);
        goto cleanup_wsa;
    }

    printf("Tactical Command Center Server listening on http://localhost:%d ...\n", port);
    fflush(stdout);

    char *buffer = malloc(BUFFER_SIZE);
    char *response_body = malloc(BUFFER_SIZE);
    char *http_response = malloc(BUFFER_SIZE + 1024);
    if (!buffer || !response_body || !http_response) {
        printf("Out of memory.\n");
        free(buffer); free(response_body); free(http_response);
        CLOSESOCK(server_fd);
        goto cleanup_wsa;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_compat client_len = sizeof(client_addr);
        sock_t client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == INVALID_SOCKET) continue;

        if (read_request(client_fd, buffer, BUFFER_SIZE) <= 0) {
            CLOSESOCK(client_fd);
            continue;
        }

        char method[16] = {0}, raw_path[512] = {0};
        sscanf(buffer, "%15s %511s", method, raw_path);

        if (strcmp(method, "OPTIONS") == 0) {
            const char *cors_res = "HTTP/1.1 204 No Content\r\n"
                                   "Access-Control-Allow-Origin: *\r\n"
                                   "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                   "Access-Control-Allow-Headers: Content-Type\r\n"
                                   "Connection: close\r\n\r\n";
            send_all(client_fd, cors_res, strlen(cors_res));
            CLOSESOCK(client_fd);
            continue;
        }

        char *body = strstr(buffer, "\r\n\r\n");
        body = body ? body + 4 : "";

        /* Static files: "/", "/index.html", "/public/..." */
        if (strcmp(method, "GET") == 0 &&
            (strcmp(raw_path, "/") == 0 || strcmp(raw_path, "/index.html") == 0 ||
             strncmp(raw_path, "/public/", 8) == 0)) {
            if (serve_static(client_fd, raw_path)) {
                CLOSESOCK(client_fd);
                continue;
            }
        }

        if (strncmp(raw_path, "/api/", 5) == 0) {
            response_body[0] = '\0';
            handler(method, raw_path, body, response_body, BUFFER_SIZE);
            size_t blen = strlen(response_body);
            int n = snprintf(http_response, BUFFER_SIZE + 1024,
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json; charset=utf-8\r\n"
                             "Content-Length: %lu\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Access-Control-Allow-Headers: Content-Type\r\n"
                             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                             "Connection: close\r\n\r\n%s", (unsigned long)blen, response_body);
            if (n > 0) send_all(client_fd, http_response, strlen(http_response));
        } else {
            const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n"
                                    "Content-Length: 15\r\nConnection: close\r\n\r\n404 Not Found\r\n";
            send_all(client_fd, not_found, strlen(not_found));
        }
        CLOSESOCK(client_fd);
    }

    /* not reached */
    free(buffer); free(response_body); free(http_response);
    CLOSESOCK(server_fd);
cleanup_wsa:
#ifdef _WIN32
    WSACleanup();
#endif
    return;
}
