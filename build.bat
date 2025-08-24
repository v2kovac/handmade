@echo off

mkdir build
pushd build

cl -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -FC -Zi ..\win32_handmade.cpp user32.lib Gdi32.lib

popd



