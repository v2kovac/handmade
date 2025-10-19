@echo off

echo -------
echo -------

set wildcard=*.h *.cpp *.inl *.c
echo STATICS FOUND
findstr -s -n -i -l "static" %wildcard%

echo -------
echo -------

echo GLOBALS FOUND
findstr -s -n -i -l "local_global" %wildcard%
findstr -s -n -i -l "global" %wildcard%

echo -------
echo -------
