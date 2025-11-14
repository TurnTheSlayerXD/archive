#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

typedef struct
{

    int s;
} S;

int main()
{
    const s = 1.;
    fprintf(stdout, "S.s = %lf, sizeof(s) = %lu\n", s, sizeof(s));
}
