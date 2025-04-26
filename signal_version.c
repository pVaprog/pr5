#define _POSIX_C_SOURCE 199309L //Для sigaction и сигналов реального времени

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

// Глобальные переменные для хранения состояния игры
int secret_number;      // Загаданное число
int attempts;          // Количество попыток
int current_guess;     // Текущее предположение
bool game_over;        // Флаг окончания игры
bool is_parent_turn;   // Чья очередь играть (родитель/потомок)
int N;                 // Верхняя граница диапазона чисел

// Обработчик сигнала SIGUSR1 (угадал число)
void handle_guess_correct(int sig) {
	(void)sig; //параметр не используется
	game_over = true;  // Устанавливаем флаг окончания игры
}

// Обработчик сигнала SIGUSR2 (не угадал число)
void handle_guess_wrong(int sig) {
	(void)sig;
	attempts++;       // Увеличиваем счетчик попыток
}

// Обработчик сигнала реального времени (получение предположения)
void handle_realtime_signal(int sig, siginfo_t *info, void *context) {
	(void)sig;
	(void)context;
	current_guess = info->si_value.sival_int;  // Запоминаем полученное предположение
}

// Функция для игры (загадывание и проверка чисел)
void play_game() {
	srand(time(NULL) ^ (getpid() << 16));
	secret_number = rand() % N + 1;
	printf("Процесс %d: Загадал число от 1 до %d...\n", getpid(), N);

	attempts = 1;
	game_over = false;

	// Оповещаем другого игрока о начале игры
	if (is_parent_turn) {
		kill(getpid() + 1, SIGUSR1);
	} else {
		kill(getppid(), SIGUSR1);
	}

	while (!game_over) {
		if (is_parent_turn) {
		    pause(); // Ожидаем сигнал
		    
		    if (!game_over) {
		        if (current_guess == secret_number) {
		            printf("Процесс %d: Верно! Угадано за %d попыток.\n", getpid(), attempts);
		            kill(getpid() + 1, SIGUSR1);
		            game_over = true;
		        } else {
		            printf("Процесс %d: %d - неверно. Пробуйте еще.\n", getpid(), current_guess);
		            kill(getpid() + 1, SIGUSR2);
		        }
		    }
		} else {
		    current_guess = rand() % N + 1;
		    printf("Процесс %d: Мое предположение - %d\n", getpid(), current_guess);
		    
		    union sigval value;
		    value.sival_int = current_guess;
		    if (sigqueue(getppid(), SIGRTMIN, value) == -1) {
		        perror("sigqueue");
		        exit(EXIT_FAILURE);
		    }
		    
		    pause();
		}
	}

	printf("Процесс %d: Игра окончена. Попыток: %d\n", getpid(), attempts);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Использование: %s <N>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	N = atoi(argv[1]);
	if (N <= 0) {
		fprintf(stderr, "N должно быть положительным числом\n");
		exit(EXIT_FAILURE);
	}

	// Настройка обработчиков сигналов
	struct sigaction sa_usr1, sa_usr2, sa_rt;

	// Обработчик для SIGUSR1
	sa_usr1.sa_handler = handle_guess_correct;
	sigemptyset(&sa_usr1.sa_mask);
	sa_usr1.sa_flags = 0;

	// Обработчик для SIGUSR2
	sa_usr2.sa_handler = handle_guess_wrong;
	sigemptyset(&sa_usr2.sa_mask);
	sa_usr2.sa_flags = 0;

	// Обработчик для сигналов реального времени
	sa_rt.sa_sigaction = handle_realtime_signal;
	sigemptyset(&sa_rt.sa_mask);
	sa_rt.sa_flags = SA_SIGINFO;

	// Установка обработчиков
	if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1 ||
		sigaction(SIGUSR2, &sa_usr2, NULL) == -1 ||
		sigaction(SIGRTMIN, &sa_rt, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid == 0) {
		// Дочерний процесс
		is_parent_turn = false;
		signal(SIGUSR2, SIG_IGN);
	} else {
		// Родительский процесс
		is_parent_turn = true;
	}

	// Играем 10 раундов (5 на каждую роль)
	for (int i = 0; i < 10; i++) {
		play_game();
		is_parent_turn = !is_parent_turn;
	}

	if (pid == 0) {
		exit(EXIT_SUCCESS);
	} else {
		wait(NULL);
	}

	return 0;
}
