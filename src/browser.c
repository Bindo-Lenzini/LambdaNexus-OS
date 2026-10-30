#include "browser.h"

#include "fb.h"

#define URL_MAX 128
#define PAGE_MAX 8192
#define TOOLBAR_H 42

extern void request_full_redraw(void);

static char g_url[URL_MAX] = "http://neverssl.com/";
static char g_page[PAGE_MAX];
static uint32_t g_page_len;
static int g_address_active;
static int g_address_replace;
static int g_loading;
static int g_failed;
static uint32_t g_request_id;

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void copy_text(char *dst, const char *src, int cap) {
    int i = 0;
    if (cap <= 0) return;
    while (src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void set_page(const char *text, int failed) {
    g_page_len = 0;
    g_page[0] = 0;
    while (text[g_page_len] && g_page_len < PAGE_MAX - 1) {
        g_page[g_page_len] = text[g_page_len];
        g_page_len++;
    }
    g_page[g_page_len] = 0;
    g_loading = 0;
    g_failed = failed;
}

void browser_init(void) {
    copy_text(g_url, "http://neverssl.com/", URL_MAX);
    g_address_active = 0;
    g_address_replace = 0;
    g_loading = 0;
    g_failed = 0;
    g_request_id = 0;
    set_page("Welcome to LambdaNexus Browser\n\nEnter an HTTP address in the address bar and press Return.\n\nThis early browser renders readable text from basic HTML. HTTPS and modern JavaScript sites are not supported yet.", 0);
}

static void submit_address(void) {
    if (g_url[0] == 0) {
        set_page("Enter a website address to continue.", 1);
        return;
    }
    if (str_len(g_url) >= 8 && g_url[0] == 'h' && g_url[1] == 't' &&
        g_url[2] == 't' && g_url[3] == 'p' && g_url[4] == 's' &&
        g_url[5] == ':' && g_url[6] == '/' && g_url[7] == '/') {
        set_page("HTTPS is not supported yet: TLS is not implemented.", 1);
        return;
    }
    g_request_id++;
    if (g_request_id == 0) g_request_id++;
    if (browser_load_url(g_url, g_request_id)) {
        g_page_len = 0;
        g_page[0] = 0;
        g_loading = 1;
        g_failed = 0;
    } else {
        set_page("Could not start the page request. Check that the browser application is available.", 1);
    }
}

void browser_key(char c) {
    if (!g_address_active) {
        if (c == '\n') {
            g_address_active = 1;
            g_address_replace = 1;
        }
        return;
    }

    int len = str_len(g_url);
    if (c == '\n') {
        g_address_active = 0;
        g_address_replace = 0;
        submit_address();
    } else if (c == '\b') {
        if (g_address_replace) {
            g_url[0] = 0;
            g_address_replace = 0;
        }
        if (len > 0) g_url[len - 1] = 0;
    } else if (c >= 0x20 && c < 0x7f && len < URL_MAX - 1) {
        if (g_address_replace) {
            g_url[0] = 0;
            len = 0;
            g_address_replace = 0;
        }
        g_url[len] = c;
        g_url[len + 1] = 0;
    }
}

void browser_click(int x, int y, int cx, int cy, int cw, int ch) {
    (void)ch;
    if (y >= cy && y < cy + TOOLBAR_H) {
        int go_x = cx + cw - 58;
        if (x >= go_x && x < cx + cw - 8) {
            g_address_active = 0;
            g_address_replace = 0;
            submit_address();
            return;
        }
        if (x >= cx + 82 && x < go_x) {
            g_address_active = 1;
            g_address_replace = 1;
            return;
        }
    }
    g_address_active = 0;
}

void browser_output_write(const char *data, uint32_t len, uint32_t request_id) {
    if (!data || len == 0 || request_id != g_request_id) return;
    g_loading = 0;
    for (uint32_t i = 0; i < len && g_page_len < PAGE_MAX - 1; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '\r') continue;
        if (c == '\n' || c == '\t' || c >= 0x20) {
            g_page[g_page_len++] = (char)c;
        } else {
            g_page[g_page_len++] = '?';
        }
    }
    g_page[g_page_len] = 0;
    request_full_redraw();
}

static void draw_page_text(int x, int y, int w, int h, const char *text,
                           uint32_t color) {
    int cols = (w - 28) / 8;
    int max_lines = (h - 28) / 18;
    if (cols < 1 || max_lines < 1) return;

    int row = 0;
    int col = 0;
    int byte_col = 0;
    char line[640];
    if (cols > 150) cols = 150;

    for (uint32_t i = 0; text[i] && row < max_lines; i++) {
        char c = text[i];
        if (c == '\n' || col >= cols) {
            line[byte_col] = 0;
            fb_draw_string_aa(x + 14, y + 14 + row * 18, line, color, 0);
            row++;
            col = 0;
            byte_col = 0;
            if (c == '\n') continue;
        }

        unsigned char lead = (unsigned char)c;
        int bytes = 1;
        if (lead >= 0xC2 && lead <= 0xDF &&
            ((unsigned char)text[i + 1] & 0xC0) == 0x80) {
            bytes = 2;
        } else if (lead >= 0xE0 && lead <= 0xEF &&
                   ((unsigned char)text[i + 1] & 0xC0) == 0x80 &&
                   ((unsigned char)text[i + 2] & 0xC0) == 0x80) {
            bytes = 3;
        } else if (lead >= 0xF0 && lead <= 0xF4 &&
                   ((unsigned char)text[i + 1] & 0xC0) == 0x80 &&
                   ((unsigned char)text[i + 2] & 0xC0) == 0x80 &&
                   ((unsigned char)text[i + 3] & 0xC0) == 0x80) {
            bytes = 4;
        }
        for (int j = 0; j < bytes; j++) line[byte_col++] = text[i + j];
        i += (uint32_t)(bytes - 1);
        col++;
    }
    if (row < max_lines && byte_col > 0) {
        line[byte_col] = 0;
        fb_draw_string_aa(x + 14, y + 14 + row * 18, line, color, 0);
    }
}

void browser_draw_content(int cx, int cy, int cw, int ch) {
    fb_fill_rect(cx, cy, cw, ch, 0xFFF7F8FA);
    fb_fill_rect(cx, cy, cw, TOOLBAR_H, 0xFFE9ECF1);
    fb_fill_rect(cx, cy + TOOLBAR_H - 1, cw, 1, 0xFFD4D9E1);

    fb_fill_circle_aa(cx + 18, cy + 20, 7, 0xFF838B98);
    fb_fill_circle_aa(cx + 42, cy + 20, 7, 0xFF838B98);
    fb_fill_circle_aa(cx + 66, cy + 20, 7, 0xFF838B98);

    char visible_url[112];
    int visible_chars = (cw - 180) / 8;
    if (visible_chars > (int)sizeof(visible_url) - 1)
        visible_chars = (int)sizeof(visible_url) - 1;
    int url_len = str_len(g_url);
    int url_start = url_len > visible_chars ? url_len - visible_chars : 0;
    int shown = 0;
    while (g_url[url_start] && shown < visible_chars) {
        visible_url[shown++] = g_url[url_start++];
    }
    visible_url[shown] = 0;

    uint32_t address_bg = g_address_active ? 0xFFFFFFFF : 0xFFF8F9FB;
    fb_fill_round_rect_aa(cx + 82, cy + 7, cw - 148, 26, 8, address_bg);
    fb_draw_string_aa(cx + 92, cy + 16, visible_url,
                      g_address_active ? 0xFF111827 : 0xFF394150, 0);
    fb_fill_round_rect_aa(cx + cw - 58, cy + 7, 50, 26, 8,
                          0xFF5B63D3);
    fb_draw_string_aa(cx + cw - 48, cy + 16, "Go", 0xFFFFFFFF, 0);

    int body_y = cy + TOOLBAR_H;
    int body_h = ch - TOOLBAR_H;
    fb_fill_rect(cx, body_y, cw, body_h, 0xFFFFFFFF);
    if (g_loading) {
        fb_draw_string_aa(cx + 28, body_y + 30, "Connecting...", 0xFF526071, 0);
        fb_draw_string_aa(cx + 28, body_y + 58,
                          "This browser currently supports plain HTTP only.",
                          0xFF7A8492, 0);
    } else if (g_page_len == 0) {
        fb_draw_string_aa(cx + 28, body_y + 30, "The page returned no readable text.",
                          0xFF526071, 0);
    } else {
        draw_page_text(cx, body_y, cw, body_h, g_page,
                       g_failed ? 0xFF9B2C2C : 0xFF202938);
    }
}
