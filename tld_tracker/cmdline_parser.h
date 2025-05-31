#ifndef CMDLINE_PARSER_H
#define CMDLINE_PARSER_H

#include <stdio.h>

typedef struct {
    char* key;
    char* value;
} CmdLinePair;

typedef struct {
    CmdLinePair* pairs;
    int count;
} CmdLineMap;

CmdLineMap parse_stream(FILE* is);
CmdLineMap parse(int argc, char** argv);
void free_cmdline_map(CmdLineMap* map);

#endif

