#include <stdio.h>
#include <unistd.h>
#include "parser.h"
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[1;34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

void redirect(char *redirection, int fd, char *mode);
void ejecutarPrimerComando(tline *line, int *pipeSalida, int *pipeEntrada, int i, int *child);
void ejecutarUltimoComando(tline *line, int *pipeSalida, int *pipeEntrada, int i);
void ejecutarComandoIntermedio(tline *line, int *pipeSalida, int *pipeEntrada, int i);
void guardarEnBackGround(int *child, char *buff);
void ejecutar(tline *line, char *buff);
void comandoFG(tline *line);
void comandoJobs();
void showCommandLine();
void manejador(int sig);
void manejadorKiller(int sig);
void comandoCd(tcommand command);
void variarSenyales(tline *line);

typedef struct pdList
{
	int pid;
	char *command;

} pidListType;

pidListType *procesosBack;
int procesosActualesEnBackground = -1;
int totalActual = 1;
int posibleParaTerminar;

int main(void)
{
	signal(SIGCHLD, manejador);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	char buf[1024];
	tline *line;
	int i, j;
	procesosBack = (pidListType *)malloc(totalActual * sizeof(pidListType));
	showCommandLine();
	while (fgets(buf, 1024, stdin))
	{
		line = tokenize(buf);
		if (strcmp(buf, "\n") == 0)
		{
			showCommandLine();
			continue;
		}
		if (strcmp(line->commands[0].argv[0], "fg") == 0)
		{
			comandoFG(line);
			showCommandLine();
			continue;
		}
		if (strcmp(line->commands[0].argv[0], "jobs") == 0)
		{
			comandoJobs(line);
			showCommandLine();
			continue;
		}
		if (strcmp(line->commands[0].argv[0], "cd") == 0)
		{
			comandoCd(line->commands[0]);
			showCommandLine();
			continue;
		}
		if (strcmp(line->commands[0].argv[0], "exit") == 0)
		{
			free(procesosBack);
			exit(0);
		}
		ejecutar(line, buf);
		showCommandLine();
	}
	return 0;
}

void redirect(char *redirection, int fd, char *mode)
{
	FILE *openFile = fopen(redirection, mode);
	if(openFile==NULL){
		fprintf(stderr,ANSI_COLOR_RED"Error: %s\n"ANSI_COLOR_RESET, strerror(errno));
	}
	int intFile = fileno(openFile);
	dup2(intFile, fd);
	close(intFile);
}


void variarSenyales(tline *line)
{
	if (line->background)
	{
		signal(SIGQUIT, SIG_IGN);
		signal(SIGINT, SIG_IGN);
	}
	else
	{
		signal(SIGQUIT, SIG_DFL);
		signal(SIGINT, SIG_DFL);
	}
}

void ejecutarPrimerComando(tline *line, int *pipeSalida, int *pipeEntrada, int i, int *child)
{

	if (line->ncommands == 1)
	{
		close(pipeSalida[0]);
		close(pipeSalida[1]);
		close(pipeEntrada[1]);
		close(pipeEntrada[0]);
	}
	*child = fork();
	if (*child < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Error al crear el hijo\n" ANSI_COLOR_RESET);
		exit(-1);
	}
	if (*child == 0)
	{
		if (line->redirect_input)
		{
			redirect(line->redirect_input, 0, "r");
		}
		if (line->redirect_output && line->ncommands == 1)
		{
			redirect(line->redirect_output, 1, "w");
		}
		if (line->redirect_error && line->ncommands == 1)
		{
			redirect(line->redirect_error, 2, "w");
		}

		variarSenyales(line);

		if (line->ncommands >= 2)
		{
			close(pipeSalida[0]);
			dup2(pipeSalida[1], 1);
			close(pipeSalida[1]);
		}

		execv(line->commands[i].filename, line->commands[i].argv);
		fprintf(stderr, ANSI_COLOR_RED "%s: No se encuentra el mandato \n" ANSI_COLOR_RESET, strerror(errno));
		exit(1);
	}
	if (!line->background || (line->ncommands > 1 && line->background))
	{
		wait(NULL);
	}
	if (line->ncommands > 1)
	{
		close(pipeSalida[1]);
	}
}

void ejecutarUltimoComando(tline *line, int *pipeSalida, int *pipeEntrada, int i)
{
	int child = fork();
	if (child < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Error al crear el hijo\n" ANSI_COLOR_RESET);
		exit(-1);
	}
	if (child == 0)
	{

		if (line->redirect_output)
		{
			redirect(line->redirect_output, 1, "w");
		}
		if (line->redirect_error)
		{
			redirect(line->redirect_error, 2, "w");
		}
		variarSenyales(line);

		dup2(pipeEntrada[0], 0);
		close(pipeEntrada[0]);

		execv(line->commands[i].filename, line->commands[i].argv);
		fprintf(stderr, ANSI_COLOR_RED "%s: No se encuentra el mandato \n" ANSI_COLOR_RESET, strerror(errno));
		exit(1);
	}

	wait(NULL);
	close(pipeEntrada[0]);
	if (line->ncommands == 2)
	{
		close(pipeSalida[1]);
		close(pipeSalida[0]);
	}
}
void ejecutarComandoIntermedio(tline *line, int *pipeSalida, int *pipeEntrada, int i)
{
	int child;
	child = fork();
	if (child < 0)
	{
		fprintf(stderr, ANSI_COLOR_RED "Error al crear el hijo\n" ANSI_COLOR_RESET);
		exit(-1);
	}
	if (child == 0)
	{
		variarSenyales(line);

		dup2(pipeEntrada[0], 0);
		close(pipeEntrada[0]);

		close(pipeSalida[0]);
		dup2(pipeSalida[1], 1);
		close(pipeSalida[1]);

		execv(line->commands[i].filename, line->commands[i].argv);
		fprintf(stderr, ANSI_COLOR_RED "%s: No se encuentra el mandato \n" ANSI_COLOR_RESET, strerror(errno));
		exit(1);
	}

	wait(NULL);
	close(pipeSalida[1]);
	close(pipeEntrada[0]);
	if (i + 2 != line->ncommands)
	{
		pipe(pipeEntrada);
	}
}

void guardarEnBackGround(int *child, char *buff)
{

	printf("[%d] %d \n", procesosActualesEnBackground + 2, *child);
	procesosActualesEnBackground = procesosActualesEnBackground + 1;
	// si se acaba la lista duplicamos el espacio que tiene
	if (procesosActualesEnBackground == totalActual)
	{
		totalActual = totalActual * 2;
		procesosBack = (pidListType *)realloc(procesosBack, totalActual * sizeof(pidListType));
	}
	procesosBack[procesosActualesEnBackground].pid = *child;
	procesosBack[procesosActualesEnBackground].command = (char *)malloc(strlen(buff) * sizeof(char));
	strcpy(procesosBack[procesosActualesEnBackground].command, buff);
}
void ejecutar(tline *line, char *buff)
{
	int child;
	char buffer[1024];
	int pipePrimero[2], pipeSegundo[2];
	pipe(pipeSegundo);
	pipe(pipePrimero);

	for (int i = 0; i < line->ncommands; i++)
	{
		if (i == 0)
		{
			ejecutarPrimerComando(line, pipePrimero, pipeSegundo, i, &child);
			continue;
		}

		if (i % 2 != 0)
		{
			if (i + 1 == line->ncommands)
			{
				ejecutarUltimoComando(line, pipeSegundo, pipePrimero, i);
				continue;
			}
			ejecutarComandoIntermedio(line, pipeSegundo, pipePrimero, i);
		}
		else
		{

			if (i + 1 == line->ncommands)
			{

				ejecutarUltimoComando(line, pipePrimero, pipeSegundo, i);
				continue;
			}
			ejecutarComandoIntermedio(line, pipePrimero, pipeSegundo, i);
		}
	}
	if (line->background)
	{
		guardarEnBackGround(&child, buff);
	}
}
void manejadorKiller(int sig)
{
	kill(posibleParaTerminar, SIGTERM);
	signal(SIGINT, SIG_IGN);
}
void comandoFG(tline *line)
{
	int jobnumber;
	signal(SIGINT, manejadorKiller);
	if (procesosActualesEnBackground != -1)
	{
		if (line->commands[0].argv[1] == NULL)
		{
			posibleParaTerminar = procesosBack[procesosActualesEnBackground].pid;
			waitpid(procesosBack[procesosActualesEnBackground].pid, NULL, 0);
			free(procesosBack[procesosActualesEnBackground].command);
			procesosActualesEnBackground--;
		}
		else
		{
			jobnumber= atoi(line->commands[0].argv[1])-1;
			if (jobnumber<=procesosActualesEnBackground)
				{
					posibleParaTerminar = procesosBack[jobnumber].pid;
					waitpid(procesosBack[jobnumber].pid, NULL, 0);
					free(procesosBack[jobnumber].command);
					for (int k = 0; k < procesosActualesEnBackground; k++)
					{
						if (k >= jobnumber)
						{
							procesosBack[k] = procesosBack[k + 1];
						}
					}
					procesosActualesEnBackground--;
			}
		}
	}
}
void comandoJobs()
{
	if (procesosActualesEnBackground != -1)
	{
		for (int i = 0; i <= procesosActualesEnBackground; i++)
		{
			printf("[%d]+ Running      %d %s", i + 1, procesosBack[i].pid, procesosBack[i].command);
		}
	}
}
void comandoCd(tcommand command)
{
	char *home;
	if (command.argv[1] == NULL)
	{
		home = getenv("HOME");
		if (chdir(home) != 0)
		{
			fprintf(stderr, "Error en la redireccion a HOME!\n");
		}
	}
	else
	{
		if (chdir(command.argv[1]) != 0)
		{
			fprintf(stderr, "Error en la redireccion a %s\n", command.argv[1]);
		}
	}
}
void manejador(int sig)
{
	int pid;
	pid = waitpid(-1, NULL, WNOHANG);
	for (int i = 0; i <= procesosActualesEnBackground; i++)
	{
		if (pid == procesosBack[i].pid)
		{
			printf("[%d]  +%d done     %s\n", i + 1, procesosBack[i].pid, procesosBack[i].command);
			free(procesosBack[i].command);
			for (int j = i; j < procesosActualesEnBackground; i++)
			{
				procesosBack[j] = procesosBack[j + 1];
			}
			procesosActualesEnBackground--;
			break;
		}
	}
}
void showCommandLine()
{
	char currentdir[1024];
	char hostname[1024];
	char user[1024];
	getcwd(currentdir, 1024);
	if (currentdir == NULL)
	{
		strcpy(user, "Directory not avaliable");
	}
	gethostname(hostname, 1024);
	if (*hostname == -1)
	{
		strcpy(user, "Hostname not avaliable");
	}
	strcpy(user, getenv("USER"));
	if (user == NULL)
	{
		strcpy(user, "user");
	}
	printf(ANSI_COLOR_GREEN "%s@%s" ANSI_COLOR_RESET
							":" ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET "$ ",
		   user, hostname, currentdir);
}