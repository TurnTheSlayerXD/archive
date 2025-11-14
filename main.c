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
    const len = sizeof(filenames) / sizeof(const char *);

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
    APPEND,
} OPT_E;

typedef struct
{
    const char *s_alias;
    const char *l_alias;
    const char **args;

    size_t arg_count;

    OPT_E num;
} cmd_opt;

bool starts_with(const char *str, const char *substr)
{
    return strstr(str, substr) == str;
}

int main(int argc, char **argv)
{

    cmd_opt opts[] =
        {
            {},
        };

    int ret_code = 0;

    const char *archname;

    while (true)
    {

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
                    break;
                }

                do
                {
                    ++arg_i;

                    right_opt->args = realloc(right_opt->args, right_opt->arg_count + 1);
                    right_opt->args[right_opt->arg_count] = argv[arg_i];
                    right_opt->arg_count += 1;

                } while (arg_i < argc && !starts_with(argv[arg_i], "-"));

                if (arg_i < argc)
                {
                    arg_i -= 1;
                }
            }
            else
            {
                archname = argv[arg_i];
            }
        }

        break;
    }

    

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
