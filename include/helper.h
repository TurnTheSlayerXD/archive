#ifndef HEADER_H
#define HEADER_H

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>

#define _XOPEN_SOURCE 500
#undef __USE_FILE_OFFSET64
#include <ftw.h>

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

void mkdir_if_no(const char *dirname)
{
    struct stat st = {0};

    if (stat(dirname, &st) == -1)
    {
        mkdir(dirname, 0700);
    }
}

void join_dir_and_file(char *restrict dst, size_t maxlen, const char *restrict dir, const char *restrict filename)
{
    const char *template = dir[strlen(dir) - 1] == '/' ? "%s%s" : "%s/%s";
    snprintf(dst, maxlen - 1, template, dir, filename);
}

char *get_clean_filename(const char *str)
{
    char *buf = strrchr(str, '/');
    return buf == NULL ? (char *)str : buf + 1;
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

void file_write_pos(int64_t pos, const void *restrict data, size_t data_size, FILE *restrict f)
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
void right_shift_file(int64_t end_off, int64_t start_off, int64_t n_shift, FILE *restrict f)
{
#define DEBUG
#undef DEBUG
#ifdef DEBUG
    void *p = malloc(end_off - start_off);
    file_read_pos(start_off, p, end_off - start_off, f);
#endif

    assert(end_off > start_off);

    char buf[SHIFT_CHUNK_SIZE] = {0};

    for (int64_t i = 0; i <= n_shift / SHIFT_CHUNK_SIZE; ++i)
    {
        file_write_pos(end_off + i * SHIFT_CHUNK_SIZE, buf, SHIFT_CHUNK_SIZE, f);
    }

    for (int64_t chunk_counter = 1; chunk_counter <= (end_off - start_off) / SHIFT_CHUNK_SIZE; ++chunk_counter)
    {
        int64_t pos = end_off - chunk_counter * SHIFT_CHUNK_SIZE;
        file_read_pos(pos, buf, SHIFT_CHUNK_SIZE, f);
        file_write_pos(n_shift + pos, buf, SHIFT_CHUNK_SIZE, f);
    }
    int64_t rest_size = (end_off - start_off) % SHIFT_CHUNK_SIZE;
    if (rest_size == 0)
    {
        return;
    }

    int64_t pos = start_off;
    file_read_pos(pos, buf, rest_size, f);
    file_write_pos(n_shift + pos, buf, rest_size, f);

#ifdef DEBUG
    void *s = malloc(end_off - start_off);
    file_read_pos(start_off + n_shift, s, end_off - start_off, f);

    assert(memcmp(p, s, end_off - start_off) == 0);

    free(p);
    free(s);
#endif
}

void left_shift_file(int64_t end_off, int64_t start_off, int64_t n_shift, FILE *restrict f)
{

    char buf[SHIFT_CHUNK_SIZE] = {0};

    assert(end_off > start_off);
    assert(start_off >= n_shift);

    for (int64_t chunk_counter = 0; chunk_counter < (end_off - start_off) / SHIFT_CHUNK_SIZE; ++chunk_counter)
    {
        int64_t pos = start_off + chunk_counter * SHIFT_CHUNK_SIZE;
        file_read_pos(pos, buf, SHIFT_CHUNK_SIZE, f);
        file_write_pos(-n_shift + pos, buf, SHIFT_CHUNK_SIZE, f);
    }
    int64_t rest_size = (end_off - start_off) % SHIFT_CHUNK_SIZE;
    if (rest_size == 0)
    {
        return;
    }

    int64_t pos = end_off - (end_off - start_off) % SHIFT_CHUNK_SIZE;
    file_read_pos(pos, buf, rest_size, f);
    file_write_pos(-n_shift + pos, buf, rest_size, f);
}

void make_unique_filename(char *name)
{
    int i = 0;
    char num[10];
    while (access(name, F_OK) == 0 && i < 10)
    {
        snprintf(num, 10, "_%d", i);
        strncat(name, num, 10);
        ++i;
    }
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int rmrf(const char *path)
{
    return ftw(path, unlink_cb, FTW_F);
}

#endif