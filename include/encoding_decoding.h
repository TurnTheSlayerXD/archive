#ifndef ENCODING_DECODING_H
#define ENCODING_DECODING_H

#include <stdio.h>
#include <stdlib.h>

#include "hamming.h"

typedef struct
{
    size_t BYTES_per_chunk;
    size_t BITS_per_chunk;
    size_t enc_BYTES_per_chunk;
    size_t enc_BITS_per_chunk;

    size_t FREE_FILE_COUNT;
} config;

config config_new(size_t bytes_per_read, size_t FREE_FILE_COUNT)
{
    size_t enc_bits_per_chunk = hamming_calc_encoded_size(bytes_per_read * BITS_IN_BYTE);
    size_t enc_bytes_per_chunk = (size_t)ceil((double)enc_bits_per_chunk / 8.);
    return (config){
        .BYTES_per_chunk = bytes_per_read,
        .BITS_per_chunk = bytes_per_read * BITS_IN_BYTE,
        .enc_BYTES_per_chunk = enc_bytes_per_chunk,
        .enc_BITS_per_chunk = enc_bits_per_chunk,
        .FREE_FILE_COUNT = FREE_FILE_COUNT,
    };
}

size_t do_file_encoding(FILE *input_file, size_t input_file_len, FILE *output_file, config cnf)
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
    size_t enc_file_len;
} encoded_file;

void do_file_decoding(encoded_file enc_file, FILE *output_file, config cnf)
{
    size_t cur_pos = 0;
    bit_vec vec = bit_vec_new(cnf.enc_BITS_per_chunk);
    assert(vec.r_size == cnf.enc_BYTES_per_chunk);
    while (cur_pos + cnf.enc_BYTES_per_chunk < enc_file.enc_file_len)
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
        cur_pos += cnf.enc_BYTES_per_chunk;
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
        cur_pos += cnf.enc_BYTES_per_chunk;
    }
    else
    {
        size_t n_bytes_enc = enc_file.enc_file_len - cur_pos;
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

size_t calc_encoded_size(size_t init_size, config cnf)
{
    size_t k = init_size / cnf.BYTES_per_chunk;
    size_t enc_whole = k * cnf.enc_BYTES_per_chunk;

    size_t left = init_size % cnf.BYTES_per_chunk;

    if (left == 0)
    {
        return enc_whole;
    }
    size_t enc_left = ceil(hamming_calc_encoded_size(left * BITS_IN_BYTE) / 8.);
    return enc_whole + enc_left;
}
#endif