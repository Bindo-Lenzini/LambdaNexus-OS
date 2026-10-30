// LambdaNexus OS application icons.
#ifndef LAMBDA_NEXUS_ICONS_H
#define LAMBDA_NEXUS_ICONS_H

#include <stdint.h>

#define ICON_SIZE 48

void icon_draw_ksh    (int x, int y, int selected);
void icon_draw_files  (int x, int y, int selected);
void icon_draw_editor (int x, int y, int selected);
void icon_draw_lambda (int x, int y, int selected);
void icon_draw_browser(int x, int y, int selected);

void icon_draw_computer(int x, int y, int selected);

// Иконка "Персонализация" — шестерёнка.
void icon_draw_settings(int x, int y, int selected);

#endif