#pragma once
#include <stdbool.h>

/* An unsuccessful conversion has no voltage. Retain the last valid sample,
 * never turn a timeout's zero-initialized output into a real UP press. */
static inline int bsp_button_sample_accept(int previous_mv, bool valid, int mv)
{
    return valid && mv >= 0 ? mv : previous_mv;
}

static inline bool bsp_button_sample_in_window(int mv, unsigned minimum, unsigned maximum)
{
    return mv >= 0 && (unsigned)mv >= minimum && (unsigned)mv < maximum;
}

/* One noisy outlier must not become a key. Invalid batches have no voltage. */
static inline int bsp_button_sample_median5(const int samples[5])
{
    int sorted[5];
    for (int i = 0; i < 5; ++i) {
        if (samples[i] < 0) return -1;
        sorted[i] = samples[i];
        for (int j = i; j > 0 && sorted[j] < sorted[j - 1]; --j) {
            int tmp = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = tmp;
        }
    }
    return sorted[2];
}
