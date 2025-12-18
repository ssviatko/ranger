#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// msort data structures
typedef int (*msort_comp)(uint8_t *, size_t, uint8_t *, size_t);
typedef void (*msort_swap)(uint8_t *, size_t, uint8_t *, size_t);
typedef void (*msort_copy)(uint8_t *, size_t, uint8_t *, size_t);

typedef struct {
    size_t range_lo;
    size_t range_hi;
    int leg; // 0 = left leg, 1 = right leg
} msort_depth;

int mlist[] = { 45, 23, 98, 342, 179, 8, 26, 77, 112, 193, 31, 3, 62, 99, 136, 87, 242, 15, 111, 32, 37 };

void msort(uint8_t *array, size_t element_size, size_t element_count, msort_comp comp_func, msort_swap swap_func, msort_copy copy_func)
{
    // sanity check element_count, can't sort 0 or 1 item
    if (element_count < 2)
        return;
    // 24 bit limit
    if (element_count > 16777215)
        return;

    msort_depth depth[25]; // hard limit of 16 million element array - root level + 24 levels
    int level = -1; // -1 indicates we're at the top of/outside of the array
    int traverse = 0; // 0 = traverse down, -1 = traverse up

    // allocate backing arrays
    uint8_t *left_backing = NULL;
    size_t left_backing_size;
    left_backing = malloc(element_size * element_count);
    if (left_backing == NULL) {
        fprintf(stderr, "memory error\n");
        exit(EXIT_FAILURE);
    }
    uint8_t *right_backing = NULL;
    size_t right_backing_size;
    right_backing = malloc(element_size * element_count);
    if (right_backing == NULL) {
        fprintf(stderr, "memory error\n");
        exit(EXIT_FAILURE);
    }
    uint8_t *center_backing = NULL;
    size_t center_backing_size;
    center_backing = malloc(element_size * element_count);
    if (center_backing == NULL) {
        fprintf(stderr, "memory error\n");
        exit(EXIT_FAILURE);
    }

    // seed depth stack
    level = 0;
    depth[level].range_lo = 0;
    depth[level].range_hi = element_count - 1;
    depth[level].leg = 0;
    left_backing_size = 0;
    right_backing_size = 0;
    center_backing_size = 0;

    // main sort loop
    while (level > -1) {
        if (traverse == -1) {
            if (depth[level + 1].leg == 0) {
                // cache center backing into left
                memcpy(left_backing, center_backing, center_backing_size * element_size);
                left_backing_size = center_backing_size;
                center_backing_size = 0;
                // prepare right leg
                size_t range_size = depth[level].range_hi - depth[level].range_lo + 1;
                depth[level + 1].range_lo = depth[level].range_lo + (range_size / 2);
                depth[level + 1].range_hi = depth[level].range_hi;
                depth[level + 1].leg = 1;
                traverse = 0;
                level++;
                continue;
            } else if (depth[level + 1].leg == 1) {
                // right leg finished, merge and then traverse up: merge left and right backing into center backing
                size_t left_ptr = 0;
                size_t right_ptr = 0;
                printf("msort: level %d left_backing_size (%ld) right_backing_size (%ld)\n", level, left_backing_size, right_backing_size);
                while ((left_ptr < left_backing_size) && (right_ptr < right_backing_size)) {
                    printf("msort: level %d comparing left %ld (%ld) with right %ld (%ld)\n", level, left_ptr, left_backing_size, right_ptr, right_backing_size);
                    int lr_compare = comp_func(left_backing, left_ptr, right_backing, right_ptr);
                    if (lr_compare == 0) {
                        // left > right
                        copy_func(right_backing, right_ptr, center_backing, center_backing_size);
                        center_backing_size++;
                        right_ptr++;
                    } else {
                        // left <= right
                        copy_func(left_backing, left_ptr, center_backing, center_backing_size);
                        center_backing_size++;
                        left_ptr++;
                    }
                }
                while (left_ptr < left_backing_size) {
                    printf("msort cleaning up left: left_ptr %ld left_backing_size %ld center_backing_size %ld\n", left_ptr, left_backing_size, center_backing_size);
                    copy_func(left_backing, left_ptr, center_backing, center_backing_size);
                    center_backing_size++;
                    left_ptr++;
                }
                while (right_ptr < right_backing_size) {
                    printf("msort cleaning up right: right_ptr %ld right_backing_size %ld center_backing_size %ld\n", right_ptr, right_backing_size, center_backing_size);
                    copy_func(right_backing, right_ptr, center_backing, center_backing_size);
                    center_backing_size++;
                    right_ptr++;
                }
                // copy center to left or right backing depending on what leg we are
                if (depth[level].leg == 0) {
                    memcpy(left_backing, center_backing, center_backing_size * element_size);
                    left_backing_size = center_backing_size;
                    center_backing_size = 0;
                    right_backing_size = 0;
                } else {
                    memcpy(right_backing, center_backing, center_backing_size * element_size);
                    right_backing_size = center_backing_size;
                    center_backing_size = 0;
                    left_backing_size = 0;
                }
                traverse = -1;
                level--;
                continue;
            }
        } else {
            // came down from above
            size_t range_size = depth[level].range_hi - depth[level].range_lo + 1;
            if (range_size == 1) {
                printf("msort: level %d leg %d one element %ld\n", level, depth[level].leg, depth[level].range_lo);
                copy_func(array, depth[level].range_lo, center_backing, 0);
                center_backing_size = 1;
                level--;
                traverse = -1;
                continue;
            } else {
                // at least 2 elements, so traverse down to the left leg
                printf("msort: level %d leg %d got range %ld - %ld\n", level, depth[level].leg, depth[level].range_lo, depth[level].range_hi);
                depth[level + 1].range_lo = depth[level].range_lo;
                depth[level + 1].range_hi = depth[level].range_lo + (range_size / 2) - 1;
                depth[level + 1].leg = 0;
                traverse = 0;
                level++;
                continue;
            }
        }
    }
    // sorted array will be in left backing, so copy it back
    memcpy(array, left_backing, element_count * element_size);

    free(left_backing);
    free(center_backing);
    free(right_backing);
}

/* comp, swap, and copy implementations */

int mlist_comp(uint8_t *srcarray, size_t srcelement, uint8_t *destarray, size_t destelement)
{
    // 0 == source > dest, -1 = source <= dest
    printf("mlist_comp: comparing %016lX:%d with %016lX:%d\n", srcarray, srcelement, destarray, destelement);
    if (((int *)srcarray)[srcelement] > ((int *)destarray)[destelement])
        return 0;
    else
        return -1;
}

void mlist_swap(uint8_t *srcarray, size_t srcelement, uint8_t *destarray, size_t destelement)
{
    printf("mlist_swap: swapping %016lX:%d with %016lX:%d\n", srcarray, srcelement, destarray, destelement);
    int hold = ((int *)srcarray)[srcelement];
    ((int *)srcarray)[srcelement] = ((int *)destarray)[destelement];
    ((int *)destarray)[destelement] = hold;
}

void mlist_copy(uint8_t *srcarray, size_t srcelement, uint8_t *destarray, size_t destelement)
{
    printf("mlist_copy: copying %016lX:%d to %016lX:%d\n", srcarray, srcelement, destarray, destelement);
    ((int *)destarray)[destelement] = ((int *)srcarray)[srcelement];
}

int main(int argc, char **argv)
{
    printf("mlist array: ");
    for (int i = 0; i < 21; ++i) printf("%d ", mlist[i]);
    printf("\n");

    msort((uint8_t *)&mlist, sizeof(int), 21, mlist_comp, mlist_swap, mlist_copy);

    printf("sorted     : ");
    for (int i = 0; i < 21; ++i) printf("%d ", mlist[i]);
    printf("\n");

    return 0;
}
