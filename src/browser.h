#ifndef LAMBDA_BROWSER_H
#define LAMBDA_BROWSER_H

#include <stdint.h>

void browser_init(void);
void browser_draw_content(int cx, int cy, int cw, int ch);
void browser_click(int x, int y, int cx, int cy, int cw, int ch);
void browser_key(char c);
void browser_output_write(const char *data, uint32_t len, uint32_t request_id);
int browser_load_url(const char *url, uint32_t request_id);

#endif
