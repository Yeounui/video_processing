#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>
#include <inttypes.h>
#include <termios.h>
#include <SDL2/SDL.h>
#include <editline/readline.h>
#include "event_handler.h"
#include "input.h"
#include "image_saver.h"

static EventHandler *g_handler     = NULL;
static bool          g_use_rl      = false;
static int           g_combo_held  = 0; /* SDLK of held combo-capable key */

typedef struct { float v[3]; } AlgDefault;
static AlgDefault g_defaults[PROCESSOR_ALGORITHM_COUNT + 1];

static const struct { const char *cmd; int id; } CMD_TABLE[] = {
    {"brighten",  1}, {"multiply",   2}, {"gamma",      3},
    {"threshold", 4}, {"autothresh", 5}, {"bitand",     6},
    {"flip",      7}, {"rotate",     8}, {"emboss",     9},
    {"stretch",   10},{"blur",       11},{"blur5",      12},
    {"gaussblur", 13},{"sharpen",    14},{"hipass",     15},
    {"boost",     16},{"diagblur",   17},{"hblur",      18},
    {"sobelh",    19},{"sobelv",     20},{"laplace",    21},
    {"dog",       22},{"histretch",  23},{"endpoint",   24},
    {"median",    25},{"grayavg",    26},{"graylum",    27},
    {"graylight", 28},{NULL, 0}
};

/* b/m/g/t/f/r/k are combo-capable and require a second key. */
static const struct { int key; int alg_id; } KEY_EFFECTS[] = {
    /* existing */
    {'e', 9},  {'u', 11}, {'d', 22}, {'z', 25},
    /* configurable algorithms */
    {'n', 6},  {'j', 13}, {'p', 15}, {'o', 16},
    {'y', 17}, {'i', 18}, {'s', 19}, {'v', 20},
    {'3', 24}, {'4', 26}, {'5', 27}, {'6', 28},
    /* fixed-param algorithms */
    {'a', 5},  {'l', 10}, {'w', 12},
    {'1', 21}, {'2', 23},
    {0, 0}
};

typedef struct {
    int         id;
    const char *name;
    const char *key_label;
} CfgEntry;

static const CfgEntry CFG_TABLE_CFG[] = {
    {1,  "brighten",  "B+/-"},
    {2,  "multiply",  "M+/-"},
    {3,  "gamma",     "G+/-"},
    {4,  "threshold", "T+/-"},
    {6,  "bitand",    "N"},
    {8,  "rotate",    "R+/-"},
    {13, "gaussblur", "J"},
    {14, "sharpen",   "K+/-"},
    {15, "hipass",    "P"},
    {16, "boost",     "O"},
    {17, "diagblur",  "Y"},
    {18, "hblur",     "I"},
    {19, "sobelh",    "S"},
    {20, "sobelv",    "V"},
    {22, "dog",       "D"},
    {24, "endpoint",  "3"},
    {0,  NULL, NULL}
};
#define N_CFG_ALGS 16

static void events_dispatch_window_resize(EventHandler *e, int w, int h);
static void events_poll_stdin(EventHandler *e);
static void dispatch_line(EventHandler *e, char *buf);
static void handle_apply_algorithm(EventHandler *e, int alg_id, const char *param_str);
static void handle_save_format(EventHandler *e, const char *fmt_and_path);
static void fill_effect_params(int alg_id, const char *param_str, EffectParams *params);
static void render_video_frame(EventHandler *e);
static void init_defaults(void);
static void format_default(int id, char *out, size_t n);
static void format_combo_action(int id, char *out, size_t n);
static void print_config_panel(int selected, bool first);
static void handle_set_default(const char *buf);
static void run_config_mode(void);
static void clear_combo(EventHandler *e);
static bool is_combo_repeat_key(int sdl_keycode);
static void events_dispatch_sdl_key(EventHandler *e, int sdl_keycode);
static void events_dispatch_sdl_keyup(EventHandler *e, int sdl_keycode);
static int match_alg_cmd(const char *buf, const char **rest_out);
static float speed_next(float cur, bool up);
static void print_help_none(void);
static void print_help_image(void);
static void print_help_video(void);
static void print_help_realtime(void);
static void print_help(AppState *state);

static void print_help_none(void) {
    puts("--- Commands ---");
    puts("  open <path>         open image, video file, /dev/videoN, or rtsp://...");
    puts("  config (C)          configure algorithm defaults");
    puts("  help   (H)          print this help");
    puts("  quit   (Q/Esc)      quit");
}

static void print_help_image(void) {
    puts("--- Image Commands ---");
    puts("  open <path>                    open image, video file, /dev/videoN, or rtsp://...");
    puts("  save <fmt> [path]              save: png  jpg  bmp  tga");
    puts("  saveppm [path]                 save PPM");
    puts("  reset             (Bksp/X)     reset to original");
    puts("  config            (C)          configure algorithm defaults");
    puts("  help              (H)          print this help");
    puts("  quit              (Q/Esc)      quit");
    puts("Algorithms (SDL key):  combos: hold first key then +/-");
    puts("  brighten (B+/-)   multiply (M+/-)   gamma (G+/-)      threshold (T+/-)");
    puts("  autothresh (A)    bitand (N)        flip (F+/-)       rotate (R+/-)");
    puts("  emboss (E)        stretch (L)       blur (U)          blur5 (W)");
    puts("  gaussblur (J)     sharpen (K+/-)    hipass (P)        boost (O)");
    puts("  diagblur (Y)      hblur (I)         sobelh (S)        sobelv (V)");
    puts("  laplace (1)       dog (D)           histretch (2)     endpoint (3)");
    puts("  median (Z)       grayavg (4)       graylum (5)      graylight (6)");
}

static void print_help_video(void) {
    puts("--- Video Commands ---");
    puts("  open <path>                    open image, video file, /dev/videoN, or rtsp://...");
    puts("  play/pause        (Space)      toggle play/pause");
    puts("  seek <0-9>                     seek to 0%-90%");
    puts("  seekinit/seekend  (Home/End)   seek to start/end and pause");
    puts("  slower/faster     (\xe2\x86\x93/\xe2\x86\x91)        speed down/up  (0.5x 0.75x 1x 1.5x 2x)");
    puts("  loop              (Tab)        toggle loop");
    puts("  info              (I)          playback info");
    puts("  step              (\xe2\x86\x90/\xe2\x86\x92)      frame step back/forward (while paused)");
    puts("  clear             (Bksp/X)     clear effect stack (max 3)");
    puts("  config            (C)          configure algorithm defaults");
    puts("  help              (H)          print this help");
    puts("  quit              (Q/Esc)      quit");
    puts("Algorithms (SDL key):  combos: hold first key then +/-");
    puts("  brighten (B+/-)   multiply (M+/-)   gamma (G+/-)      threshold (T+/-)");
    puts("  autothresh (A)    bitand (N)        flip (F+/-)       rotate (R+/-)");
    puts("  emboss (E)        stretch (L)       blur (U)          blur5 (W)");
    puts("  gaussblur (J)     sharpen (K+/-)    hipass (P)        boost (O)");
    puts("  diagblur (Y)      hblur (-)         sobelh (S)        sobelv (V)");
    puts("  laplace (1)       dog (D)           histretch (2)     endpoint (3)");
    puts("  median (Z)       grayavg (4)       graylum (5)      graylight (6)");
}

static void print_help_realtime(void) {
    puts("--- Realtime Commands ---");
    puts("  open <url>             open new stream (/dev/videoN or rtsp://...)");
    puts("  play        (Space)    freeze / resume display");
    puts("  reconnect              reconnect stream");
    puts("  buffer                 buffer status");
    puts("  info                   stream info");
    puts("  clear       (Bksp/X)   clear effect stack (max 3)");
    puts("  config      (C)        configure algorithm defaults");
    puts("  help        (H)        print this help");
    puts("  quit        (Q/Esc)    quit");
    puts("Effect stack:  <name> [param]");
    puts("SDL shortcuts:  B+/-=brighten M+/-=multiply G+/-=gamma T+/-=threshold");
    puts("  F+/-=flip R+/-=rotate K+/-=sharpen  E=emboss U=blur D=dog Z=median");
    puts("  A=autothresh L=stretch W=blur5 J=gaussblur P=hipass O=boost");
    puts("  Y=diagblur I=hblur S=sobelh V=sobelv 1=laplace 2=histretch 3=endpoint N=bitand");
    puts("  4=grayavg 5=graylum 6=graylight");
}

static void print_help(AppState *state) {
    if (!state || state->sourceType == SOURCE_NONE)
        print_help_none();
    else if (state->sourceType == SOURCE_IMAGE)
        print_help_image();
    else if (state->sourceType == SOURCE_VIDEO_FILE)
        print_help_video();
    else if (state->sourceType == SOURCE_REALTIME_STREAM)
        print_help_realtime();
}

static char *cmd_generator(const char *text, int state) {
    static const char *cmds[] = {
        "brighten", "multiply", "gamma", "threshold", "autothresh", "bitand",
        "flip", "rotate", "emboss", "stretch", "blur", "blur5", "gaussblur",
        "sharpen", "hipass", "boost", "diagblur", "hblur", "sobelh", "sobelv",
        "laplace", "dog", "histretch", "endpoint", "median",
        "grayavg", "graylum", "graylight",
        "open ", "save ", "saveppm", "reset", "config", "clear", "play",
        "loop", "info", "step", "buffer", "reconnect", "seek ", "seekend",
        "seekinit", "slower", "faster", "help", "quit", NULL
    };
    static int idx;
    const char *c;
    if (!state) idx = 0;
    while ((c = cmds[idx]) != NULL) {
        idx++;
        if (strncmp(c, text, strlen(text)) == 0)
            return strdup(c);
    }
    return NULL;
}

static char *fmt_generator(const char *text, int state) {
    static const char *fmts[] = {"png", "jpg", "bmp", "tga", NULL};
    static int idx;
    const char *f;
    if (!state) idx = 0;
    while ((f = fmts[idx]) != NULL) {
        idx++;
        if (strncmp(f, text, strlen(text)) == 0)
            return strdup(f);
    }
    return NULL;
}

static char *flip_generator(const char *text, int state) {
    static const char *dirs[] = {"h", "v", NULL};
    static int idx;
    const char *d;
    if (!state) idx = 0;
    while ((d = dirs[idx]) != NULL) {
        idx++;
        if (strncmp(d, text, strlen(text)) == 0)
            return strdup(d);
    }
    return NULL;
}

static char **my_completion(const char *text, int start, int end) {
    char first[64];
    const char *sp;
    size_t wlen;
    (void)end;

    if (start == 0) {
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, cmd_generator);
    }

    sp = strchr(rl_line_buffer, ' ');
    first[0] = '\0';
    if (sp) {
        wlen = (size_t)(sp - rl_line_buffer);
        if (wlen >= sizeof first) wlen = sizeof first - 1;
        memcpy(first, rl_line_buffer, wlen);
        first[wlen] = '\0';
    }

    if (strcmp(first, "flip") == 0) {
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, flip_generator);
    }
    if (strcmp(first, "s") == 0 || strcmp(first, "save") == 0) {
        const char *base = strcmp(first, "s") == 0 ? rl_line_buffer + 2 : rl_line_buffer + 5;
        const char *sp2 = strchr(base, ' ');
        if (sp2 != NULL && start > (int)(sp2 - rl_line_buffer)) {
            return NULL;
        }
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, fmt_generator);
    }
    if (strcmp(first, "o") == 0 || strcmp(first, "open") == 0 ||
        strcmp(first, "p") == 0 || strcmp(first, "saveppm") == 0) {
        return NULL;
    }
    rl_attempted_completion_over = 1;
    return NULL;
}

static void on_readline_line(char *line) {
    if (!line) {
        g_handler->state->running = false;
        return;
    }
    if (line[0] != '\0') {
        add_history(line);
        dispatch_line(g_handler, line);
    }
    free(line);
    if (g_handler->state->running)
        rl_callback_handler_install("> ", on_readline_line);
}

void events_init(EventHandler *e, AppState *state, ImageProcessor *p, Renderer *r) {
    e->state     = state;
    e->processor = p;
    e->renderer  = r;
    init_defaults();
    if (isatty(STDIN_FILENO)) {
        g_handler = e;
        g_use_rl  = true;
        rl_attempted_completion_function = my_completion;
        rl_callback_handler_install("> ", on_readline_line);
    } else {
        printf("> ");
        fflush(stdout);
    }
}

void events_process(EventHandler *e) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            e->state->running = false;
        } else if (ev.type == SDL_WINDOWEVENT &&
                   ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            events_dispatch_window_resize(e, ev.window.data1, ev.window.data2);
        } else if (ev.type == SDL_KEYDOWN &&
                   (ev.key.repeat == 0 || is_combo_repeat_key((int)ev.key.keysym.sym))) {
            events_dispatch_sdl_key(e, (int)ev.key.keysym.sym);
        } else if (ev.type == SDL_KEYUP) {
            events_dispatch_sdl_keyup(e, (int)ev.key.keysym.sym);
        }
    }
    events_poll_stdin(e);
}

void events_destroy(EventHandler *e) {
    (void)e;
    if (g_use_rl) {
        rl_callback_handler_remove();
        clear_history();
        g_use_rl  = false;
        g_handler = NULL;
    }
}

static void init_defaults(void) {
    g_defaults[1].v[0]  = 30.0f;
    g_defaults[2].v[0]  = 1.2f;    g_defaults[2].v[1] = 1.0f / 1.2f;   /* multiply + and 1/+ */
    g_defaults[3].v[0]  = 1.1f;    g_defaults[3].v[1] = 1.0f / 1.1f;   /* gamma + and 1/+ */
    g_defaults[4].v[0]  = 159.0f;  g_defaults[4].v[1] = 95.0f;         /* threshold T+ and T- (independent) */
    g_defaults[6].v[0]  = (float)0xC9u;
    g_defaults[7].v[0]  = 0.0f;
    g_defaults[8].v[0]  = 30.0f;
    g_defaults[13].v[0] = 5.0f;
    g_defaults[13].v[1] = 1.0f;
    g_defaults[14].v[0] = 1.25f;  g_defaults[14].v[1] = 1.0f / 1.25f; /* sharpen + and 1/+ */
    g_defaults[15].v[0] = 1.0f;
    g_defaults[16].v[0] = 1.0f;
    g_defaults[17].v[0] = 9.0f;
    g_defaults[18].v[0] = 9.0f;
    g_defaults[19].v[0] = 1.0f;
    g_defaults[20].v[0] = 1.0f;
    g_defaults[22].v[0] = 1.0f;
    g_defaults[22].v[1] = 2.0f;
    g_defaults[22].v[2] = 1.0f;
    g_defaults[24].v[0] = 127.0f;
}

static void format_default(int id, char *out, size_t n) {
    switch (id) {
    case 1:  snprintf(out, n, "delta = +/-%.0f",  g_defaults[1].v[0]); break;
    case 2:  snprintf(out, n, "x%.3f",                   g_defaults[2].v[0]); break;
    case 3:  snprintf(out, n, "%.3f",                    g_defaults[3].v[0]); break;
    case 4:  snprintf(out, n, "T+=%.0f  T-=%.0f",        g_defaults[4].v[0], g_defaults[4].v[1]); break;
    case 5:  snprintf(out, n, "(auto: image mean)");     break;
    case 6:  snprintf(out, n, "mask = 0x%02X",  (unsigned)g_defaults[6].v[0]); break;
    case 7: {
        const char *s = "h";
        if ((int)g_defaults[7].v[0] == 1) s = "v";
        else if ((int)g_defaults[7].v[0] == 2) s = "both";
        snprintf(out, n, "mode = %s", s);
        break;
    }
    case 8:  snprintf(out, n, "angle = +/-%.0f", g_defaults[8].v[0]); break;
    case 9:  snprintf(out, n, "(fixed kernel)");         break;
    case 10: snprintf(out, n, "(auto: min/max)");        break;
    case 11: snprintf(out, n, "(fixed 3x3)");            break;
    case 12: snprintf(out, n, "(fixed 5x5)");            break;
    case 13: snprintf(out, n, "sz=%.0f sig=%.2f",    g_defaults[13].v[0], g_defaults[13].v[1]); break;
    case 14: snprintf(out, n, "a = %.3f",                g_defaults[14].v[0]); break;
    case 15: snprintf(out, n, "strength = %.3f",         g_defaults[15].v[0]); break;
    case 16: snprintf(out, n, "beta = %.3f",             g_defaults[16].v[0]); break;
    case 17: snprintf(out, n, "dist = %.0f",             g_defaults[17].v[0]); break;
    case 18: snprintf(out, n, "dist = %.0f",             g_defaults[18].v[0]); break;
    case 19: snprintf(out, n, "scale = %.3f",            g_defaults[19].v[0]); break;
    case 20: snprintf(out, n, "scale = %.3f",            g_defaults[20].v[0]); break;
    case 21: snprintf(out, n, "(fixed kernel)");         break;
    case 22: snprintf(out, n, "%.2f %.2f %.2f", g_defaults[22].v[0], g_defaults[22].v[1], g_defaults[22].v[2]); break;
    case 23: snprintf(out, n, "(auto: histogram)");      break;
    case 24: snprintf(out, n, "val = %.0f",              g_defaults[24].v[0]); break;
    case 25: snprintf(out, n, "(fixed 3x3)");            break;
    case 26: snprintf(out, n, "(fixed RGB average)");    break;
    case 27: snprintf(out, n, "(fixed luminosity)");     break;
    case 28: snprintf(out, n, "(fixed lightness)");      break;
    default: snprintf(out, n, "?");                      break;
    }
}

static void format_combo_action(int id, char *out, size_t n) {
    switch (id) {
    case 1:
        snprintf(out, n, "+%.0f / -%.0f",
                 g_defaults[1].v[0], g_defaults[1].v[0]);
        break;
    case 2:
        snprintf(out, n, "x%.3f / x%.3f",
                 g_defaults[2].v[0], g_defaults[2].v[1]);
        break;
    case 3:
        snprintf(out, n, "%.3f / %.3f",
                 g_defaults[3].v[0], g_defaults[3].v[1]);
        break;
    case 4:
        snprintf(out, n, "T+:%.0f T-:%.0f",
                 g_defaults[4].v[0], g_defaults[4].v[1]);
        break;
    case 8:
        snprintf(out, n, "+%.0f / -%.0f",
                 g_defaults[8].v[0], g_defaults[8].v[0]);
        break;
    case 14:
        snprintf(out, n, "a%.3f / a%.3f",
                 g_defaults[14].v[0], g_defaults[14].v[1]);
        break;
    default:
        snprintf(out, n, "applies default");
        break;
    }
}

#define PANEL_LINES 20

static void print_config_panel(int selected, bool first) {
    int i;

    if (!first)
        printf("\033[%dA", PANEL_LINES);

    printf("\033[2K\r+-- Config (Up/Down=select Enter=edit q=exit) ---------+\n");
    printf("\033[2K\r|%c %-5s %-10s %-18s %-16s|\n",
           ' ', "Key", "Algorithm", "Current Default", "Action");
    printf("\033[2K\r+------------------------------------------------------+\n");

    for (i = 0; i < N_CFG_ALGS; i++) {
        const CfgEntry *ce = &CFG_TABLE_CFG[i];
        char def_buf[64];
        char act_buf[64];
        char cursor;
        format_default(ce->id, def_buf, sizeof def_buf);
        format_combo_action(ce->id, act_buf, sizeof act_buf);
        cursor = (i == selected) ? '>' : ' ';
        printf("\033[2K\r|%c %-5s %-10s %-18s %-16s|\n",
               cursor, ce->key_label, ce->name, def_buf, act_buf);
    }
    printf("\033[2K\r+------------------------------------------------------+\n");
    fflush(stdout);
}

static int match_alg_cmd(const char *buf, const char **rest_out) {
    int i;
    for (i = 0; CMD_TABLE[i].cmd != NULL; i++) {
        size_t len = strlen(CMD_TABLE[i].cmd);
        if (strncmp(buf, CMD_TABLE[i].cmd, len) == 0) {
            if (buf[len] == '\0') {
                *rest_out = NULL;
                return CMD_TABLE[i].id;
            }
            if (buf[len] == ' ') {
                *rest_out = buf + len + 1;
                return CMD_TABLE[i].id;
            }
        }
    }
    return 0;
}

static void handle_set_default(const char *buf) {
    char name[64];
    const char *rest;
    const char *sp;
    int alg_id;
    int i;

    sp = strchr(buf, ' ');
    if (sp) {
        size_t nlen = (size_t)(sp - buf);
        if (nlen >= sizeof name) nlen = sizeof name - 1;
        memcpy(name, buf, nlen);
        name[nlen] = '\0';
        rest = sp + 1;
    } else {
        strncpy(name, buf, sizeof name - 1);
        name[sizeof name - 1] = '\0';
        rest = buf + strlen(buf);
    }

    alg_id = 0;
    for (i = 0; CMD_TABLE[i].cmd != NULL; i++) {
        if (strcasecmp(CMD_TABLE[i].cmd, name) == 0) {
            alg_id = CMD_TABLE[i].id;
            break;
        }
    }
    if (!alg_id) {
        printf("  unknown: '%s'\n", name);
        return;
    }

    switch (alg_id) {
    case 2:
        if (rest[0]) {
            float val = (float)atof(rest);
            if (val <= 1.0f) {
                printf("  multiply: factor must be > 1.0\n");
                return;
            }
            g_defaults[2].v[0] = val;
            g_defaults[2].v[1] = 1.0f / val;
        }
        break;
    case 3:
        if (rest[0]) {
            float val = (float)atof(rest);
            if (val <= 1.0f) {
                printf("  gamma: value must be > 1.0\n");
                return;
            }
            g_defaults[3].v[0] = val;
            g_defaults[3].v[1] = 1.0f / val;
        }
        break;
    case 4:
        if (rest[0]) {
            float plus_val = 0.0f, minus_val = 0.0f;
            int cnt = sscanf(rest, "%f %f", &plus_val, &minus_val);
            if (cnt < 2) {
                printf("  threshold: enter two values: T+_val T-_val (e.g. 150 90)\n");
                return;
            }
            g_defaults[4].v[0] = plus_val;
            g_defaults[4].v[1] = minus_val;
        }
        break;
    case 6:
        if (rest[0]) g_defaults[6].v[0] = (float)strtoul(rest, NULL, 0);
        break;
    case 13:
        if (rest[0]) sscanf(rest, "%f %f", &g_defaults[13].v[0], &g_defaults[13].v[1]);
        break;
    case 14:
        if (rest[0]) {
            float val = (float)atof(rest);
            if (val <= 1.0f) {
                printf("  sharpen: alpha must be > 1.0\n");
                return;
            }
            g_defaults[14].v[0] = val;
            g_defaults[14].v[1] = 1.0f / val;
        }
        break;
    case 22:
        if (rest[0]) sscanf(rest, "%f %f %f",
                            &g_defaults[22].v[0], &g_defaults[22].v[1], &g_defaults[22].v[2]);
        break;
    case 7: case 9: case 10: case 11: case 12: case 21: case 23: case 25:
    case 26: case 27: case 28:
        printf("  %s has no configurable parameters\n", name);
        return;
    case 5:
        printf("  autothresh uses auto-computed threshold (no param)\n");
        return;
    default:
        if (rest[0]) g_defaults[alg_id].v[0] = (float)atof(rest);
        break;
    }

    {
        char buf2[128];
        format_default(alg_id, buf2, sizeof buf2);
        printf("  %s: %s\n", CMD_TABLE[alg_id - 1].cmd, buf2);
    }
}

static void run_config_mode(void) {
    struct termios old_tio, raw_tio;
    int selected = 0;
    bool done = false;
    bool need_fresh = true;

    if (!g_use_rl) {
        puts("  (config mode requires interactive terminal)");
        return;
    }

    tcgetattr(STDIN_FILENO, &old_tio);
    raw_tio = old_tio;
    raw_tio.c_lflag &= (tcflag_t)~(ICANON | ECHO | IEXTEN);
    raw_tio.c_cc[VMIN]  = 0;
    raw_tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_tio);

    rl_callback_handler_remove();

    while (!done) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                g_handler->state->running = false;
                done = true;
                break;
            } else if (ev.type == SDL_WINDOWEVENT &&
                       ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                events_dispatch_window_resize(g_handler,
                    ev.window.data1, ev.window.data2);
            } else if (ev.type == SDL_KEYDOWN) {
                int sym = (int)ev.key.keysym.sym;
                if (sym == SDLK_UP) {
                    if (selected > 0) { selected--; }
                } else if (sym == SDLK_DOWN) {
                    if (selected < N_CFG_ALGS - 1) { selected++; }
                } else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                    char prompt[64];
                    char *line;
                    char full[256];
                    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
                    printf("\n");
                    snprintf(prompt, sizeof prompt, "  %s> ",
                             CFG_TABLE_CFG[selected].name);
                    line = readline(prompt);
                    if (line && line[0] != '\0') {
                        add_history(line);
                        snprintf(full, sizeof full, "%s %s",
                                 CFG_TABLE_CFG[selected].name, line);
                        handle_set_default(full);
                    }
                    free(line);
                    tcsetattr(STDIN_FILENO, TCSANOW, &raw_tio);
                    need_fresh = true;
                } else if (sym == SDLK_ESCAPE || sym == SDLK_q) {
                    done = true;
                }
            }
        }
        if (done) break;

        print_config_panel(selected, need_fresh);
        need_fresh = false;

        {
            char ch = 0;
            ssize_t nr = read(STDIN_FILENO, &ch, 1);
            if (nr > 0) {
                if (ch == '\033') {
                    char seq[2] = {0, 0};
                    raw_tio.c_cc[VTIME] = 1;
                    tcsetattr(STDIN_FILENO, TCSANOW, &raw_tio);
                    if (read(STDIN_FILENO, &seq[0], 1) > 0 && seq[0] == '[') {
                        read(STDIN_FILENO, &seq[1], 1);
                        if (seq[1] == 'A') {
                            if (selected > 0) { selected--; }
                        } else if (seq[1] == 'B') {
                            if (selected < N_CFG_ALGS - 1) { selected++; }
                        }
                    }
                    raw_tio.c_cc[VTIME] = 0;
                    tcsetattr(STDIN_FILENO, TCSANOW, &raw_tio);
                } else if (ch == '\r' || ch == '\n') {
                    char prompt[64];
                    char *line;
                    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
                    printf("\n");
                    snprintf(prompt, sizeof prompt, "  %s> ",
                             CFG_TABLE_CFG[selected].name);
                    line = readline(prompt);
                    if (line && line[0] != '\0') {
                        char full[256];
                        add_history(line);
                        snprintf(full, sizeof full, "%s %s",
                                 CFG_TABLE_CFG[selected].name, line);
                        handle_set_default(full);
                    }
                    free(line);
                    tcsetattr(STDIN_FILENO, TCSANOW, &raw_tio);
                    need_fresh = true;
                } else if (ch == 'q' || ch == 'Q' || ch == 3) {
                    done = true;
                }
            }
        }

        SDL_Delay(16);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    printf("\n");

    rl_callback_handler_install("> ", on_readline_line);
}

static void clear_combo(EventHandler *e) {
    (void)e;
    g_combo_held = 0;
}

static bool is_combo_repeat_key(int sdl_keycode) {
    return sdl_keycode == SDLK_PLUS ||
           sdl_keycode == SDLK_EQUALS ||
           sdl_keycode == SDLK_KP_PLUS ||
           sdl_keycode == SDLK_MINUS ||
           sdl_keycode == SDLK_KP_MINUS;
}

static void events_dispatch_sdl_keyup(EventHandler *e, int sdl_keycode) {
    (void)e;
    if (sdl_keycode == g_combo_held)
        g_combo_held = 0;
}

static void events_dispatch_sdl_key(EventHandler *e, int sdl_keycode) {
    int i;
    AppState *state = e->state;

    /* Universal keys */
    if (sdl_keycode == SDLK_ESCAPE || sdl_keycode == SDLK_q) {
        clear_combo(e);
        state->running = false;
        return;
    }
    if (sdl_keycode == SDLK_h) {
        clear_combo(e);
        print_help(state);
        return;
    }
    if (sdl_keycode == SDLK_c) {
        clear_combo(e);
        run_config_mode();
        return;
    }
    if (sdl_keycode == SDLK_BACKSPACE || sdl_keycode == SDLK_x) {
        clear_combo(e);
        if (state->sourceType == SOURCE_IMAGE) {
            state->effectCount = 0;
            processor_reset(state);
            renderer_upload_source_texture(e->renderer, &state->outImage);
            renderer_resample(e->renderer, state->outImage.width, state->outImage.height);
            puts("reset");
        } else if (state->sourceType == SOURCE_VIDEO_FILE ||
                   state->sourceType == SOURCE_REALTIME_STREAM) {
            state->effectCount = 0;
            state->imgRotateAngle = 0.0f;
            state->imgFlipH = false;
            state->imgFlipV = false;
            render_video_frame(e);
            puts("effect stack cleared");
        }
        return;
    }
    if (sdl_keycode == SDLK_SPACE) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            if (!state->video.playing && state->video.at_end) {
                input_video_seek(state, 0.0f);
            }
            state->video.playing = !state->video.playing;
            if (state->video.playing) input_video_reset_timing();
            printf("%s\n", state->video.playing ? "playing" : "paused");
        } else if (state->sourceType == SOURCE_REALTIME_STREAM) {
            state->realtime.playing = !state->realtime.playing;
            printf("%s\n", state->realtime.playing ? "live" : "frozen");
        }
        return;
    }
    if (sdl_keycode == SDLK_RIGHT) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE && !state->video.playing) {
            input_video_step_frame(state);
            render_video_frame(e);
            printf("step: %.3fs\n", state->video.current_pts_s);
        }
        return;
    }
    if (sdl_keycode == SDLK_LEFT) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE && !state->video.playing) {
            input_video_step_frame_backward(state);
            render_video_frame(e);
            printf("step: %.3fs\n", state->video.current_pts_s);
        }
        return;
    }
    if (sdl_keycode == SDLK_UP) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            state->video.speed = speed_next(state->video.speed, true);
            input_video_reset_timing();
            printf("speed: %.2fx\n", (double)state->video.speed);
        }
        return;
    }
    if (sdl_keycode == SDLK_DOWN) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            state->video.speed = speed_next(state->video.speed, false);
            input_video_reset_timing();
            printf("speed: %.2fx\n", (double)state->video.speed);
        }
        return;
    }
    if (sdl_keycode == SDLK_HOME) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            input_video_seek(state, 0.0f);
            state->video.playing = false;
            puts("seekinit");
        }
        return;
    }
    if (sdl_keycode == SDLK_END) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            input_video_seek_end(state);
            puts("seekend");
        }
        return;
    }

    /* Combo second-key: +/- resolve a held combo key. */
    if (sdl_keycode == SDLK_PLUS || sdl_keycode == SDLK_EQUALS || sdl_keycode == SDLK_KP_PLUS) {
        char _p[32];
        if (g_combo_held == SDLK_b) { snprintf(_p,sizeof _p,"%.0f", g_defaults[1].v[0]); handle_apply_algorithm(e,1,_p); return; }
        if (g_combo_held == SDLK_m) { snprintf(_p,sizeof _p,"%.6g", g_defaults[2].v[0]); handle_apply_algorithm(e,2,_p); return; }
        if (g_combo_held == SDLK_g) { snprintf(_p,sizeof _p,"%.6g", g_defaults[3].v[0]); handle_apply_algorithm(e,3,_p); return; }
        if (g_combo_held == SDLK_t) { snprintf(_p,sizeof _p,"%.0f", g_defaults[4].v[0]); handle_apply_algorithm(e,4,_p); return; }
        if (g_combo_held == SDLK_f) { handle_apply_algorithm(e, 7,"h"); return; }
        if (g_combo_held == SDLK_r) { snprintf(_p,sizeof _p,"%.1f", g_defaults[8].v[0]); handle_apply_algorithm(e,8,_p); return; }
        if (g_combo_held == SDLK_k) { snprintf(_p,sizeof _p,"%.6g", g_defaults[14].v[0]); handle_apply_algorithm(e,14,_p); return; }
        return;
    }
    if (sdl_keycode == SDLK_MINUS || sdl_keycode == SDLK_KP_MINUS) {
        char _p[32];
        if (g_combo_held == SDLK_b) { snprintf(_p,sizeof _p,"-%.0f",g_defaults[1].v[0]); handle_apply_algorithm(e,1,_p); return; }
        if (g_combo_held == SDLK_m) { snprintf(_p,sizeof _p,"%.6g", g_defaults[2].v[1]); handle_apply_algorithm(e,2,_p); return; }
        if (g_combo_held == SDLK_g) { snprintf(_p,sizeof _p,"%.6g", g_defaults[3].v[1]); handle_apply_algorithm(e,3,_p); return; }
        if (g_combo_held == SDLK_t) { snprintf(_p,sizeof _p,"%.0f", g_defaults[4].v[1]); handle_apply_algorithm(e,4,_p); return; }
        if (g_combo_held == SDLK_f) { handle_apply_algorithm(e, 7,"v"); return; }
        if (g_combo_held == SDLK_r) { snprintf(_p,sizeof _p,"-%.1f",g_defaults[8].v[0]); handle_apply_algorithm(e,8,_p); return; }
        if (g_combo_held == SDLK_k) { snprintf(_p,sizeof _p,"%.6g", g_defaults[14].v[1]); handle_apply_algorithm(e,14,_p); return; }
        return;
    }

    if (state->sourceType == SOURCE_NONE) { clear_combo(e); return; }

    /* Combo first-key: stage until key-up; no single-key effect fires. */
    if (sdl_keycode == SDLK_b) { g_combo_held = SDLK_b; return; }
    if (sdl_keycode == SDLK_m) { g_combo_held = SDLK_m; return; }
    if (sdl_keycode == SDLK_g) { g_combo_held = SDLK_g; return; }
    if (sdl_keycode == SDLK_t) { g_combo_held = SDLK_t; return; }
    if (sdl_keycode == SDLK_f) { g_combo_held = SDLK_f; return; }
    if (sdl_keycode == SDLK_r) { g_combo_held = SDLK_r; return; }
    if (sdl_keycode == SDLK_k) { g_combo_held = SDLK_k; return; }

    /* Video/realtime-specific keys that intercept before KEY_EFFECTS */
    if (sdl_keycode == SDLK_TAB) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            state->video.loop = !state->video.loop;
            printf("loop: %s\n", state->video.loop ? "on" : "off");
        }
        return;
    }
    if (sdl_keycode == SDLK_i &&
        (state->sourceType == SOURCE_VIDEO_FILE ||
         state->sourceType == SOURCE_REALTIME_STREAM)) {
        clear_combo(e);
        if (state->sourceType == SOURCE_VIDEO_FILE) {
            printf("pts: %.3fs / %.3fs  speed: %.2fx  loop: %s  playing: %s\n",
                   state->video.current_pts_s, state->video.duration_s,
                   (double)state->video.speed,
                   state->video.loop ? "on" : "off",
                   state->video.playing ? "yes" : "no");
        } else {
            printf("realtime: %dx%d @ %.1ffps  state: %s%s\n",
                   state->realtime.width, state->realtime.height,
                   (double)state->realtime.fps,
                   state->realtime.playing ? "live" : "frozen",
                   state->realtime.disconnected ? " (disconnected)" : "");
        }
        return;
    }
    /* SDLK_i in image mode: falls through to KEY_EFFECTS (hblur) */

    /* Non-combo effect keys */
    clear_combo(e);
    for (i = 0; KEY_EFFECTS[i].key != 0; i++) {
        if (sdl_keycode == KEY_EFFECTS[i].key) {
            handle_apply_algorithm(e, KEY_EFFECTS[i].alg_id, NULL);
            return;
        }
    }
}

static float speed_next(float cur, bool up) {
    static const float s[] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
    int n = 5;
    int i;
    for (i = 0; i < n; i++) {
        if (fabsf(cur - s[i]) < 0.01f) break;
    }
    if (i == n) i = 2;
    if (up) i = (i < n - 1) ? i + 1 : n - 1;
    else    i = (i > 0)     ? i - 1 : 0;
    return s[i];
}

static void render_video_frame(EventHandler *e) {
    AppState *state = e->state;
    size_t bc;
    int k;
    bool used_gpu_stack = false;
    bool has_geom = false;

    if ((state->sourceType != SOURCE_VIDEO_FILE && state->sourceType != SOURCE_REALTIME_STREAM) ||
        state->inImage.data == NULL || state->outImage.data == NULL) {
        return;
    }

    has_geom = state->imgFlipH || state->imgFlipV ||
               state->imgRotateAngle < -0.001f || state->imgRotateAngle > 0.001f;
    bc = (size_t)state->inImage.width * (size_t)state->inImage.height * 3u;
    if (!has_geom && state->effectCount > 0 && e->processor->gpuReady) {
        GpuImage gpu_frame;
        char err[256];
        if (processor_apply_glsl_stack_to_texture(e->processor, &state->inImage,
                                                  state->effectStack, state->effectCount,
                                                  &gpu_frame, err, sizeof err)) {
            renderer_use_external_texture(e->renderer, gpu_frame.textureId);
            renderer_resample(e->renderer, gpu_frame.width, gpu_frame.height);
            used_gpu_stack = true;
        } else {
            fprintf(stderr, "GLSL stack failed, falling back to CPU: %s\n", err);
        }
    }

    if (!used_gpu_stack) {
        state->outImage.width = state->inImage.width;
        state->outImage.height = state->inImage.height;
        memcpy(state->outImage.data, state->inImage.data, bc);
        for (k = 0; k < state->effectCount; k++) {
            (void)processor_apply_cpu_effect(e->processor, &state->outImage,
                                             state->effectStack[k].algorithmId,
                                             &state->effectStack[k].params);
        }
        if (state->imgFlipH || state->imgFlipV) {
            FlipMode fm = (state->imgFlipH && state->imgFlipV) ? FLIP_BOTH :
                          state->imgFlipH ? FLIP_HORIZONTAL : FLIP_VERTICAL;
            (void)processor_flip(e->processor, &state->outImage, fm);
        }
        if (state->imgRotateAngle < -0.001f || state->imgRotateAngle > 0.001f) {
            (void)processor_rotate_expand(e->processor, &state->outImage, state->imgRotateAngle);
        }
        renderer_upload_source_texture(e->renderer, &state->outImage);
        renderer_resample(e->renderer, state->outImage.width, state->outImage.height);
    }
}

static void events_dispatch_window_resize(EventHandler *e, int w, int h) {
    e->renderer->windowW = w;
    e->renderer->windowH = h;
    if (e->state->outImage.data) {
        renderer_resample(e->renderer, e->state->outImage.width, e->state->outImage.height);
    }
}

void events_open_path(EventHandler *e, const char *path) {
    char err[256];
    size_t n = strlen(path);
    bool ok;

    if (n == 0) { puts("usage: o <path>"); return; }

    if (strncmp(path, "rtsp://", 7) == 0 || strncmp(path, "rtsps://", 8) == 0 ||
        strncmp(path, "http://", 7) == 0  || strncmp(path, "/dev/video", 10) == 0) {
        ok = input_open_realtime_stream(e->state, path, err, sizeof err);
        if (!ok) { fprintf(stderr, "load failed: %s\n", err); return; }
        renderer_upload_source_texture(e->renderer, &e->state->outImage);
        renderer_resample(e->renderer, e->state->outImage.width, e->state->outImage.height);
        printf("realtime: %dx%d @ %.1ffps\n",
               e->state->realtime.width, e->state->realtime.height,
               (double)e->state->realtime.fps);
        print_help_realtime();
        return;
    }

    if (input_is_video_path(path)) {
        ok = input_open_video(e->state, path, err, sizeof err);
        if (!ok) { fprintf(stderr, "load failed: %s\n", err); return; }
        renderer_upload_source_texture(e->renderer, &e->state->outImage);
        renderer_resample(e->renderer, e->state->outImage.width, e->state->outImage.height);
        printf("loaded video: %dx%d, duration %.3fs\n",
               e->state->outImage.width, e->state->outImage.height,
               e->state->video.duration_s);
        print_help_video();
        return;
    }

    AppState loaded = {0};
    if (n >= 4 && strcasecmp(path + n - 4, ".ppm") == 0) {
        ok = input_load_ppm(&loaded, path, err, sizeof err);
    } else {
        ok = input_load_image(&loaded, path, err, sizeof err);
    }
    if (!ok) {
        free(loaded.inImage.data);
        free(loaded.outImage.data);
        fprintf(stderr, "load failed: %s\n", err);
        return;
    }

    input_close_source(e->state);
    e->state->inImage = loaded.inImage;
    e->state->outImage = loaded.outImage;
    loaded.inImage = (ImageBuffer){0};
    loaded.outImage = (ImageBuffer){0};
    e->state->sourceType = SOURCE_IMAGE;
    e->state->effectCount = 0;
    processor_reset(e->state);
    renderer_upload_source_texture(e->renderer, &e->state->outImage);
    renderer_resample(e->renderer, e->state->outImage.width, e->state->outImage.height);
    printf("loaded: %dx%d\n", e->state->outImage.width, e->state->outImage.height);
    print_help_image();
}

static void fill_effect_params(int alg_id, const char *param_str, EffectParams *params) {
    bool has = (param_str != NULL && param_str[0] != '\0');
    int i;
    for (i = 0; i < EFFECT_PARAM_SLOTS; i++) params->values[i] = 0.0f;

    switch (alg_id) {
    case 1:  params->values[0] = has ? (float)atof(param_str) : g_defaults[1].v[0]; break;
    case 2:  params->values[0] = has ? (float)atof(param_str) : g_defaults[2].v[0]; break;
    case 3:  params->values[0] = has ? (float)atof(param_str) : g_defaults[3].v[0]; break;
    case 4:  params->values[0] = has ? (float)atof(param_str) : g_defaults[4].v[0]; break;
    case 6:  params->values[0] = has ? (float)strtoul(param_str, NULL, 0) : g_defaults[6].v[0]; break;
    case 7:
        params->values[0] = g_defaults[7].v[0];
        if (has) {
            if (strcasecmp(param_str, "v") == 0 || strcasecmp(param_str, "vertical") == 0)
                params->values[0] = (float)FLIP_VERTICAL;
            else if (strcasecmp(param_str, "both") == 0 || strcasecmp(param_str, "b") == 0)
                params->values[0] = (float)FLIP_BOTH;
            else
                params->values[0] = (float)FLIP_HORIZONTAL;
        }
        break;
    case 8:  params->values[0] = has ? (float)atof(param_str) : g_defaults[8].v[0]; break;
    case 13:
        params->values[0] = g_defaults[13].v[0];
        params->values[1] = g_defaults[13].v[1];
        if (has) sscanf(param_str, "%f %f", &params->values[0], &params->values[1]);
        break;
    case 14: params->values[0] = has ? (float)atof(param_str) : g_defaults[14].v[0]; break;
    case 15: params->values[0] = has ? (float)atof(param_str) : g_defaults[15].v[0]; break;
    case 16: params->values[0] = has ? (float)atof(param_str) : g_defaults[16].v[0]; break;
    case 17: params->values[0] = has ? (float)atof(param_str) : g_defaults[17].v[0]; break;
    case 18: params->values[0] = has ? (float)atof(param_str) : g_defaults[18].v[0]; break;
    case 19: params->values[0] = has ? (float)atof(param_str) : g_defaults[19].v[0]; break;
    case 20: params->values[0] = has ? (float)atof(param_str) : g_defaults[20].v[0]; break;
    case 22:
        params->values[0] = g_defaults[22].v[0];
        params->values[1] = g_defaults[22].v[1];
        params->values[2] = g_defaults[22].v[2];
        if (has) sscanf(param_str, "%f %f %f",
                        &params->values[0], &params->values[1], &params->values[2]);
        break;
    case 24: params->values[0] = has ? (float)atof(param_str) : g_defaults[24].v[0]; break;
    default: break;
    }
}

static void handle_apply_algorithm(EventHandler *e, int alg_id, const char *param_str) {
    AppState *state = e->state;
    bool ok = false;
    EffectParams params = {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};

    if (alg_id < 1 || alg_id > PROCESSOR_ALGORITHM_COUNT) { puts("algorithm out of range"); return; }
    if (state->sourceType == SOURCE_NONE) { puts("no source loaded"); return; }

    if (state->sourceType == SOURCE_VIDEO_FILE ||
        state->sourceType == SOURCE_REALTIME_STREAM) {
        fill_effect_params(alg_id, param_str, &params);
        if (alg_id == 8) {
            float angle = params.values[0];
            if (angle < -360.0f) angle = -360.0f;
            if (angle > 360.0f) angle = 360.0f;
            state->imgRotateAngle += angle;
            if (state->imgRotateAngle < -360.0f) state->imgRotateAngle = -360.0f;
            if (state->imgRotateAngle > 360.0f)  state->imgRotateAngle =  360.0f;
            render_video_frame(e);
            printf("rotation: %.1f deg\n", state->imgRotateAngle);
        } else if (alg_id == 7) {
            int mode = (int)params.values[0];
            if (mode == (int)FLIP_HORIZONTAL || mode == (int)FLIP_BOTH)
                state->imgFlipH = !state->imgFlipH;
            if (mode == (int)FLIP_VERTICAL || mode == (int)FLIP_BOTH)
                state->imgFlipV = !state->imgFlipV;
            render_video_frame(e);
            printf("flip: H=%s V=%s\n",
                   state->imgFlipH ? "on" : "off",
                   state->imgFlipV ? "on" : "off");
        } else {
            if (state->effectCount >= MAX_EFFECT_STACK) { puts("effect stack full (max 3)"); return; }
            state->effectStack[state->effectCount].algorithmId = alg_id;
            state->effectStack[state->effectCount].backend     = BACKEND_CPU_GLSL;
            state->effectStack[state->effectCount].params      = params;
            state->effectStack[state->effectCount].passCount   = 1;
            state->effectCount++;
            render_video_frame(e);
            printf("effect stack [%d/%d]: alg %d (%s)\n",
                   state->effectCount, MAX_EFFECT_STACK,
                   alg_id, processor_algorithm_name(alg_id));
        }
        return;
    }

    /* Image mode: unlimited effects, direct apply; rotate always last via preRotateImage */
    fill_effect_params(alg_id, param_str, &params);

    if (alg_id == 8) {
        float angle;
        size_t bc2;
        if (state->preRotateImage.data == NULL) {
            bc2 = (size_t)state->outImage.width * (size_t)state->outImage.height * 3u;
            state->preRotateImage.data = (uint8_t *)malloc(bc2);
            if (!state->preRotateImage.data) { puts("OOM"); return; }
            state->preRotateImage.width    = state->outImage.width;
            state->preRotateImage.height   = state->outImage.height;
            state->preRotateImage.channels = state->outImage.channels;
            memcpy(state->preRotateImage.data, state->outImage.data, bc2);
        }
        angle = params.values[0];
        if (angle < -360.0f) angle = -360.0f;
        if (angle > 360.0f) angle = 360.0f;
        state->imgRotateAngle += angle;
        if (state->imgRotateAngle < -360.0f) state->imgRotateAngle = -360.0f;
        if (state->imgRotateAngle > 360.0f)  state->imgRotateAngle =  360.0f;
        bc2 = (size_t)state->preRotateImage.width * (size_t)state->preRotateImage.height * 3u;
        free(state->outImage.data);
        state->outImage.data = (uint8_t *)malloc(bc2);
        if (!state->outImage.data) { puts("OOM"); return; }
        state->outImage.width    = state->preRotateImage.width;
        state->outImage.height   = state->preRotateImage.height;
        state->outImage.channels = state->preRotateImage.channels;
        memcpy(state->outImage.data, state->preRotateImage.data, bc2);
        ok = processor_rotate_expand_masked(e->processor, &state->outImage,
                                            &state->geometryMask,
                                            &state->geometryMaskW,
                                            &state->geometryMaskH,
                                            state->imgRotateAngle);
    } else if (state->preRotateImage.data != NULL) {
        /* Rotate already applied: affect preRotateImage then re-apply rotate */
        size_t bc2;
        ok = processor_apply_cpu_effect(e->processor, &state->preRotateImage, alg_id, &params);
        if (ok) {
            bc2 = (size_t)state->preRotateImage.width * (size_t)state->preRotateImage.height * 3u;
            free(state->outImage.data);
            state->outImage.data = (uint8_t *)malloc(bc2);
            if (!state->outImage.data) { puts("OOM"); return; }
            state->outImage.width    = state->preRotateImage.width;
            state->outImage.height   = state->preRotateImage.height;
            state->outImage.channels = state->preRotateImage.channels;
            memcpy(state->outImage.data, state->preRotateImage.data, bc2);
            (void)processor_rotate_expand_masked(e->processor, &state->outImage,
                                                 &state->geometryMask,
                                                 &state->geometryMaskW,
                                                 &state->geometryMaskH,
                                                 state->imgRotateAngle);
        }
    } else {
        /* No rotate: direct apply with GLSL if available */
        if (e->processor->gpuReady && e->processor->effectPrograms[alg_id] != 0) {
            char glsl_err[256];
            ok = processor_apply_glsl_effect(e->processor, state, alg_id, &params,
                                             glsl_err, sizeof glsl_err);
            if (!ok) fprintf(stderr, "GLSL failed, falling back to CPU: %s\n", glsl_err);
        }
        if (!ok)
            ok = processor_apply_cpu_effect(e->processor, &state->outImage, alg_id, &params);
    }

    if (!ok) { puts("algorithm failed (OOM?)"); return; }
    renderer_upload_source_texture(e->renderer, &state->outImage);
    renderer_resample(e->renderer, state->outImage.width, state->outImage.height);
    printf("applied algorithm %d (%s)\n", alg_id, processor_algorithm_name(alg_id));
}

static void handle_save_format(EventHandler *e, const char *fmt_and_path) {
    char fmt[512] = {0};
    const char *custom_path = NULL;
    const char *sp;

    if (e->state->sourceType != SOURCE_IMAGE) {
        puts("save unavailable in this mode");
        return;
    }

    sp = strchr(fmt_and_path, ' ');
    if (sp != NULL) {
        size_t fmtlen = (size_t)(sp - fmt_and_path);
        if (fmtlen >= sizeof(fmt)) fmtlen = sizeof(fmt) - 1;
        memcpy(fmt, fmt_and_path, fmtlen);
        custom_path = sp + 1;
    } else {
        strncpy(fmt, fmt_and_path, sizeof(fmt) - 1);
        fmt[sizeof(fmt) - 1] = '\0';
    }

    if (strcasecmp(fmt, "png") == 0 || strcmp(fmt, "1") == 0) {
        saver_save_png(&e->state->outImage, custom_path, NULL, 0);
    } else if (strcasecmp(fmt, "jpg") == 0 || strcasecmp(fmt, "jpeg") == 0 || strcmp(fmt, "2") == 0) {
        saver_save_jpg(&e->state->outImage, custom_path, NULL, 0);
    } else if (strcasecmp(fmt, "bmp") == 0 || strcmp(fmt, "3") == 0) {
        saver_save_bmp(&e->state->outImage, custom_path, NULL, 0);
    } else if (strcasecmp(fmt, "tga") == 0 || strcmp(fmt, "4") == 0) {
        saver_save_tga(&e->state->outImage, custom_path, NULL, 0);
    } else {
        printf("unknown format: %s  (use png jpg bmp tga)\n", fmt);
    }
}

static void dispatch_line(EventHandler *e, char *buf) {
    size_t n = strlen(buf);

    if (strcmp(buf, "q") == 0 || strcmp(buf, "quit") == 0) {
        e->state->running = false;
        return;
    }
    if (strcmp(buf, "h") == 0 || strcmp(buf, "help") == 0) {
        print_help(e->state);
    } else if (strcmp(buf, "reset") == 0) {
        if (e->state->sourceType == SOURCE_IMAGE) {
            e->state->effectCount = 0;
            processor_reset(e->state);
            renderer_upload_source_texture(e->renderer, &e->state->outImage);
            renderer_resample(e->renderer, e->state->outImage.width, e->state->outImage.height);
            puts("reset");
        } else if (e->state->sourceType == SOURCE_VIDEO_FILE) {
            e->state->video.loop = !e->state->video.loop;
            printf("loop: %s\n", e->state->video.loop ? "on" : "off");
        } else {
            e->state->effectCount = 0;
            puts("reset");
        }
    } else if (strcmp(buf, "p") == 0 || strcmp(buf, "saveppm") == 0 ||
               (n >= 2 && buf[0] == 'p' && buf[1] == ' ') ||
               (n >= 8 && strncmp(buf, "saveppm ", 8) == 0)) {
        const char *custom_path = NULL;
        if (n >= 2 && buf[0] == 'p' && buf[1] == ' ') custom_path = buf + 2;
        else if (n >= 8 && strncmp(buf, "saveppm ", 8) == 0) custom_path = buf + 8;
        switch (e->state->sourceType) {
        case SOURCE_IMAGE:
            saver_save_ppm(&e->state->outImage, custom_path, NULL, 0);
            break;
        case SOURCE_VIDEO_FILE:
            puts("p: save unavailable in video mode");
            break;
        case SOURCE_REALTIME_STREAM:
            input_reconnect_realtime_stream(e->state);
            break;
        default:
            break;
        }
    } else if ((n >= 2 && buf[0] == 'o' && buf[1] == ' ') ||
               (n >= 5 && strncmp(buf, "open ", 5) == 0)) {
        events_open_path(e, buf[0] == 'o' ? buf + 2 : buf + 5);
    } else if (strcmp(buf, "config") == 0 || strcmp(buf, "c") == 0 ||
               strcmp(buf, "defaults") == 0) {
        run_config_mode();
    } else if ((n >= 2 && buf[0] == 's' && buf[1] == ' ') ||
               (n >= 5 && strncmp(buf, "save ", 5) == 0)) {
        handle_save_format(e, buf[0] == 's' ? buf + 2 : buf + 5);
    } else if (strcmp(buf, "space") == 0 || strcmp(buf, "play") == 0) {
        if (e->state->sourceType == SOURCE_VIDEO_FILE) {
            e->state->video.playing = !e->state->video.playing;
            if (e->state->video.playing) input_video_reset_timing();
            printf("%s\n", e->state->video.playing ? "playing" : "paused");
        } else if (e->state->sourceType == SOURCE_REALTIME_STREAM) {
            e->state->realtime.playing = !e->state->realtime.playing;
            printf("%s\n", e->state->realtime.playing ? "live" : "frozen");
        } else {
            puts("video/realtime mode only");
        }
    } else if (strcmp(buf, "l") == 0 || strcmp(buf, "loop") == 0) {
        if (e->state->sourceType == SOURCE_VIDEO_FILE) {
            e->state->video.loop = !e->state->video.loop;
            printf("loop: %s\n", e->state->video.loop ? "on" : "off");
        } else {
            puts("loop: video file mode only");
        }
    } else if (strcmp(buf, "i") == 0 || strcmp(buf, "info") == 0) {
        if (e->state->sourceType == SOURCE_VIDEO_FILE) {
            printf("pts: %.3fs / %.3fs  speed: %.2fx  loop: %s  playing: %s\n",
                   e->state->video.current_pts_s, e->state->video.duration_s,
                   (double)e->state->video.speed,
                   e->state->video.loop ? "on" : "off",
                   e->state->video.playing ? "yes" : "no");
        } else if (e->state->sourceType == SOURCE_REALTIME_STREAM) {
            printf("realtime: %dx%d @ %.1ffps  state: %s%s\n",
                   e->state->realtime.width, e->state->realtime.height,
                   (double)e->state->realtime.fps,
                   e->state->realtime.playing ? "live" : "frozen",
                   e->state->realtime.disconnected ? " (disconnected)" : "");
        } else {
            puts("no source loaded");
        }
    } else if (strcmp(buf, "f") == 0 || strcmp(buf, "step") == 0) {
        if (e->state->sourceType == SOURCE_VIDEO_FILE) {
            if (e->state->video.playing) { puts("pause first (type: space)"); }
            else {
                input_video_step_frame(e->state);
                render_video_frame(e);
                printf("step: %.3fs\n", e->state->video.current_pts_s);
            }
        } else {
            puts("f: video file mode only");
        }
    } else if (strcmp(buf, "clear") == 0) {
        if (e->state->sourceType == SOURCE_VIDEO_FILE || e->state->sourceType == SOURCE_REALTIME_STREAM) {
            e->state->effectCount = 0;
            e->state->imgRotateAngle = 0.0f;
            e->state->imgFlipH = false;
            e->state->imgFlipV = false;
            render_video_frame(e);
            puts("effect stack cleared");
        } else {
            puts("clear: video/realtime mode only");
        }
    } else if (strcmp(buf, "end") == 0 || strcmp(buf, "seekend") == 0) {
        if (e->state->sourceType != SOURCE_VIDEO_FILE) { puts("video mode only"); }
        else { input_video_seek_end(e->state); puts("seek to end"); }
    } else if (strcmp(buf, "seekinit") == 0) {
        if (e->state->sourceType != SOURCE_VIDEO_FILE) { puts("video mode only"); }
        else {
            input_video_seek(e->state, 0.0f);
            e->state->video.playing = false;
            puts("seekinit");
        }
    } else if (strcmp(buf, "<") == 0 || strcmp(buf, "slower") == 0) {
        if (e->state->sourceType != SOURCE_VIDEO_FILE) { puts("video mode only"); }
        else {
            e->state->video.speed = speed_next(e->state->video.speed, false);
            input_video_reset_timing();
            printf("speed: %.2fx\n", (double)e->state->video.speed);
        }
    } else if (strcmp(buf, ">") == 0 || strcmp(buf, "faster") == 0) {
        if (e->state->sourceType != SOURCE_VIDEO_FILE) { puts("video mode only"); }
        else {
            e->state->video.speed = speed_next(e->state->video.speed, true);
            input_video_reset_timing();
            printf("speed: %.2fx\n", (double)e->state->video.speed);
        }
    } else if ((n == 1 && buf[0] >= '0' && buf[0] <= '9') ||
               (n == 6 && strncmp(buf, "seek ", 5) == 0 && buf[5] >= '0' && buf[5] <= '9')) {
        if (e->state->sourceType != SOURCE_VIDEO_FILE) { puts("video mode only"); }
        else {
            char digit = (buf[0] == 's') ? buf[5] : buf[0];
            float frac = (digit == '0') ? 0.0f : (float)(digit - '0') * 0.1f;
            input_video_seek(e->state, frac);
            printf("seek to %.0f%%\n", (double)frac * 100.0);
        }
    } else if (strcmp(buf, "b") == 0 || strcmp(buf, "buffer") == 0) {
        if (e->state->sourceType == SOURCE_REALTIME_STREAM) {
            int cnt, cap;
            uint64_t dropped;
            input_realtime_status(&cnt, &cap, &dropped);
            printf("realtime: buffer %d/%d (dropped %" PRIu64 ")\n", cnt, cap, dropped);
        } else {
            puts("b: realtime mode only");
        }
    } else if (strcmp(buf, "reconnect") == 0) {
        if (e->state->sourceType == SOURCE_REALTIME_STREAM) {
            input_reconnect_realtime_stream(e->state);
        } else {
            puts("reconnect: realtime mode only");
        }
    } else {
        int alg_id;
        const char *rest = NULL;
        alg_id = match_alg_cmd(buf, &rest);
        if (alg_id > 0) {
            handle_apply_algorithm(e, alg_id, rest);
        } else {
            printf("unknown: '%s'  (type h for help)\n", buf);
        }
    }
}

static void events_poll_stdin(EventHandler *e) {
    fd_set fds;
    struct timeval tv = {0, 0};

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return;

    if (g_use_rl) {
        rl_callback_read_char();
        return;
    }

    {
        char buf[512];
        size_t m;
        if (!fgets(buf, sizeof buf, stdin)) {
            e->state->running = false;
            return;
        }
        m = strlen(buf);
        if (m > 0 && buf[m-1] == '\n') buf[--m] = '\0';
        if (m == 0) {
            printf("> ");
            fflush(stdout);
            return;
        }
        dispatch_line(e, buf);
        printf("> ");
        fflush(stdout);
    }
}
