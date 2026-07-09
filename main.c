

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define APP_NAME       "wyrmshell"
#define DEFAULT_FONT   "JetBrains Mono 11"
#define REPL_FONT      "JetBrains Mono 10"
#define SYSINFO_FONT   "JetBrains Mono 10"
#define SCROLLBACK     10000
#define SCRATCH_FILE   "wyrm_scratch.py"
#define SYSINFO_HEIGHT_PX 300  /* fixed height for the fastfetch panel */

static gdouble font_scale = 1.0;
static gchar **g_envp = NULL;
static GPid main_shell_pid = 0;

static GtkWidget *paned;
static GtkWidget *panel_box;
static VteTerminal *repl_vte;
static gboolean panel_visible = TRUE;



static void
generic_spawn_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    if (error != NULL) {
        g_printerr("wyrmshell: failed to spawn: %s\n", error->message);
    }
}

static void
main_spawn_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    if (error != NULL) {
        g_printerr("wyrmshell: failed to spawn shell: %s\n", error->message);
        return;
    }
    main_shell_pid = pid;
}



static void
spawn_shell_into(VteTerminal *vte)
{
    const char *shell = g_getenv("SHELL");
    if (!shell || !*shell) shell = "/bin/bash";
    char *argv_shell[] = { (char *)shell, NULL };
    char *cwd = g_get_current_dir();

    vte_terminal_spawn_async(
        vte, VTE_PTY_DEFAULT, cwd, argv_shell, g_envp,
        G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL,
        main_spawn_callback, NULL
    );
    g_free(cwd);
}

static char *get_shell_cwd(void); 

static void
spawn_python_repl(void)
{
    char *argv_py[] = { "python3", NULL };
    char *cwd = get_shell_cwd();

    vte_terminal_spawn_async(
        repl_vte, VTE_PTY_DEFAULT, cwd, argv_py, g_envp,
        G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL,
        generic_spawn_callback, NULL
    );
    g_free(cwd);
}

static void
spawn_sysinfo_once(VteTerminal *vte)
{
    char *argv_ff[] = { "fastfetch", "--logo", "none", NULL };
    char *cwd = g_get_current_dir();

    vte_terminal_spawn_async(
        vte, VTE_PTY_DEFAULT, cwd, argv_ff, g_envp,
        G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL,
        generic_spawn_callback, NULL
    );
    g_free(cwd);
}

static void
on_child_exited(VteTerminal *terminal, gint status, gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(user_data));
}

static void
on_repl_exited(VteTerminal *terminal, gint status, gpointer user_data)
{
    spawn_python_repl(); 
}

static void
on_title_changed(VteTerminal *terminal, gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    const char *title = vte_terminal_get_window_title(terminal);
    gtk_window_set_title(window, title && *title ? title : APP_NAME);
}



static char *
get_shell_cwd(void)
{
    if (main_shell_pid == 0) return g_get_current_dir();

    char proc_path[64];
    g_snprintf(proc_path, sizeof(proc_path), "/proc/%d/cwd", (int)main_shell_pid);

    char buf[4096];
    ssize_t len = readlink(proc_path, buf, sizeof(buf) - 1);
    if (len < 0) return g_get_current_dir();
    buf[len] = '\0';
    return g_strdup(buf);
}



static char *
extract_repl_code(VteTerminal *vte)
{
    char *screen_text = vte_terminal_get_text(vte, NULL, NULL, NULL);
    if (!screen_text) return g_strdup("");

    GString *out = g_string_new(NULL);
    gchar **lines = g_strsplit(screen_text, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        const char *line = lines[i];
        if (g_str_has_prefix(line, ">>> ")) {
            g_string_append(out, line + 4);
            g_string_append_c(out, '\n');
        } else if (g_str_has_prefix(line, "... ")) {
            g_string_append(out, line + 4);
            g_string_append_c(out, '\n');
        }
    }
    g_strfreev(lines);
    g_free(screen_text);
    return g_string_free(out, FALSE);
}

static void
on_save_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *window = GTK_WIDGET(user_data);

    char *code = extract_repl_code(repl_vte);
    char *stripped = g_strdup(code);
    g_strstrip(stripped);
    if (strlen(stripped) == 0) {
        GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            "Nothing to save yet -- type some code into the shell first.");
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        g_free(code); g_free(stripped);
        return;
    }
    g_free(stripped);

    char *dir = get_shell_cwd();
    char *filepath = g_build_filename(dir, SCRATCH_FILE, NULL);

    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "Save this session's code as %s\nin %s\nand run it?",
        SCRATCH_FILE, dir);
    int response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response != GTK_RESPONSE_YES) {
        g_free(code); g_free(dir); g_free(filepath);
        return;
    }

    GError *err = NULL;
    if (!g_file_set_contents(filepath, code, -1, &err)) {
        GtkWidget *errdlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Couldn't write %s: %s", filepath, err->message);
        gtk_dialog_run(GTK_DIALOG(errdlg));
        gtk_widget_destroy(errdlg);
        g_error_free(err);
        g_free(code); g_free(dir); g_free(filepath);
        return;
    }

    char *exec_line = g_strdup_printf("exec(open('%s').read())\n", SCRATCH_FILE);
    vte_terminal_feed_child(repl_vte, exec_line, strlen(exec_line));

    g_free(exec_line);
    g_free(code); g_free(dir); g_free(filepath);
}



static void
toggle_panel(GtkWidget *window)
{
    panel_visible = !panel_visible;
    if (panel_visible) {
        gtk_widget_show(panel_box);
        int w = gtk_widget_get_allocated_width(window);
        gtk_paned_set_position(GTK_PANED(paned), (int)(w * 0.66));
    } else {
        gtk_widget_hide(panel_box);
    }
}



static void
apply_font_scale(VteTerminal *terminal)
{
    vte_terminal_set_font_scale(terminal, font_scale);
}

static gboolean
on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    GtkWidget *window = GTK_WIDGET(user_data);
    VteTerminal *terminal = VTE_TERMINAL(g_object_get_data(G_OBJECT(window), "main-vte"));
    guint mods = event->state & gtk_accelerator_get_default_mod_mask();

    if (mods == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        switch (event->keyval) {
            case GDK_KEY_C: case GDK_KEY_c:
                vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);
                return TRUE;
            case GDK_KEY_V: case GDK_KEY_v:
                vte_terminal_paste_clipboard(terminal);
                return TRUE;
            case GDK_KEY_Q: case GDK_KEY_q:
                gtk_main_quit();
                return TRUE;
            case GDK_KEY_P: case GDK_KEY_p:
                toggle_panel(window);
                return TRUE;
        }
    }
    if (mods == GDK_CONTROL_MASK) {
        switch (event->keyval) {
            case GDK_KEY_plus: case GDK_KEY_equal:
                font_scale += 0.1; apply_font_scale(terminal); return TRUE;
            case GDK_KEY_minus: case GDK_KEY_underscore:
                font_scale = MAX(0.3, font_scale - 0.1); apply_font_scale(terminal); return TRUE;
            case GDK_KEY_0: case GDK_KEY_parenright:
                font_scale = 1.0; apply_font_scale(terminal); return TRUE;
        }
    }
    return FALSE;
}



static void
set_colors(VteTerminal *terminal)
{
    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, "#e8e8e8");
    gdk_rgba_parse(&bg, "#14171c");

    GdkRGBA palette[16];
    const char *colors[16] = {
        "#14171c", "#e0605a", "#5cbf7a", "#e0b34f",
        "#5b8ec4", "#c9856b", "#8b9bb4", "#e8e8e8",
        "#5a6270", "#f0847e", "#7fd699", "#f0cd73",
        "#7fa8dc", "#e8a68d", "#a9b8d0", "#ffffff"
    };
    for (int i = 0; i < 16; i++) gdk_rgba_parse(&palette[i], colors[i]);
    vte_terminal_set_colors(terminal, &fg, &bg, palette, 16);
}

static void
style_widget(GtkWidget *w, const char *css)
{
    GtkStyleContext *ctx = gtk_widget_get_style_context(w);
    GtkCssProvider *prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, css, -1, NULL);
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static VteTerminal *
make_terminal(const char *font_str)
{
    GtkWidget *w = vte_terminal_new();
    VteTerminal *vte = VTE_TERMINAL(w);
    PangoFontDescription *font = pango_font_description_from_string(font_str);
    vte_terminal_set_font(vte, font);
    pango_font_description_free(font);
    vte_terminal_set_scrollback_lines(vte, SCROLLBACK);
    vte_terminal_set_cursor_blink_mode(vte, VTE_CURSOR_BLINK_ON);
    vte_terminal_set_cursor_shape(vte, VTE_CURSOR_SHAPE_BLOCK);
    vte_terminal_set_mouse_autohide(vte, TRUE);
    vte_terminal_set_scroll_on_keystroke(vte, TRUE);
    vte_terminal_set_bold_is_bright(vte, TRUE);
    set_colors(vte);
    return vte;
}



static GtkWidget *
build_panel(GtkWidget *window)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);


    GtkWidget *sysinfo_w = GTK_WIDGET(make_terminal(SYSINFO_FONT));
    VteTerminal *sysinfo_vte = VTE_TERMINAL(sysinfo_w);
    gtk_widget_set_size_request(sysinfo_w, -1, SYSINFO_HEIGHT_PX);
    gtk_box_pack_start(GTK_BOX(box), sysinfo_w, FALSE, FALSE, 0);
    spawn_sysinfo_once(sysinfo_vte);


    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label),
        "<span foreground='#8b9bb4' size='small'>  PYTHON SHELL</span>");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(label, TRUE);
    GtkWidget *save_btn = gtk_button_new_with_label("Save as script");
    style_widget(save_btn,
        "button { background-color: #e0b34f; color: #14171c; font-weight: bold; padding: 3px 10px; border-radius: 4px; }");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), window);
    gtk_box_pack_start(GTK_BOX(header), label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), save_btn, FALSE, FALSE, 6);
    style_widget(header, "box { background-color: #1b1f27; padding: 4px; }");
    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);


    GtkWidget *repl_w = GTK_WIDGET(make_terminal(REPL_FONT));
    repl_vte = VTE_TERMINAL(repl_w);
    g_signal_connect(repl_vte, "child-exited", G_CALLBACK(on_repl_exited), NULL);
    gtk_box_pack_start(GTK_BOX(box), repl_w, TRUE, TRUE, 0);

    return box;
}

int
main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), APP_NAME);
    gtk_window_set_default_size(GTK_WINDOW(window), 1300, 750);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    g_envp = g_get_environ();
    g_envp = g_environ_setenv(g_envp, "WYRMSHELL", "1", TRUE);
    g_envp = g_environ_setenv(g_envp, "TERM_PROGRAM", "wyrmshell", TRUE);

 
    VteTerminal *main_vte = make_terminal(DEFAULT_FONT);
    g_object_set_data(G_OBJECT(window), "main-vte", main_vte);
    g_signal_connect(main_vte, "child-exited", G_CALLBACK(on_child_exited), window);
    g_signal_connect(main_vte, "window-title-changed", G_CALLBACK(on_title_changed), window);

 
    panel_box = build_panel(window);

    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(paned), GTK_WIDGET(main_vte), TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), panel_box, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 860);

    gtk_container_add(GTK_CONTAINER(window), paned);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), window);

    gtk_widget_show_all(window);

    spawn_shell_into(main_vte);
    spawn_python_repl();

    gtk_main();
    return 0;
}
