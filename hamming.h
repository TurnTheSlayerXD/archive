#ifndef HAMMING_H
#define HAMMING_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

typedef struct bit_vec
{
    char *ptr;
    size_t r_size;
    size_t bit_count;
} bit_vec;

bit_vec bit_vec_new(size_t bit_count)
{
    size_t r_size = bit_count / 8 + ((bit_count % 8) > 0 ? 1 : 0);
    return (bit_vec){
        .ptr = calloc(r_size, sizeof(char)),
        .r_size = r_size,
        .bit_count = bit_count};
}

void bit_vec_delete(bit_vec *ptr)
{
    free(ptr->ptr);
    *ptr = (bit_vec){0};
}

char bit_vec_get_bit_at(const bit_vec *vec, size_t i)
{
    assert((i / 8) < vec->r_size);
    return (vec->ptr[i / 8] >> (i % 8)) & 1;
}

void bit_vec_set_bit_at(bit_vec *vec, size_t i, char val)
{
    assert((i / 8) < vec->r_size);
    size_t j = i % 8;
    vec->ptr[i / 8] = ((~(val << j)) & vec->ptr[i / 8]) | (val << j);
}

typedef struct bit_mat
{
    bit_vec *ptr;

    size_t rows;
    size_t cols;
} bit_mat;

bit_mat create_bit_mat(size_t rows, size_t cols)
{
    bit_mat mat = {.ptr = malloc(sizeof(bit_vec) * rows), .rows = rows, .cols = cols};
    for (size_t i = 0; i < rows; ++i)
    {
        mat.ptr[i] = bit_vec_new(cols);
    }
    return mat;
}

void delete_bit_mat(bit_mat *mat)
{
    for (size_t i = 0; i < mat->rows; ++i)
    {
        bit_vec_delete(&mat->ptr[i]);
    }
    free(mat->ptr);
    *mat = (bit_mat){0};
}

char *bit_vec_to_str(const bit_vec *vec);

typedef struct
{
    bit_mat mat;
    char *prod_res;
} mat_and_product_result;

size_t *build_product_result(const bit_vec vec, size_t K)
{
    bit_mat mat = create_bit_mat(K, vec.bit_count);

    for (size_t i = 0; i < vec.bit_count; ++i)
    {
        size_t num = i + 1;
        for (size_t k = 0; k < 8 * sizeof(size_t) && (num >> k) > 0; ++k)
        {
            if ((num >> k) & 1)
            {
                bit_vec_set_bit_at(&mat.ptr[k], i, 1);
            }
        }
    }

    size_t *res = calloc(K, sizeof(size_t));

    for (size_t i = 0; i < K; ++i)
    {
        for (size_t j = 0; j < vec.bit_count; ++j)
        {
            res[i] += bit_vec_get_bit_at(&vec, j) * bit_vec_get_bit_at(&mat.ptr[i], j);
        }
    }

    for (size_t i = 0; i < K; ++i)
    {
        res[i] &= 1;
    }

    delete_bit_mat(&mat);
    return res;
}

bit_vec hamming_algo(const bit_vec vec)
{
    size_t K = 1;
    const size_t N = vec.bit_count;

    while (K != (size_t)ceil(log2(N + K + 1)))
    {
        ++K;
    }

    bit_vec encode_vec = bit_vec_new(N + K);

    for (size_t i = 0, j = 1, k = 0; i < encode_vec.bit_count; ++i)
    {
        if (i + 1 != j)
        {
            bit_vec_set_bit_at(&encode_vec, i, bit_vec_get_bit_at(&vec, k));
            k += 1;
        }
        else
        {
            j *= 2;
        }
    }

    size_t *control_bits = build_product_result(encode_vec, K);
    for (size_t m = 1, i = 0; i < K; m *= 2, ++i)
    {
        bit_vec_set_bit_at(&encode_vec, m - 1, (char)control_bits[i]);
    }
    free(control_bits);
    return encode_vec;
}

typedef struct
{
    bool ok;
    bit_vec vec;
} hamming_decode_res;

hamming_decode_res hamming_decode(const bit_vec vec)
{
    const size_t K = (size_t)ceil(log2(vec.bit_count + 1));

    const size_t N = vec.bit_count - K;

    size_t *control_bits = build_product_result(vec, K);

    for (size_t i = 0; i < K; ++i)
    {
        if (control_bits[i] != 0)
        {
            free(control_bits);
            return (hamming_decode_res){.ok = false, .vec = {0}};
        }
    }

    bit_vec decoded = bit_vec_new(N);
    for (size_t m = 1, i = 0, j = 0; i < vec.bit_count; ++i)
    {
        if (i + 1 != m)
        {
            bit_vec_set_bit_at(&decoded, j, bit_vec_get_bit_at(&vec, i));
            j += 1;
        }
        else
        {
            m *= 2;
        }
    }
    free(control_bits);

    return (hamming_decode_res){.ok = true, .vec = decoded};
}

char *bit_vec_to_str(const bit_vec *vec)
{
    char *str = malloc(vec->bit_count + 1);
    for (size_t i = 0; i < vec->bit_count; ++i)
    {
        str[i] = bit_vec_get_bit_at(vec, i) + '0';
    }
    str[vec->bit_count] = '\0';
    return str;
}

#endif