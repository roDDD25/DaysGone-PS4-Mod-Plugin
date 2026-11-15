param([int]$stage = 5, [int]$mask = 0, [int]$release = 0)
$repo = "C:\ps4dev\GoldHEN_Plugins_Repository"
$sdk  = "C:\ps4dev\GoldHEN_Plugins_SDK"
$tc   = $env:OO_PS4_TOOLCHAIN
if (-not $tc) { $tc = (Get-ChildItem C:\ps4dev\OpenOrbis -Recurse -Filter link.x | Select-Object -First 1).DirectoryName }
$name = "daysgone_modloader"
Set-Location $repo
$bd = "$repo\plugin_src\$name\build"
Remove-Item $bd -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $bd | Out-Null
$defs = @("-D_BSD_SOURCE=1","-D__BSD_VISIBLE=1","-D__PS4__=1","-DOO=1","-D__OPENORBIS__=1","-D__OOPS4__=1","-D__FINAL__=1","-DSTAGE=$stage","-DDG5_MASK=$mask")
if ($release) { $defs += "-DRELEASE=1" }
$cflags = @("--target=x86_64-pc-freebsd12-elf","-fPIC","-funwind-tables","-isysroot",$tc,"-isystem","$tc\include","-I$sdk\include","-Wno-c99-designator") + $defs
& clang @cflags "-I$repo\common" -c "$repo\common\plugin_common.c" -o "$bd\plugin_common.c.o"; if ($LASTEXITCODE) { throw "fallo plugin_common.c" }
& clang @cflags "-I$repo\common" "-I$repo\plugin_src\$name\source" -c "$repo\plugin_src\$name\source\main.c" -o "$bd\main.c.o"; if ($LASTEXITCODE) { throw "fallo main.c" }
$libs = @("-lSceLibcInternal","-lGoldHEN_Hook","-lkernel","-lSceSysmodule")
& ld.lld -m elf_x86_64 -pie -e _init --script "$tc\link.x" --eh-frame-hdr "-L$tc\lib" "-L$sdk" @libs "$bd\plugin_common.c.o" "$bd\main.c.o" -o "$bd\$name.elf"; if ($LASTEXITCODE) { throw "fallo el enlazado" }
& "$tc\bin\windows\create-fself.exe" "-in=$bd\$name.elf" "-out=$bd\$name.oelf" "-lib=$bd\$name.prx" --paid 0x3800000000000011; if ($LASTEXITCODE) { throw "fallo create-fself" }
Copy-Item "$bd\$name.prx" "$repo\bin\plugins\" -Force
Get-Item "$repo\bin\plugins\$name.prx" | Select-Object FullName, Length, LastWriteTime
