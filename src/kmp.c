#include "kmp.h"
#include <malloc.h>
#include <stdbool.h>

void get_next(int* next, const char pattern[], int pattern_len)
{
    int j = 0, k = -1;

    next[0] = -1;
    while (j < pattern_len - 1) {
        if (k == -1 || pattern[j] == pattern[k]) {
            ++j;
            ++k;

            if (pattern[j] == pattern[k]) {
                next[j] = next[k];
            } else {
                next[j] = k;
            }
        } else {
            k = next[k];
        }
    }
}

int kmp(const char str[], int len, char pattern[], int pattern_len, int* next)
{
    bool flag = false;

    if (next == NULL) {
        next = (int*)malloc(pattern_len * sizeof(int));
        get_next(next, pattern, pattern_len);
        flag = true;
    }

    int i = 0, j = 0;

    while (i < len && j < pattern_len) {
        if (j == -1 || str[i] == pattern[j]) {
            ++i;
            ++j;
        } else {
            j = next[j];
        }
    }

    if (flag) {
        free(next);
    }

    if (j >= pattern_len) {
        return i - pattern_len;
    }

    return -1;
}