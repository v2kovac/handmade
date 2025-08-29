@echo off

mkdir build
pushd build

:: 4201 means something about unamed strucs
:: 4100 is unused func parameters
:: 4189 is unused local variable

cl ^
    -WX ^
    -W4 ^
    -wd4201 ^
    -wd4100 ^
    -wd4189 ^
    -DHANDMADE_SLOW=1 ^
    -DHANDMADE_INTERNAL=1 ^
    -DHANDMADE_WIN32=1 ^
    -Od ^
    -Oi ^
    -FC ^
    -Zi ^
    -GR- ^
    -EHa- ^
    -nologo ^
    -Gm- ^
    -Fmwin32_handmade.map ^
    ..\win32_handmade.cpp user32.lib Gdi32.lib winmm.lib

popd
