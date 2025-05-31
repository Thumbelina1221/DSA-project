#include "cmdline_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper: trim leading and trailing whitespace
static char* strdup_trim(const char* s) {
    while (isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) len--;
    char* out = malloc(len + 1);
    if (out) {
        memcpy(out, s, len);
        out[len] = '\0';
    }
    return out;
}

CmdLineMap parse_stream(FILE* is) {
    CmdLineMap map = {0};
    char buf[512];
    while (fscanf(is, "%511s", buf) == 1) {
        size_t len = strlen(buf);
        if (len >= 2 && buf[0] == '-' && buf[1] == '-') {
            char* equal = strchr(buf, '=');
            char* key = NULL;
            char* val = NULL;
            if (equal) {
                size_t klen = equal - (buf + 2);
                key = malloc(klen + 1);
                memcpy(key, buf + 2, klen);
                key[klen] = '\0';
                val = strdup(equal + 1);
            } else {
                key = strdup(buf + 2);
                val = strdup("unknown");
            }
            map.pairs = realloc(map.pairs, sizeof(CmdLinePair) * (map.count + 1));
            map.pairs[map.count].key = key;
            map.pairs[map.count].value = val;
            map.count++;
        }
    }
    return map;
}

CmdLineMap parse(int argc, char** argv) {
    CmdLineMap map = {0};
    for (int i = 0; i < argc; ++i) {
        const char* arg = argv[i];
        if (strlen(arg) >= 2 && arg[0] == '-' && arg[1] == '-') {
            char* equal = strchr(arg, '=');
            char* key = NULL;
            char* val = NULL;
            if (equal) {
                size_t klen = equal - (arg + 2);
                key = malloc(klen + 1);
                memcpy(key, arg + 2, klen);
                key[klen] = '\0';
                val = strdup(equal + 1);
            } else {
                key = strdup(arg + 2);
                val = strdup("unknown");
            }
            map.pairs = realloc(map.pairs, sizeof(CmdLinePair) * (map.count + 1));
            map.pairs[map.count].key = key;
            map.pairs[map.count].value = val;
            map.count++;
        }
    }
    return map;
}

void free_cmdline_map(CmdLineMap* map) {
    if (!map || !map->pairs) return;
    for (int i = 0; i < map->count; ++i) {
        free(map->pairs[i].key);
        if (map->pairs[i].value)
            free(map->pairs[i].value);
    }
    free(map->pairs);
    map->pairs = NULL;
    map->count = 0;
}

