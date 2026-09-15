#ifndef UI_H
#define UI_H

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

#define UI_MAX_BUTTONS 16

typedef struct {
    float x, y, w, h;
    const char *label;
    int toggled;
    int id;
} UIButton;

void ui_init(void);
int  ui_add_button(float x, float y, float w, float h, const char *label);
int  ui_touch_up(float px, float py);
void ui_draw(int screen_w, int screen_h);
UIButton *ui_get(int id);
void ui_cleanup(void);

#endif
