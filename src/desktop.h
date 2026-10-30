// BrainixKernel — desktop.h
#ifndef LAMBDA_NEXUS_DESKTOP_H
#define LAMBDA_NEXUS_DESKTOP_H

#define DI_BROWSER      0
#define DI_FILES        1
#define DI_TERMINAL     2
#define DI_EDITOR       3
#define DI_PERSONALIZE  4
#define DI_ABOUT        5
#define DI_COUNT        6

int  desktop_hit(int x, int y);

void desktop_select(int idx);
int  desktop_selected(void);
void desktop_clear_selection(void);

const char *desktop_label(int idx);

void desktop_draw(void);

#endif