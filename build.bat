@echo off

mkdir build
pushd build

REM 4201 means something about unamed strucs
REM 4100 is unused func parameters
REM 4189 is unused local variable
REM 4505 unreferenced function
set warning_flags=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505

set env_variables=-DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1

set compiler_flags=-MTd -Od -Oi -FC -Z7 -GR- -EHa- -nologo -Gm-

set linker_flags=-incremental:no -opt:ref

REM 64-bit-windows-11 build

del *pdb > NUL 2> NUL
del *dll > NUL 2> NUL
set game_linker_flags=-EXPORT:game_update_and_render -EXPORT:game_get_sound_samples -PDB:"handmade_%random%.pdb"
cl %warning_flags% %env_variables% %compiler_flags% -Fmhandmade.map -LD ..\handmade.cpp -link %linker_flags% %game_linker_flags%

set win32_linker_flags=user32.lib Gdi32.lib winmm.lib
cl %warning_flags% %env_variables% %compiler_flags% -Fmwin32_handmade.map ..\win32_handmade.cpp -link %linker_flags% %win32_linker_flags%

popd
