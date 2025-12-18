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
    if ((((bwt_stack_t *)a)->first == ((bwt_stack_t *)b)->first)) {
        // they are equal so take into consideration the subsequent values in the string
        size_t subseqa = ((bwt_stack_t *)a)->pos;
        size_t subseqb = ((bwt_stack_t *)b)->pos;
        do {
            subseqa++;
            if (subseqa == bwt_stack_size)
                subseqa = 0;
            subseqb++;
            if (subseqb == bwt_stack_size)
                subseqb = 0;
            printf("qsort_comp equal %ld %ld\n", subseqa, subseqb);
            if (instr[subseqa] == instr[subseqb])
                continue;
            return instr[subseqa] - instr[subseqb];
        } while (subseqa != ((bwt_stack_t *)a)->pos);
    } else {
        return (((bwt_stack_t *)a)->first - ((bwt_stack_t *)b)->first);
    }
}

int qsort_char_comp(const void *a, const void *b)
{
    return (*(char *)a - *(char *)b);
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
//    strcpy(instr, "It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness, it was the epoch of belief, it was the epoch of incredulity, it was the season of light, it was the season of darkness, it was the spring of hope, it was the winter of despair.");
    strcpy(instr, "^BANANA$");
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

    // to unconfuckulate:
    // copy BWT transform into the "last" position in the bwt_stack elements, sort it, and copy the sorted into the "first" positions.
    // It's already set up that way so we will proceed along
    // we already know the first position (first variable above), so we can reuse the "pos" to hold a count of how many times we have
    // seen a particular symbol in the last column.

    uint8_t count[256];
    for (int i = 0; i < 256; ++i)
        count[i] = 0;

    for (int i = first; i < bwt_stack_size; ++i) {
        bwt_stack[i].pos = count[bwt_stack[i].last];
        count[bwt_stack[i].last]++;
    }
    for (int i = 0; i < first; ++i) {
        bwt_stack[i].pos = count[bwt_stack[i].last];
        count[bwt_stack[i].last]++;
    }
    printf("counted: ");
    for (int i = 0; i < bwt_stack_size; ++i)
        printf("<%ld %c%c> ", bwt_stack[i].pos, bwt_stack[i].last, bwt_stack[i].first);
    printf("\n");

    for (int i = 0; i < 256; ++i)
        count[i] = 0;
    int in_ptr = first;
    size_t out_ptr = 1;
    outstr[0] = bwt_stack[first].first;
    char pair1 = outstr[0];

    while (out_ptr < bwt_stack_size) {
        int i;
        for (i = 0; i < bwt_stack_size; ++i) {
            if ((bwt_stack[i].last == pair1) && (bwt_stack[i].pos == count[pair1]))
                break;
        }
        count[pair1]++;
        outstr[out_ptr++] = bwt_stack[i].first;
        pair1 = bwt_stack[i].first;
        out_ptr++;
    }
    outstr[out_ptr] = 0;

    printf("decoded: %s\n", outstr);
    return 0;
}
