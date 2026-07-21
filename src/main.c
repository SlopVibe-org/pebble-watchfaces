/*
 * SlopVibe Watchface — Pebble Time 2 (Emery)
 * Layout (200×228 px):
 *   [Battery bar - 2px]
 *   [Time - 50% screen - large bold]
 *   [Date row - 20% - weather icon | date | temp]
 *   [Calendar event - 20%]
 *   [Step bar - 2px]
 */

#include <pebble.h>

// ============ Message Keys ============
#define KEY_WEATHER_TEMP  0
#define KEY_WEATHER_CODE  1
#define KEY_CAL_COUNT     2
#define KEY_CAL_1_TITLE   3
#define KEY_CAL_1_TIME    4
#define KEY_CAL_2_TITLE   5
#define KEY_CAL_2_TIME    6
#define KEY_STEP_GOAL     7

// ============ Persist Keys ============
#define PERSIST_WEATHER_TEMP  1
#define PERSIST_WEATHER_CODE  2
#define PERSIST_CAL_1_TITLE   3
#define PERSIST_CAL_1_TIME    4
#define PERSIST_CAL_2_TITLE   5
#define PERSIST_CAL_2_TIME    6

// ============ Constants ============
#define DEFAULT_STEP_GOAL 10000

// Layout Y positions (screen 200×228)
#define BATTERY_BAR_Y   0
#define BATTERY_BAR_H   2
#define TIME_Y          6
#define TIME_H          104
#define DATE_ROW_Y      114
#define DATE_ROW_H      44
#define CAL_AREA_Y      160
#define CAL_AREA_H      44
#define STEP_BAR_Y      226
#define STEP_BAR_H      2

#define WEATHER_ICON_W  36
#define WEATHER_ICON_H  36
#define WEATHER_ICON_X  6
#define WEATHER_ICON_Y  (DATE_ROW_Y + 4)

// ============ Layers ============
static Window *s_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_temp_layer;
static TextLayer *s_cal_title_layer;
static TextLayer *s_cal_time_layer;
static Layer *s_battery_bar_layer;
static Layer *s_step_bar_layer;
static Layer *s_weather_icon_layer;

// ============ State ============
static int s_battery_pct = 100;
static int s_step_goal = DEFAULT_STEP_GOAL;

static int s_weather_temp = -999;
static int s_weather_code = -1;

static char s_cal_1_title[64] = "";
static uint32_t s_cal_1_time = 0;
static char s_cal_2_title[64] = "";
static uint32_t s_cal_2_time = 0;
static bool s_has_cal_1 = false;
static bool s_has_cal_2 = false;

// ============ Forward declarations ============
static void update_calendar_display(void);
static void update_weather_display(void);

// =============================================
// Battery bar — custom Layer draw proc
// =============================================
static void battery_bar_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    int bar_width = (bounds.size.w * s_battery_pct) / 100;

    // Background (dim)
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // Foreground — color depends on level
    if (s_battery_pct > 50) {
        graphics_context_set_fill_color(ctx, GColorGreen);
    } else if (s_battery_pct > 20) {
        graphics_context_set_fill_color(ctx, GColorYellow);
    } else {
        graphics_context_set_fill_color(ctx, GColorRed);
    }

    if (bar_width > 0) {
        graphics_fill_rect(ctx, GRect(0, 0, bar_width, bounds.size.h), 0, GCornerNone);
    }
}

// =============================================
// Step bar — custom Layer draw proc
// =============================================
static void step_bar_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    // Fetch today's step count
    HealthValue steps = health_service_sum_today(HealthMetricStepCount);
    if (steps < 0) steps = 0;

    int pct = (int)((steps * 100) / s_step_goal);
    if (pct > 100) pct = 100;
    int bar_width = (bounds.size.w * pct) / 100;

    // Background (dim)
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // Foreground
    if (pct >= 100) {
        graphics_context_set_fill_color(ctx, GColorGreen);
    } else {
        graphics_context_set_fill_color(ctx, GColorPictonBlue);
    }

    if (bar_width > 0) {
        graphics_fill_rect(ctx, GRect(0, 0, bar_width, bounds.size.h), 0, GCornerNone);
    }
}

// =============================================
// Weather icon — custom Layer draw proc
// =============================================
static void weather_icon_update_proc(Layer *layer, GContext *ctx) {
    if (s_weather_code < 0) return;

    GRect bounds = layer_get_bounds(layer);
    int cx = bounds.size.w / 2;
    int cy = bounds.size.h / 2;
    int code = s_weather_code;

    graphics_context_set_antialiased(ctx, true);

    // --- Sun (clear sky: code 0) ---
    if (code == 0) {
        graphics_context_set_fill_color(ctx, GColorYellow);
        graphics_fill_circle(ctx, GPoint(cx, cy), 8);
        // 8 rays
        graphics_context_set_stroke_color(ctx, GColorYellow);
        graphics_context_set_stroke_width(ctx, 2);
        static const int rays[8][4] = {
            { 0, -11,  0, -15},   // N
            { 8,  -8, 11, -11},   // NE
            {11,   0, 15,   0},   // E
            { 8,   8, 11,  11},   // SE
            { 0,  11,  0,  15},   // S
            {-8,   8,-11,  11},   // SW
            {-11,  0,-15,   0},   // W
            {-8,  -8,-11, -11},   // NW
        };
        for (int i = 0; i < 8; i++) {
            graphics_draw_line(ctx,
                GPoint(cx + rays[i][0], cy + rays[i][1]),
                GPoint(cx + rays[i][2], cy + rays[i][3]));
        }
        return;
    }

    // --- Partly cloudy (codes 1-3) ---
    if (code >= 1 && code <= 3) {
        // Small sun top-left
        graphics_context_set_fill_color(ctx, GColorYellow);
        graphics_fill_circle(ctx, GPoint(cx - 5, cy - 5), 5);
        // Cloud bottom-right
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy + 3), 7);
        graphics_fill_circle(ctx, GPoint(cx + 8, cy + 1), 5);
        graphics_fill_circle(ctx, GPoint(cx - 3, cy + 3), 5);
        graphics_fill_rect(ctx, GRect(cx - 3, cy + 3, 12, 6), 0, GCornerNone);
        return;
    }

    // --- Fog (codes 45-48) ---
    if (code >= 45 && code <= 48) {
        graphics_context_set_stroke_color(ctx, GColorLightGray);
        graphics_context_set_stroke_width(ctx, 2);
        for (int dy = -6; dy <= 6; dy += 4) {
            graphics_draw_line(ctx,
                GPoint(4, cy + dy),
                GPoint(bounds.size.w - 4, cy + dy));
        }
        return;
    }

    // --- Cloud base (used by rain, snow, thunderstorm) ---
    // Rain/Drizzle (codes 51-57, 61-67, 80-82)
    if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) ||
        (code >= 80 && code <= 82)) {
        // Cloud
        graphics_context_set_fill_color(ctx, GColorLightGray);
        graphics_fill_circle(ctx, GPoint(cx - 5, cy - 6), 6);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy - 7), 7);
        graphics_fill_circle(ctx, GPoint(cx + 8, cy - 5), 5);
        graphics_fill_rect(ctx, GRect(cx - 5, cy - 6, 14, 6), 0, GCornerNone);
        // Rain drops
        graphics_context_set_fill_color(ctx, GColorPictonBlue);
        graphics_fill_circle(ctx, GPoint(cx - 4, cy + 5), 2);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy + 7), 2);
        graphics_fill_circle(ctx, GPoint(cx + 8, cy + 5), 2);
        return;
    }

    // Snow (codes 71-77, 85-86)
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
        // Cloud
        graphics_context_set_fill_color(ctx, GColorLightGray);
        graphics_fill_circle(ctx, GPoint(cx - 5, cy - 6), 6);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy - 7), 7);
        graphics_fill_circle(ctx, GPoint(cx + 8, cy - 5), 5);
        graphics_fill_rect(ctx, GRect(cx - 5, cy - 6, 14, 6), 0, GCornerNone);
        // Snow flakes (small dots)
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_fill_circle(ctx, GPoint(cx - 4, cy + 6), 1);
        graphics_fill_circle(ctx, GPoint(cx + 1, cy + 8), 1);
        graphics_fill_circle(ctx, GPoint(cx + 6, cy + 6), 1);
        graphics_fill_circle(ctx, GPoint(cx - 1, cy + 5), 1);
        graphics_fill_circle(ctx, GPoint(cx + 4, cy + 9), 1);
        return;
    }

    // Thunderstorm (codes 95-99)
    if (code >= 95) {
        // Cloud (dark)
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_circle(ctx, GPoint(cx - 5, cy - 6), 6);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy - 7), 7);
        graphics_fill_circle(ctx, GPoint(cx + 8, cy - 5), 5);
        graphics_fill_rect(ctx, GRect(cx - 5, cy - 6, 14, 6), 0, GCornerNone);
        // Lightning bolt
        graphics_context_set_stroke_color(ctx, GColorYellow);
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_line(ctx, GPoint(cx, cy),     GPoint(cx - 3, cy + 5));
        graphics_draw_line(ctx, GPoint(cx - 3, cy + 5), GPoint(cx + 1, cy + 5));
        graphics_draw_line(ctx, GPoint(cx + 1, cy + 5), GPoint(cx - 2, cy + 12));
        return;
    }

    // Default: overcast cloud
    graphics_context_set_fill_color(ctx, GColorLightGray);
    graphics_fill_circle(ctx, GPoint(cx - 5, cy - 2), 7);
    graphics_fill_circle(ctx, GPoint(cx + 3, cy - 3), 8);
    graphics_fill_circle(ctx, GPoint(cx + 9, cy - 1), 6);
    graphics_fill_rect(ctx, GRect(cx - 5, cy - 2, 15, 7), 0, GCornerNone);
}

// =============================================
// Update functions
// =============================================
static void update_time_display(struct tm *tick_time) {
    static char time_buffer[8];
    if (!tick_time) {
        time_t temp = time(NULL);
        tick_time = localtime(&temp);
    }
    strftime(time_buffer, sizeof(time_buffer),
        clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
    text_layer_set_text(s_time_layer, time_buffer);
}

static void update_date_display(struct tm *tick_time) {
    static char date_buffer[16];
    if (!tick_time) {
        time_t temp = time(NULL);
        tick_time = localtime(&temp);
    }
    // Format: DD - MM - YY
    strftime(date_buffer, sizeof(date_buffer), "%d - %m - %y", tick_time);
    text_layer_set_text(s_date_layer, date_buffer);
}

static void update_weather_display(void) {
    static char temp_buffer[12];
    if (s_weather_temp > -999) {
        snprintf(temp_buffer, sizeof(temp_buffer), "%d°", s_weather_temp);
    } else {
        snprintf(temp_buffer, sizeof(temp_buffer), "--°");
    }
    text_layer_set_text(s_temp_layer, temp_buffer);
    layer_mark_dirty(s_weather_icon_layer);
}

static void update_calendar_display(void) {
    // Check if event 1 has passed
    if (s_has_cal_1 && s_cal_1_time > 0) {
        time_t now = time(NULL);
        if ((time_t)s_cal_1_time <= now) {
            // Event 1 has passed — shift event 2 to 1
            if (s_has_cal_2) {
                strncpy(s_cal_1_title, s_cal_2_title, sizeof(s_cal_1_title) - 1);
                s_cal_1_title[sizeof(s_cal_1_title) - 1] = '\0';
                s_cal_1_time = s_cal_2_time;
                persist_write_string(PERSIST_CAL_1_TITLE, s_cal_1_title);
                persist_write_int(PERSIST_CAL_1_TIME, s_cal_1_time);
            } else {
                s_has_cal_1 = false;
                s_cal_1_title[0] = '\0';
                persist_write_string(PERSIST_CAL_1_TITLE, "");
            }
            s_has_cal_2 = false;
            s_cal_2_title[0] = '\0';
            persist_write_string(PERSIST_CAL_2_TITLE, "");
        }
    }

    // Update display
    if (s_has_cal_1 && strlen(s_cal_1_title) > 0) {
        text_layer_set_text(s_cal_title_layer, s_cal_1_title);
        // Format time
        static char cal_time_buf[32];
        if (s_cal_1_time > 0) {
            time_t t = (time_t)s_cal_1_time;
            struct tm *tm = localtime(&t);
            strftime(cal_time_buf, sizeof(cal_time_buf), "%H:%M", tm);
        } else {
            snprintf(cal_time_buf, sizeof(cal_time_buf), "—");
        }
        text_layer_set_text(s_cal_time_layer, cal_time_buf);
    } else {
        text_layer_set_text(s_cal_title_layer, "No upcoming events");
        text_layer_set_text(s_cal_time_layer, "");
    }
}

static void update_battery_display(BatteryChargeState charge) {
    s_battery_pct = charge.charge_percent;
    layer_mark_dirty(s_battery_bar_layer);
}

// =============================================
// Handlers
// =============================================
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time_display(tick_time);
    update_date_display(tick_time);
    update_calendar_display();

    // Refresh step bar every minute
    layer_mark_dirty(s_step_bar_layer);

    // Request weather + calendar from JS every 60 minutes (at minute 0)
    if (tick_time->tm_min % 60 == 0) {
        // The JS side listens for AppMessage and fetches on its own timer,
        // but we can also trigger via sendMessage if needed.
        // For now, JS handles its own 60-min refresh cycle.
    }
}

static void battery_state_handler(BatteryChargeState charge) {
    update_battery_display(charge);
}

// =============================================
// AppMessage
// =============================================
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    Tuple *t;

    // Weather
    if ((t = dict_find(iter, KEY_WEATHER_TEMP)) != NULL) {
        s_weather_temp = (int)t->value->int32;
        persist_write_int(PERSIST_WEATHER_TEMP, s_weather_temp);
    }
    if ((t = dict_find(iter, KEY_WEATHER_CODE)) != NULL) {
        s_weather_code = (int)t->value->int32;
        persist_write_int(PERSIST_WEATHER_CODE, s_weather_code);
    }

    // Calendar count
    if ((t = dict_find(iter, KEY_CAL_COUNT)) != NULL) {
        int count = (int)t->value->int32;
        if (count == 0) {
            s_has_cal_1 = false;
            s_has_cal_2 = false;
            s_cal_1_title[0] = '\0';
            s_cal_2_title[0] = '\0';
            persist_write_string(PERSIST_CAL_1_TITLE, "");
            persist_write_string(PERSIST_CAL_2_TITLE, "");
        }
    }

    // Calendar event 1
    if ((t = dict_find(iter, KEY_CAL_1_TITLE)) != NULL) {
        strncpy(s_cal_1_title, t->value->cstring, sizeof(s_cal_1_title) - 1);
        s_cal_1_title[sizeof(s_cal_1_title) - 1] = '\0';
        persist_write_string(PERSIST_CAL_1_TITLE, s_cal_1_title);
        s_has_cal_1 = true;
    }
    if ((t = dict_find(iter, KEY_CAL_1_TIME)) != NULL) {
        s_cal_1_time = t->value->uint32;
        persist_write_int(PERSIST_CAL_1_TIME, (int32_t)s_cal_1_time);
    }

    // Calendar event 2
    if ((t = dict_find(iter, KEY_CAL_2_TITLE)) != NULL) {
        strncpy(s_cal_2_title, t->value->cstring, sizeof(s_cal_2_title) - 1);
        s_cal_2_title[sizeof(s_cal_2_title) - 1] = '\0';
        persist_write_string(PERSIST_CAL_2_TITLE, s_cal_2_title);
        s_has_cal_2 = true;
    }
    if ((t = dict_find(iter, KEY_CAL_2_TIME)) != NULL) {
        s_cal_2_time = t->value->uint32;
        persist_write_int(PERSIST_CAL_2_TIME, (int32_t)s_cal_2_time);
    }

    // Step goal override
    if ((t = dict_find(iter, KEY_STEP_GOAL)) != NULL) {
        s_step_goal = (int)t->value->int32;
        layer_mark_dirty(s_step_bar_layer);
    }

    // Update displays
    update_weather_display();
    update_calendar_display();
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    // Silently ignore — we'll retry next cycle
}

static void outbox_sent_handler(DictionaryIterator *iter, void *context) {
    // OK
}

static void outbox_failed_handler(DictionaryIterator *iter,
                                  AppMessageResult reason, void *context) {
    // Silently ignore
}

// =============================================
// Load persisted data
// =============================================
static void load_persisted_data(void) {
    if (persist_exists(PERSIST_WEATHER_TEMP)) {
        s_weather_temp = persist_read_int(PERSIST_WEATHER_TEMP);
    }
    if (persist_exists(PERSIST_WEATHER_CODE)) {
        s_weather_code = persist_read_int(PERSIST_WEATHER_CODE);
    }
    if (persist_exists(PERSIST_CAL_1_TITLE)) {
        persist_read_string(PERSIST_CAL_1_TITLE, s_cal_1_title, sizeof(s_cal_1_title));
        if (strlen(s_cal_1_title) > 0) s_has_cal_1 = true;
    }
    if (persist_exists(PERSIST_CAL_1_TIME)) {
        s_cal_1_time = (uint32_t)persist_read_int(PERSIST_CAL_1_TIME);
    }
    if (persist_exists(PERSIST_CAL_2_TITLE)) {
        persist_read_string(PERSIST_CAL_2_TITLE, s_cal_2_title, sizeof(s_cal_2_title));
        if (strlen(s_cal_2_title) > 0) s_has_cal_2 = true;
    }
    if (persist_exists(PERSIST_CAL_2_TIME)) {
        s_cal_2_time = (uint32_t)persist_read_int(PERSIST_CAL_2_TIME);
    }
}

// =============================================
// Window setup
// =============================================
static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    window_set_background_color(window, GColorBlack);

    // --- Battery bar (top, 2px) ---
    s_battery_bar_layer = layer_create(
        GRect(0, BATTERY_BAR_Y, bounds.size.w, BATTERY_BAR_H));
    layer_set_update_proc(s_battery_bar_layer, battery_bar_update_proc);
    layer_add_child(window_layer, s_battery_bar_layer);

    // --- Time (large bold, centered) ---
    s_time_layer = text_layer_create(
        GRect(0, TIME_Y, bounds.size.w, TIME_H));
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_font(s_time_layer,
        fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

    // --- Weather icon (left side of date row) ---
    s_weather_icon_layer = layer_create(
        GRect(WEATHER_ICON_X, WEATHER_ICON_Y, WEATHER_ICON_W, WEATHER_ICON_H));
    layer_set_update_proc(s_weather_icon_layer, weather_icon_update_proc);
    layer_add_child(window_layer, s_weather_icon_layer);

    // --- Date (center of date row) ---
    s_date_layer = text_layer_create(
        GRect(WEATHER_ICON_X + WEATHER_ICON_W, DATE_ROW_Y + 8,
              bounds.size.w - (WEATHER_ICON_X + WEATHER_ICON_W) * 2, 28));
    text_layer_set_background_color(s_date_layer, GColorClear);
    text_layer_set_text_color(s_date_layer, GColorWhite);
    text_layer_set_font(s_date_layer,
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

    // --- Temperature (right side of date row) ---
    s_temp_layer = text_layer_create(
        GRect(bounds.size.w - 50, DATE_ROW_Y + 8, 44, 28));
    text_layer_set_background_color(s_temp_layer, GColorClear);
    text_layer_set_text_color(s_temp_layer, GColorWhite);
    text_layer_set_font(s_temp_layer,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_temp_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(s_temp_layer));

    // --- Calendar event title ---
    s_cal_title_layer = text_layer_create(
        GRect(8, CAL_AREA_Y, bounds.size.w - 16, 24));
    text_layer_set_background_color(s_cal_title_layer, GColorClear);
    text_layer_set_text_color(s_cal_title_layer, GColorWhite);
    text_layer_set_font(s_cal_title_layer,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_cal_title_layer, GTextAlignmentLeft);
    text_layer_set_overflow_mode(s_cal_title_layer, GTextOverflowModeTrailingEllipsis);
    layer_add_child(window_layer, text_layer_get_layer(s_cal_title_layer));

    // --- Calendar event time ---
    s_cal_time_layer = text_layer_create(
        GRect(8, CAL_AREA_Y + 22, bounds.size.w - 16, 18));
    text_layer_set_background_color(s_cal_time_layer, GColorClear);
    text_layer_set_text_color(s_cal_time_layer, GColorLightGray);
    text_layer_set_font(s_cal_time_layer,
        fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_cal_time_layer, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(s_cal_time_layer));

    // --- Step bar (bottom, 2px) ---
    s_step_bar_layer = layer_create(
        GRect(0, STEP_BAR_Y, bounds.size.w, STEP_BAR_H));
    layer_set_update_proc(s_step_bar_layer, step_bar_update_proc);
    layer_add_child(window_layer, s_step_bar_layer);

    // Initial displays
    update_time_display(NULL);
    update_date_display(NULL);
    update_weather_display();
    update_calendar_display();

    BatteryChargeState charge = battery_state_service_peek();
    update_battery_display(charge);
}

static void main_window_unload(Window *window) {
    text_layer_destroy(s_time_layer);
    text_layer_destroy(s_date_layer);
    text_layer_destroy(s_temp_layer);
    text_layer_destroy(s_cal_title_layer);
    text_layer_destroy(s_cal_time_layer);
    layer_destroy(s_battery_bar_layer);
    layer_destroy(s_step_bar_layer);
    layer_destroy(s_weather_icon_layer);
}

// =============================================
// Init / Deinit
// =============================================
static void init(void) {
    load_persisted_data();

    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    window_stack_push(s_window, true);

    // Services
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    battery_state_service_subscribe(battery_state_handler);

    // AppMessage
    app_message_open(256, 64);
    app_message_register_inbox_received(inbox_received_handler, NULL);
    app_message_register_inbox_dropped(inbox_dropped_handler, NULL);
    app_message_register_outbox_sent(outbox_sent_handler, NULL);
    app_message_register_outbox_failed(outbox_failed_handler, NULL);
}

static void deinit(void) {
    window_destroy(s_window);
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
