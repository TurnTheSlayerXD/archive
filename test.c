#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "helper.h"

int main()
{
    const char *test = "test.txt";
    const char *dst = "other_test.txt";

    FILE *f_test = fopen(test, "r");
    FILE *f_dst = fopen(dst, "w+");

    void *buf = malloc(file_size(f_test));
    fread(buf, file_size(f_test), 1, f_test);
    fwrite(buf, file_size(f_test), 1, f_dst);

    shift_data_in_file(file_size(f_dst), 0, 100, f_dst);

    free(buf);
}
