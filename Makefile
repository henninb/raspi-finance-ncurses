UNAME := $(shell uname -s)

all:
ifeq ($(UNAME), Darwin)
	@gcc -Wall -Wextra -Werror -g raspi-finance-ncurses.c -o raspi-finance-ncurses.exe -lncurses -lform -lcurl -lcjson
endif
ifeq ($(UNAME), Linux)
	@gcc -Wall -Wextra -Werror -g raspi-finance-ncurses.c -o raspi-finance-ncurses.exe -lncurses -lform -lcurl -luuid -lcjson
endif
