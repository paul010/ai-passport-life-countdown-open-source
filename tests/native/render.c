/* Exact device renderer on native LVGL; these frames are NOT device evidence. */
#include "life_quest_ui.h"
#include "ui_pixel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char framebuffer[240 * 320 * 2] __attribute__((aligned(8)));
static unsigned char draw_buffer[240 * 20 * 2] __attribute__((aligned(8)));
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels; lv_display_flush_ready(display);
}
static void check_bounds(lv_obj_t *parent)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_check_type(child, &lv_label_class) || lv_obj_check_type(child, &lv_image_class)) {
            lv_area_t outer, inner;
            lv_obj_get_content_coords(parent, &outer); lv_obj_get_coords(child, &inner);
            if (inner.x1 < outer.x1 || inner.y1 < outer.y1 || inner.x2 > outer.x2 || inner.y2 > outer.y2) {
                fprintf(stderr, "Clipped label/image: %s (%d,%d)-(%d,%d) in (%d,%d)-(%d,%d)\n",
                    lv_obj_check_type(child, &lv_label_class) ? lv_label_get_text(child) : "avatar/sprite",
                    inner.x1, inner.y1, inner.x2, inner.y2, outer.x1, outer.y1, outer.x2, outer.y2);
                abort();
            }
        }
        check_bounds(child);
    }
}
static void frame(life_quest_ui_t *ui, const char *folder, const char *name)
{
    lv_obj_update_layout(ui->screen);
    check_bounds(ui->screen);
    lv_mem_monitor_t before; lv_mem_monitor(&before);
    printf("%s: before snapshot LVGL free %lu bytes\n", name, (unsigned long)before.free_size);
    lv_draw_buf_t buf;
    assert(lv_draw_buf_init(&buf, 240, 320, LV_COLOR_FORMAT_RGB565, 480,
                            framebuffer, sizeof(framebuffer)) == LV_RESULT_OK);
    assert(lv_snapshot_take_to_draw_buf(ui->screen, LV_COLOR_FORMAT_RGB565, &buf) == LV_RESULT_OK);
    char path[512]; snprintf(path, sizeof(path), "%s/%s.ppm", folder, name);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < 240U * 320U; ++i) {
        unsigned v = framebuffer[2*i] | framebuffer[2*i+1] << 8;
        unsigned char rgb[3] = {(v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63,
                                (v & 31) * 255 / 31};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
    lv_mem_monitor_t mem; lv_mem_monitor(&mem);
    printf("%s: LVGL free %lu bytes\n", name, (unsigned long)mem.free_size);
    assert(mem.free_size > 2000);
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    setbuf(stdout, NULL);
    lv_init();
    lv_display_t *d = lv_display_create(240, 320);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(d, flush);
    lv_display_set_buffers(d, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_obj_t *new_row = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(new_row, 10, 77); lv_obj_set_size(new_row, 220, 33);
    ui_pixel_mascot_jump(new_row);
    assert(lv_obj_get_y(new_row) == 77); /* no first-frame jump into the title */
    lv_obj_delete(new_row);
    life_quest_state_t s; life_quest_default(&s);
    life_quest_ui_t ui;
    life_date_t date = {2026,10,1};
    for (int i = 0; i < 9; ++i) {
        life_quest_ui_home(&ui, &s, date, -1, i);
        lv_label_set_text(ui.clock, "2026.10.01 09:00");
        char name[40]; snprintf(name, sizeof(name), "home-focus-%d", i);
        frame(&ui, argv[1], name);
        assert(ui.selected);
    }
    uint8_t reward;
    for (int i = 0; i < 3; ++i) assert(life_quest_complete(&s, life_quest_day(date), i, &reward) == LQ_OK);
    life_quest_ui_home(&ui, &s, date, -1, 0);
    lv_label_set_text(ui.clock, "2026.10.01 09:00");
    frame(&ui, argv[1], "home-cleared");
    s.name = 1; s.planning_age = 120; s.birth = (life_date_t){2000,2,29};
    life_quest_ui_home(&ui, &s, date, -1, 0);
    lv_label_set_text(ui.clock, "2026.10.01 09:00");
    frame(&ui, argv[1], "home-sample2");
    puts("Shared device UI rendering: PASS");
    lv_deinit(); return 0;
}
