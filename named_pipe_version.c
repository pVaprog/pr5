#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#define PIPE_PARENT_TO_CHILD "parent_to_child.pipe"
#define PIPE_CHILD_TO_PARENT "child_to_parent.pipe"

int main(int argc, char *argv[]) {
	// Проверка аргументов
	if (argc != 2) {
		fprintf(stderr, "Использование: %s <N>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	//проверка на корректность
	int N = atoi(argv[1]);
	if (N <= 0) {
		fprintf(stderr, "N должно быть положительным числом\n");
		exit(EXIT_FAILURE);
	}

	// Создаем именованные каналы (FIFO)
	if (mkfifo(PIPE_PARENT_TO_CHILD, 0666) == -1 && errno != EEXIST) {
		perror("Ошибка создания канала parent_to_child");
		exit(EXIT_FAILURE);
	}

	if (mkfifo(PIPE_CHILD_TO_PARENT, 0666) == -1 && errno != EEXIST) {
		perror("Ошибка создания канала child_to_parent");
		unlink(PIPE_PARENT_TO_CHILD);  //Удаляем первый канал при ошибке
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("Ошибка при создании процесса");
		unlink(PIPE_PARENT_TO_CHILD);
		unlink(PIPE_CHILD_TO_PARENT);
		exit(EXIT_FAILURE);
	}

	if (pid == 0) { // Дочерний процесс (угадывающий)
		int fd_read = open(PIPE_PARENT_TO_CHILD, O_RDONLY);
		int fd_write = open(PIPE_CHILD_TO_PARENT, O_WRONLY);
		
		if (fd_read == -1 || fd_write == -1) {
		    perror("Ошибка открытия каналов в потомке");
		    exit(EXIT_FAILURE);
		}

		srand(time(NULL) ^ (getpid() << 16));
		int secret, guess, attempts;
		char buffer[32];
		
		for (int game = 0; game < 5; game++) { // 5 игр в роли угадывающего
		    // Читаем загаданное число от родителя
		    if (read(fd_read, &secret, sizeof(int)) != sizeof(int)) {
		        perror("Ошибка чтения числа");
		        break;
		    }
		    
		    printf("Потомок: Получено число для угадывания (1-%d)\n", N);
		    
		    attempts = 0;
		    bool correct = false;
		    
		    while (!correct && attempts < 100) { // Защита от бесконечного цикла
		        attempts++;
		        guess = rand() % N + 1;
		        printf("Потомок: Попытка %d - предполагаю %d\n", attempts, guess);
		        
		        // Отправляем предположение родителю
		        if (write(fd_write, &guess, sizeof(int)) != sizeof(int)) {
		            perror("Ошибка записи предположения");
		            break;
		        }
		        
		        // Ждем ответ
		        if (read(fd_read, buffer, 32) <= 0) {
		            perror("Ошибка чтения ответа");
		            break;
		        }
		        
		        if (strcmp(buffer, "correct") == 0) {
		            printf("Потомок: Угадал число %d за %d попыток!\n", secret, attempts);
		            correct = true;
		        }
		    }
		}
		
		close(fd_read);
		close(fd_write);
		exit(EXIT_SUCCESS);
	} 
	else { // Родительский процесс (загадывающий)
		int fd_write = open(PIPE_PARENT_TO_CHILD, O_WRONLY);
		int fd_read = open(PIPE_CHILD_TO_PARENT, O_RDONLY);
		
		if (fd_write == -1 || fd_read == -1) {
		    perror("Ошибка открытия каналов в родителе");
		    kill(pid, SIGTERM); // Завершаем потомка
		    wait(NULL);
		    unlink(PIPE_PARENT_TO_CHILD);
		    unlink(PIPE_CHILD_TO_PARENT);
		    exit(EXIT_FAILURE);
		}

		srand(time(NULL));
		int secret, guess;
		char response[32];
		
		for (int game = 0; game < 5; game++) { // 5 игр в роли загадывающего
		    secret = rand() % N + 1;
		    printf("Родитель: Загадал число %d\n", secret);
		    
		    // Отправляем число потомку (но не само число, а только диапазон)
		    if (write(fd_write, &N, sizeof(int)) != sizeof(int)) {
		        perror("Ошибка записи числа");
		        break;
		    }
		    
		    bool correct = false;
		    while (!correct) {
		        // Получаем предположение
		        if (read(fd_read, &guess, sizeof(int)) != sizeof(int)) {
		            perror("Ошибка чтения предположения");
		            break;
		        }
		        
		        printf("Родитель: Получено предположение %d\n", guess);
		        
		        // Проверяем и отправляем ответ
		        if (guess == secret) {
		            strcpy(response, "correct");
		            correct = true;
		        } else {
		            strcpy(response, "wrong");
		        }
		        
		        if (write(fd_write, response, strlen(response) + 1) <= 0) {
		            perror("Ошибка записи ответа");
		            break;
		        }
		    }
		}
		
		close(fd_read);
		close(fd_write);
		wait(NULL); // Ждем завершения потомка
		
		// Удаляем каналы
		unlink(PIPE_PARENT_TO_CHILD);
		unlink(PIPE_CHILD_TO_PARENT);
	}

	return 0;
}
