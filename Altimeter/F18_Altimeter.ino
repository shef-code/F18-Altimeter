// Designed for Waveshare 1.85" round lcd without touch screen
// https://www.waveshare.com/wiki/ESP32-S3-LCD-1.85
// 
// LVGL version is 8.3.11
// Display_ST77916, esp_lcd_st77916, I2C_Driver files sourced from WaveShare's demo code for the display

#define DCSBIOS_DEFAULT_SERIAL
#define DCSBIOS_DISABLE_SERVO 
#include <DcsBios.h>


#include <Arduino.h>
#include "Display_ST77916.h"
#include <lvgl.h>

// LVGL bitmaps
#include "altimeterBackground.c"
#include "altimeterMarquee.c"
#include "altimeterMarquee2.c"
#include "altimeterNeedle.c"

#define DISP_WIDTH  360
#define DISP_HEIGHT 360

static int marquee_offset = 0;
#define DIGIT_HEIGHT 41  // one digit height in pixels (350 ÷ 10 digits = 35)
#define DIGIT_HEIGHT2 37  // one digit height in pixels (350 ÷ 10 digits = 35)
#define SCROLL_SPEED 1    // pixels per update


static int offset1000s = 0;
static int offset10000s = 0;

static int dir1000s = 1;   // +1 = up, -1 = down

// 10,000s scroll animation
static bool scrolling10000s = false;
static int scrollProgress10000s = 0;
static int scrollDir10000s = 1;  // +1 = scroll up, -1 = scroll down



// LVGL draw buffers
static lv_color_t buf1[DISP_WIDTH * 40];
static lv_color_t buf2[DISP_WIDTH * 40];

// ===== Globals =====
lv_obj_t *img_altimeterBackground;
lv_obj_t *img_altimeterMarquee;
lv_obj_t *img_altimeterMarquee2;
lv_obj_t *img_altimeterNeedle;

// Center and radius
const int16_t center_x = DISP_WIDTH / 2;
const int16_t center_y = DISP_HEIGHT / 2;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    LCD_addWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
    lv_disp_flush_ready(disp);
}



void setup() {
    DcsBios::setup();

    ST77916_Init();
    Backlight_Init();
    Set_Backlight(25);

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_WIDTH * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_WIDTH;
    disp_drv.ver_res = DISP_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // ===== Black background =====
    lv_obj_t *bg_rect = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg_rect, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_style_bg_color(bg_rect, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(bg_rect, 0, 0);
    lv_obj_clear_flag(bg_rect, LV_OBJ_FLAG_SCROLLABLE);


    // ===== Altimeter background =====
    img_altimeterBackground = lv_img_create(lv_scr_act());
    lv_img_set_src(img_altimeterBackground, &altimeterBackground);
    lv_obj_align(img_altimeterBackground, LV_ALIGN_CENTER, 0, 0);



    // ===== Altimeter 1000s Drum =====
    // Create a container that defines the visible window

    lv_obj_t *marquee_window = lv_obj_create(lv_scr_act());
    lv_obj_set_size(marquee_window, 32, 49); // only this area is visible
    lv_obj_set_style_bg_opa(marquee_window, LV_OPA_TRANSP, 0);  // transparent background
    lv_obj_set_style_border_width(marquee_window, 0, 0);        // no border
    lv_obj_clear_flag(marquee_window, LV_OBJ_FLAG_SCROLLABLE);  // disable scrolling
    lv_obj_set_scrollbar_mode(marquee_window, LV_SCROLLBAR_MODE_OFF);

    // Position the window where you want the digit "cutout"
    lv_obj_align(marquee_window, LV_ALIGN_TOP_LEFT, 117, 100);

    // Create the marquee image as a child of the window
    img_altimeterMarquee = lv_img_create(marquee_window);
    lv_img_set_src(img_altimeterMarquee, &altimeterMarquee);
    lv_obj_align(img_altimeterMarquee, LV_ALIGN_TOP_MID, 0, 0);




    // ===== Altimeter 10000s Drum =====
    // Create a container that defines the visible window

    lv_obj_t *marquee_window2 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(marquee_window2, 32, 49); // only this area is visible
    lv_obj_set_style_bg_opa(marquee_window2, LV_OPA_TRANSP, 0);  // transparent background
    lv_obj_set_style_border_width(marquee_window2, 0, 0);        // no border
    lv_obj_clear_flag(marquee_window2, LV_OBJ_FLAG_SCROLLABLE);  // disable scrolling
    lv_obj_set_scrollbar_mode(marquee_window2, LV_SCROLLBAR_MODE_OFF);

    // Position the window where you want the digit "cutout"
    lv_obj_align(marquee_window2, LV_ALIGN_TOP_LEFT, 79, 98);

    // Create the marquee image as a child of the window
    img_altimeterMarquee2 = lv_img_create(marquee_window2);
    lv_img_set_src(img_altimeterMarquee2, &altimeterMarquee2);
    lv_obj_align(img_altimeterMarquee2, LV_ALIGN_TOP_MID, 0, 0);


    // ===== Altimeter Needle =====
    img_altimeterNeedle = lv_img_create(lv_scr_act());
    lv_img_set_src(img_altimeterNeedle, &altimeterNeedle);

    // Pivot at hub center of the needle image
    lv_point_t pivot = { altimeterNeedle.header.w / 2, altimeterNeedle.header.h / 2 };
    lv_img_set_pivot(img_altimeterNeedle, pivot.x, pivot.y);

    // Position pivot exactly at gauge center
    lv_obj_set_pos(img_altimeterNeedle,
                  center_x - pivot.x,
                  center_y - pivot.y);

}

void loop() {
    DcsBios::loop();
    lv_timer_handler();

    static int dir = 1;          // climb (+1) or descend (-1)
    static int altitude = 0;     // current altitude in feet

    // === Update altitude ===
    altitude += dir * 50;        // climb/descent rate (50 ft per tick)
    if (altitude >= 49000) {     // top limit
        altitude = 49000;
        dir = -1;
    }
    if (altitude <= 0) {         // bottom limit
        altitude = 0;
        dir = +1;
    }

    // === Needle ===
    // Full 360° = 0–1000 ft
    float frac1000 = (altitude % 1000) / 1000.0f;
    float angle = frac1000 * 360.0f;
    lv_img_set_angle(img_altimeterNeedle, angle * 10); // LVGL expects 0.1° units

    // === 1000s drum ===
    int thousands = (altitude / 1000) % 10;
    float offset1000s = (thousands + frac1000) * DIGIT_HEIGHT;
    lv_img_set_offset_y(img_altimeterMarquee, -(int)offset1000s);

    // === 10,000s drum ===
    // Smooth scroll between digits (0 → 4)
    float frac10000 = (altitude % 10000) / 10000.0f;
    int tenThousands = (altitude / 10000);
    float offset10000s = (tenThousands + frac10000) * DIGIT_HEIGHT2;
    lv_img_set_offset_y(img_altimeterMarquee2, -(int)offset10000s);

    delay(20); // tune for smoothness
}