#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>



struct http_request {
    char method[10];
    char path[1024];
    char version[10];

    char host[256];
    int connection;

    char *body;
    int content_length;
};

struct http_response {
    int status_code;
    const char *status_text;

    const char *content_type;
    int content_length;

    const char *connection;

    char *body;
};



void send_response(int client_fd, struct http_response *res) {
    char header[4096];

    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: %s\r\n"
        "\r\n",
        res->status_code,
        res->status_text,
        res->content_type,
        res->content_length,
        res->connection
    );

    write(client_fd, header, header_len);
    write(client_fd, res->body, res->content_length);
}



int parse_request(char *raw, struct http_request *req)
{
    memset(req, 0, sizeof(struct http_request));

    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char request_line[512];
    int len = line_end - raw;
    strncpy(request_line, raw, len);
    request_line[len] = '\0';

    if (sscanf(request_line, "%s %s %s", req->method, req->path, req->version) != 3)
        return -1;

    const char *p = line_end + 2;

    while (1) {
        const char *next = strstr(p, "\r\n");
        if (!next) return -1;

        if (next == p) {
            p += 2;
            break;
        }

        char header[512];
        len = next - p;
        strncpy(header, p, len);
        header[len] = '\0';

        if (strncasecmp(header, "Host:", 5) == 0) {
            sscanf(header + 5, "%255s", req->host);
        }

        if (strncasecmp(header, "Content-Length:", 15) == 0) {
            req->content_length = atoi(header + 15);
        }

        if (strncasecmp(header, "Connection:", 11) == 0) {
            char tmp[32];
            sscanf(header + 11, "%31s", tmp);
            req->connection = (strcasecmp(tmp, "close") == 0);
        }

        p = next + 2;
    }

    if (req->content_length > 0) {
        req->body = malloc(req->content_length + 1);
        memcpy(req->body, p, req->content_length);
        req->body[req->content_length] = '\0';
    }

    return 0;
}


const char* get_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    if (strstr(path, ".txt"))  return "text/plain";
    return "application/octet-stream";
}



char* load_file(const char *filepath, int *out_len) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(len);
    fread(buffer, 1, len, f);
    fclose(f);

    *out_len = len;
    return buffer;
}




void* handle_client(void *arg)
{
    int client_fd = *(int*)arg;
    free(arg);

    char buffer[8192];
    int n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }

    buffer[n] = '\0';

    struct http_request req;
    parse_request(buffer, &req);

    struct http_response res;
    char filepath[2048];

    

    if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/echo") == 0) {

        res.status_code = 200;
        res.status_text = "OK";
        res.content_type = "application/json";
        res.body = req.body;
        res.content_length = req.content_length;
        res.connection = "close";

        send_response(client_fd, &res);
    }

    else if(strcmp(req.path,"/") == 0) {

        char *body = "<h1>Hello from your Threaded HTTP server!</h1>";

        res.status_code = 200;
        res.status_text = "OK";
        res.content_type = "text/html";
        res.body = body;
        res.content_length = strlen(body);
        res.connection = "close";

        send_response(client_fd, &res);

    }

    else if (strcmp(req.path, "/json") == 0) {

        char *body = "{ \"message\": \"Hello JSON\" }";

        res.status_code = 200;
        res.status_text = "OK";
        res.content_type = "application/json";
        res.body = body;
        res.content_length = strlen(body);
        res.connection = "close";

        send_response(client_fd, &res);
    }

    else if(strcmp(req.path,"/time") == 0) {

        time_t t = time(NULL);
        char *formatted = ctime(&t);
        formatted[strlen(formatted) - 1] = '\0';

        res.status_code = 200;
        res.status_text = "OK";
        res.content_type = "text/plain";
        res.body = formatted;
        res.content_length = strlen(formatted);
        res.connection = "close";

        send_response(client_fd, &res);
    }

    else {
        snprintf(filepath, sizeof(filepath), ".%s", req.path);

        int file_len = 0;
        char *file_data = load_file(filepath, &file_len);

        if (file_data) {
            res.status_code = 200;
            res.status_text = "OK";
            res.content_type = get_type(filepath);
            res.body = file_data;
            res.content_length = file_len;
        } else {
            char *msg = "<h1>404 File Not Found</h1>";
            res.status_code = 404;
            res.status_text = "Not Found";
            res.content_type = "text/html";
            res.body = msg;
            res.content_length = strlen(msg);
        }

        res.connection = "close";

        send_response(client_fd, &res);
        free(file_data);
    }

    close(client_fd);
    return NULL;
}



int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 1000);

    printf("Threaded HTTP Server running at http://localhost:8080\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        int *fd_ptr = malloc(sizeof(int));
        *fd_ptr = client_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, fd_ptr);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}

