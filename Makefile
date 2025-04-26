# Компилятор
CC = gcc
# Флаги компиляции
CFLAGS = -Wall -Wextra -std=gnu11
# Цели по умолчанию
all: signal_prog pipe_prog

# Сборка версии с сигналами
signal_prog: signal_version.c
	$(CC) $(CFLAGS) -o signal_prog signal_version.c

# Сборка версии с именованными каналами
pipe_prog: named_pipe_version.c
	$(CC) $(CFLAGS) -o pipe_prog named_pipe_version.c

# Очистка
clean:
	rm -f signal_prog pipe_prog *.pipe
