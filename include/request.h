#include <pthread.h>
#include <netinet/in.h>
#include <stdbool.h>

enum ReqMethod {
    M_GET,
    M_POST,
    M_ERROR
};

typedef struct Request Request;
struct Request {
    int client_sock;
    struct sockaddr_in client_sockaddr_in;
    pthread_t thread_id;
    int remote_port;

    enum ReqMethod method;
    char path[480];
    char real_path[512];
    char query_string[256];
    char protocol[16];

    char content_type[64];
    int content_length;
    char* cookie;

    char* body;

    bool is_cgi;

    struct Request* next;
    struct Request* prev;

    bool is_finish;
};

Request* alloc_req();
void append_req(Request* req_list, Request* req);
void clear_all_reqs(Request* req_list);

void output_req_info(Request* req);
