#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

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

    WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

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
