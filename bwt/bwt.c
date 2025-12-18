#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

char instr[1024];

typedef struct {
    uint8_t first;
    size_t pos;
    uint8_t last;
} bwt_stack_t;

bwt_stack_t bwt_stack[1024];
size_t bwt_stack_size = 0;

int qsort_comp(const void *a, const void *b)
{
    return (((bwt_stack_t *)a)->first - ((bwt_stack_t *)b)->first);
}

void ingest(char *str)
{
    bwt_stack_size = strlen(str);
    for (int i = 0; i < strlen(str); ++i) {
        bwt_stack[i].first = str[i];
        if (i > 0)
            bwt_stack[i].last = str[i - 1];
        else
            bwt_stack[i].last = str[bwt_stack_size - 1];
        bwt_stack[i].pos = i;
    }
}
int main(int argc, char **argv)
{
    strcpy(instr, "It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness, it was the epoch of belief, it was the epoch of incredulity, it was the season of light, it was the season of darkness, it was the spring of hope, it was the winter of despair.");
    ingest(instr);
//    printf("original: ");
//    for (int i = 0; i < bwt_stack_size; ++i)
//        printf("<%ld %c %c> ", bwt_stack[i].pos, bwt_stack[i].first, bwt_stack[i].last);
//    printf("\n");
    qsort(bwt_stack, bwt_stack_size, sizeof(bwt_stack_t), qsort_comp);
//    printf("sorted: ");
//    for (int i = 0; i < bwt_stack_size; ++i)
//        printf("<%ld %c %c> ", bwt_stack[i].pos, bwt_stack[i].first, bwt_stack[i].last);
//    printf("\n");
    char outstr[1024];
    int first = -1;
    for (int i = 0; i < bwt_stack_size; ++i) {
        outstr[i] = bwt_stack[i].last;
        if (bwt_stack[i].pos == 0)
            first = i;
    }
    outstr[bwt_stack_size] = 0;
    printf("BWT transform: (%d): %s\n", first, outstr);
    return 0;
}
