#ifndef PERF_H
#define PERF_H

void perf_init(void);
void perf_frame_begin(void);
void perf_add_draw_call(int triangles, int vertices);
void perf_frame_end(void);
int  perf_visible(void);
void perf_toggle(void);
void perf_draw(int screen_w, int screen_h);

#endif
