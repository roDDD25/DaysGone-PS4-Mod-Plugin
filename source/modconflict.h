#pragma once
/* modconflict.h - aviso de choques de rutas entre paks (STAGE >= 6).
   SOLO LEE Y AVISA: nunca desactiva ni modifica nada.
   Soporta pak v3 (UE4). Cualquier otro caso queda como "NO ANALIZADO", nunca como "sin choques".
   Requiere modmanager.h y las variables ml_names / ml_off / ml_n de modlist.h. */
#if STAGE >= 6
typedef unsigned int mcu32;
typedef unsigned long long mcu64;

#define MC_TABLE    4096
#define MC_MAXPATHS 3000   /* total de rutas entre todos los paks; mas que eso = no analizado */
#define MC_MAXLOG   12     /* lineas CHOQUE como maximo en manager.txt */
#define MC_FOOTER   44
#define MC_MAGIC    0x5A6F12E1u
#define MC_WIN      2048

static mcu32 mc_h[MC_TABLE];
static u8    mc_o[MC_TABLE];
static s32   mc_cnt = 0;
static u8    mc_st[ML_MAXMODS];  /* 0 sin tratar, 1 analizado, 2 no analizado, 3 desactivado */
static s32   mc_nconf = 0, mc_nuna = 0, mc_nok = 0, mc_nlog = 0;
static mcu64 mc_d1 = 0, mc_d2 = 0; /* datos de diagnostico para las lineas NO ANALIZADO */

static s32   mc_fd;
static mcu64 mc_pos, mc_end;
static u8    mc_win[MC_WIN];
static s32   mc_wn, mc_wp, mc_err;

static mcu32 mc_le32(const u8 *b){ return (mcu32)b[0] | ((mcu32)b[1] << 8) | ((mcu32)b[2] << 16) | ((mcu32)b[3] << 24); }
static mcu64 mc_le64(const u8 *b){ return (mcu64)mc_le32(b) | ((mcu64)mc_le32(b + 4) << 32); }

static void mc_get(void *dst, s32 n){
  u8 *d = (u8 *)dst;
  while (n > 0 && !mc_err){
    if (mc_wp >= mc_wn){
      mcu64 left = mc_end - mc_pos;
      if (left == 0){ mc_err = 1; return; }
      s32 want = left < (mcu64)MC_WIN ? (s32)left : MC_WIN;
      s64 r = (s64)sceKernelPread(mc_fd, mc_win, (size_t)want, (off_t)mc_pos);
      if (r <= 0 || r > want){ mc_err = 1; return; }
      mc_pos += (mcu64)r; mc_wn = (s32)r; mc_wp = 0;
    }
    s32 a = mc_wn - mc_wp; if (a > n) a = n;
    for (s32 i = 0; i < a; i++) d[i] = mc_win[mc_wp + i];
    mc_wp += a; d += a; n -= a;
  }
}
static void mc_skip(mcu64 n){
  if (mc_err) return;
  s32 a = mc_wn - mc_wp;
  if (a > 0){ mcu64 t = n < (mcu64)a ? n : (mcu64)a; mc_wp += (s32)t; n -= t; }
  if (n == 0) return;
  if (n > mc_end - mc_pos){ mc_err = 1; return; }
  mc_pos += n;
}
static mcu32 mc_rd32(void){ u8 b[4] = {0, 0, 0, 0}; mc_get(b, 4); return mc_le32(b); }

/* FString (ASCII, longitud con NUL final). Actualiza el hash FNV-1a (minusculas, '\\' -> '/').
   Devuelve nº de caracteres guardados en out, o -1 si hay error (incluye UTF-16). */
static s32 mc_fstr(mcu32 *h, char *out, s32 omax){
  s32 L = (s32)mc_rd32();
  if (mc_err || L < 1 || L > 1024) return -1;
  s32 o = 0;
  for (s32 i = 0; i < L; i++){
    u8 c; mc_get(&c, 1);
    if (mc_err) return -1;
    if (i == L - 1) return c == 0 ? o : -1;
    if (c == 0) return -1;
    char ch = (char)c;
    if (ch == '\\') ch = '/';
    ch = mm_lc(ch);
    *h = (*h ^ (mcu32)(u8)ch) * 16777619u;
    if (o < omax - 1) out[o++] = ch;
  }
  return -1;
}

/* -1 = nueva o propia del mismo pak; >=0 = indice del pak que ya la tenia; -2 = tabla llena */
static s32 mc_insert(mcu32 h, s32 pak){
  if (!h) h = 1;
  mcu32 i = h & (MC_TABLE - 1);
  for (;;){
    if (!mc_h[i]){
      if (mc_cnt >= MC_MAXPATHS) return -2;
      mc_h[i] = h; mc_o[i] = (u8)pak; mc_cnt++; return -1;
    }
    if (mc_h[i] == h) return mc_o[i] == pak ? -1 : (s32)mc_o[i];
    i = (i + 1) & (MC_TABLE - 1);
  }
}

static const char *mc_analyze_fd(s32 fd, s32 k, s32 *np){
  s64 size = (s64)sceKernelLseek(fd, 0, 2);
  mc_d1 = (mcu64)size; mc_d2 = 0;
  if (size < MC_FOOTER + 8) return "archivo muy pequeno";
  u8 f[MC_FOOTER];
  s64 r = (s64)sceKernelPread(fd, f, MC_FOOTER, (off_t)(size - MC_FOOTER));
  if (r != MC_FOOTER) return "error de lectura";
  mcu32 magic = mc_le32(f), ver = mc_le32(f + 4);
  mcu64 off = mc_le64(f + 8), isz = mc_le64(f + 16);
  mc_d1 = ver; mc_d2 = (mcu64)size;
  if (magic != MC_MAGIC) return "no es un pak (firma)";
  if (ver != 3) return "version de pak no soportada";
  mcu64 lim = (mcu64)(size - MC_FOOTER);
  if (isz < 8 || off > lim || isz > lim - off) return "indice fuera de rango";

  mc_fd = fd; mc_pos = off; mc_end = off + isz; mc_wn = 0; mc_wp = 0; mc_err = 0;
  char tmp[8];
  mcu32 hm = 2166136261u;
  if (mc_fstr(&hm, tmp, (s32)sizeof(tmp)) < 0) return "indice ilegible (mount point)";
  s32 n = (s32)mc_rd32();
  if (mc_err) return "indice ilegible";
  if (n < 0 || n > MC_MAXPATHS){ mc_d1 = (mcu64)(mcu32)n; return "num de rutas invalido o demasiado grande"; }

  static char nm[200];
  for (s32 e = 0; e < n; e++){
    mcu32 hh = hm;
    s32 len = mc_fstr(&hh, nm, (s32)sizeof(nm));
    if (len < 0) return "indice ilegible (ruta)";
    nm[len] = 0;
    mc_skip(24);                 /* offset, size, uncompressed size */
    mcu32 comp = mc_rd32();      /* metodo de compresion */
    mc_skip(20);                 /* hash SHA1 */
    if (comp != 0){
      mcu32 nb = mc_rd32();
      if (mc_err || nb > 1048576u) return "indice ilegible (bloques)";
      mc_skip((mcu64)nb * 16);
    }
    mc_skip(5);                  /* flags + tamaño de bloque */
    if (mc_err) return "indice ilegible (registro)";
    s32 o = mc_insert(hh, k);
    if (o == -2) return "demasiadas rutas en total";
    (*np)++;
    if (o >= 0){
      mc_nconf++;
      if (mc_nlog < MC_MAXLOG){
        mc_nlog++;
        char lg[420]; char *q = lg;
        q = mm_str(q, "CHOQUE "); q = mm_str(q, ml_names[o]); q = mm_str(q, " / "); q = mm_str(q, ml_names[k]);
        q = mm_str(q, ": "); q = mm_str(q, nm); *q++ = '\n';
        mm_write(MM_LOG, lg, (s32)(q - lg));
      }
    }
  }
  mcu64 used = mc_pos - (mcu64)(mc_wn - mc_wp) - off;
  if (used != isz){ mc_d1 = used; mc_d2 = isz; return "tamano de indice inesperado (leidos/total)"; }
  return 0;
}

static void mc_run(void){
  for (s32 k = 0; k < ml_n; k++){
    if (ml_off[k]){ mc_st[k] = 3; continue; }
    char path[ML_NAMEMAX + 260]; char *p = path;
    p = mm_str(p, ml_pakdir); *p++ = '/'; p = mm_str(p, ml_names[k]); *p = 0;
    s32 np = 0;
    const char *why;
    mc_d1 = 0; mc_d2 = 0;
    s32 fd = sceKernelOpen(path, O_RDONLY, 0);
    if (fd < 0) why = "no se pudo abrir";
    else { why = mc_analyze_fd(fd, k, &np); sceKernelClose(fd); }
    char lg[200]; char *q = lg;
    q = mm_str(q, "ANALISIS "); q = mm_str(q, ml_names[k]); q = mm_str(q, ": ");
    if (!why){
      mc_st[k] = 1; mc_nok++;
      q = mm_num(q, np); q = mm_str(q, " rutas");
    } else {
      mc_st[k] = 2; mc_nuna++;
      q = mm_str(q, "NO ANALIZADO ("); q = mm_str(q, why);
      q = mm_str(q, ") d1="); q = mm_num(q, (s32)mc_d1); q = mm_str(q, " d2="); q = mm_num(q, (s32)mc_d2);
    }
    *q++ = '\n';
    mm_write(MM_LOG, lg, (s32)(q - lg));
  }
  char lg[220]; char *q = lg;
  q = mm_str(q, "CHOQUES: "); q = mm_num(q, mc_nconf);
  q = mm_str(q, "; NO ANALIZADOS: "); q = mm_num(q, mc_nuna);
  q = mm_str(q, "; ANALIZADOS: "); q = mm_num(q, mc_nok);
  q = mm_str(q, " (choque = misma ruta interna en 2 paks; no implica incompatibilidad ni compatibilidad)\n");
  mm_write(MM_LOG, lg, (s32)(q - lg));
}
#endif
