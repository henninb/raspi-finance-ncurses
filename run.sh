#!/bin/sh

mkdir -p ssl
ctags -R *.c
cppcheck --enable=all --suppress=missingIncludeSystem raspi-finance-ncurses.c
make
export CURL_CA_BUNDLE=ssl/hornsup-raspi-finance-cert.pem
./raspi-finance-ncurses.exe

exit 0
