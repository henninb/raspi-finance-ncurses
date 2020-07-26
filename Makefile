UNAME := $(shell uname -s)

all:
ifeq ($(UNAME), Darwin)
	@gcc raspi-finance-ncurses.c -o raspi-finance-ncurses.exe -lncurses -lform -lcurl
endif
ifeq ($(UNAME), Linux)
	@gcc raspi-finance-ncurses.c -o raspi-finance-ncurses.exe -lncurses -lform -lcurl -luuid
endif
