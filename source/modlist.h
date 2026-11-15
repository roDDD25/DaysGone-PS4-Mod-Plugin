#pragma once
/* modlist.h - cartel con la lista de mods detectados (STAGE >= 6) */
#if STAGE >= 6
#define ML_AFRDIR  GOLDHEN_PATH "/AFR"
#define ML_PAKSUB  "/bendgame/saved/paks"
#define ML_DEFTID  "CUSA08966" /* solo si no se encuentra ninguna carpeta CUSA valida */
#define ML_MAXMODS 24
#define ML_NAMEMAX 48

static char ml_names[ML_MAXMODS][ML_NAMEMAX];
static u8   ml_off[ML_MAXMODS];
static s32  ml_n = 0;
static char ml_pakdir[256] = GOLDHEN_PATH "/AFR/" ML_DEFTID ML_PAKSUB;
#include "modconflict.h"

static char *ml_lit(char *p, char *e, const char *s){ while (*s && p < e) *p++ = *s++; return p; }
__attribute__((unused)) static char *ml_name(char *p, char *e, const char *s){
  s32 n = mm_len(s);
  if (mm_ends(s, ".pak")) n -= 4;
  for (s32 i = 0; i < n && p < e; i++){
    char c = s[i];
    s32 ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    *p++ = ok ? c : '_';
  }
  return p;
}

/* Busca en /data/GoldHEN/AFR la primera carpeta CUSAnnnnn que tenga bendgame/saved/paks.
   Si no encuentra ninguna, deja la ruta por defecto (CUSA08966). Registra en manager.txt cual uso. */
static void ml_resolve(void){
  static char rb[2048];
  s32 found = 0;
  s32 fd = sceKernelOpen(ML_AFRDIR, 0x20000, 0);
  if (fd >= 0){
    s64 base = 0;
    for (s32 it = 0; it < 4 && !found; it++){
      s32 r = sceKernelGetdirentries(fd, rb, (s32)sizeof(rb), (s32 *)&base);
      if (r <= 0) break;
      s32 o = 0;
      while (o + 8 <= r && !found){
        s32 reclen = (s32)(u8)rb[o + 4] | ((s32)(u8)rb[o + 5] << 8);
        s32 namlen = (s32)(u8)rb[o + 7];
        if (reclen < 8 || o + reclen > r || 8 + namlen >= reclen || rb[o + 8 + namlen] != 0) break;
        const char *nm = rb + o + 8;
        s32 vivo = (rb[o] | rb[o + 1] | rb[o + 2] | rb[o + 3]) != 0;
        s32 ok = vivo && namlen == 9 && nm[0] == 'C' && nm[1] == 'U' && nm[2] == 'S' && nm[3] == 'A';
        for (s32 i = 4; ok && i < 9; i++) if (nm[i] < '0' || nm[i] > '9') ok = 0;
        if (ok){
          char cand[256]; char *c = cand;
          c = mm_str(c, ML_AFRDIR "/"); c = mm_str(c, nm); c = mm_str(c, ML_PAKSUB); *c = 0;
          s32 t = sceKernelOpen(cand, 0x20000, 0);
          if (t >= 0){
            sceKernelClose(t);
            for (s32 i = 0; i < 255 && cand[i]; i++){ ml_pakdir[i] = cand[i]; ml_pakdir[i + 1] = 0; }
            found = 1;
          }
        }
        o += reclen;
      }
    }
    sceKernelClose(fd);
  }
  char lg[400]; char *q = lg;
  q = mm_str(q, found ? "RUTA: " : "RUTA: no se encontro CUSA valida en AFR, uso por defecto: ");
  q = mm_str(q, ml_pakdir); *q++ = '\n';
  mm_write(MM_LOG, lg, (s32)(q - lg));
}

static s32 ml_scan(void){
  static char db[4096];
  ml_resolve();
  s32 fd = sceKernelOpen(ml_pakdir, 0x20000, 0);
  if (fd < 0) return -1;
  s64 base = 0;
  s32 rc = 0;
  for (s32 it = 0; it < 8 && rc == 0; it++){
    s32 r = sceKernelGetdirentries(fd, db, (s32)sizeof(db), (s32 *)&base);
    if (r <= 0) break;
    s32 o = 0;
    while (o + 8 <= r){
      s32 reclen = (s32)(u8)db[o + 4] | ((s32)(u8)db[o + 5] << 8);
      s32 namlen = (s32)(u8)db[o + 7];
      if (reclen < 8 || o + reclen > r || 8 + namlen >= reclen || db[o + 8 + namlen] != 0){ rc = -2; break; }
      const char *nm = db + o + 8;
      s32 vivo = (db[o] | db[o + 1] | db[o + 2] | db[o + 3]) != 0;
      if (vivo && namlen > 0 && nm[0] != '.' && mm_ends(nm, ".pak") && ml_n < ML_MAXMODS){
        s32 i = 0;
        while (i < namlen && i < ML_NAMEMAX - 1){ ml_names[ml_n][i] = nm[i]; i++; }
        ml_names[ml_n][i] = 0;
        ml_off[ml_n] = 0;
        for (s32 k = 0; k < mm_ndis; k++) if (mm_eq(ml_names[ml_n], mm_dis[k])) ml_off[ml_n] = 1;
        ml_n++;
      }
      o += reclen;
    }
  }
  sceKernelClose(fd);
  return rc;
}

static void ml_announce(void){
  if (mm_safe){
    NotifyStatic(TEX_ICON_SYSTEM, "MODO SEGURO: mods desactivados. Borra modloader/estado.txt para reactivar");
    return;
  }
  s32 rc = ml_scan();
  char b[160]; char *p = b; char *e = b + 118;
  if (rc < 0){
    p = ml_lit(p, e, "Mods: lista no disponible");
    char lg[48]; char *q = lg;
    q = mm_str(q, "LISTA: error "); q = mm_num(q, rc); *q++ = '\n';
    mm_write(MM_LOG, lg, (s32)(q - lg));
  } else if (ml_n == 0){
    p = ml_lit(p, e, "Mods: ninguno en saved/paks");
  } else {
    s32 on = 0, off = 0;
    for (s32 i = 0; i < ml_n; i++){ if (ml_off[i]) off++; else on++; }
    p = ml_lit(p, e, "Mods "); p = mm_num(p, on); p = ml_lit(p, e, "/"); p = mm_num(p, ml_n);
    p = ml_lit(p, e, " activos");
    (void)off; /* cartel corto: los nombres solo van a manager.txt */
    mc_run(); /* choques de rutas entre paks: solo avisa */
    if (mc_nconf > 0){
      p = ml_lit(p, e, ". AVISO: "); p = mm_num(p, mc_nconf); p = ml_lit(p, e, " choques");
      if (mc_nuna > 0){ p = ml_lit(p, e, ", "); p = mm_num(p, mc_nuna); p = ml_lit(p, e, " sin analizar"); }
      p = ml_lit(p, e, ". Ver manager.txt");
    } else if (mc_nuna > 0){
      p = ml_lit(p, e, ". "); p = mm_num(p, mc_nuna); p = ml_lit(p, e, " sin analizar. Ver manager.txt");
    }
    char lg[400]; char *q = lg;
    q = mm_str(q, "MODS "); q = mm_num(q, ml_n); q = mm_str(q, ":");
    for (s32 i = 0; i < ml_n; i++){
      if (q + ML_NAMEMAX + 8 > lg + 395) break;
      *q++ = ' '; q = mm_str(q, ml_names[i]); if (ml_off[i]) q = mm_str(q, "[off]");
    }
    *q++ = '\n';
    mm_write(MM_LOG, lg, (s32)(q - lg));
  }
  if (p >= e){ b[115] = '.'; b[116] = '.'; b[117] = '.'; p = b + 118; }
  *p = 0;
  NotifyStatic(TEX_ICON_SYSTEM, b);
}
#define MM_ANNOUNCE() ml_announce()
#else
#define MM_ANNOUNCE() do {} while (0)
#endif
