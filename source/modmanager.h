#pragma once
/* modmanager.h - STAGE 6: gestor de mods por archivo de configuracion.
   Lee /data/GoldHEN/modloader/mods.ini al cargar el plugin.
   Si el juego intenta abrir un pak desactivado, se le responde "no existe". */
#if STAGE >= 6
#define MM_CFG    GOLDHEN_PATH "/modloader/mods.ini"
#define MM_LOG    GOLDHEN_PATH "/modloader/manager.txt"
#define MM_MARK   GOLDHEN_PATH "/modloader/loaded.txt"
#define MM_MAXDIS 32
#define MM_NAMELEN 64
#define MM_RET_ENOENT ((s32)0x80020002u)

static char mm_dis[MM_MAXDIS][MM_NAMELEN];
static s32 mm_ndis = 0;
static s32 mm_hide_startup = 0;
static s32 mm_ready = 0;
static volatile s32 mm_hit[MM_MAXDIS + 1];

static char mm_lc(char c){ return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static s32 mm_len(const char *s){ s32 n = 0; while (s[n]) n++; return n; }
static char *mm_str(char *p, const char *s){ while (*s) *p++ = *s++; return p; }
static char *mm_num(char *p, s32 v){
  char t[12]; s32 n = 0;
  if (v < 0){ *p++ = '-'; v = -v; }
  if (!v) t[n++] = '0';
  while (v > 0 && n < 11){ t[n++] = (char)('0' + v % 10); v /= 10; }
  while (n) *p++ = t[--n];
  return p;
}
static s32 mm_eq(const char *a, const char *b){
  while (*a && *b){ if (mm_lc(*a) != mm_lc(*b)) return 0; a++; b++; }
  return *a == *b;
}
static s32 mm_ends(const char *s, const char *suf){
  s32 a = mm_len(s), b = mm_len(suf);
  if (b > a) return 0;
  for (s32 i = 0; i < b; i++) if (mm_lc(s[a - b + i]) != mm_lc(suf[i])) return 0;
  return 1;
}
static s32 mm_has(const char *h, const char *n){
  s32 hl = mm_len(h), nl = mm_len(n);
  for (s32 i = 0; i + nl <= hl; i++){
    s32 j = 0;
    while (j < nl && mm_lc(h[i + j]) == mm_lc(n[j])) j++;
    if (j == nl) return 1;
  }
  return 0;
}
static const char *mm_base(const char *p){
  const char *b = p;
  for (; *p; p++) if (*p == '/') b = p + 1;
  return b;
}
static s32 mm_key_is(const char *l, s32 kn, const char *k){
  if (mm_len(k) != kn) return 0;
  for (s32 i = 0; i < kn; i++) if (mm_lc(l[i]) != k[i]) return 0;
  return 1;
}
static void mm_write(const char *file, const char *buf, s32 n){
  s32 f = sceKernelOpen(file, O_WRONLY | O_CREAT | O_APPEND, 0777);
  if (f < 0) return;
  sceKernelWrite(f, buf, (size_t)n);
  sceKernelClose(f);
}
#include "modsafe.h"
static void mm_write_default(void){
  static const char d[] =
    "; daysgone_modloader - configuracion de mods\n"
    "; Para desactivar un mod sin borrarlo, quita el ; de una linea disable= y pon el nombre\n"
    "; del pak tal como esta en saved/paks (en minusculas). Ejemplo (desactivado):\n"
    ";disable=chunk1_p.pak\n"
    "; hide_startuppackages: 1 = oculta startuppackages.pak (algunos mods lo necesitan). EXPERIMENTAL.\n"
    "hide_startuppackages=0\n"
    "; safe_mode: 1 = si el juego se cierra 3 veces seguidas antes de 60 segundos, los mods se\n"
    "; desactivan solos. Para volver a activarlos, borra modloader/estado.txt. 0 = sin proteccion.\n"
    "safe_mode=1\n";
  mm_write(MM_CFG, d, (s32)(sizeof(d) - 1));
}
static void mm_parse_line(const char *l, s32 n){
  while (n > 0 && (l[n-1] == '\r' || l[n-1] == ' ' || l[n-1] == '\t')) n--;
  while (n > 0 && (*l == ' ' || *l == '\t')){ l++; n--; }
  if (n <= 0 || *l == ';' || *l == '#') return;
  s32 eq = -1;
  for (s32 i = 0; i < n; i++) if (l[i] == '='){ eq = i; break; }
  if (eq < 1) return;
  s32 kn = eq;
  while (kn > 0 && (l[kn-1] == ' ' || l[kn-1] == '\t')) kn--;
  const char *v = l + eq + 1; s32 vn = n - eq - 1;
  while (vn > 0 && (*v == ' ' || *v == '\t')){ v++; vn--; }
  if (mm_key_is(l, kn, "disable")){
    if (vn > 0 && mm_ndis < MM_MAXDIS){
      s32 i = 0;
      while (i < vn && i < MM_NAMELEN - 1){ mm_dis[mm_ndis][i] = v[i]; i++; }
      mm_dis[mm_ndis][i] = 0;
      mm_ndis++;
    }
  } else if (mm_key_is(l, kn, "hide_startuppackages")){
    mm_hide_startup = (vn > 0 && v[0] == '1') ? 1 : 0;
  } else if (mm_key_is(l, kn, "safe_mode")){
    mm_safe_enabled = (vn > 0 && v[0] == '0') ? 0 : 1;
  }
}
static void mm_load(void){
  static char buf[2048];
  s32 total = 0, created = 0;
  s32 f = sceKernelOpen(MM_CFG, O_RDONLY, 0);
  if (f < 0){
    mm_write_default(); created = 1;
  } else {
    for (;;){
      s64 r = sceKernelRead(f, buf + total, (size_t)(sizeof(buf) - 1 - total));
      if (r <= 0) break;
      total += (s32)r;
      if (total >= (s32)sizeof(buf) - 1) break;
    }
    sceKernelClose(f);
    buf[total] = 0;
    s32 start = 0;
    for (s32 i = 0; i <= total; i++){
      if (i == total || buf[i] == '\n'){ mm_parse_line(buf + start, i - start); start = i + 1; }
    }
  }
  char b[128]; char *p = b;
  p = mm_str(p, created ? "mods.ini creado (plantilla)" : "mods.ini leido");
  p = mm_str(p, ": desactivados="); p = mm_num(p, mm_ndis);
  p = mm_str(p, " hide_startuppackages="); p = mm_num(p, mm_hide_startup);
  *p++ = '\n';
  mm_write(MM_MARK, b, (s32)(p - b));
}
static void mm_log_block(s32 idx, const char *name){
  if (idx < 0 || idx > MM_MAXDIS) return;
  if (__atomic_exchange_n(&mm_hit[idx], 1, __ATOMIC_RELAXED)) return;
  char b[128]; char *p = b;
  p = mm_str(p, "BLOQUEADO "); p = mm_str(p, name); *p++ = '\n';
  mm_write(MM_LOG, b, (s32)(p - b));
}
static s32 mm_veto(const char *path){
  if (!mm_ready || !path || path[0] != '/') return 0;
  if (mm_safe){
    if (mm_ends(path, ".pak") && mm_has(path, "/saved/paks/")){ ms_note(); return 1; }
    return 0;
  }
  if (mm_ndis == 0 && !mm_hide_startup) return 0;
  if (mm_ndis > 0 && mm_ends(path, ".pak") && mm_has(path, "/saved/paks/")){
    const char *b = mm_base(path);
    for (s32 i = 0; i < mm_ndis; i++)
      if (mm_eq(b, mm_dis[i])){ mm_log_block(i, mm_dis[i]); return 1; }
  }
  if (mm_hide_startup && mm_ends(path, "/sfpaks/startuppackages.pak")){
    mm_log_block(MM_MAXDIS, "startuppackages.pak"); return 1;
  }
  return 0;
}
#ifdef RELEASE
static void mm_reset_logs(void){
  static const char *f[2] = { MM_MARK, MM_LOG };
  for (s32 i = 0; i < 2; i++){
    s32 fd = sceKernelOpen(f[i], O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd >= 0) sceKernelClose(fd);
  }
}
#define MM_RESET_LOGS() mm_reset_logs()
#else
#define MM_RESET_LOGS() do {} while (0)
#endif
#define MM_INIT()  do { mm_load(); ms_boot(); mm_ready = 1; } while (0)
#define MM_VETO(p) mm_veto(p)
#else
#define MM_RESET_LOGS() do {} while (0)
#define MM_INIT()  do {} while (0)
#define MM_VETO(p) 0
#define MM_RET_ENOENT 0
#define MS_TICK() do {} while (0)
#define mm_safe 0
#endif
