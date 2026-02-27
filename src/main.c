#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <gdk/gdkkeysyms.h>

/* LS Paint 0.3 — Native wrapper
 * Loads ls-paint.html from the same directory as the binary
 * into a native GTK + WebKitGTK window. */

static void on_destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

/* Resolve HTML path: same dir as the executable, or /usr/share/ls-paint/ */
static char *find_html(const char *argv0) {
    char exe[4096];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        char *dir = dirname(exe);
        char *path = g_build_filename(dir, "ls-paint.html", NULL);
        if (g_file_test(path, G_FILE_TEST_EXISTS)) return path;
        g_free(path);
    }
    /* Fallback: installed location */
    const char *installed = "/usr/share/ls-paint/ls-paint.html";
    if (g_file_test(installed, G_FILE_TEST_EXISTS))
        return g_strdup(installed);
    return NULL;
}

/* MIME type from file extension */
static const char *mime_from_path(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    if (g_ascii_strcasecmp(dot, ".png") == 0)  return "image/png";
    if (g_ascii_strcasecmp(dot, ".jpg") == 0)  return "image/jpeg";
    if (g_ascii_strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (g_ascii_strcasecmp(dot, ".gif") == 0)  return "image/gif";
    if (g_ascii_strcasecmp(dot, ".bmp") == 0)  return "image/bmp";
    if (g_ascii_strcasecmp(dot, ".webp") == 0) return "image/webp";
    if (g_ascii_strcasecmp(dot, ".svg") == 0)  return "image/svg+xml";
    if (g_ascii_strcasecmp(dot, ".ico") == 0)  return "image/x-icon";
    if (g_ascii_strcasecmp(dot, ".tif") == 0)  return "image/tiff";
    if (g_ascii_strcasecmp(dot, ".tiff") == 0) return "image/tiff";
    return NULL;
}

/* Intercept drag-and-drop: read dropped file URIs, convert to data URI,
 * and inject into the webview via insertImageFromURI(). */
static void on_drag_data_received(GtkWidget *widget, GdkDragContext *context,
    gint x, gint y, GtkSelectionData *sel_data, guint info, guint time, gpointer data)
{
    WebKitWebView *webview = WEBKIT_WEB_VIEW(data);
    gchar **uris = gtk_selection_data_get_uris(sel_data);
    if (!uris || !uris[0]) {
        g_strfreev(uris);
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    gchar *filepath = g_filename_from_uri(uris[0], NULL, NULL);
    g_strfreev(uris);
    if (!filepath) { gtk_drag_finish(context, FALSE, FALSE, time); return; }

    const char *mime = mime_from_path(filepath);
    if (!mime) { g_free(filepath); gtk_drag_finish(context, FALSE, FALSE, time); return; }

    gchar *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(filepath, &contents, &length, NULL)) {
        g_free(filepath);
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }
    g_free(filepath);

    gchar *base64 = g_base64_encode((guchar *)contents, length);
    g_free(contents);

    gchar *js = g_strdup_printf(
        "pasteImageFromURI('data:%s;base64,%s');", mime, base64);
    g_free(base64);

    webkit_web_view_evaluate_javascript(webview, js, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(js);

    gtk_drag_finish(context, TRUE, FALSE, time);
}

static gboolean on_drag_motion(GtkWidget *widget, GdkDragContext *context,
    gint x, gint y, guint time, gpointer data)
{
    gdk_drag_status(context, GDK_ACTION_COPY, time);
    return TRUE;
}

static gboolean on_drag_drop(GtkWidget *widget, GdkDragContext *context,
    gint x, gint y, guint time, gpointer data)
{
    GdkAtom target = gtk_drag_dest_find_target(widget, context, NULL);
    if (target != GDK_NONE) {
        gtk_drag_get_data(widget, context, target, time);
        return TRUE;
    }
    return FALSE;
}

/* Intercept Ctrl+V: if GTK clipboard has file URIs pointing to images,
 * read the file, encode as data URI, and inject into the webview. */
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (!((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_v))
        return FALSE;

    GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    WebKitWebView *webview = WEBKIT_WEB_VIEW(data);

    /* Try raw image data first (screenshots, browser copies) */
    GdkPixbuf *pixbuf = gtk_clipboard_wait_for_image(clip);
    if (pixbuf) {
        gchar *buf = NULL;
        gsize buf_len = 0;
        if (gdk_pixbuf_save_to_buffer(pixbuf, &buf, &buf_len, "png", NULL, NULL)) {
            gchar *base64 = g_base64_encode((guchar *)buf, buf_len);
            g_free(buf);
            gchar *js = g_strdup_printf(
                "pasteImageFromURI('data:image/png;base64,%s');", base64);
            g_free(base64);
            webkit_web_view_evaluate_javascript(webview, js, -1, NULL, NULL, NULL, NULL, NULL);
            g_free(js);
        }
        g_object_unref(pixbuf);
        return TRUE;
    }

    /* Fallback: file URIs */
    gchar **uris = gtk_clipboard_wait_for_uris(clip);
    if (!uris || !uris[0]) {
        g_strfreev(uris);
        return FALSE;  /* No file URIs — let webview handle normally */
    }

    gchar *filepath = g_filename_from_uri(uris[0], NULL, NULL);
    g_strfreev(uris);
    if (!filepath) return FALSE;

    const char *mime = mime_from_path(filepath);
    if (!mime) { g_free(filepath); return FALSE; }

    gchar *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(filepath, &contents, &length, NULL)) {
        g_free(filepath);
        return FALSE;
    }
    g_free(filepath);

    gchar *base64 = g_base64_encode((guchar *)contents, length);
    g_free(contents);

    gchar *js = g_strdup_printf(
        "pasteImageFromURI('data:%s;base64,%s');", mime, base64);
    g_free(base64);

    webkit_web_view_evaluate_javascript(webview, js, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(js);

    return TRUE;  /* Consume event — we handled the paste */
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    char *html_path = find_html(argv[0]);
    if (!html_path) {
        fprintf(stderr, "Error: ls-paint.html not found\n");
        return 1;
    }

    /* Read HTML file */
    char *html = NULL;
    gsize html_len = 0;
    if (!g_file_get_contents(html_path, &html, &html_len, NULL)) {
        fprintf(stderr, "Error: could not read %s\n", html_path);
        g_free(html_path);
        return 1;
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "LS Paint");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 860);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);

    /* Set window icon — try same dir as exe, then installed location */
    {
        char exe[4096];
        ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        const char *icon_path = NULL;
        char *local_icon = NULL;
        if (len > 0) {
            exe[len] = '\0';
            local_icon = g_build_filename(dirname(exe), "ls-paint-icon.png", NULL);
            if (g_file_test(local_icon, G_FILE_TEST_EXISTS)) icon_path = local_icon;
        }
        if (!icon_path) {
            if (g_file_test("/usr/local/share/ls-paint/ls-paint-icon.png", G_FILE_TEST_EXISTS))
                icon_path = "/usr/local/share/ls-paint/ls-paint-icon.png";
            else if (g_file_test("/usr/share/pixmaps/ls-paint.png", G_FILE_TEST_EXISTS))
                icon_path = "/usr/share/pixmaps/ls-paint.png";
        }
        if (icon_path) {
            GdkPixbuf *icon = gdk_pixbuf_new_from_file(icon_path, NULL);
            if (icon) { gtk_window_set_icon(GTK_WINDOW(window), icon); g_object_unref(icon); }
        }
        g_free(local_icon);
    }

    WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

    /* Intercept Ctrl+V for file clipboard paste */
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), webview);

    /* Intercept drag-and-drop for image files */
    {
        GtkTargetEntry targets[] = { { "text/uri-list", 0, 0 } };
        gtk_drag_dest_set(GTK_WIDGET(webview), 0,
            targets, G_N_ELEMENTS(targets),
            GDK_ACTION_COPY);
        g_signal_connect(GTK_WIDGET(webview), "drag-motion",
            G_CALLBACK(on_drag_motion), webview);
        g_signal_connect(GTK_WIDGET(webview), "drag-drop",
            G_CALLBACK(on_drag_drop), webview);
        g_signal_connect(GTK_WIDGET(webview), "drag-data-received",
            G_CALLBACK(on_drag_data_received), webview);
    }

    /* Build a file:// base URI so relative resources work */
    char *base_uri = g_filename_to_uri(html_path, NULL, NULL);
    webkit_web_view_load_html(webview, html, base_uri);

    g_free(html);
    g_free(html_path);
    g_free(base_uri);

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
