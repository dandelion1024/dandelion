#include "dandelion.h"
#include "kmp.h"
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FINISH_REQ(req)                                                                 \
    req->is_finish = true;                                                              \
    close(req->client_sock);                                                            \
    return NULL;

int server_sock = -1;
unsigned short server_port = 0;
struct sockaddr_in server_sockaddr_in;
Request* req_list = NULL;
char doc_root[31] = "www";
char cgi_root[31] = "cgi-bin";

int main(int argc, char* argv[])
{
    socklen_t sockaddr_in_len = sizeof(struct sockaddr_in);
    Request* req = NULL;

    startup();
    printf("Dandelion running on port %u\n", server_port);

    signal(SIGINT, halt);

    while (true) {
        if (!(req = alloc_req())) {
            stop();
        }

        req->client_sock = accept(
            server_sock, (struct sockaddr*)&req->client_sockaddr_in, &sockaddr_in_len);

        if (req->client_sock == -1) {
            free(req);
            stop();
        }

        if (pthread_create(&req->thread_id, NULL, accept_request, (void*)req)) {
            perror("failed to create thread\n");
            free(req);
        }

        append_req(req_list, req);
    }

    stop();

    return 0;
}

void halt(int signo) { stop(); }

void stop()
{
    printf("stop server\n");

    if (server_sock != -1) {
        close(server_sock);
    }

    clear_all_reqs(req_list);
    exit(0);
}

void* accept_request(void* param)
{
    Request* req = (Request*)param;
    char buf[2048];
    int numchars = get_line(req->client_sock, buf, sizeof(buf));

    handle_req_line(buf, sizeof(buf), req);

    if (req->method != M_GET && req->method != M_POST) {
        while (numchars > 0 && strcmp("\n", buf)) {
            numchars = get_line(req->client_sock, buf, sizeof(buf));
        }

        unimplemented(req);
        FINISH_REQ(req)
    }

    if (req->path[strlen(req->path) - 1] == '/') {
        req->path[strlen(req->real_path) - 1] = '\0';
    }

    sprintf(req->real_path, "%s%s", doc_root, req->path);

    // if not static file
    if (req->is_cgi || access(req->real_path, F_OK) == -1) {
        sprintf(req->real_path, "%s%s", cgi_root, req->path);
    }

    struct stat st;

    if (stat(req->real_path, &st) == -1) {
        while (numchars > 0 && strcmp("\n", buf)) {
            numchars = get_line(req->client_sock, buf, sizeof(buf));
        }

        not_found(req);
        FINISH_REQ(req)
    }

    // if dir
    if (S_ISDIR(st.st_mode)) {
        strcat(req->real_path, "/index.html");
    }

    // if executable
    if (st.st_mode & S_IXUSR || st.st_mode & S_IXGRP || st.st_mode & S_IXOTH) {
        req->is_cgi = true;
    }

    char ch;

    numchars = get_line(req->client_sock, buf, sizeof(buf));

    if (req->is_cgi) {
        req->content_length = -1;
    }

    while (numchars > 0 && strcmp("\n", buf) != 0) {
        // get content_len
        ch = buf[15];
        buf[15] = '\0';

        if (strcasecmp(buf, "Content-Length:") == 0) {
            req->content_length = atoi(&buf[16]);
            buf[15] = ch;
        } else {
            // get content_type
            ch = buf[13];
            if (strcasecmp(buf, "Content-Type:") == 0) {
                strcpy(req->content_type, &buf[14]);
                buf[13] = ch;
            }
        }

        numchars = get_line(req->client_sock, buf, sizeof(buf));
    }

    if (!req->is_cgi) {
        handle_static(req);
    } else {
        if (req->content_length == -1) {
            if (req->method == M_POST) {
                bad_request(req);
                FINISH_REQ(req)
            } else {
                req->content_length = 0;
            }
        }
        handle_cgi(req);
    }

    FINISH_REQ(req)
}

void handle_req_line(char* buf, size_t buf_size, Request* req)
{
    char url[255], method[255];
    int i = 0, j = 0;

    /* get request method */
    while (!isspace(buf[j]) && i < sizeof(method) - 1) {
        method[i] = buf[j];
        ++i;
        ++j;
    }
    method[i] = '\0';

    if (strcasecmp("GET", method) == 0) {
        req->method = M_GET;
    } else if (strcasecmp("POST", method) == 0) {
        req->method = M_POST;
        req->is_cgi = true;
    } else {
        req->method = M_ERROR;
        return;
    }

    /* skip space */
    while (isspace(buf[j]) && j < buf_size) {
        ++j;
    }

    /* get url */
    i = 0;
    while (!isspace(buf[j]) && i < sizeof(url) - 1 && j < buf_size) {
        url[i] = buf[j];
        ++i;
        ++j;
    }
    url[i] = '\0';

    /* get query string */
    char* query_string = NULL;

    if (req->method == M_GET) {
        query_string = url;

        while (*query_string != '?' && *query_string != '\0') {
            ++query_string;
        }

        if (*query_string == '?') {
            req->is_cgi = true;
            *query_string = '\0';
            ++query_string;
        }
    }

    strcpy(req->path, url);
    strcpy(req->query_string, query_string);

    /* skip space */
    while (isspace(buf[j]) && j < buf_size) {
        ++j;
    }

    /* get protocol */
    i = 0;
    while (!isspace(buf[j]) && i < sizeof(req->protocol) - 1 && j < buf_size) {
        req->protocol[i] = buf[j];
        ++i;
        ++j;
    }

    req->protocol[i] = '\0';
}

void handle_static(Request* req)
{
    FILE* fp = fopen(req->real_path, "r");

    if (!fp) {
        not_found(req);
        return;
    }

    char* r = &req->real_path[strlen(req->real_path)];
    char content_type[127] = "";

    if (!strcmp(r - 4, ".gif")) {
        strcat(content_type, "image/gif");
    } else if (!strcmp(r - 4, ".jpg") || !strcmp(r - 5, ".jpeg")) {
        strcat(content_type, "image/jpeg");
    } else if (!strcmp(r - 4, ".htm") || !strcmp(r - 5, ".html")) {
        strcat(content_type, "text/html");
    } else if (!strcmp(r - 3, ".js")) {
        strcat(content_type, "application/x-javascript");
    } else if (!strcmp(r - 4, ".css")) {
        strcat(content_type, "text/css");
    } else {
        strcat(content_type, "text/plain");
    }

    char buf[1024];

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n", content_type);
    send(req->client_sock, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(req->client_sock, buf, strlen(buf), 0);

    fgets(buf, sizeof(buf), fp);
    while (!feof(fp)) {
        send(req->client_sock, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), fp);
    }

    fclose(fp);
}

void handle_cgi(Request* req)
{
    int cgi_input[2];
    int cgi_output[2];

    if (pipe(cgi_output) < 0 || pipe(cgi_input) < 0) {
        cannot_execute(req);
        return;
    }

    if ((req->pid = fork()) < 0) {
        cannot_execute(req);
        return;
    }

    /*
     * Request: Parent process read from sock and write to cgi_input[1]
     * then the child process read from cgi_input[0];
     *
     * Reponse: The child process write data to cgi_output[1],
     * then parent process read from cgi_output[0] and send to sock.
     */

    char c;
    int status;

    if (req->pid) { // parent process
        close(cgi_output[1]);
        close(cgi_input[0]);

        if (req->method == M_POST) {
            for (int i = 0; i < req->content_length; ++i) {
                recv(req->client_sock, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }

        while (read(cgi_output[0], &c, 1) > 0) {
            send(req->client_sock, &c, 1, 0);
        }

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(req->pid, &status, 0);
    } else { // child process
        char meth_env[32];
        char query_env[270];
        char type_env[45];
        char length_env[32];

        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);

        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(type_env, "CONTENT_TYPE=%s", req->content_type);
        putenv(type_env);

        if (req->method == M_GET) { // GET
            sprintf(meth_env, "REQUEST_METHOD=GET");
            sprintf(query_env, "QUERY_STRING=%s", req->query_string);
            putenv(query_env);
        } else {
            sprintf(length_env, "CONTENT_LENGTH=%d", req->content_length);
            putenv(length_env);
        }

        putenv(meth_env);

        execl(req->real_path, req->real_path, req->query_string, NULL);
        exit(0);
    }
}

void startup()
{
    server_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    socklen_t sockaddr_in_len = sizeof(struct sockaddr_in);

    if (server_sock == -1) {
        exit(0);
    }

    memset(&server_sockaddr_in, 0, sockaddr_in_len);
    server_sockaddr_in.sin_family = AF_INET;
    server_sockaddr_in.sin_port = htons(server_port);
    server_sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sock, (struct sockaddr*)&server_sockaddr_in, sockaddr_in_len) < 0) {
        stop();
    }

    if (server_port == 0) {
        if (getsockname(
                server_sock, (struct sockaddr*)&server_sockaddr_in, &sockaddr_in_len)
            == -1) {
            stop();
        }

        server_port = ntohs(server_sockaddr_in.sin_port);
    }

    if (listen(server_sock, 5) < 0) {
        printf("listen port %u failed\n", server_port);
        stop();
    }
}

int get_line(int sock, char* buf, int size)
{
    int i = 0, n;
    char c = '\0';

    while (i < size - 1 && c != '\n') {
        n = recv(sock, &c, 1, 0);

        if (n > 0) {
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);

                if (n > 0 && c == '\n') { /* \r\n */
                    recv(sock, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }

            buf[i] = c;
            ++i;
        } else {
            c = '\n';
        }
    }

    buf[i] = '\0';

    return i;
}

size_t increase_strcat(char* dest, size_t capacity, const char src[])
{
    if (strlen(dest) + strlen(src) >= capacity) {
        dest = (char*)realloc(dest, 2 * capacity);
    }

    strcat(dest, src);

    return 2 * capacity;
}

void unimplemented(Request* req)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<html>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<head>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<title>Method Not Implemented</title>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</head>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<body>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<p>HTTP request method not supported.</p>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</body>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</html>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
}

void not_found(Request* req)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 Not Found\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<html><title>Not Found</title>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<body><p>The server could not fulfill\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</body></html>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
}

void no_permission(Request* req)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 Bad Request\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<html><titleBad Request</title>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<body><p>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Permission deny\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</p>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "</body></html>\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
}

void bad_request(Request* req)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(req->client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(req->client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(req->client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "<p>Your browser sent a bad request, ");
    send(req->client_sock, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.</p>\r\n");
    send(req->client_sock, buf, sizeof(buf), 0);
}

void cannot_execute(Request* req)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(req->client_sock, buf, strlen(buf), 0);
}
