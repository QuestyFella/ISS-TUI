#include <assert.h>
#include <string.h>

#include "bloom.h"

int main(void) {
    const char *name = bloom_name();
    assert(name != NULL);
    assert(strcmp(name, "bloom-valuation") == 0);
    return 0;
}
