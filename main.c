#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "hamming.h"

#define READ_CHUNK_LENGTH 4

size_t file_size(FILE *f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_SET);

    long l = ftell(f);
    fseek(f, 0, SEEK_END);
    long r = ftell(f);

    fseek(f, pos, 0);

    return r - l;
}

typedef struct
{
    FILE *f;
    bit_vec vec;
} file_iter;

file_iter file_iter_init(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        fprintf(stderr, "Could not open file [%s] for READ", filename);
        assert(false);
    }


    

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

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    test_hamming();

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
