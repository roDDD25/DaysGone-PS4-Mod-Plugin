# daysgone_modloader (v0.1.0)

Plugin de GoldHEN para PS4 que acompaña a **AFR** para gestionar mods `.pak` de Days Gone: los cuenta, avisa si dos mods tocan la misma ruta interna, permite desactivar uno sin borrarlo y tiene un modo seguro.

**No carga los mods**: eso lo hace AFR. Este plugin solo los gestiona y avisa.

## Probado
Solo en una PS4 Slim, firmware 12.00, con GoldHEN 2.4b18.12, Days Gone `CUSA08966` y 5 mods (datos dados por el autor). No se probó en otras consolas, firmwares, regiones ni versiones del juego. Úsalo bajo tu responsabilidad.

## Requisitos
- GoldHEN con el plugin AFR instalado y funcionando.
- Mods `.pak` con nombre en **minúsculas** y terminado en `_p.pak` (por ejemplo `chunk1_p.pak`), dentro de
  `/data/GoldHEN/AFR/CUSA?????/bendgame/saved/paks/`. El plugin busca la carpeta `CUSA` solo.

## Instalación (por FTP)
1. Copia `daysgone_modloader.prx` a `/data/GoldHEN/plugins/`.
2. En `/data/GoldHEN/plugins.ini` pon AFR primero y este plugin después, con salto de línea al final.
3. Arranca el juego.

## Qué verás
| Cartel | Significado |
|---|---|
| `Mods N/M activos` | M paks encontrados, N activos. Sin avisos. **No significa que sean compatibles**: solo que no se detectaron rutas repetidas. |
| `... AVISO: X choques. Ver manager.txt` | Dos paks contienen la misma ruta interna. No se desactiva nada solo. |
| `... N sin analizar. Ver manager.txt` | Un pak no se pudo analizar (otro formato, dañado, demasiado grande). **No se sabe si choca.** |
| `MODO SEGURO: mods desactivados...` | Ver más abajo. |

Detalle en `/data/GoldHEN/modloader/manager.txt` (se reinicia en cada arranque): ruta usada, rutas por pak, choques y paks bloqueados. `loaded.txt` resume la carga del plugin.

## mods.ini (`/data/GoldHEN/modloader/mods.ini`)
Se crea solo la primera vez. Claves:
- `disable=nombre_p.pak`: desactiva ese mod sin borrarlo (una línea por mod).
- `hide_startuppackages=1`: oculta `startuppackages.pak`. Experimental. Algunos mods de cámara lo necesitan.
- `safe_mode=1` (por defecto) o `0`.

## Modo seguro
Cada arranque suma 1 a `modloader/estado.txt`; si el juego sigue abierto 60 segundos, vuelve a 0. Con 3 arranques seguidos que no llegan a 60 s, los mods se desactivan en el siguiente arranque (el juego corre sin mods). Para volver: **borra `estado.txt`**. Ojo: abrir y cerrar el juego rápido 3 veces también lo activa.

## Si algo sale mal
- El juego se cierra al arrancar: quita `daysgone_modloader.prx` de `/data/GoldHEN/plugins/` (o su linea en `plugins.ini`), o desactiva el mod nuevo con `disable=` en `mods.ini`.
- Ves `MODO SEGURO`: borra `/data/GoldHEN/modloader/estado.txt`.
- Para pedir ayuda, adjunta `manager.txt` y `loaded.txt`.

## Límites conocidos
- El análisis de choques solo entiende pak versión 3; otros formatos aparecen como "sin analizar".
- No sé qué pak "gana" cuando hay un choque.
- Solo sirve para Days Gone (`bendgame`).

Licencia: GPL-3.0 (ver el archivo LICENSE). El codigo fuente esta en la carpeta source. Este proyecto no está afiliado a Sony, Bend Studio ni a los autores de GoldHEN o AFR.

Para recompilar: la carpeta source solo tiene el codigo de este plugin. Se compila con GoldHEN_Plugins_Repository (GPL-3.0, de ahi viene common/plugin_common.c), GoldHEN_Plugins_SDK (MIT) y el toolchain OpenOrbis; build.ps1 muestra el orden de compilacion.
