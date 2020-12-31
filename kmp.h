#ifndef KMP_H
#define KMP_H

void get_next(int* next, const char pattern[], int pattern_len);
int kmp(const char str[], int len, char pattern[], int pattern_len, int* next);

#endif
