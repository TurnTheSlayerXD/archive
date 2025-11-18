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

    arch_file_header *file_hdrs;
} arch_instance;

void arch_instance_close(arch_instance *inst)
{
    free(inst->file_hdrs);
    fclose(inst->f);
    *inst = (arch_instance){0};
}

arch_instance arch_instance_create_empty(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "arch (created) at path [%s] could not be created\n", path);
        return (arch_instance){0};
    }
    arch_header hdr = {.file_count = 0, .id = "HAM", .free_file_count = 0, .bytes_per_read = 100};
    arch_instance inst = {
        .f = f,
        .name = path,
        .hdr = hdr,
        .file_hdrs = NULL,
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
        arch_instance inst = (arch_instance){.f = f, .name = path, .hdr = hdr, .file_hdrs = NULL};
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

    return arch_instance_create_empty(path);
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

arch_file_header *arch_get_new_headers(arch_instance *inst, const file_to_append *new_files, size_t new_file_count, config cnf)
{
    assert(new_file_count > 0);

    if (inst->hdr.file_count == 0)
    {
        inst->file_hdrs = realloc(inst->file_hdrs, new_file_count * sizeof(arch_file_header));
        inst->hdr.free_file_count = cnf.FREE_FILE_COUNT;

        size_t f_offset = sizeof(arch_file_header) + (new_file_count + inst->hdr.free_file_count) * sizeof(arch_file_header);
        for (size_t i = 0; i < new_file_count; ++i)
        {
            arch_file_header hdr;
            hdr.init_size = new_files[i].file_size;
            hdr.enc_size = calc_encoded_size(new_files[i].file_size, cnf);
            strncpy(hdr.filename, new_files[i].filename, arch_file_header_NAME_LEN - 1);
            hdr.offset = f_offset;

            inst->file_hdrs[i] = hdr;
            f_offset += hdr.enc_size;
        }

        inst->hdr.file_count += new_file_count;
        return inst->file_hdrs;
    }

    if (inst->hdr.free_file_count < new_file_count)
    {
        inst->hdr.free_file_count = cnf.FREE_FILE_COUNT;

        // move old files
        size_t new_offset_for_first_file = (inst->hdr.file_count + new_file_count + inst->hdr.free_file_count) * sizeof(arch_file_header) + sizeof(arch_file_header);
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
        inst->hdr.free_file_count -= new_file_count;
    }

    inst->file_hdrs = realloc(inst->file_hdrs, sizeof(arch_file_header) * (inst->hdr.file_count + new_file_count));
    arch_file_header *last_file = &inst->file_hdrs[inst->hdr.file_count - 1];
    size_t f_offset = last_file->offset + last_file->enc_size;
    for (size_t i = 0; i < new_file_count; ++i)
    {
        arch_file_header hdr;
        hdr.init_size = new_files[i].file_size;
        strncpy(hdr.filename, new_files[i].filename, arch_file_header_NAME_LEN - 1);
        hdr.enc_size = calc_encoded_size(hdr.init_size, cnf);
        hdr.offset = f_offset;

        inst->file_hdrs[inst->hdr.file_count + i] = hdr;
        f_offset += hdr.enc_size;
    }

    inst->hdr.file_count += new_file_count;

    return &inst->file_hdrs[inst->hdr.file_count - new_file_count];
}

void arch_insert_files(arch_instance *inst, const char **filenames, size_t file_count, config cnf)
{
    assert(file_count > 0);
    file_to_append *files = calloc(file_count, sizeof(file_to_append));
    while (true)
    {
        for (size_t i = 0; i < file_count; ++i)
        {
            files[i] = file_to_append_open(filenames[i]);
            if (!files[i].f_stream)
            {
                break;
            }
        }
        arch_file_header *new_hdrs = arch_get_new_headers(inst, files, file_count, cnf);
        for (size_t i = 0; i < file_count; ++i)
        {
            if (fseek(inst->f, new_hdrs[i].offset, SEEK_SET))
            {
                assert(false && "fseek(inst.f, hdrs[i].offset, SEEK_SET)");
            }
            do_file_encoding(files[i].f_stream, new_hdrs[i].init_size, inst->f, cnf);
        }
        file_write_pos(0, &inst->hdr, sizeof(arch_header), inst->f);
        file_write_pos(sizeof(arch_header), inst->file_hdrs, sizeof(arch_file_header) * inst->hdr.file_count, inst->f);
        break;
    }

    for (size_t i = 0; i < file_count; ++i)
    {
        file_to_append_close(&files[i]);
    }
    free(files);
}

bool __arch_extract_single(arch_instance *inst, const arch_file_header *hdr, const char *dir, config cnf)
{
    char fin_name[150] = {0};
    snprintf(fin_name, 150, "%s/%s", dir, hdr->filename);
    FILE *f = fopen(fin_name, "w");
    if (!f)
    {
        fprintf(stderr, "Could not create file to extract: %s\n", fin_name);
        return false;
    }
    if (fseek(inst->f, hdr->offset, SEEK_SET))
    {
        assert(false && "fseek(inst->f, hdr->offset, SEEK_SET)");
    }
    do_file_decoding((encoded_file){.file = inst->f, .src_file_len = hdr->init_size, .enc_file_len = hdr->enc_size}, f, cnf);
    fclose(f);
    return true;
}

void arch_extract_files(arch_instance *inst, const char *dir, const char **files, size_t files_len, config cnf)
{
    if (files_len == 0)
    {
        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            __arch_extract_single(inst, &inst->file_hdrs[i], dir, cnf);
        }
        return;
    }

    for (size_t name_i = 0; name_i < files_len; ++name_i)
    {
        const arch_file_header *hdr = NULL;
        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            if (strcmp(inst->file_hdrs[i].filename, files[name_i]) == 0)
            {
                hdr = &inst->file_hdrs[i];
                break;
            }
        }

        if (!hdr)
        {
            fprintf(stderr, "No file [%s] in archive [%s]", files[name_i], inst->name);
            continue;
        }
        __arch_extract_single(inst, hdr, dir, cnf);
    }
}

void arch_delete_files(arch_instance *inst, const char **files, size_t files_len, const char *dir, config cnf)
{
    for (size_t i = 0; i < files_len; ++i)
    {
        const char *fname = &files[i];

        arch_file_header *hdr = NULL;
        for (size_t hdr_i = 0; hdr_i < inst->hdr.file_count; ++hdr_i)
        {
            if (strcmp(inst->file_hdrs[hdr_i].filename, fname) == 0)
            {
                hdr = &inst->file_hdrs[hdr_i];
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
                continue;
            }
        }

        free(buf);
    }
}

void arch_concat_archs(arch_instance *inst_arr, size_t inst_arr_len, config *config);

#endif