#pragma once
/* modsafe.h - modo seguro (STAGE >= 6).
   Cada arranque suma 1 al contador de /data/GoldHEN/modloader/estado.txt. Si el juego sigue vivo
   MS_OKSECS segundos despues de cargar el plugin, el contador vuelve a 0.
   Con MS_MAXFAIL arranques seguidos sin llegar a ese tiempo, el plugin pasa a MODO SEGURO:
   le responde "no existe" a todos los paks de saved/paks y no oculta startuppackages.pak.
   Para salir del modo seguro: borrar estado.txt por FTP. Para desactivarlo: safe_mode=0 en mods.ini. */
#if STAGE >= 6
#define MS_STATE   GOLDHEN_PATH "/modloader/estado.txt"
#define MS_MAXFAIL 3
#define MS_OKSECS  60

static s32 mm_safe = 0;
static s32 mm_safe_enabled = 1;
static volatile s32 ms_ok = 0;
static volatile s32 ms_noted = 0;
static unsigned long long ms_t0 = 0;

static void ms_store(s32 n){
  char c = (char)('0' + (n > 9 ? 9 : n));
  s32 f = sceKernelOpen(MS_STATE, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (f < 0) return;
  sceKernelWrite(f, &c, 1);
  sceKernelClose(f);
}
static s32 ms_load(void){
  char c = 0;
  s32 f = sceKernelOpen(MS_STATE, O_RDONLY, 0);
  if (f < 0) return 0;
  s64 r = (s64)sceKernelRead(f, &c, 1);
  sceKernelClose(f);
  if (r != 1 || c < '0' || c > '9') return 0;
  return c - '0';
}
static void ms_logs(const char *a, s32 n){
  char b[100]; char *p = b;
  p = mm_str(p, a); if (n >= 0) p = mm_num(p, n); *p++ = '\n';
  mm_write(MM_LOG, b, (s32)(p - b));
}
/* Llamar despues de mm_load() y antes de activar el veto. */
static void ms_boot(void){
  if (!mm_safe_enabled){ ms_ok = 1; ms_logs("SEGURO: desactivado en mods.ini", -1); return; }
  s32 n = ms_load();
  if (n >= MS_MAXFAIL){
    mm_safe = 1; mm_hide_startup = 0; ms_ok = 1;   /* el contador NO se toca: sigue hasta borrar estado.txt */
    ms_logs("SEGURO: MODO SEGURO ACTIVO, arranques sin completar=", n);
    return;
  }
  ms_t0 = (unsigned long long)sceKernelGetProcessTime();
  ms_store(n + 1);
  ms_logs("SEGURO: arranques previos sin completar=", n);
}
/* Llamar desde un hook que se ejecute a menudo (sceKernelOpen). */
static void ms_tick(void){
  if (ms_ok) return;
  unsigned long long now = (unsigned long long)sceKernelGetProcessTime();
  if (now < ms_t0 || now - ms_t0 < (unsigned long long)MS_OKSECS * 1000000ull) return;
  if (__atomic_exchange_n(&ms_ok, 1, __ATOMIC_RELAXED)) return;
  ms_store(0);
  ms_logs("SEGURO: arranque completado, contador a 0", -1);
}
static void ms_note(void){
  if (__atomic_exchange_n(&ms_noted, 1, __ATOMIC_RELAXED)) return;
  ms_logs("SEGURO: paks de mods bloqueados", -1);
}
#define MS_TICK() ms_tick()
#endif
