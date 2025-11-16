#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>


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



typedef void (*route_fn)(int client_fd, struct http_request *req);

struct route_entry {
    char *key;                
    route_fn handler;
    struct route_entry *next;
};

#define ROUTER_BUCKETS 1024

struct router {
    struct route_entry *buckets[ROUTER_BUCKETS];
};

static struct router global_router;


static unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static void router_init(struct router *r) {
    for (int i = 0; i < ROUTER_BUCKETS; ++i) r->buckets[i] = NULL;
}

static void router_add(struct router *r, const char *method, const char *path, route_fn handler) {
    size_t keylen = strlen(method) + 1 + strlen(path) + 1;
    char *key = malloc(keylen);
    if (!key) return;
    snprintf(key, keylen, "%s:%s", method, path);

    unsigned long h = hash_str(key) % ROUTER_BUCKETS;
    struct route_entry *e = malloc(sizeof(*e));
    e->key = key;
    e->handler = handler;
    e->next = r->buckets[h];
    r->buckets[h] = e;
}


static route_fn router_find(struct router *r, const char *method, const char *path) {
    char keybuf[1280];
    snprintf(keybuf, sizeof(keybuf), "%s:%s", method, path);
    unsigned long h = hash_str(keybuf) % ROUTER_BUCKETS;
    struct route_entry *e = r->buckets[h];
    while (e) {
        if (strcmp(e->key, keybuf) == 0) return e->handler;
        e = e->next;
    }
    return NULL;
}



static const char* get_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    if (strstr(path, ".txt"))  return "text/plain";
    return "application/octet-stream";
}

static char* load_file(const char *filepath, int *out_len) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc((size_t)len);
    if (!buffer) { fclose(f); return NULL; }

    size_t read = fread(buffer, 1, len, f);
    fclose(f);

    *out_len = (int)read;
    return buffer;
}

void send_response(int client_fd, struct http_response *res) {
    
    char header[4096];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: %s\r\n"
        "\r\n",
        res->status_code, res->status_text,
        res->content_type, res->content_length,
        res->connection ? res->connection : "close"
    );

   
    ssize_t w = write(client_fd, header, header_len);
    (void)w;
    if (res->body && res->content_length > 0) {
        ssize_t remaining = res->content_length;
        char *ptr = res->body;
        while (remaining > 0) {
            ssize_t wrote = write(client_fd, ptr, remaining);
            if (wrote <= 0) break;
            remaining -= wrote;
            ptr += wrote;
        }
    }
}



int parse_request(char *raw, struct http_request *req)
{
    memset(req, 0, sizeof(struct http_request));
    req->body = NULL;
    req->content_length = 0;

    const char *line_end = strstr(raw, "\r\n");
    if (!line_end) return -1;

    char request_line[512];
    int len = (int)(line_end - raw);
    if (len >= (int)sizeof(request_line)) return -1;
    strncpy(request_line, raw, len);
    request_line[len] = '\0';

    if (sscanf(request_line, "%9s %1023s %9s", req->method, req->path, req->version) != 3)
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
        len = (int)(next - p);
        if (len >= (int)sizeof(header)) return -1;
        strncpy(header, p, len);
        header[len] = '\0';

        if (strncasecmp(header, "Host:", 5) == 0) {
            sscanf(header + 5, "%255s", req->host);
        } else if (strncasecmp(header, "Content-Length:", 15) == 0) {
            req->content_length = atoi(header + 15);
        } else if (strncasecmp(header, "Connection:", 11) == 0) {
            char tmp[32];
            sscanf(header + 11, "%31s", tmp);
            req->connection = (strcasecmp(tmp, "close") == 0);
        }

        p = next + 2;
    }

    if (req->content_length > 0) {
        req->body = malloc(req->content_length + 1);
        if (!req->body) return -1;
        memcpy(req->body, p, req->content_length);
        req->body[req->content_length] = '\0';
    }

    return 0;
}


void handler_root(int client_fd, struct http_request *req) {
    const char *body = "<h1>Hello from your Threaded HTTP server!</h1>";

    struct http_response res;
    res.status_code = 200;
    res.status_text = "OK";
    res.content_type = "text/html";
    res.body = (char *)body;
    res.content_length = (int)strlen(body);
    res.connection = "close";

    send_response(client_fd, &res);
}

void handler_json(int client_fd, struct http_request *req) {
    const char *body = "{ \"message\": \"Hello JSON\" }";

    struct http_response res;
    res.status_code = 200;
    res.status_text = "OK";
    res.content_type = "application/json";
    res.body = (char *)body;
    res.content_length = (int)strlen(body);
    res.connection = "close";

    send_response(client_fd, &res);
}

void handler_echo(int client_fd, struct http_request *req) {
    
    if (!req->body) {
        const char *empty = "{}";
        struct http_response res;
        res.status_code = 200;
        res.status_text = "OK";
        res.content_type = "application/json";
        res.body = (char *)empty;
        res.content_length = (int)strlen(empty);
        res.connection = "close";
        send_response(client_fd, &res);
        return;
    }

    struct http_response res;
    res.status_code = 200;
    res.status_text = "OK";
    res.content_type = "application/json";
    res.body = req->body; // body is malloc'd in parse_request
    res.content_length = req->content_length;
    res.connection = "close";

    send_response(client_fd, &res);
}

void handler_time(int client_fd, struct http_request *req) {
    time_t t = time(NULL);
    char formatted[64];
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(formatted, sizeof(formatted), "%a, %d %b %Y %H:%M:%S %z", &tm);

    struct http_response res;
    res.status_code = 200;
    res.status_text = "OK";
    res.content_type = "text/plain";
    res.body = formatted;
    res.content_length = (int)strlen(formatted);
    res.connection = "close";

    send_response(client_fd, &res);
}


void handler_static(int client_fd, struct http_request *req) {
    char filepath[2048];
    snprintf(filepath, sizeof(filepath), ".%s", req->path);

   
    if (req->path[strlen(req->path) - 1] == '/') {
        size_t n = strlen(filepath);
        if (n + strlen("index.html") < sizeof(filepath)) {
            strncat(filepath, "index.html", sizeof(filepath) - n - 1);
        }
    }

    int file_len = 0;
    char *file_data = load_file(filepath, &file_len);

    struct http_response res;
    if (file_data) {
        res.status_code = 200;
        res.status_text = "OK";
        res.content_type = get_type(filepath);
        res.body = file_data;
        res.content_length = file_len;
        res.connection = "close";

        send_response(client_fd, &res);
        free(file_data);
    } else {
        const char *msg = "<h1>404 File Not Found</h1>";
        res.status_code = 404;
        res.status_text = "Not Found";
        res.content_type = "text/html";
        res.body = (char *)msg;
        res.content_length = (int)strlen(msg);
        res.connection = "close";
        send_response(client_fd, &res);
    }
}



void* handle_client(void *arg)
{
    int client_fd = *(int*)arg;
    free(arg);

    
    char buffer[16384];
    int n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    buffer[n] = '\0';

    struct http_request req;
    if (parse_request(buffer, &req) != 0) {
        
        const char *msg = "Bad Request";
        struct http_response res;
        res.status_code = 400;
        res.status_text = "Bad Request";
        res.content_type = "text/plain";
        res.body = (char *)msg;
        res.content_length = (int)strlen(msg);
        res.connection = "close";
        send_response(client_fd, &res);
        close(client_fd);
        return NULL;
    }

    
    route_fn handler = router_find(&global_router, req.method, req.path);
    if (handler) {
        handler(client_fd, &req);
    } else {
        
        handler_static(client_fd, &req);
    }

    if (req.body) free(req.body);
    close(client_fd);
    return NULL;
}



int main() {
   
    router_init(&global_router);
    router_add(&global_router, "GET", "/", handler_root);
    router_add(&global_router, "GET", "/json", handler_json);
    router_add(&global_router, "POST", "/echo", handler_echo);
    router_add(&global_router, "GET", "/time", handler_time);
   
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1000) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Threaded HTTP Server with router running at http://localhost:8080\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        int *fd_ptr = malloc(sizeof(int));
        if (!fd_ptr) {
            close(client_fd);
            continue;
        }
        *fd_ptr = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, fd_ptr) != 0) {
            perror("pthread_create");
            free(fd_ptr);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}

