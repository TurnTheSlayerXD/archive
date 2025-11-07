#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include "hamming.h"

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

typedef struct
{
    size_t BYTES_per_chunk;
    size_t BITS_per_chunk;
    size_t enc_BYTES_per_chunk;
    size_t enc_BITS_per_chunk;
} config;
config cnf = {0};

config config_new(size_t bytes_per_read)
{
    size_t enc_bits_per_chunk = hamming_calc_encoded_size(bytes_per_read * BITS_IN_BYTE);
    size_t enc_bytes_per_chunk = (size_t)ceil((double)enc_bits_per_chunk / 8.);
    return (config){.BYTES_per_chunk = bytes_per_read,
                    .BITS_per_chunk = bytes_per_read * BITS_IN_BYTE,
                    .enc_BYTES_per_chunk = enc_bytes_per_chunk,
                    .enc_BITS_per_chunk = enc_bits_per_chunk};
}

size_t do_file_encoding(FILE *input_file, size_t input_file_len, FILE *output_file)
{
    assert(input_file_len > 0);
    size_t total_bytes_written = 0;
    size_t cur_pos = ftell(input_file);
    bit_vec vec = bit_vec_new(cnf.BITS_per_chunk);
    while (cur_pos + cnf.BYTES_per_chunk < input_file_len)
    {
        if (cnf.BYTES_per_chunk != fread(vec.ptr, 1, cnf.BYTES_per_chunk, input_file))
        {
            assert(false && "Expected to read cnf.BYTES_per_chunk");
        }
        bit_vec encoded = hamming_algo(vec);
        assert(encoded.bit_count == cnf.enc_BITS_per_chunk);
        assert(encoded.r_size == cnf.enc_BYTES_per_chunk);
        total_bytes_written += cnf.enc_BYTES_per_chunk;
        if (cnf.enc_BYTES_per_chunk != fwrite(encoded.ptr, 1, cnf.enc_BYTES_per_chunk, output_file))
        {
            assert(false && "Expected to write cnf.enc_BYTES_per_chunk");
        }
        bit_vec_delete(&encoded);
        cur_pos = ftell(input_file);
    }

    size_t bytes_left_to_read = input_file_len - cur_pos;
    bit_vec last_vec = bit_vec_new(bytes_left_to_read * BITS_IN_BYTE);
    if (bytes_left_to_read != fread(last_vec.ptr, 1, bytes_left_to_read, input_file))
    {
        assert(false && "Expected to read cnf.BYTES_per_chunk");
    }
    bit_vec encoded = hamming_algo(last_vec);
    total_bytes_written += encoded.r_size;
    if (encoded.r_size != fwrite(encoded.ptr, 1, encoded.r_size, output_file))
    {
        assert(false && "Expected to write cnf.enc_BYTES_per_chunk");
    }
    bit_vec_delete(&encoded);
    bit_vec_delete(&last_vec);
    bit_vec_delete(&vec);
    return total_bytes_written;
}

typedef struct
{
    FILE *file;
    size_t src_file_len;
    size_t file_len;
} encoded_file;

void do_file_decoding(encoded_file enc_file, FILE *output_file)
{
    size_t cur_pos = ftell(enc_file.file);
    bit_vec vec = bit_vec_new(cnf.enc_BITS_per_chunk);
    assert(vec.r_size == cnf.enc_BYTES_per_chunk);
    while (cur_pos + cnf.enc_BYTES_per_chunk < enc_file.file_len)
    {
        if (cnf.enc_BYTES_per_chunk != fread(vec.ptr, 1, cnf.enc_BYTES_per_chunk, enc_file.file))
        {
            assert(false && "do_file_decoding : expected to read cnf.enc_BYTES_per_chunk");
        }
        hamming_decode_res res = hamming_decode(vec);
        if (!res.ok)
        {
            fprintf(stderr, "Error while decoding\n");
            continue;
        }
        assert(res.vec.bit_count == cnf.BITS_per_chunk);
        if (cnf.BYTES_per_chunk != fwrite(res.vec.ptr, 1, cnf.BYTES_per_chunk, output_file))
        {
            assert(false && "do_file_decoding : expected to write cnf.BYTES_per_chunk");
        }
        bit_vec_delete(&res.vec);
        cur_pos = ftell(enc_file.file);
    }
    size_t n_bytes_src = enc_file.src_file_len % cnf.BYTES_per_chunk;
    if (n_bytes_src == 0)
    {
        if (cnf.enc_BYTES_per_chunk != fread(vec.ptr, 1, cnf.enc_BYTES_per_chunk, enc_file.file))
        {
            assert(false && "do_file_decoding : expected to read cnf.enc_BYTES_per_chunk");
        }
        hamming_decode_res res = hamming_decode(vec);
        if (!res.ok)
        {
            fprintf(stderr, "Error while decoding\n");
            return;
        }
        assert(res.vec.bit_count == cnf.BITS_per_chunk);
        if (cnf.BYTES_per_chunk != fwrite(res.vec.ptr, 1, cnf.BYTES_per_chunk, output_file))
        {
            assert(false && "do_file_decoding : expected to write cnf.BYTES_per_chunk");
        }
        bit_vec_delete(&res.vec);
        cur_pos = ftell(enc_file.file);
    }
    else
    {
        size_t n_bytes_enc = enc_file.file_len - cur_pos;
        size_t n_bits_enc = hamming_calc_encoded_size(n_bytes_src * BITS_IN_BYTE);
        bit_vec last_vec = bit_vec_new(n_bits_enc);
        assert(last_vec.r_size == n_bytes_enc);
        if (n_bytes_enc != fread(last_vec.ptr, 1, n_bytes_enc, enc_file.file))
        {
            assert(false && "do_file_decoding : expected to read n_bytes_in_last_chunk");
        }
        hamming_decode_res res = hamming_decode(last_vec);
        if (!res.ok)
        {
            fprintf(stderr, "do_file_decoding: failed decoding last chunk\n");
        }
        else
        {
            assert(res.vec.r_size * 8 == res.vec.bit_count && "do_file_decoding : expected to have even decoding of last chunk");
            if (res.vec.r_size != fwrite(res.vec.ptr, 1, res.vec.r_size, output_file))
            {
                assert(false && "do_file_decoding : expected to write decoded n_bytes_in_last_chunk");
            }
        }
        bit_vec_delete(&last_vec);
        bit_vec_delete(&res.vec);
    }
    bit_vec_delete(&vec);
}

const char *init_nm = "./test.txt";
const char *enc_nm = "./test-encoded.txt";
const char *dec_nm = "./test-encoded-decoded.txt";

void test_encoding()
{

    FILE *input_f = fopen(init_nm, "r");
    FILE *enc_f = fopen(enc_nm, "w+");
    if (!input_f || !enc_f)
    {
        fclose(input_f);
        fclose(enc_f);

        fprintf(stderr, "Could not open one of the files");
        return;
    }
    do_file_encoding(input_f, file_size(input_f), enc_f);

    fclose(input_f);
    fclose(enc_f);
}

void test_decoding()
{

    FILE *source = fopen(init_nm, "r");

    FILE *input_f = fopen(enc_nm, "r");
    FILE *dec_f = fopen(dec_nm, "w+");

    if (!input_f || !dec_f || !source)
    {
        fclose(input_f);
        fclose(dec_f);
        fclose(source);

        fprintf(stderr, "Could not open one of the files");
        return;
    }

    do_file_decoding((encoded_file){.file = input_f,
                                    .file_len = file_size(input_f),
                                    .src_file_len = file_size(source)},
                     dec_f);

    fclose(input_f);
    fclose(dec_f);
    fclose(source);
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
} arch_header;

typedef struct
{
    size_t init_size;
    size_t enc_size;
    size_t offset;
} file_desc;

typedef struct
{
    FILE *f;
    const char *name;
    size_t file_count;
    file_desc *file_hdrs;
} arch_instance;

arch_instance arch_instance_create(const char *path)
{
    FILE *f;
    arch_instance inst;
    if (access(path, F_OK) == 0)
    {
        f = fopen(path, "a+");
        if (!f)
        {
            fpritnf(stdout, "arch (updated) at path %s could not be opened", path);
            return (arch_instance){0};
        }
        inst = (arch_instance){.f = f, .name = path};
        if (!inst.file_count)
        {
            fprintf(stdout, "arch (updated) at path %s is empty", path);
            return (arch_instance){0};
        }
        if (fread(&inst.file_count, sizeof(file_desc), 1, f) != 1)
        {
            fprintf(stderr, "arch (updated) at path [%s] is corrupted, could not read file count", path);
            return (arch_instance){0};
        }
        inst.file_hdrs = malloc(sizeof(file_desc) * inst.file_count);
        if (fread(inst.file_hdrs, sizeof(file_desc), inst.file_count, f) != inst.file_count)
        {
            fprintf(stderr, "arch (updated) at path [%s] is corrupted, could not read file headers", path);
            return (arch_instance){0};
        }
    }
    else
    {
        f = fopen(path, "w+");
        if (!f)
        {
            fprintf(stderr, "arch (created) at path [%s] could not be created", path);
            return (arch_instance){0};
        }
        inst = (arch_instance){.f = f, .name = path, .file_count = 0, .file_hdrs = NULL};
        arch_header hdr = {.file_count = inst.file_count, .id = "HAM"};
        if (fwrite(&hdr, sizeof(arch_header), 1, f) != 1)
        {
            fprintf(stderr, "arch (created) at path [%s] could not be written with header", path);
            return (arch_instance){0};
        }
    }
    return inst;
};

void arch_instance_close(arch_instance *inst)
{
    free(inst->file_hdrs);
    fclose(inst->f);
    *inst = (arch_instance){0};
}


int main(int argc, char **argv)
{
    cnf = config_new(100000);

    (void)argc;
    (void)argv;

    test_encoding();
    test_decoding();

    FILE *init = fopen(init_nm, "r");
    FILE *after = fopen(dec_nm, "r");

    size_t f_size = file_size(init);
    if (f_size != file_size(after))
    {
        fprintf(stderr, "Sizes do not match: %ld != %ld\n", file_size(init), file_size(after));
        fclose(init);
        fclose(after);
        return 0;
    }

    char *init_data = malloc(f_size);
    char *after_data = malloc(f_size);

    if (fread(init_data, 1, f_size, init) != f_size)
    {
        fprintf(stderr, "error reading main");
    }
    if (fread(after_data, 1, f_size, after) != f_size)
    {
        fprintf(stderr, "error reading main");
    }

    if (memcmp(init_data, after_data, f_size) != 0)
    {
        fprintf(stderr, "Failure: mistmatch!\n");
    }
    else
    {
        fprintf(stdout, "Success: Data matches!\n");
    }

    fclose(init);
    fclose(after);
    free(init_data);
    free(after_data);

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
