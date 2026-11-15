// daysgone_modloader - DIAGNOSTICO POR ETAPAS (solo lectura, no modifica nada)
//
// Se elige la etapa al compilar con -DSTAGE=1, 2, 3 o 4 (por defecto 1).
//
//  STAGE 1: solo carga y avisa en pantalla. Sin hook, sin archivos de registro.
//  STAGE 2: STAGE 1 + hook de sceKernelGetdirentries que solo llama al original.
//  STAGE 3: STAGE 2 + registro en dirlog.txt de listados que contienen "chunk".
//  STAGE 4: STAGE 3 + registro de las primeras 12 llamadas a getdirentries (sea cual
//           sea su contenido) + traza en trace.txt de los sceKernelOpen cuya ruta
//           contiene ".pak", "Paks" o "paks" (rutas unicas, maximo 96).
//
// Todo el registro usa formato manual (sin libc) y un solo hilo a la vez.

#include "Common.h"
#include "plugin_common.h"
#include <fcntl.h>
#include "readtrace.h"
#include "modmanager.h"
#include "modlist.h"

#ifndef STAGE
#define STAGE 1
#endif

attr_public const char *g_pluginName = "daysgone_modloader";
attr_public const char *g_pluginDesc = "Days Gone mod loader (diagnostico por etapas)";
attr_public const char *g_pluginAuth = "community";
#ifdef RELEASE
attr_public u32 g_pluginVersion = 0x00000010; // 0.1.0
#else
attr_public u32 g_pluginVersion = 0x00000103; // 1.03
#endif

#if STAGE >= 2 && !defined(RELEASE)
HOOK_INIT(sceKernelGetdirentries);
static volatile int g_calls = 0;
#endif
#if STAGE >= 4
HOOK_INIT(sceKernelOpen);
#endif

#define XSTR(x) #x
#define STR(x) XSTR(x)
#define ML_DIR      GOLDHEN_PATH "/modloader"
#define ML_LOG      ML_DIR "/dirlog.txt"
#define ML_MARK     ML_DIR "/loaded.txt"
#define ML_TRACE    ML_DIR "/trace.txt"
#define MAX_HITS    24
#define MAX_TRACE   96
#define DUMP_BYTES  160
// Banderas de fcntl.h del toolchain: con ellas la consola SI creo archivos (loaded.txt).
#define ML_O_WRONLY O_WRONLY
#define ML_O_APPEND O_APPEND
#define ML_O_CREAT  O_CREAT

static volatile int g_hits = 0;
static volatile int g_lock = 0;
static char g_buf[1400];

static int put_s(char *d, int p, const char *s) { while (*s) d[p++] = *s++; return p; }
static int put_d(char *d, int p, int v)
{
    char t[12]; int n = 0;
    if (v < 0) { d[p++] = '-'; v = -v; }
    if (v == 0) t[n++] = '0';
    while (v > 0 && n < 11) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0) d[p++] = t[--n];
    return p;
}
static int put_hex(char *d, int p, unsigned char b)
{
    static const char h[] = "0123456789abcdef";
    d[p++] = h[b >> 4]; d[p++] = h[b & 15]; d[p++] = ' ';
    return p;
}
static int put_x(char *d, int p, unsigned int v)
{
    static const char h[] = "0123456789abcdef";
    int started = 0;
    d[p++] = '0'; d[p++] = 'x';
    for (int s = 28; s >= 0; s -= 4) {
        unsigned int n = (v >> s) & 15u;
        if (n || started || s == 0) { d[p++] = h[n]; started = 1; }
    }
    return p;
}

static void write_file(const char *file, const char *data, int n)
{
    s32 fd = sceKernelOpen(file, ML_O_WRONLY | ML_O_CREAT | ML_O_APPEND, 0777);
    if (fd < 0) return;
    sceKernelWrite(fd, data, (size_t)n);
    sceKernelClose(fd);
}

// Marca de diagnostico: deja constancia en loaded.txt de que una parte del plugin se ejecuto.
static void ml_mark(const char *msg)
{
    char m[96]; int p = 0;
    while (msg[p] && p < 95) { m[p] = msg[p]; p++; }
    write_file(ML_MARK, m, p);
}

#if STAGE >= 3
static int contains_chunk(const char *b, s32 len)
{
    for (s32 i = 0; i + 5 <= len; i++)
        if (b[i] == 'c' && b[i+1] == 'h' && b[i+2] == 'u' && b[i+3] == 'n' && b[i+4] == 'k')
            return 1;
    return 0;
}
#endif

#if STAGE >= 4
static volatile int g_trace = 0;
static unsigned int g_seen[MAX_TRACE];
static int g_seen_n = 0;

static int has_str(const char *s, const char *needle)
{
    for (int i = 0; s[i] && i < 300; i++) {
        int j = 0;
        while (needle[j] && s[i + j] && s[i + j] == needle[j]) j++;
        if (!needle[j]) return 1;
    }
    return 0;
}
static int path_interesting(const char *path)
{
    if (!path) return 0;
    return has_str(path, ".pak") || has_str(path, "Paks") || has_str(path, "paks");
}
static unsigned int fnv(const char *s)
{
    unsigned int h = 2166136261u;
    for (int i = 0; s[i] && i < 300; i++) { h ^= (unsigned char)s[i]; h *= 16777619u; }
    return h;
}

// Registra una ruta unica en trace.txt (si otro hilo esta registrando, simplemente se omite).
static void trace_open(const char *path, s32 flags, s32 ret)
{
    if (g_trace >= MAX_TRACE) return;
    if (__atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE) != 0) return;
    unsigned int h = fnv(path);
    int dup = 0;
    for (int i = 0; i < g_seen_n; i++) if (g_seen[i] == h) { dup = 1; break; }
    if (!dup && g_seen_n < MAX_TRACE) {
        g_seen[g_seen_n++] = h;
        int p = 0;
        p = put_s(g_buf, p, "OPEN #"); p = put_d(g_buf, p, ++g_trace);
        p = put_s(g_buf, p, " flags="); p = put_x(g_buf, p, (unsigned int)flags);
        p = put_s(g_buf, p, " ret=");   p = put_d(g_buf, p, ret);
        p = put_s(g_buf, p, " path=");
        for (int i = 0; path[i] && i < 300; i++) g_buf[p++] = path[i];
        g_buf[p++] = '\n';
        write_file(ML_TRACE, g_buf, p);
    }
    __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE);
}

s32 sceKernelOpen_hook(const char *path, s32 flags, OrbisKernelMode mode)
{
    MS_TICK();
    if (MM_VETO(path)) return MM_RET_ENOENT;
    s32 fd = HOOK_CONTINUE(sceKernelOpen,
                           s32 (*)(const char *, s32, OrbisKernelMode),
                           path, flags, mode); 
#ifndef RELEASE
    dg5_note_open(path, fd);
#endif
    #ifndef RELEASE
    if (path_interesting(path)) trace_open(path, flags, fd);
#endif
    return fd;
}
#endif

#if STAGE >= 2 && !defined(RELEASE)
s32 sceKernelGetdirentries_hook(s32 fd, char *buf, s32 nbytes, s64 *basep)
{
    s32 ret = HOOK_CONTINUE(sceKernelGetdirentries,
                            s32 (*)(s32, char *, s32, s64 *),
                            fd, buf, nbytes, basep);
    int c = __atomic_fetch_add(&g_calls, 1, __ATOMIC_RELAXED) + 1;

#if STAGE >= 3
    int wanted = 0;
    if (ret > 0 && buf && g_hits < MAX_HITS) {
        wanted = contains_chunk(buf, ret);
#if STAGE >= 4
        if (c <= 12) wanted = 1;
#endif
    }
    if (wanted)
    {
        // Un solo hilo a la vez; los demas simplemente no registran.
        if (__atomic_exchange_n(&g_lock, 1, __ATOMIC_ACQUIRE) == 0)
        {
            int hit = ++g_hits;
            int p = 0;
            p = put_s(g_buf, p, "HIT ");   p = put_d(g_buf, p, hit);
            p = put_s(g_buf, p, " call="); p = put_d(g_buf, p, c);
            p = put_s(g_buf, p, " fd=");   p = put_d(g_buf, p, fd);
            p = put_s(g_buf, p, " ret=");  p = put_d(g_buf, p, ret);
            p = put_s(g_buf, p, " nbytes="); p = put_d(g_buf, p, nbytes);
            p = put_s(g_buf, p, "\nHEX: ");
            s32 hn = ret < DUMP_BYTES ? ret : DUMP_BYTES;
            for (s32 i = 0; i < hn; i++) p = put_hex(g_buf, p, (unsigned char)buf[i]);
            p = put_s(g_buf, p, "\nASCII: ");
            for (s32 i = 0; i < hn; i++)
                g_buf[p++] = (buf[i] >= 32 && buf[i] < 127) ? buf[i] : '.';
            p = put_s(g_buf, p, "\n\n");
            write_file(ML_LOG, g_buf, p);
            __atomic_store_n(&g_lock, 0, __ATOMIC_RELEASE);
        }
    }
#else
    (void)c;
#endif
    return ret;
}
#endif

s32 attr_public plugin_load(s32 argc, const char *argv[])
{
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s STAGE=%d\n", g_pluginName, g_pluginVersion, __func__, STAGE);

    sceKernelMkdir(ML_DIR, 0777);
    ml_mark("plugin_load llamado, STAGE=" STR(STAGE) "\n");

#if STAGE >= 2 && !defined(RELEASE)
    HOOK32(sceKernelGetdirentries);
    ml_mark("hook getdirentries instalado\n");
#endif
#if STAGE >= 4
    MM_INIT(); HOOK32(sceKernelOpen); DG5_HOOK_ALL();
    ml_mark("hook open instalado\n");
#endif

#if STAGE == 1
    NotifyStatic(TEX_ICON_SYSTEM, "daysgone_modloader v1.03: STAGE 1 cargado");
#elif STAGE == 2
    NotifyStatic(TEX_ICON_SYSTEM, "daysgone_modloader v1.03: STAGE 2 hook instalado");
#elif STAGE == 3
    NotifyStatic(TEX_ICON_SYSTEM, "daysgone_modloader v1.03: STAGE 3 registro activo");
#else
    #ifdef RELEASE
    NotifyStatic(TEX_ICON_SYSTEM, "daysgone_modloader v0.1.0: gestor de mods activo");
#else
    NotifyStatic(TEX_ICON_SYSTEM, "daysgone_modloader v1.03: STAGE 4 traza activa");
#endif
#endif
    MM_ANNOUNCE();
    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char *argv[])
{
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
#if STAGE >= 4
    UNHOOK(sceKernelOpen); DG5_UNHOOK_ALL();
#endif
#if STAGE >= 2 && !defined(RELEASE)
    UNHOOK(sceKernelGetdirentries);
#endif
    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    sceKernelMkdir(ML_DIR, 0777);
    MM_RESET_LOGS();
    ml_mark("module_start llamado\n");
    return 0;
}

s32 attr_module_hidden module_stop(s64 argc, const void *args)
{
    return 0;
}