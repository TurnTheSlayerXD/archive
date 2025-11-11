#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>

#include "hamming.h"
#include "encoding_decoding.h"

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

void test_hamming()
{

    const char bytes[] = {1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1};

    bit_vec init = bit_vec_new(sizeof(bytes));
    for (size_t i = 0; i < sizeof(bytes); ++i)
    {
        bit_vec_set_bit_at(&init, i, bytes[i]);
    }

    char *init_str = bit_vec_to_str(&init);
    fprintf(stdout, "Before encoding %s\n", init_str);

    bit_vec after = hamming_algo(init);

    char *aft_str = bit_vec_to_str(&after);
    fprintf(stdout, "After encoding %s\n", aft_str);

    hamming_decode_res res = hamming_decode(after);

    assert(res.ok);

    char *dec_str = bit_vec_to_str(&res.vec);

    assert(strcmp(init_str, dec_str) == 0);

    free(init_str);
    free(aft_str);
    free(dec_str);

    bit_vec_delete(&init);
    bit_vec_delete(&after);
    bit_vec_delete(&res.vec);
}

typedef struct
{
    char id[3];
    size_t file_count;
    size_t files_available;
} arch_header;

typedef struct
{
    size_t init_size;
    size_t enc_size;
    size_t offset;
    size_t name_len;
} file_header;

typedef struct
{
    file_header hdr;
    char *filename;
} file_desc;

typedef struct
{
    FILE *f;
    const char *name;
    size_t file_count;
    size_t files_available;
    file_desc *file_descs;
} arch_instance;

void arch_instance_close(arch_instance *inst)
{
    for (size_t i = 0; i < inst->file_count; ++i)
    {
        free(&inst->file_descs[i].filename);
        inst->file_descs[i].filename = NULL;
    }
    free(inst->file_descs);
    fclose(inst->f);
    *inst = (arch_instance){0};
}

arch_instance arch_instance_create(const char *path)
{
    if (access(path, F_OK) == 0)
    {
        FILE *f;
        f = fopen(path, "a+");
        if (!f)
        {
            fprintf(stderr, "arch (updated) at path %s could not be opened", path);
            return (arch_instance){0};
        }
        arch_instance inst = (arch_instance){.f = f, .name = path, .file_count = 0, .file_descs = NULL};

        arch_header hdr;
        if (fread(&hdr, sizeof(arch_header), 1, f) != 1)
        {
            fprintf(stderr, "arch (updated) Failed reading arch header from file %s", path);
            return (arch_instance){0};
        }

        if (hdr.id[0] != 'H' || hdr.id[1] != 'A' || hdr.id[2] != 'M')
        {
            fprintf(stderr, "arch (updated) Failed confirming HAM from %s", path);
            return (arch_instance){0};
        }

        inst.file_count = hdr.file_count;
        inst.files_available = hdr.files_available;
        fprintf(stdout, "arch (updated) at path %s contains n = %lu files, available files = %lu", path, inst.file_count, inst.files_available);

        if (!inst.file_count)
        {
            return inst;
        }

        inst.file_descs = calloc(sizeof(file_desc), inst.file_count);

        for (size_t i = 0; i < inst.file_count; ++i)
        {
            file_header hdr;
            if (fread(&hdr, sizeof(file_header), 1, f) != 1)
            {
                fprintf(stderr, "(update) could not properly read %lu-th file HEADER from arch %s", i, path);
                arch_instance_close(&inst);
                return (arch_instance){0};
            }
            inst.file_descs[i].hdr = hdr;
            inst.file_descs[i].filename = malloc(hdr.name_len);
            if (fread(inst.file_descs[i].filename, hdr.name_len, 1, f) != 1)
            {
                fprintf(stderr, "(update) could not properly read %lu-th file NAME from arch %s, expected to read %lu bytes", i, path, hdr.name_len);
                arch_instance_close(&inst);
                return (arch_instance){0};
            }
        }
        return inst;
    }
    FILE *f = fopen(path, "w+");
    if (!f)
    {
        fprintf(stderr, "arch (created) at path [%s] could not be created", path);
        return (arch_instance){0};
    }
    arch_instance inst = {
        .f = f,
        .name = path,
        .file_count = 0,
        .files_available = 0,
        .file_descs = NULL,
    };
    arch_header hdr = {.file_count = 0, .id = "HAM"};
    if (fwrite(&hdr, sizeof(arch_header), 1, f) != 1)
    {
        fprintf(stderr, "arch (created) at path [%s] could not be written with header", path);
        return (arch_instance){0};
    }

    return inst;
}

typedef struct
{
    const char *filename;
    FILE *f_stream;
    size_t file_size;
    size_t cur_pos;
} file_to_append;

file_to_append file_to_append_open(const char *filename)
{
    file_to_append str = {.filename = filename, .f_stream = fopen(filename, "r"), .cur_pos = 0};
    if (!str.f_stream)
    {
        fprintf(stderr, "could not obtain file %s", filename);
        return {0};
    }
    fseek(str.f_stream, 0, SEEK_END);
    str.file_size = ftell(str.f_stream);
    fseek(str.f_stream, 0, SEEK_SET);
    return str;
}

void arch_instance_insert_files(arch_instance inst, const char **filenames, size_t exp_file_count)
{
    file_to_append *files = calloc(exp_file_count, sizeof(file_to_append));
    while (true)
    {
        for (size_t i = 0; i < exp_file_count; ++i)
        {
            files[i] = file_to_append_open(filenames[i]);
            if (!files[i].f_stream)
            {
                break;
            }
        }

        break;
    }

    for (size_t i = 0; i < exp_file_count; ++i)
    {
        if (files[i].f_stream)
        {
            fclose(files[i].f_stream);
        }
    }

    free(files);
}

int main(int argc, char **argv)
{
    config cnf = config_new(100000);

    (void)argc;
    (void)argv;

    return 0;
}

/*


    if (argc >= 2)
    {

        if (strcmp(argv[0], "-f"))
        {
            const char *path = argv[1];

            printf("provided path to file is %s\n", path);

            FILE *f_hndl = fopen(path, "rb");

            if (!f_hndl)
            {
                fprintf(stderr, "No such file: %s\n", path);
                return 69;
            }

            long begin = ftell(f_hndl);
            fseek(f_hndl, 0, SEEK_END);
            long end = ftell(f_hndl);

            fprintf(stdout, "length of file %s in bytes is %ld\n", path, end - begin);
        }
    }
    else
    {
        fprintf(stderr, "expected args count to be 2\n");
        return 69;
    }
*/
