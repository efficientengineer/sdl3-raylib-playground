#ifndef TEXT_H
#define TEXT_H

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

void text_init(const unsigned char *ttf_data, float pixel_height);
void text_draw(const char *str, float x, float y, float sx, float sy,
               float r, float g, float b);
void text_cleanup(void);

#endif
