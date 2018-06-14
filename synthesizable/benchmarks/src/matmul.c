#ifdef __unix__
#include <stdio.h>
#endif

#include <matmul.h>

int main(void)
{
    int i=0;
    int j;
    int k;
    TYPE sum;
    int kk;
    unsigned int count = 0;

    for (i=0; i<SIZE; i++)
    {
        for (j=0; j<SIZE; j++)
        {
            sum = 0;
            for(k = 0; k<SIZE; k++)
                sum += A[(i<<SHIFT) + k] * B[(k<<SHIFT) + j];
            result[(i<<SHIFT) + j] = sum;
        }
    }
    
#ifdef __unix__
    for(i = 0; i < SIZE; ++i)
    {
        for(j = 0; j < SIZE; ++j)
            printf("%d ", result[(i<<SHIFT) + j]);
        printf("\n");
    }
#endif

    return 0;
}
