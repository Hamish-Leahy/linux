/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <linux/kernel.h> // For pr_err, pr_info, etc.
#include <linux/slab.h>  // For kmalloc and kfree
#include <linux/types.h> // For size_t

// Error handling macro
#define HANDLE_ERROR(msg) \
    do { pr_err("%s\n", msg); return NULL; } while (0)

// Function to allocate memory
static inline void *my_malloc(size_t size) {
    void *ptr = kmalloc(size, GFP_KERNEL);
    if (!ptr) {
        HANDLE_ERROR("Memory allocation failed");
    }
    return ptr;
}

// Function to free allocated memory
static inline void my_free(void *ptr) {
    if (ptr) {
        kfree(ptr);
    } else {
        pr_warn("Attempted to free a NULL pointer\n");
    }
}

// Function to convert string to integer
static inline int my_atoi(const char *str) {
    int result = 0;
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

// Function to generate a random number
static inline int my_rand(void) {
    return (int)(get_random_u32() % RAND_MAX);
}

// Function to print an integer
static inline void print_int(const char *label, int value) {
    pr_info("%s: %d\n", label, value);
}

#endif /* _STDLIB_H */
