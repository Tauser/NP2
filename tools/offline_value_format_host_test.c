#include <assert.h>
#include <string.h>

#include "offline_value_format.h"

int main(void)
{
    char text[16] = {0};
    assert(offline_format_temperature(-5, text, sizeof(text)) && strcmp(text, "-0.5") == 0);
    assert(offline_format_temperature(0, text, sizeof(text)) && strcmp(text, "0.0") == 0);
    assert(offline_format_temperature(-1000, text, sizeof(text)) && strcmp(text, "-100.0") == 0);
    assert(offline_format_market_change(-25, text, sizeof(text)) && strcmp(text, "-0.25") == 0);
    assert(offline_format_market_change(25, text, sizeof(text)) && strcmp(text, "0.25") == 0);
    assert(!offline_format_temperature(1, text, 2U));
    return 0;
}
