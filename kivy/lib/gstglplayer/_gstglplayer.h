#include <glib.h>
#define GST_USE_UNSTABLE_API
#include <gst/gst.h>
#include <gst/video/video-info.h>
#include <gst/gl/gl.h>
#include <gst/gl/x11/gstgldisplay_x11.h>

#include <GL/glx.h>

#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL.h>

SDL_Window *sdl_window = NULL;
SDL_GLContext *sdl_gl_context;

GstGLDisplay *gst_gl_display;
GstGLContext *gst_gl_context;


static void c_glib_iteration(int count)
{
	while (count > 0 && g_main_context_pending(NULL))
	{
		count --;
		g_main_context_iteration(NULL, FALSE);
	}
}

static void g_object_set_void(GstElement *element, char *name, void *value)
{
	g_object_set(G_OBJECT(element), name, value, NULL);
}

static void g_object_set_double(GstElement *element, char *name, double value)
{
	g_object_set(G_OBJECT(element), name, value, NULL);
}

static void g_object_set_caps(GstElement *element, char *value)
{
	GstCaps *caps = gst_caps_from_string(value);
	g_object_set(G_OBJECT(element), "caps", caps, NULL);
}

static void g_object_set_int(GstElement *element, char *name, int value)
{
    g_object_set(G_OBJECT(element), name, value, NULL);
}

typedef void(*appcallback_t)(void *, int, int, char *, int);
typedef void(*buscallback_t)(void *, GstMessage *);
typedef struct {
	appcallback_t callback;
	buscallback_t bcallback;
	char eventname[15];
	PyObject *userdata;
} callback_data_t;

static void c_signal_free_data(gpointer data, GClosure *closure)
{
    callback_data_t *cdata = data;
    Py_DECREF(cdata->userdata);
    free(cdata);
}

static void c_signal_disconnect(GstElement *element, gulong handler_id)
{
    g_signal_handler_disconnect(element, handler_id);
}

static gboolean c_on_bus_message(GstBus *bus, GstMessage *message, callback_data_t *data)
{
    data->bcallback(data->userdata, message);
    return TRUE;
}

static gulong c_bus_connect_message(GstBus *bus, buscallback_t callback, PyObject *userdata)
{
    callback_data_t *data = (callback_data_t *)malloc(sizeof(callback_data_t));
    if(data == NULL)
        return 0;
    data->callback = NULL;
    data->bcallback = callback;
    data->userdata = userdata;

    Py_INCREF(data->userdata);

    return g_signal_connect_data(
        (GstElement *)bus, "sync-message",
        G_CALLBACK(c_on_bus_message), data,
        c_signal_free_data, 0
    );
}

void
gst_gl_init(SDL_Window *_sdl_window) {
    GLXContext glx_context = NULL;

    if(sdl_window == NULL) {
        sdl_window = _sdl_window;
        sdl_gl_context = SDL_GL_GetCurrentContext();

        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        SDL_GetWindowWMInfo(sdl_window, &info);

        gst_gl_display = (GstGLDisplay *)gst_gl_display_x11_new_with_display(
            info.info.x11.display
        );
    }
    glx_context = glXGetCurrentContext();
    SDL_GL_MakeCurrent(sdl_window, NULL);
    gst_gl_context = gst_gl_context_new_wrapped(
        gst_gl_display,
        (guintptr)glx_context,
        gst_gl_platform_from_string("glx"),
        GST_GL_API_OPENGL
    );
    SDL_GL_MakeCurrent(sdl_window, sdl_gl_context);
}

static
gboolean gst_gl_bus_cb(GstBus *bus, GstMessage *msg, gpointer *data) {
    switch(GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_NEED_CONTEXT:
        {
            const gchar *context_type;

            gst_message_parse_context_type(msg, &context_type);

            if(g_strcmp0(context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
                GstContext *display_context = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
                gst_context_set_gl_display(display_context, gst_gl_display);
                gst_element_set_context(GST_ELEMENT(msg->src), display_context);
                return TRUE;
            } else if(g_strcmp0(context_type, "gst.gl.app_context") == 0) {
                GstContext *app_context = gst_context_new("gst.gl.app_context", TRUE);
                GstStructure *s = gst_context_writable_structure(app_context);
                gst_structure_set(s, "context", GST_GL_TYPE_CONTEXT, gst_gl_context, NULL);
                gst_element_set_context(GST_ELEMENT(msg->src), app_context);
                return TRUE;
            }
            break;
        }

        default:
            break;
    }
    return FALSE;
}

void
gst_gl_set_bus_cb(GstBus *bus) {
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "sync-message", G_CALLBACK(gst_gl_bus_cb), NULL);
}

void
gst_gl_stop_pipeline(GstPipeline *pipeline) {
    SDL_GL_MakeCurrent(sdl_window, sdl_gl_context);
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    SDL_GL_MakeCurrent(sdl_window, NULL);
}

unsigned int
get_texture_id_from_buffer(GstBuffer *buf, GstVideoInfo *v_info)
{
    GstVideoFrame v_frame;
    guint texture = 0;

    if(!gst_video_frame_map(
        &v_frame,
        v_info,
        buf,
        (GstMapFlags)(GST_MAP_READ | GST_MAP_GL)
    )) {
        g_warning("Failed to map the video buffer");
        return texture;
    }

    texture = *(guint *)v_frame.data[0];

    gst_video_frame_unmap(&v_frame);

    return texture;
}
