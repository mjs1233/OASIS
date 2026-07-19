#include "firmware.h"

void app_main(void)
{
    printf("Hello World!\n");
    print_size();
}

void print_size() {

    printf("size of int = %d\n", sizeof(int));
    printf("size of long = %d\n", sizeof(long));
    printf("size of long long = %d\n", sizeof(long long));
}