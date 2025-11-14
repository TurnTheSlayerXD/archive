#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "arch_instance.h"

int main()
{
    const char *test = "test.ham";
    FILE *f = fopen("./test.ham", "r+");
    arch_header buf = {.id = "HAM", .bytes_per_read = 100, .file_count = 2, .free_file_count = 1};
    fseek(f, 0, SEEK_SET);
    // fwrite(&buf, 1, sizeof(arch_header), f);
    fread(&buf, 1, sizeof(buf), f);
    fprintf(stdout, "HAM id = %s, file_count = %d, free_file_count = %d, bytes_per_read = %d\n", buf.id, buf.file_count, buf.free_file_count, buf.bytes_per_read);
    fclose(f);
}
