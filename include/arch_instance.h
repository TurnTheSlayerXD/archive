#ifndef ARCH_INSTANCE_H

#define ARCH_INSTANCE_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libgen.h>
#include <string.h>

#include "helper.h"
#include "encoding_decoding.h"

#define DEFAULT_BYTES_PER_CHUNK 100
#define DEFAULT_FREE_FILE_COUNT 1

typedef struct
{
    char id[3];
    size_t file_count;
    size_t free_file_count;
    size_t bytes_per_read;
} arch_header;

#define arch_file_header_NAME_LEN 100
typedef struct
{
    size_t init_size;
    size_t enc_size;
    size_t offset;
    char filename[arch_file_header_NAME_LEN];
} arch_file_header;

typedef struct
{
    FILE *f;
    const char *name;

    arch_header hdr;
    config cnf;
    arch_file_header *file_hdrs;
} arch_instance;

typedef struct
{
    const char **arr;
    size_t len;
} string_array;

typedef struct
{
    char **arr;
    size_t len;
} string_array_to_free;

void string_array_to_free_close(string_array_to_free *inst)
{
    for (size_t i = 0; i < inst->len; ++i)
    {
        free(inst->arr[i]);
    }
    *inst = (string_array_to_free){0};
}

void arch_instance_close(arch_instance *inst)
{
    free(inst->file_hdrs);
    fclose(inst->f);
    *inst = (arch_instance){0};
}

arch_instance arch_instance_create_empty(const char *path, config cnf)
{
    if (cnf.BYTES_per_chunk == 0)
    {
        cnf = config_new(DEFAULT_BYTES_PER_CHUNK, DEFAULT_FREE_FILE_COUNT);
    }
    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "arch (created) at path [%s] could not be created\n", path);
        return (arch_instance){0};
    }
    arch_header hdr = {.file_count = 0, .id = "HAM", .free_file_count = 0, .bytes_per_read = cnf.BYTES_per_chunk};
    arch_instance inst = {
        .f = f,
        .name = path,
        .hdr = hdr,
        .file_hdrs = NULL,
        .cnf = cnf,
    };
    if (fwrite(&hdr, sizeof(arch_header), 1, f) != 1)
    {
        fprintf(stderr, "arch (created) at path [%s] could not be written with header\n", path);
        return (arch_instance){0};
    }

    return inst;
}

arch_instance arch_instance_create(const char *path, bool should_exist)
{
    if (access(path, F_OK) == 0 || should_exist)
    {
        FILE *f;
        f = fopen(path, "r+");
        if (!f)
        {
            fprintf(stderr, "arch (updated) at path %s could not be opened\n", path);
            return (arch_instance){0};
        }
        if (fseek(f, 0, SEEK_SET))
        {
            assert(false && "fseek(f, 0, SEEK_SET)");
        }

        arch_header hdr;
        if (fread(&hdr, sizeof(arch_header), 1, f) != 1)
        {
            fprintf(stderr, "arch (updated) Failed reading arch header from file %s\n", path);
            return (arch_instance){0};
        }

        if (hdr.id[0] != 'H' || hdr.id[1] != 'A' || hdr.id[2] != 'M')
        {
            fprintf(stderr, "arch (updated) Failed confirming HAM from %s\n", path);
            return (arch_instance){0};
        }
        if (hdr.bytes_per_read == 0)
        {
            fprintf(stderr, "arch (updated) Invalid bytes per chunk value = %d in arch %s\n", hdr.bytes_per_read, path);
            return (arch_instance){0};
        }

        arch_instance inst = (arch_instance){
            .f = f,
            .name = path,
            .hdr = hdr,
            .file_hdrs = NULL,
            .cnf = config_new(hdr.bytes_per_read, DEFAULT_FREE_FILE_COUNT),
        };
        fprintf(stdout, "arch (updated) at path %s contains n = %lu files, available space = %lu\n", path, inst.hdr.file_count, inst.hdr.free_file_count);

        if (!inst.hdr.file_count)
        {
            return inst;
        }

        inst.file_hdrs = calloc(sizeof(arch_file_header), inst.hdr.file_count);

        for (size_t i = 0; i < inst.hdr.file_count; ++i)
        {
            if (fread(&inst.file_hdrs[i], sizeof(arch_file_header), 1, f) != 1)
            {
                fprintf(stderr, "(update) could not properly read %lu-th file HEADER from arch %s\n", i, path);
                arch_instance_close(&inst);
                return (arch_instance){0};
            }
        }
        return inst;
    }

    return arch_instance_create_empty(path, (config){0});
}

typedef struct
{
    char *filename;
    FILE *f_stream;
    size_t file_size;
} file_to_append;

file_to_append file_to_append_open(const char *filename)
{
    char *cp = strdup(filename);
    char *base_filename = strdup(basename(cp));

    free(cp);
    file_to_append str = {.filename = base_filename, .f_stream = fopen(filename, "r")};
    if (!str.f_stream)
    {
        fprintf(stderr, "could not obtain file %s\n", filename);
        return (file_to_append){0};
    }
    fseek(str.f_stream, 0, SEEK_END);
    str.file_size = ftell(str.f_stream);
    fseek(str.f_stream, 0, SEEK_SET);
    return str;
}

void file_to_append_close(file_to_append *ptr)
{
    free(ptr->filename);
    if (ptr->f_stream)
    {
        fclose(ptr->f_stream);
    }
    *ptr = (file_to_append){0};
}

void test_shift_data_in_file()
{
}

typedef struct
{
    const file_to_append *arr;
    size_t len;
} file_to_append_array;

arch_file_header *arch_get_new_headers(arch_instance *inst, file_to_append_array new_files)
{
    config cnf = inst->cnf;
    assert(new_files.len > 0);

    if (inst->hdr.file_count == 0)
    {
        inst->file_hdrs = realloc(inst->file_hdrs, new_files.len * sizeof(arch_file_header));
        inst->hdr.free_file_count = cnf.FREE_FILE_COUNT;

        size_t f_offset = sizeof(arch_file_header) + (new_files.len + inst->hdr.free_file_count) * sizeof(arch_file_header);
        for (size_t i = 0; i < new_files.len; ++i)
        {
            arch_file_header hdr;
            hdr.init_size = new_files.arr[i].file_size;
            hdr.enc_size = calc_encoded_size(new_files.arr[i].file_size, cnf);
            strncpy(hdr.filename, new_files.arr[i].filename, arch_file_header_NAME_LEN - 1);
            hdr.offset = f_offset;

            inst->file_hdrs[i] = hdr;
            f_offset += hdr.enc_size;
        }

        inst->hdr.file_count += new_files.len;
        return inst->file_hdrs;
    }

    if (inst->hdr.free_file_count < new_files.len)
    {
        inst->hdr.free_file_count = cnf.FREE_FILE_COUNT;

        // move old files
        size_t new_offset_for_first_file = (inst->hdr.file_count + new_files.len + inst->hdr.free_file_count) * sizeof(arch_file_header) + sizeof(arch_file_header);
        size_t old_offset_for_first_file = inst->file_hdrs[0].offset;

        size_t shift_len = new_offset_for_first_file - old_offset_for_first_file;

        arch_file_header *first_file_hdr = &inst->file_hdrs[0];
        arch_file_header *last_file_hdr = &inst->file_hdrs[inst->hdr.file_count - 1];
        size_t file_data_end_offset = last_file_hdr->offset + last_file_hdr->enc_size;
        size_t file_data_begin_offset = first_file_hdr->offset;

        shift_data_in_file(file_data_end_offset, file_data_begin_offset, shift_len, inst->f);

        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            inst->file_hdrs[i].offset += shift_len;
        }
    }
    else
    {
        inst->hdr.free_file_count -= new_files.len;
    }

    inst->file_hdrs = realloc(inst->file_hdrs, sizeof(arch_file_header) * (inst->hdr.file_count + new_files.len));
    arch_file_header *last_file = &inst->file_hdrs[inst->hdr.file_count - 1];
    size_t f_offset = last_file->offset + last_file->enc_size;
    for (size_t i = 0; i < new_files.len; ++i)
    {
        arch_file_header hdr;
        hdr.init_size = new_files.arr[i].file_size;
        strncpy(hdr.filename, new_files.arr[i].filename, arch_file_header_NAME_LEN - 1);
        hdr.enc_size = calc_encoded_size(hdr.init_size, cnf);
        hdr.offset = f_offset;

        inst->file_hdrs[inst->hdr.file_count + i] = hdr;
        f_offset += hdr.enc_size;
    }

    inst->hdr.file_count += new_files.len;

    return &inst->file_hdrs[inst->hdr.file_count - new_files.len];
}

void __arch_insert_file_streams(arch_instance *inst, file_to_append_array files)
{
    arch_file_header *new_hdrs = arch_get_new_headers(inst, files);
    for (size_t i = 0; i < files.len; ++i)
    {
        if (fseek(inst->f, new_hdrs[i].offset, SEEK_SET))
        {
            assert(false && "fseek(inst.f, hdrs[i].offset, SEEK_SET)");
        }
        do_file_encoding(files.arr[i].f_stream, new_hdrs[i].init_size, inst->f, inst->cnf);
    }
    file_write_pos(0, &inst->hdr, sizeof(arch_header), inst->f);
    file_write_pos(sizeof(arch_header), inst->file_hdrs, sizeof(arch_file_header) * inst->hdr.file_count, inst->f);
}

void arch_insert_files(arch_instance *inst, string_array filenames)
{
    assert(filenames.len > 0);
    file_to_append *files = calloc(filenames.len, sizeof(file_to_append));
    while (true)
    {
        for (size_t i = 0; i < filenames.len; ++i)
        {
            files[i] = file_to_append_open(filenames.arr[i]);
            if (!files[i].f_stream)
            {
                break;
            }
        }
        __arch_insert_file_streams(inst, (file_to_append_array){.arr = files, .len = filenames.len});
        break;
    }

    for (size_t i = 0; i < filenames.len; ++i)
    {
        file_to_append_close(&files[i]);
    }
    free(files);
}

char *__arch_extract_single(arch_instance *inst, const arch_file_header *hdr, const char *dir)
{
    char fin_name[150] = {0};
    snprintf(fin_name, 150, "%s/%s", dir, hdr->filename);
    FILE *f = fopen(fin_name, "w");
    if (!f)
    {
        fprintf(stderr, "Could not create file to extract: %s\n", fin_name);
        return NULL;
    }
    if (fseek(inst->f, hdr->offset, SEEK_SET))
    {
        assert(false && "fseek(inst->f, hdr->offset, SEEK_SET)");
    }
    do_file_decoding((encoded_file){.file = inst->f, .src_file_len = hdr->init_size, .enc_file_len = hdr->enc_size}, f, inst->cnf);
    fclose(f);
    return strdup(fin_name);
}

string_array_to_free arch_extract_files(arch_instance *inst, const char *dir, string_array filenames)
{
    string_array_to_free result_fnames = {.arr = calloc(filenames.len, sizeof(char *)), .len = filenames.len};
    if (filenames.len == 0)
    {
        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            result_fnames.arr[i] = __arch_extract_single(inst, &inst->file_hdrs[i], dir);
        }
        return;
    }

    for (size_t name_i = 0; name_i < filenames.len; ++name_i)
    {
        const arch_file_header *hdr = NULL;
        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            if (strcmp(inst->file_hdrs[i].filename, filenames.arr[name_i]) == 0)
            {
                hdr = &inst->file_hdrs[i];
                break;
            }
        }

        if (!hdr)
        {
            fprintf(stderr, "No file [%s] in archive [%s]", filenames.arr[name_i], inst->name);
            continue;
        }
        __arch_extract_single(inst, hdr, dir);
    }
}

void arch_delete_files(arch_instance *inst, string_array filenames, const char *dir)
{
    for (size_t i = 0; i < filenames.len; ++i)
    {
        const char *fname = &filenames.arr[i];

        arch_file_header *hdr = NULL;
        size_t hdr_index;
        for (size_t hdr_i = 0; hdr_i < inst->hdr.file_count; ++hdr_i)
        {
            if (strcmp(inst->file_hdrs[hdr_i].filename, fname) == 0)
            {
                hdr = &inst->file_hdrs[hdr_i];
                hdr_index = hdr_i;
                break;
            }
        }
        if (!hdr)
        {
            fprintf(stderr, "Could not locate file [%s] to delete", fname);
            continue;
        }

        const size_t offset = hdr->offset;

        const size_t arch_size = file_size(inst->f);

        char buf[100];
        snprintf(buf, 99, "%s/%s", dir, fname);
        FILE *dst = fopen(buf, "w");

        config cnf = inst->cnf;
        char *buf = malloc(cnf.BYTES_per_chunk);

        for (size_t pos = offset; pos < arch_size; pos += cnf.BYTES_per_chunk)
        {
            file_read_pos(pos, buf, offset, inst->f);
            if (fwrite(buf, cnf.BYTES_per_chunk, 1, dst) != 1)
            {
                fprintf(stderr, "COuld not write chunk of data with size %lu from archive %s to %s", cnf.BYTES_per_chunk, inst->name, fname);
                continue;
            }
        }

        if (arch_size % offset > 0)
        {
            size_t n_rest = arch_size % offset;
            file_read_pos(arch_size - n_rest, buf, n_rest, inst->f);
            if (fwrite(buf, n_rest, 1, dst) != 1)
            {
                fprintf(stderr, "COuld not write chunk of data with size %lu from archive %s to %s", n_rest, inst->name, fname);
            }
        }
        free(buf);

        if (hdr_index == inst->hdr.file_count - 1)
        {
            // skip for now
        }
        else
        {
            int64_t end_offset = inst->file_hdrs[inst->hdr.file_count - 1].offset + inst->file_hdrs[inst->hdr.file_count - 1].enc_size;
            int64_t start_offset = hdr->offset + hdr->enc_size;
            int64_t shift_len = hdr->enc_size;
            shift_data_in_file(end_offset, start_offset, shift_len, inst->f);
        }

        // fix offsets
        for (size_t i = hdr_index + 1; i < inst->hdr.file_count; ++i)
        {
            inst->file_hdrs[i - 1] = inst->file_hdrs[i];
        }
        inst->hdr.file_count -= 1;

        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            inst->file_hdrs->offset -= hdr->enc_size;
        }
    }
}

typedef struct
{
    arch_instance *arr;
    size_t len;
} arch_array;

void arch_concat_archs(const char *dst_name, arch_array archs)
{
    arch_instance dst_inst = {0};

    for (size_t i = 0; i < archs.len; ++i)
    {
        if (strcmp(archs.arr[i].name, dst_name) == 0)
        {
            dst_inst = archs.arr[i];
            for (size_t j = i + 1; j < archs.len; ++j)
            {
                archs.arr[j - 1] = archs.arr[j];
            }
            archs.len -= 1;
            break;
        }
    }

    if (!dst_inst.f)
    {
        dst_inst = arch_instance_create_empty(dst_name, (config){0});
        if (!dst_inst.f)
        {
            fprintf(stderr, "Could not create dst arch with path [%s]", dst_name);
            return;
        }
    }

    for (size_t arch_i = 0; arch_i < archs.len; ++arch_i)
    {
        arch_instance *cur_arch = &archs.arr[arch_i];
        config cur_cnf = archs.arr[arch_i].cnf;
        for (size_t file_i = 0; file_i < cur_arch->hdr.file_count; ++file_i)
        {
            arch_file_header *cur_file = &cur_arch->file_hdrs[file_i];
            if (fseek(cur_arch->f, cur_file->offset, SEEK_SET))
            {
                fprintf(stderr, "Failed seeking, arch: %s, file_index: %lu\n", cur_arch->name, file_i);
                continue;
            }

            const char *temp_dir = "_temp";

            mkdir_if_no(temp_dir);

            void *buf = malloc(cur_cnf.enc_BYTES_per_chunk);
            arch_extract_files(cur_arch, temp_dir, (string_array){0});

            arch_insert_files(dst_inst, )
                // for (size_t i = 0; i + cur_cnf.enc_BYTES_per_chunk < cur_file->enc_size; i += cur_cnf.enc_BYTES_per_chunk)
                // {
                //     if (fwrite(buf, cur_cnf.enc_BYTES_per_chunk, 1, cur_arch->f) != 1)
                //     {
                //         fprintf(stderr, "Failed writing %s\n", cur_arch->name);
                //         continue;
                //     }
                // }
                // if (cur_file->enc_size % cur_cnf.enc_BYTES_per_chunk == 0)
                // {
                //     if (fwrite(buf, cur_cnf.enc_BYTES_per_chunk, 1, cur_arch->f) != 1)
                //     {
                //         fprintf(stderr, "Failed writing %s\n", cur_arch->name);
                //         continue;
                //     }
                // }
                // else
                // {
                //     if (fwrite(buf, cur_file->enc_size % cur_cnf.enc_BYTES_per_chunk, 1, cur_arch->f) != 1)
                //     {
                //         fprintf(stderr, "Failed writing %s\n", cur_arch->name);
                //         continue;
                //     }
                // }

                free(buf);
        }
    }

    arch_instance_close(&dst_inst);
}

#endif