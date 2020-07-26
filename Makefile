UNAME := $(shell uname -s)

all:
ifeq ($(UNAME), Darwin)
	@gcc -g raspi-finance-ncurses.c -o raspi-finance-ncurses.exe -lncurses -lform -lcurl
endif
ifeq ($(UNAME), Linux)
	@gcc -g raspi-finance-ncurses.c -o raspi-finance-ncurses.exe -lncurses -lform -lcurl -luuid
endif
