#ifndef HTTPSERVER_H
#define HTTPSERVER_H

typedef void (*ApiHandler)(const char *method, const char *path, const char *body, char *responseBuffer, int bufferSize);

void startHttpServer(int port, ApiHandler handler);

#endif // HTTPSERVER_H
