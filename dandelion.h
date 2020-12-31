#ifndef _DANDELION_H
#define _DANDELION_H

#include "request.h"

#define SERVER_STRING "Server idlehttpd/0.1.0\r\n"
void startup();
void stop();
void halt(int signo);
void* accept_request(void* param);
int get_line(int sock, char* buf, int size);
size_t increase_strcat(char* dest, size_t capacity, const char src[]);
void handle_req_line(char* buf, size_t buf_size, Request* req);
void handle_static(Request* req);
void handle_cgi(Request* req);

void unimplemented(Request* req);
void not_found(Request* req);
void no_permission(Request* req);
void bad_request(Request* req);
    
#endif
