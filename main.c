#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

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
    config cnf = config_new(inst.hdr.bytes_per_read, 2);

    const char *filenames[] = {"test.txt", "main.c", "test.c"};
    const int len = sizeof(filenames) / sizeof(const char *);

    arch_insert_files(&inst, filenames, len, cnf);
    fprintf(stdout, "inst.hdr.file_count = %lu\n", inst.hdr.file_count);
    arch_instance_close(&inst);
}

void test_extract()
{
    arch_instance inst = arch_instance_create("test.ham", true);
    config cnf = config_new(inst.hdr.bytes_per_read, 2);
    arch_extract_files(&inst, "./testdir", cnf);
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
    const char **args;

    size_t arg_count;
    bool appears;

    OPT_E code;
} cmd_opt;

bool starts_with(const char *str, const char *substr)
{
    return strstr(str, substr) == str;
}

int main(int argc, char **argv)
{

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

    const char *archname = NULL;

    for (int arg_i = 0; arg_i < argc; ++arg_i)
    {
        const char *arg = argv[arg_i];
        if (starts_with(arg, "-"))
        {

            cmd_opt *right_opt = NULL;

            for (size_t opt_i = 0; opt_i < sizeof(opts); ++opt_i)
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
            }

            if (!right_opt)
            {
                fprintf(stderr, "Error parsing: unknown opt = %s", arg);
                ret_code = 69;
                goto early_exit;
            }

            right_opt->appears = true;

            do
            {
                ++arg_i;

                right_opt->args = realloc(right_opt->args, right_opt->arg_count + 1);
                right_opt->args[right_opt->arg_count] = argv[arg_i];
                right_opt->arg_count += 1;
            } while (arg_i < argc && !starts_with(argv[arg_i], "-"));

            arg_i -= 1;
        }
        else
        {
            if (archname)
            {
                fprintf(stderr, "Arch name was already mentioned, prev value = %s, next value = %s", archname, argv[arg_i]);
                ret_code = 69;
                goto early_exit;
            }
            archname = argv[arg_i];
        }
    }

    for (size_t i = 0; i < sizeof(opts) / sizeof(cmd_opt); ++i)
    {
        const cmd_opt *opt = &opts[i];
        if (!opt->appears)
        {
            continue;
        }

        switch (opt->code)
        {

        case OPT_CREATE:
        {
            
            break;
        }
        case OPT_APPEND:
        {
            break;
        }
        case OPT_CONCAT:
        {
            break;
        }
        case OPT_DELETE:
        {
            break;
        }
        case OPT_EXTRACT:
        {
            break;
        }
        case OPT_FILE:
        {
            break;
        }
        case OPT_LIST:
        {
            break;
        }

        default:
        {
            break;
        }
        }
    }

early_exit:
    for (int i = 0; i < argc; ++i)
    {
        free(opts[i].args);
        opts[i] = (cmd_opt){0};
    }

    return ret_code;
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
