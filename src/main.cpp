#include <gst/gst.h>
#include <glib.h>
#include <cstdio>
#include <unistd.h>
#include <sys/select.h>

typedef struct {
    GObjectClass parent_class;
} CustomDataClass;

struct CustomData {
    GObject parent_instance;
    GQueue *data_queue;
    GMainLoop *main_loop;
};

G_DEFINE_TYPE(CustomData, custom_data, G_TYPE_OBJECT)

static void custom_data_class_init(CustomDataClass *klass) {
    // No special initialization needed
}

static void custom_data_init(CustomData *self) {
    self->data_queue = g_queue_new();
    self->main_loop = nullptr;
}

gpointer handle_input(gpointer data) {
    CustomData *custom_data = static_cast<CustomData *>(data);
    GQueue *data_queue = custom_data->data_queue;
    gboolean *playing = static_cast<gboolean *>(g_queue_pop_head(data_queue));
    GstElement *pipeline = static_cast<GstElement *>(g_queue_pop_head(data_queue));
    int input_fd = *static_cast<int *>(g_queue_pop_head(data_queue));

    char input;
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(input_fd, &read_fds);

        if (select(input_fd + 1, &read_fds, nullptr, nullptr, nullptr) > 0) {
            if (FD_ISSET(input_fd, &read_fds)) {
                ssize_t bytes_read = read(input_fd, &input, 1);
                if (bytes_read > 0) {
                    if (input == 'p') {
                        if (*playing) {
                            gst_element_set_state(pipeline, GST_STATE_PAUSED);
                            *playing = FALSE;
                            g_print("Paused.\n");
                        } else {
                            gst_element_set_state(pipeline, GST_STATE_PLAYING);
                            *playing = TRUE;
                            g_print("Resumed.\n");
                        }
                    } else if (input == 'q') {
                        g_main_loop_quit(custom_data->main_loop);
                        break;
                    }
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create the pipeline elements
    GstElement *source = gst_element_factory_make("v4l2src", "source");
    GstElement *convert = gst_element_factory_make("videoconvert", "convert");
    GstElement *sink = gst_element_factory_make("autovideosink", "sink");

    // Create the pipeline
    GstElement *pipeline = gst_pipeline_new("camera-pipeline");

    // Get the bus
    GstBus *bus = gst_element_get_bus(pipeline);
    if (!bus) {
        g_printerr("Could not get bus from pipeline.\n");
        return -1;
    }

    if (!pipeline || !source || !convert || !sink) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    // Set the source device (adjust if necessary)
    g_object_set(source, "device", "/dev/video0", NULL);

    // Add elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL);

    // Link the elements
    if (gst_element_link_many(source, convert, sink, NULL) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Start playing the pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Set up an input thread (using a pipe instead of getchar)
    int input_fds[2];
    if (pipe(input_fds) == -1) {
        perror("pipe");
        return 1;
    }

    // Main loop for controls
    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    CustomData *custom_data = static_cast<CustomData *>(g_object_new(custom_data_get_type(), NULL));
    custom_data->main_loop = loop;
    gboolean playing = TRUE;

    g_queue_push_tail(custom_data->data_queue, &playing);
    g_queue_push_tail(custom_data->data_queue, pipeline);
    g_queue_push_tail(custom_data->data_queue, &input_fds[0]);

    // Print controls
    g_print("\nCamera Controls:\n");
    g_print("p - Play/Pause\n");
    g_print("q - Quit\n\n");

    // Set up an input thread
    GThread *input_thread = g_thread_new("input-thread", handle_input, custom_data);

    // Set up bus watch
    gst_bus_add_watch(bus, (GstBusFunc) [](GstBus *bus, GstMessage *msg, const gpointer data) -> gboolean {
        auto *g_main_loop = static_cast<GMainLoop *>(data);

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n",
                           GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n",
                           debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                g_main_loop_quit(g_main_loop);
                break;
            }
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                g_main_loop_quit(g_main_loop);
                break;
            default:
                break;
        }
        return TRUE;
    }, loop);

    // Run the main loop
    g_main_loop_run(loop);

    // Clean up
    g_thread_join(input_thread);
    close(input_fds[0]);
    close(input_fds[1]);

    g_queue_free(custom_data->data_queue);
    g_object_unref(custom_data);

    gst_bus_remove_watch(bus);
    g_main_loop_unref(loop);

    // Free resources
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
