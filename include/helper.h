#ifndef HEADER_H
#define HEADER_H

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

void mkdir_if_no(const char *dirname)
{
    struct stat st = {0};

    if (stat(dirname, &st) == -1)
    {
        mkdir(dirname, 0700);
    }
}

size_t file_size(FILE *f)
{
    size_t init = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t l = ftell(f);
    fseek(f, 0, SEEK_END);
    size_t r = ftell(f);
    fseek(f, init, SEEK_SET);
    return r - l;
}

void file_write_pos(int64_t pos, const void *data, size_t data_size, FILE *f)
{
    if (fseek(f, pos, SEEK_SET))
    {
        assert("fseek(f, pos, SEEK_SET)" && false);
    }
    if (fwrite(data, data_size, 1, f) != 1)
    {
        assert(false && "fwrite(shift_buf, n_shift, 1, f) != 1");
    }
}

void file_read_pos(int64_t pos, void *dst, size_t data_size, FILE *f)
{
    if (fseek(f, pos, SEEK_SET))
    {
        assert("fseek(f, pos, SEEK_SET)" && false);
    }
    if (fread(dst, data_size, 1, f) != 1)
    {
        assert(false && "fwrite(shift_buf, n_shift, 1, f) != 1");
    }
}

#define SHIFT_CHUNK_SIZE 100ll
void shift_data_in_file(int64_t end_offset, int64_t start_offset, int64_t n_shift, FILE *restrict f)
{
    char buf[SHIFT_CHUNK_SIZE];
    void *shift_buf = calloc(1, n_shift);
    file_write_pos(end_offset, shift_buf, n_shift, f);
    free(shift_buf);
    for (int64_t chunk_counter = 1; end_offset - chunk_counter * SHIFT_CHUNK_SIZE >= start_offset; ++chunk_counter)
    {
        int64_t off = end_offset - SHIFT_CHUNK_SIZE * chunk_counter;
        file_read_pos(off, buf, SHIFT_CHUNK_SIZE, f);
        file_write_pos(end_offset + n_shift - SHIFT_CHUNK_SIZE * chunk_counter, buf, SHIFT_CHUNK_SIZE, f);
    }
    size_t rest_size = (end_offset - start_offset) % SHIFT_CHUNK_SIZE;
    if (rest_size == 0)
    {
        return;
    }
    void *rest_buf = malloc(rest_size);
    file_read_pos(start_offset, rest_buf, rest_size, f);
    int64_t fin_write_off = end_offset + n_shift - ((end_offset - start_offset) / SHIFT_CHUNK_SIZE) * SHIFT_CHUNK_SIZE - rest_size;
    file_write_pos(fin_write_off, rest_buf, rest_size, f);
    free(rest_buf);
}

#endif