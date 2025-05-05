#define _POSIX_C_SOURCE 199309L  //для сигналов реального времени (sigaction, sigqueue)
#include <stdio.h> //Ввод-вывод (printf, perror)
#include <stdlib.h> //Стандартные функции
#include <unistd.h> //Системные вызовы (fork, pause)
#include <signal.h> //Работа с сигналами
#include <time.h> //Генерация случайных чисел (time, srand, rand)
#include <sys/types.h> //Типы (pid_t)
#include <sys/wait.h> //Ожидание процессов (waitpid)

#define MIN_NUM 1
#define MAX_GAMES 10

int max_num = 50;
int secret_number; //загаданное число
pid_t child_pid; //pid дочернего процесса
volatile sig_atomic_t got_guess = 0;  //флаг получения числа с безопасным типом для использования в обработчиках сигналов
volatile sig_atomic_t current_guess = 0;  //текущее предположение

//Обработчик синала SIGRTMIN
void handle_guess(int sig, siginfo_t *info, void *context) {
    (void)sig; (void)context;
    current_guess = info->si_value.sival_int; //Получаем число из сигнала
    got_guess = 1; //Устанавливаем флаг
}

//Логика угадывания
void play_guesser() {
    int attempts = 0;  //Счетчик попыток
    int min = MIN_NUM;
    int max = max_num;
    
    while (1) {
        attempts++;
        int guess = (min + max) / 2;  //Бинарный поиск
        printf("Guesser's attempt #%d: %d\n", attempts, guess);
        
		//Отправка предположений родителю
        union sigval value;
        value.sival_int = guess;
        if (sigqueue(getppid(), SIGRTMIN, value) == -1) {
            perror("sigqueue");
            exit(EXIT_FAILURE);
        }
        
        // Ждем ответа
        pause();
        
        if (current_guess == 0) { //Угадал
            printf("Correct! Needed %d attempts\n", attempts);
            break;
        } else if (current_guess > 0) {
            min = guess + 1;
        } else {
            max = guess - 1;
        }
        
        if (min > max) {
            fprintf(stderr, "Game logic error!\n");
            exit(EXIT_FAILURE);
        }
    }
}

//логика загадывания
void play_thinker() {
    secret_number = rand() % max_num + MIN_NUM;
    printf("Thinker: I'm thinking of a number between %d and %d\n", MIN_NUM, max_num);
    
    while (1) {
        while (!got_guess) pause(); //Ждем предположения
        got_guess = 0;
        
        printf("Thinker: received guess %d\n", current_guess);
        
		//Формирование ответа
        union sigval reply;
        if (current_guess == secret_number) {
            reply.sival_int = 0; // Correct
            printf("Thinker: Correct! The number was %d\n", secret_number);
        } else if (current_guess < secret_number) {
            reply.sival_int = 1; // Higher
        } else {
            reply.sival_int = -1; // Lower
        }
        
		//Отправка ответа
        if (sigqueue(child_pid, SIGRTMIN+1, reply) == -1) {
            perror("sigqueue reply");
            exit(EXIT_FAILURE);
        }
        
        if (reply.sival_int == 0) break; //Игра окончена
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) max_num = atoi(argv[1]);
    
    struct sigaction sa = {
        .sa_sigaction = handle_guess,  //обработчик
        .sa_flags = SA_SIGINFO //для получения данных сигнала
    };
    sigemptyset(&sa.sa_mask);  //Инициализация маски сигналов
    
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    signal(SIGRTMIN+1, SIG_IGN); // Родитель игнорирует ответы
    
    srand(time(NULL));
    
    for (int game = 1; game <= MAX_GAMES; game++) {
        printf("\n=== Game %d/%d ===\n", game, MAX_GAMES);
        
        child_pid = fork();  //Дочерний процесс
        if (child_pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
        if (child_pid == 0) {
            // Дочерний процесс (угадывающий)
            signal(SIGRTMIN, SIG_IGN); //игнор от себя
            
            struct sigaction sa_child = {
                .sa_sigaction = handle_guess,
                .sa_flags = SA_SIGINFO
            };
            sigemptyset(&sa_child.sa_mask); //Настройка обработчика ответов
            
            if (sigaction(SIGRTMIN+1, &sa_child, NULL) == -1) {
                perror("sigaction child");
                exit(EXIT_FAILURE);
            }
            
            play_guesser(); //запуск игры
            exit(EXIT_SUCCESS);
        } else {
            // Родительский процесс (загадывающий)
            play_thinker();
            waitpid(child_pid, NULL, 0); //ожидание завершение потомка
        }
    }
    
    printf("\nAll games completed!\n");
    return 0;
}
