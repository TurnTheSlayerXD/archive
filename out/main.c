#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "hamming.h"
#include "encoding_decoding.h"
#include "arch_instance.h"

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

void test_insert()
{
    arch_instance inst = arch_instance_create("test.ham", false);
    if (!inst.f)
    {
        fprintf(stderr, "Failed opening or creating archive");
        return;
    }

    char f1[] = "test.txt", f2[] = "main.c", f3[] = "test.c";

    char *filenames[3] = {f1, f2, f3};
    const int len = COUNT_OF(filenames);
    arch_insert_files(&inst, (string_array){.arr = filenames, .len = len});
    fprintf(stdout, "inst.hdr.file_count = %lu\n", inst.hdr.file_count);
    arch_instance_close(&inst);
}

void test_extract()
{
    arch_instance inst = arch_instance_create("test.ham", true);
    arch_extract_files(&inst, "./testdir", (string_array){0});
    arch_instance_close(&inst);
}

typedef enum
{
    OPT_CREATE,
    OPT_FILE,
    OPT_LIST,
    OPT_EXTRACT,
    OPT_APPEND,
    OPT_DELETE,
    OPT_CONCAT,
} OPT_E;

typedef struct
{
    const char *s_alias;
    const char *l_alias;
    char **args;

    size_t arg_count;
    bool appears;

    OPT_E code;
} cmd_opt;

#define EXIT_EARLY \
    ret_code = 69; \
    goto early_exit;

bool starts_with(const char *str, const char *substr)
{
    return strstr(str, substr) == str;
}

bool check_no_args(const cmd_opt *restrict all, size_t all_len, const OPT_E *restrict except, size_t except_len)
{
    for (size_t opt_i = 0; opt_i < all_len; ++opt_i)
    {
        bool exists_in_except = false;
        const cmd_opt *cur_opt = &all[opt_i];

        for (size_t j = 0; j < except_len; ++j)
        {
            if (except[j] == cur_opt->code)
            {
                exists_in_except = true;
                break;
            }
        }

        if (!exists_in_except && cur_opt->appears)
        {
            fprintf(stderr, "Unexpected argument: %s\n", cur_opt->s_alias);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    argc -= 1;
    argv += 1;

    cmd_opt opts[] =
        {
            {
                .s_alias = "-c",
                .l_alias = "--create",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_CREATE,
            },
            {
                .s_alias = "-f",
                .l_alias = "--file",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_FILE,
            },
            {
                .s_alias = "-l",
                .l_alias = "--list",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_LIST,
            },
            {
                .s_alias = "-x",
                .l_alias = "--extract",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_EXTRACT,
            },
            {
                .s_alias = "-a",
                .l_alias = "--append",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_APPEND,
            },
            {
                .s_alias = "-d",
                .l_alias = "--delete",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_DELETE,
            },
            {
                .s_alias = "-A",
                .l_alias = "--concatenate",
                .appears = false,
                .args = NULL,
                .arg_count = 0,
                .code = OPT_CONCAT,
            },
        };

    int ret_code = 0;

    for (int arg_i = 0; arg_i < argc; ++arg_i)
    {
        char *arg = argv[arg_i];
        if (starts_with(arg, "-"))
        {

            cmd_opt *right_opt = NULL;

            for (size_t opt_i = 0; opt_i < COUNT_OF(opts); ++opt_i)
            {
                cmd_opt *opt = &opts[opt_i];
                if (strcmp(opt->l_alias, arg) == 0)
                {
                    right_opt = opt;
                    break;
                }
                else if (strcmp(opt->s_alias, arg) == 0)
                {
                    right_opt = opt;
                    break;
                }
                else if (starts_with(arg, "--file="))
                {
                    right_opt = &opts[OPT_FILE];
                    right_opt->args = realloc(right_opt->args, (right_opt->arg_count + 1) * sizeof(char *));
                    right_opt->args[right_opt->arg_count] = arg + sizeof("--file=") - 1;
                    right_opt->arg_count += 1;
                    break;
                }
            }

            if (!right_opt)
            {
                fprintf(stderr, "Error parsing: unknown opt = %s", arg);
                EXIT_EARLY;
            }

            if (right_opt->appears)
            {
                fprintf(stderr, "Error: options cannot repeat more than once.\nRepeated opt: %s\n", right_opt->l_alias);
                EXIT_EARLY;
            }

            right_opt->appears = true;

            arg_i += 1;
            for (; arg_i < argc && !starts_with(argv[arg_i], "-"); ++arg_i)
            {
                right_opt->args = realloc(right_opt->args, (right_opt->arg_count + 1) * sizeof(char *));
                right_opt->args[right_opt->arg_count] = argv[arg_i];
                right_opt->arg_count += 1;
            }
            arg_i -= 1;
        }
        else
        {
            fprintf(stdout, "Ignoring argument - %s\n", arg);
        }
    }

    if (!opts[OPT_FILE].appears)
    {
        fprintf(stderr, "Expected --file option\n");
        EXIT_EARLY;
    }
    if (opts[OPT_FILE].arg_count < 1)
    {
        fprintf(stderr, "Expected --file option to have at least one arg = [archname]\n");
        EXIT_EARLY;
    }
    const char *archname = opts[OPT_FILE].args[0];

    if (opts[OPT_CREATE].appears)
    {
        OPT_E allowed[] = {OPT_CREATE, OPT_FILE};
        if (!check_no_args(opts, COUNT_OF(opts), allowed, COUNT_OF(allowed)))
        {
            EXIT_EARLY;
        }

        if (opts[OPT_CREATE].arg_count != 0)
        {
            fprintf(stderr, "Expected --create option to have one ZERO args\n");
            EXIT_EARLY;
        }

        arch_instance inst = arch_instance_create_empty(archname, (config){0});
        if (!inst.f)
        {
            EXIT_EARLY;
        }
        if (opts[OPT_FILE].arg_count < 2)
        {
            fprintf(stdout, "No files passed to insert to archive [%s]\n", archname);
            goto early_exit;
        }

        arch_insert_files(&inst, (string_array){.arr = opts[OPT_FILE].args + 1, .len = opts[OPT_FILE].arg_count - 1});
        arch_instance_close(&inst);
    }
    else if (opts[OPT_EXTRACT].appears)
    {
        OPT_E allowed[] = {OPT_EXTRACT, OPT_FILE};
        if (!check_no_args(opts, COUNT_OF(opts), allowed, COUNT_OF(allowed)))
        {
            EXIT_EARLY;
        }

        arch_instance inst = arch_instance_create(archname, true);
        if (!inst.f)
        {
            EXIT_EARLY;
        }

        char dir[100] = "./extract_dir_";

        strncat(dir, get_clean_filename(archname), 100 - 1);
        mkdir_if_no(dir);

        string_array_to_free files = arch_extract_files(&inst, dir, (string_array){.arr = opts[OPT_EXTRACT].args, .len = opts[OPT_EXTRACT].arg_count});
        string_array_to_free_close(&files);
        arch_instance_close(&inst);
    }
    else if (opts[OPT_DELETE].appears)
    {
        OPT_E allowed[] = {OPT_DELETE, OPT_FILE};
        if (!check_no_args(opts, COUNT_OF(opts), allowed, COUNT_OF(allowed)))
        {
            EXIT_EARLY;
        }

        arch_instance inst = arch_instance_create(archname, true);
        if (!inst.f)
        {
            EXIT_EARLY;
        }

        char dir[100] = "./delete_dir_";
        strncat(dir, inst.name, COUNT_OF(dir) - COUNT_OF("./delete_dir_") - 1);

        mkdir_if_no(dir);

        arch_delete_files(&inst, (string_array){.arr = opts[OPT_DELETE].args, .len = opts[OPT_DELETE].arg_count}, dir);
        arch_instance_close(&inst);
    }
    else if (opts[OPT_LIST].appears)
    {
        OPT_E allowed[] = {OPT_LIST, OPT_FILE};
        if (!check_no_args(opts, COUNT_OF(opts), allowed, COUNT_OF(allowed)))
        {
            EXIT_EARLY;
        }

        arch_instance inst = arch_instance_create(archname, true);
        if (!inst.f)
        {
            EXIT_EARLY;
        }

        arch_list_files(&inst);
        arch_instance_close(&inst);
    }
    else if (opts[OPT_CONCAT].appears)
    {
        OPT_E allowed[] = {OPT_CONCAT, OPT_FILE};
        if (!check_no_args(opts, COUNT_OF(opts), allowed, COUNT_OF(allowed)))
        {
            EXIT_EARLY;
        }

        size_t archs_len = opts[OPT_CONCAT].arg_count;
        arch_instance *archs = calloc(archs_len, sizeof(arch_instance));

        bool is_err = false;
        while (true)
        {
            for (size_t i = 0; i < archs_len; ++i)
            {
                archs[i] = arch_instance_create(opts[OPT_CONCAT].args[i], true);
                if (!archs[i].f)
                {
                    fprintf(stderr, "Concatenation err: could not open archive on path %s", opts[OPT_CONCAT].args[i]);
                    is_err = true;
                    break;
                }
            }
            arch_concat_archs(archname, (arch_array){.arr = archs, .len = archs_len});

            break;
        }

        for (size_t i = 0; i < archs_len; ++i)
        {
            if (archs->f)
            {
                arch_instance_close(&archs[i]);
            }
        }

        if (is_err)
        {
            EXIT_EARLY;
        }
    }
    else if (opts[OPT_APPEND].appears)
    {
        OPT_E allowed[] = {OPT_APPEND, OPT_FILE};
        if (!check_no_args(opts, COUNT_OF(opts), allowed, COUNT_OF(allowed)))
        {
            EXIT_EARLY;
        }
        if (opts[OPT_FILE].arg_count != 1)
        {
            fprintf(stderr, "Expected archive name\n");
            EXIT_EARLY;
        }
        arch_instance inst = arch_instance_create(opts[OPT_FILE].args[0], false);
        arch_insert_files(&inst, (string_array){.arr = opts[OPT_APPEND].args, .len = opts[OPT_APPEND].arg_count});
        arch_instance_close(&inst);
    }
    else
    {
        fprintf(stdout, "No args were passed to hamarc except path to arch = [%s]\n", archname);
    }

early_exit:
    for (size_t i = 0; i < COUNT_OF(opts); ++i)
    {
        free(opts[i].args);
        opts[i] = (cmd_opt){0};
    }

    return ret_code;
}
