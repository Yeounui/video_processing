#ifndef VIDEO_PROCESSING_EVENT_HANDLER_H
#define VIDEO_PROCESSING_EVENT_HANDLER_H

#include "types.h"
#include "image_processor.h"
#include "renderer.h"

typedef struct {
    AppState       *state;
    ImageProcessor *processor;
    Renderer       *renderer;
} EventHandler;

void events_init(EventHandler *e, AppState *state, ImageProcessor *p, Renderer *r);
void events_process(EventHandler *e);
void events_destroy(EventHandler *e);
void events_open_path(EventHandler *e, const char *path);

#endif /* VIDEO_PROCESSING_EVENT_HANDLER_H */
