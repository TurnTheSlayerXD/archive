#ifndef ARCH_INSTANCE_H

#define ARCH_INSTANCE_H

#include <stdio.h>
#include <unistd.h>
#include <encoding_decoding.h>

typedef struct
{
    char id[3];
    size_t file_count;
    size_t free_file_count;
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
        arch_instance inst = (arch_instance){.f = f, .name = path, .hdr = hdr, .file_hdrs = NULL};
        fprintf(stdout, "arch (updated) at path %s contains n = %lu files, available space = %lu", path, inst.hdr.file_count, inst.hdr.free_file_count);

        if (!inst.hdr.file_count)
        {
            return inst;
        }

        inst.file_hdrs = calloc(sizeof(arch_file_header), inst.hdr.file_count);

        for (size_t i = 0; i < inst.hdr.file_count; ++i)
        {
            if (fread(&inst.file_hdrs[i], sizeof(arch_file_header), 1, f) != 1)
            {
                fprintf(stderr, "(update) could not properly read %lu-th file HEADER from arch %s", i, path);
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
    arch_header hdr = {.file_count = 0, .id = "HAM", .free_file_count = 0};
    arch_instance inst = {
        .f = f,
        .name = path,
        .hdr = hdr,
        .file_hdrs = NULL,
    };
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
        return (file_to_append){0};
    }
    fseek(str.f_stream, 0, SEEK_END);
    str.file_size = ftell(str.f_stream);
    fseek(str.f_stream, 0, SEEK_SET);
    return str;
}

arch_file_header *arch_instance_fit_files(arch_instance *inst, const file_to_append *new_files, size_t new_file_count, config cnf)
{
    assert(new_file_count > 0);

    if (inst->hdr.file_count == 0)
    {
        inst->file_hdrs = realloc(inst->file_hdrs, new_file_count * sizeof(arch_file_header));
        inst->hdr.file_count = new_file_count;

        size_t f_offset = sizeof(arch_file_header) + (new_file_count + inst->hdr.free_file_count) * sizeof(arch_file_header);
        for (size_t i = 0; i < new_file_count; ++i)
        {
            arch_file_header hdr;
            hdr.init_size = new_files[i].file_size;
            hdr.enc_size = calc_encoded_size(new_files[i].file_size, cnf);
            strncpy(hdr.filename, new_files->filename, arch_file_header_NAME_LEN);
            hdr.offset = f_offset;

            inst->file_hdrs[i] = hdr;
            f_offset += hdr.enc_size;
        }
        return inst->file_hdrs;
    }

    if (inst->hdr.free_file_count < new_file_count)
    {
        inst->hdr.free_file_count = cnf.FREE_FILE_COUNT;

        // move old files
        size_t new_offset_for_first_file = (inst->hdr.file_count + new_file_count + inst->hdr.free_file_count) * sizeof(arch_file_header) + sizeof(arch_file_header);
        size_t old_offset_for_first_file = inst->file_hdrs[0].offset;

        size_t dif = new_offset_for_first_file - old_offset_for_first_file;

        for (size_t i = 0; i < inst->hdr.file_count; ++i)
        {
            inst->file_hdrs[i].offset += dif;
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
        strncpy(hdr.filename, new_files[i].filename, arch_file_header_NAME_LEN);
        hdr.enc_size = calc_encoded_size(hdr.init_size, cnf);
        hdr.offset = f_offset;

        inst->file_hdrs[inst->hdr.file_count + i] = hdr;
        f_offset += hdr.enc_size;
    }

    inst->hdr.file_count += new_file_count;

    return &inst->file_hdrs[inst->hdr.file_count - new_file_count];
}

void arch_instance_insert_files(arch_instance inst, const char **filenames, size_t file_count, config cnf)
{

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

        arch_file_header *hdrs = arch_instance_fit_files(&inst, files, file_count, cnf);

        for (size_t i = 0; i < file_count; ++i)
        {
            

            do_file_encoding()
        }

        break;
    }

    for (size_t i = 0; i < file_count; ++i)
    {
        if (files[i].f_stream)
        {
            fclose(files[i].f_stream);
        }
    }

    free(files);
}

#endif