#include "rf24_common.h"
#include <string.h>

bool
BoolParser(const char* str) {
    return  !(strcasecmp(str, "no") == 0 ||
            strcasecmp(str, "0") == 0 ||
            strcasecmp(str, "off") == 0 ||
            strcasecmp(str, "false") == 0);
}
