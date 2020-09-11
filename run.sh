#!/bin/sh

mkdir -p ssl
ctags -R *.c
cppcheck --enable=all raspi-finance-ncurses.c
make

exit 0
