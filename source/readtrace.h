#pragma once
/* readtrace.h - STAGE 5: traza de lecturas sobre paks */
#if STAGE >= 5
#define DG5_MAXFD 1024
static volatile s32 dg5_busy = 0;
static volatile u8  dg5_tag[DG5_MAXFD];
static volatile u32 dg5_cnt[DG5_MAXFD];
static volatile u64 dg5_bytes[DG5_MAXFD];
static char dg5_name[DG5_MAXFD][48];

static s32 dg5_len(const char *s){ s32 n = 0; while (s[n]) n++; return n; }
static s32 dg5_has(const char *h, const char *n){
  s32 hl = dg5_len(h), nl = dg5_len(n);
  for (s32 i = 0; i + nl <= hl; i++){ s32 j = 0; while (j < nl && h[i+j] == n[j]) j++; if (j == nl) return 1; }
  return 0;
}
static char *dg5_str(char *p, const char *s){ while (*s) *p++ = *s++; return p; }
static char *dg5_num(char *p, u64 v){
  char t[24]; s32 n = 0;
  if (!v) t[n++] = '0';
  while (v){ t[n++] = (char)('0' + (v % 10)); v /= 10; }
  while (n) *p++ = t[--n];
  return p;
}
static void dg5_write(const char *buf, s32 len){
  s32 f = sceKernelOpen("/data/GoldHEN/modloader/reads.txt", 0x0001 | 0x0200 | 0x0008, 0777);
  if (f >= 0){ sceKernelWrite(f, buf, (u64)len); sceKernelClose(f); }
}
static void dg5_note_open(const char *path, s32 fd){
  if (!path || fd < 0 || fd >= DG5_MAXFD) return;
  u8 t = 0;
  if (dg5_has(path, ".pak")){
    if (dg5_has(path, "/saved/paks/")) t = 1;
    else if (dg5_has(path, "/content/paks/")) t = 2;
  }
  dg5_tag[fd] = 0; dg5_cnt[fd] = 0; dg5_bytes[fd] = 0;
  if (t){
    s32 l = dg5_len(path); const char *s = (l > 47) ? path + l - 47 : path; s32 i = 0;
    while (s[i] && i < 47){ dg5_name[fd][i] = s[i]; i++; }
    dg5_name[fd][i] = 0;
    dg5_tag[fd] = t;
  }
}
static void dg5_line(const char *tag, s32 fd, u64 c, s64 len, s64 off){
  if (__sync_lock_test_and_set(&dg5_busy, 1)) return;
  char b[200]; char *p = b;
  p = dg5_str(p, tag); p = dg5_str(p, " fd="); p = dg5_num(p, (u64)fd);
  p = dg5_str(p, " n="); p = dg5_num(p, c);
  if (len >= 0){ p = dg5_str(p, " len="); p = dg5_num(p, (u64)len); }
  if (off >= 0){ p = dg5_str(p, " off="); p = dg5_num(p, (u64)off); }
  p = dg5_str(p, " tot="); p = dg5_num(p, dg5_bytes[fd]);
  p = dg5_str(p, " "); p = dg5_str(p, dg5_name[fd]); p = dg5_str(p, "\n");
  dg5_write(b, (s32)(p - b));
  __sync_lock_release(&dg5_busy);
}
static void dg5_on_read(s32 fd, s64 r, s64 off){
  if (fd < 0 || fd >= DG5_MAXFD) return;
  u8 t = dg5_tag[fd];
  if (!t || r < 0) return;
  u32 c = __sync_add_and_fetch(&dg5_cnt[fd], 1);
  __sync_fetch_and_add(&dg5_bytes[fd], (u64)r);
  s32 want = (t == 1) ? (c <= 8 || (c % 500) == 0) : (c <= 2);
  if (want) dg5_line(t == 1 ? "R1" : "R2", fd, c, r, off);
}
static void dg5_note_close(s32 fd){
  if (fd < 0 || fd >= DG5_MAXFD) return;
  u8 t = dg5_tag[fd];
  if (t == 1 || (t == 2 && dg5_cnt[fd] > 0)) dg5_line(t == 1 ? "C1" : "C2", fd, dg5_cnt[fd], -1, -1);
  dg5_tag[fd] = 0;
}

HOOK_INIT(sceKernelRead);
s64 sceKernelRead_hook(s32 fd, void *buf, u64 n){
  s64 r = HOOK_CONTINUE(sceKernelRead, s64 (*)(s32, void *, u64), fd, buf, n);
  dg5_on_read(fd, r, -1);
  return r;
}
HOOK_INIT(sceKernelPread);
s64 sceKernelPread_hook(s32 fd, void *buf, u64 n, s64 off){
  s64 r = HOOK_CONTINUE(sceKernelPread, s64 (*)(s32, void *, u64, s64), fd, buf, n, off);
  dg5_on_read(fd, r, off);
  return r;
}
HOOK_INIT(sceKernelClose);
s32 sceKernelClose_hook(s32 fd){
  dg5_note_close(fd);
  return HOOK_CONTINUE(sceKernelClose, s32 (*)(s32), fd);
}
#ifndef DG5_MASK
#define DG5_MASK 1
#endif
#define DG5_HOOK_ALL()   do { if (DG5_MASK & 1) { HOOK32(sceKernelPread); } if (DG5_MASK & 2) { HOOK32(sceKernelRead); } if (DG5_MASK & 4) { HOOK32(sceKernelClose); } } while (0)
#define DG5_UNHOOK_ALL() do { if (DG5_MASK & 1) { UNHOOK(sceKernelPread); } if (DG5_MASK & 2) { UNHOOK(sceKernelRead); } if (DG5_MASK & 4) { UNHOOK(sceKernelClose); } } while (0)
#else
static inline void dg5_note_open(const char *p, s32 f){ (void)p; (void)f; }
#define DG5_HOOK_ALL()   do {} while (0)
#define DG5_UNHOOK_ALL() do {} while (0)
#endif
