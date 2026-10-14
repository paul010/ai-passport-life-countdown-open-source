#include "bsp_button_sample.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    int mv = -1;
    mv = bsp_button_sample_accept(mv, false, 0);
    assert(!bsp_button_sample_in_window(mv, 0, 150));
    mv = bsp_button_sample_accept(mv, true, 2858);
    for (int i = 0; i < 100; ++i) {
        mv = bsp_button_sample_accept(mv, false, 0);
        assert(mv == 2858 && !bsp_button_sample_in_window(mv, 0, 150));
    }
    mv = bsp_button_sample_accept(mv, true, 0);
    assert(bsp_button_sample_in_window(mv, 0, 150));
    mv = bsp_button_sample_accept(mv, false, 0);
    assert(bsp_button_sample_in_window(mv, 0, 150));
    mv = bsp_button_sample_accept(mv, true, 150);
    assert(!bsp_button_sample_in_window(mv, 0, 150));
    assert(bsp_button_sample_in_window(mv, 150, 447));
    assert(bsp_button_sample_in_window(447, 447, 1900));
    assert(!bsp_button_sample_in_window(1900, 447, 1900));
    assert(bsp_button_sample_median5((int[]){2858, 0, 2858, 2855, 2859}) == 2858);
    assert(bsp_button_sample_median5((int[]){599, 2858, 600, 0, 600}) == 600);
    assert(bsp_button_sample_median5((int[]){2858, 2858, -1, 2858, 2858}) == -1);
    puts("Checked ADC button samples: PASS");
}
