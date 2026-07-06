#include <assert.h>
#include <string.h>

#include "iss_tui.h"

int main(void) {
    const char *name = iss_tui_name();
    assert(name != NULL);
    assert(strcmp(name, "ISS-TUI") == 0);
    return 0;
}
