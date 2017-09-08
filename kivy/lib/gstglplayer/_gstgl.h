#include <gst/gst.h>
#include <gst/video/video-info.h>

#include "SDL2/SDL.h"

void gst_gl_init (SDL_Window *window);
void gst_gl_set_bus_cb (GstBus *bus);
void gst_gl_stop_pipeline (GstPipeline *pipeline);

guint get_texture_id_from_buffer (GstBuffer *buf, GstVideoInfo *v_info);
