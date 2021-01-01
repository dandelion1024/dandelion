#include "request.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

Request* alloc_req()
{
    Request* req = (Request*)malloc(sizeof(Request));

    if (req) {
        memset(req, 0, sizeof(Request));
    }

    return req;
}

void append_req(Request* req_list, Request* req)
{
    if (!req_list) {
        req_list = req;
    } else {
        Request* p = req_list;

        while (p->next) {
            p = p->next;
        }

        p->next = req;
        req->prev = p;
        req->next = NULL;
    }
}

void clear_all_reqs(Request* req_list)
{
    Request* p = req_list;

    while (req_list) {
        p = req_list;
        req_list = req_list->next;
        free(p);
    }
}

void output_req_info(Request* req)
{
    printf("client_sock: %d\n", req->client_sock);

    if (req->method == M_GET) {
        printf("method: GET\n");
    } else if (req->method == M_POST) {
        printf("method: POST\n");
    } else {
        printf("method: ERROR\n");
    }

    printf("path: %s\n", req->path);
    printf("real_path: %s\n", req->real_path);
    printf("query_string: %s\n", req->query_string);
    printf("protocol: %s\n", req->protocol);
    printf("content_length: %d\n", req->content_length);
    printf("\n");
}
