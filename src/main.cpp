#include <gst/gst.h>

int main(int argc, char *argv[]) {
    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create the pipeline elements
    GstElement *source = gst_element_factory_make("v4l2src", "source");
    GstElement *convert = gst_element_factory_make("videoconvert", "convert");
    // sink = gst_element_factory_make("ximagesink", "sink");
    // sink = gst_element_factory_make("waylandsink", "sink");
    GstElement *sink = gst_element_factory_make("autovideosink", "sink");

    // Create the pipeline
    GstElement *pipeline = gst_pipeline_new("camera-pipeline");

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

    // Wait until error or EOS (End of Stream)
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                 static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    // Parse message
    if (msg != nullptr) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                break;
            default:
                g_printerr("Unexpected message received.\n");
                break;
        }
        gst_message_unref(msg);
    }

    // Free resources
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
