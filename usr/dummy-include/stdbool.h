/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STDBOOL_H
#define _STDBOOL_H

#include <linux/kernel.h> // For pr_err, pr_info, etc. if needed

// Define boolean values
typedef enum {
    false = 0,
    true = 1
} bool;

// Macros for boolean operations
#define BOOL_TO_STRING(b) ((b) ? "true" : "false")

// Function to toggle a boolean value
static inline bool bool_toggle(bool b) {
    return !b;
}

// Function to check if a boolean value is true
static inline bool bool_is_true(bool b) {
    return b == true;
}

// Function to check if a boolean value is false
static inline bool bool_is_false(bool b) {
    return b == false;
}

// Function to print a boolean value
static inline void print_bool(const char *label, bool b) {
    pr_info("%s: %s\n", label, BOOL_TO_STRING(b));
}

#endif /* _STDBOOL_H */
