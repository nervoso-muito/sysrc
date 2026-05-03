#include <glib.h>
#include <unistd.h>
#include <sys/stat.h>

#define DEFAULT_RCCONF "/etc/rc.conf"

static gboolean show_value_only = FALSE;
static gboolean show_all = FALSE;
static gboolean delete_mode = FALSE;
static gboolean quiet_mode = FALSE;
static gboolean query_only = FALSE;
static gboolean file_ended_with_newline = TRUE;

static gchar *rcconf_file = NULL;
static gchar *delete_var = NULL;

static void usage(const gchar *prog) {
    g_printerr("Usage: %s [-a] [-n] [-q] [-Q] [-x var] [-f file] [var[=value] ...]\n", prog);
    exit(1);
}

/* Load lines from file, track if file ended with newline */
static GPtrArray* load_lines(const gchar *filename) {
    GPtrArray *lines = g_ptr_array_new_with_free_func(g_free);
    gchar *content = NULL;
    gsize length = 0;
    GError *err = NULL;

    if (!g_file_get_contents(filename, &content, &length, &err)) {
        if (err && err->code != G_FILE_ERROR_NOENT) {
            g_warning("Error reading %s: %s", filename, err->message);
            g_error_free(err);
        }
        return lines;
    }

    // strip ending newline from content
    g_strstrip(content);

    gchar **split = g_strsplit(content, "\n", -1);
    for (gint i = 0; split[i] != NULL; i++) {
        g_ptr_array_add(lines, g_strdup(split[i]));
    }

    g_strfreev(split);
    g_free(content);
    return lines;
}

/* Save lines back to file atomically */
static void save_lines(GPtrArray *lines, const gchar *filename) {
    GString *buf = g_string_new(NULL);

    for (guint i = 0; i < lines->len; i++) {
        g_string_append(buf, g_ptr_array_index(lines, i));
        g_string_append_c(buf, '\n');   // ✅ Always newline after each line
    }

    GError *err = NULL;
    if (!g_file_set_contents(filename, buf->str, buf->len, &err)) {
        g_printerr("Error writing %s: %s\n", filename, err->message);
        g_error_free(err);
    } else if (!quiet_mode) {
        g_print("Saved changes to %s\n", filename);
    }

    g_string_free(buf, TRUE);
}

/* Get last value of a var (strip quotes) */
static gchar* get_var(GPtrArray *lines, const gchar *var) {
    gchar *value = NULL;
    gsize len = strlen(var);

    for (guint i = 0; i < lines->len; i++) {
        gchar *line = g_ptr_array_index(lines, i);
        if (g_str_has_prefix(line, var) && line[len] == '=') {
            g_free(value);
            value = g_strdup(line + len + 1);
            // Remove surrounding quotes if present
            if (value[0] == '"' && value[strlen(value)-1] == '"') {
                gchar *unquoted = g_strndup(value+1, strlen(value)-2);
                g_free(value);
                value = unquoted;
            }
        }
    }
    return value;
}

/* Add or update variable */
static void set_var(GPtrArray *lines, const gchar *var, const gchar *val) {
    gsize len = strlen(var);
    gboolean updated = FALSE;

    // Update if already present
    for (guint i = 0; i < lines->len; i++) {
        gchar *line = g_ptr_array_index(lines, i);
        if (g_str_has_prefix(line, var) && line[len] == '=') {
            g_free(line);
            g_ptr_array_index(lines, i) = g_strdup_printf("%s=\"%s\"", var, val);
            updated = TRUE;
            if (!quiet_mode)
                g_print("Editing %s: updated %s=\"%s\"\n", rcconf_file, var, val);
        }
    }

    // Otherwise append at the end
    if (!updated) {
        // If file didn’t end with newline, push an empty line first
        if (!file_ended_with_newline && lines->len > 0) {
            g_ptr_array_add(lines, g_strdup(""));
            file_ended_with_newline = TRUE;
        }
        g_ptr_array_add(lines, g_strdup_printf("%s=\"%s\"", var, val));
        if (!quiet_mode)
            g_print("Editing %s: added %s=\"%s\"\n", rcconf_file, var, val);
    }
}

/* Delete variable */
static void delete_var_line(GPtrArray *lines, const gchar *var) {
    gsize len = strlen(var);
    gboolean found = FALSE;

    for (gint i = lines->len - 1; i >= 0; i--) {
        gchar *line = g_ptr_array_index(lines, i);
        if (g_str_has_prefix(line, var) && line[len] == '=') {
            g_ptr_array_remove_index(lines, i);
            found = TRUE;
        }
    }

    if (!quiet_mode) {
        if (found)
            g_print("Editing %s: removed %s\n", rcconf_file, var);
        else
            g_print("%s: %s not found\n", rcconf_file, var);
    }
}

/* List all VAR=VALUE lines */
static void list_all(GPtrArray *lines) {
    g_print("# Listing all variables from %s\n", rcconf_file);
    for (guint i = 0; i < lines->len; i++) {
        gchar *line = g_ptr_array_index(lines, i);
        if (strchr(line, '=') && line[0] != '\0' && line[0] != '#') {
            g_print("rc.conf: %s\n", line);
        }
    }
}

int main(int argc, char **argv) {
    int opt;
    rcconf_file = g_strdup(DEFAULT_RCCONF);

    while ((opt = getopt(argc, argv, "anf:x:qQ")) != -1) {
        switch (opt) {
            case 'a': show_all = TRUE; break;
            case 'n': show_value_only = TRUE; break;
            case 'f': g_free(rcconf_file); rcconf_file = g_strdup(optarg); break;
            case 'x': delete_mode = TRUE; delete_var = g_strdup(optarg); break;
            case 'q': quiet_mode = TRUE; break;
            case 'Q': query_only = TRUE; break;
            default: usage(argv[0]);
        }
    }
    argc -= optind;
    argv += optind;

    GPtrArray *lines = load_lines(rcconf_file);

    if (show_all) {
        list_all(lines);
        g_ptr_array_free(lines, TRUE);
        g_free(rcconf_file);
        g_free(delete_var);
        return 0;
    }

    if (delete_mode) {
        delete_var_line(lines, delete_var);
        save_lines(lines, rcconf_file);
        g_ptr_array_free(lines, TRUE);
        g_free(rcconf_file);
        g_free(delete_var);
        return 0;
    }

    int status = 0;

    for (int i = 0; i < argc; i++) {
        gchar *arg = argv[i];
        gchar *eq = strchr(arg, '=');

        if (eq) {
            /* set mode */
            gchar *var = g_strndup(arg, eq - arg);
            gchar *val = g_strdup(eq + 1);
            set_var(lines, var, val);
            g_free(var);
            g_free(val);
        } else {
            /* query mode */
            gchar *val = get_var(lines, arg);
            if (val) {
                if (query_only || show_value_only)
                    g_print("%s\n", val);
                else
                    g_print("%s: %s\n", arg, val);
                if (g_strcmp0(val, "YES") != 0)
                    status = 1;
                g_free(val);
            } else {
                if (!query_only && !show_value_only)
                    g_print("%s is unset\n", arg);
                status = 1;
            }
        }
    }

    /* Save if any set operation occurred */
    for (int i = 0; i < argc; i++) {
        if (strchr(argv[i], '=')) {
            save_lines(lines, rcconf_file);
            break;
        }
    }

    g_ptr_array_free(lines, TRUE);
    g_free(rcconf_file);
    g_free(delete_var);

    return status;
}

