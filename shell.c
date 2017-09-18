#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<pwd.h>
#include<errno.h>
#include<sys/wait.h>
#include<signal.h>

//global constants
#define HOSTNAME_MAX 1024
#define BUFFER_SIZE  2048
#define BUILTIN_SIZE 6
#define PROCESS_MAX 1024

//global delimiter array for parsing input
const char delim[] = " \t\n\a\r";

//structure for storing pid and related info of all background process by our shell
struct bgprocess
{
	pid_t pid;
	char *name;
	int status, flag;							//flag for checking if process is over or not for the program to check
};

//no of background processes, and limit of number of background processes
int process_num = 0, procsize = PROCESS_MAX;

//array for all background process
struct bgprocess *bg;
//initialize the array
void init_bg()
{
	bg = malloc(procsize * sizeof(struct bgprocess));
}

//array of builtin functions keywords
char *BuiltIn[] = {"cd", "pwd", "echo", "clear", "pinfo", "exit"};

//adds background process PID and name
void add_background_process(pid_t pid, char *procname, int status)
{
	bg[process_num].name = malloc(sizeof(char) * PROCESS_MAX);
	bg[process_num].pid = pid;
	strcpy(bg[process_num].name, procname);
	bg[process_num].status = status;
	bg[process_num].flag = 1;
	process_num++;
	if(process_num == procsize)
	{
		procsize += PROCESS_MAX;
		bg = realloc(bg, procsize);

		if(!bg)
		{
			fprintf(stderr, "Memory Allocation Error!\n");
			exit(EXIT_FAILURE);
		}
	}
}

//kill zombie processes
void clean_background_process()
{
	int i;
	for(i = 0; i < process_num; i++)
	{
		if(bg[i].flag == 1)
		{
			if(waitpid(bg[i].pid, &bg[i].status, WNOHANG) > 0)				//check if the bg process stopped
			{
				printf("%s with pid %d exited ", bg[i].name, bg[i].pid);	//give the status that it stopped
				bg[i].flag == 0;											//flag that the process has stopped
				if(WIFEXITED(bg[i].status))
					printf("normally\n");
				else if(WIFSIGNALED(bg[i].status))
					printf("abnormally\n");
			}
		}
	}
}

//function that redirects to different builtin functions
int BuiltInFunc(int index, char **args, char *Path)
{
	if(index == 0)
		return change_dir(args, Path);

	else if(index == 1)
		return present_working_dir(args);

	else if(index == 2)
		return display(args);

	else if(index == 3)
		return clear();

	else if(index == 4)
		return pinfo(args);

	else if(index == 5)
		return 0;

}

//Returns process info of different processes
int pinfo(char **args)
{
	pid_t pid;
	int bufsize = BUFFER_SIZE, linksize;
	char *buf = malloc(sizeof(char) * bufsize);							//buffer for path of proc for process path
	char *path = malloc(sizeof(char) * bufsize);						//final path for info about process path
	char *statpath = malloc(sizeof(char) * bufsize);					//path of proc to get info about process
	char *stat = malloc(sizeof(char) * bufsize);						//buffer to read from the status file of process

	if (!buf || !path || !statpath || !stat)
	{
		fprintf(stderr, "Memory Allocation Error!\n");
		return 1;
	}

	size_t len = 0;
    ssize_t read;

	strcpy(buf, "/proc");
	//if pinfo is called just without any pid
	if(args[1] == NULL)
	{
		pid = getpid();
		strcat(buf, "/self");
	}
	//if a pid is given to check
	else
	{
		pid = atoi(args[1]);
		strcat(buf, "/");
		strcat(buf, args[1]);
	}
	strcpy(statpath, buf);
	strcat(buf, "/exe");
	printf("PID -- %d\n", pid);
	linksize = readlink(buf, path, bufsize);
	if(linksize == -1)
		perror("Error fetching executable path");
	else
	{
		path[linksize] = '\0';
		printf("Executable - %s\n", path);
	}

	strcat(statpath, "/status");
	FILE *fp = fopen(statpath, "r");
	if(fp == NULL)
	{
		fprintf(stderr, "Error Opening proc file\n");
		return 1;
	}
	//read from the proc status file of the process
	while((read = getline(&stat, &len, fp)) != -1)
	{
		char *token;
		token = strtok(stat, ": ");
		if(strcmp(token, "State") == 0)
		{
			token = strtok(NULL, " \t");
			printf("Process Status: %s\n", token);
		}
		else if(strcmp(token, "VmSize") == 0)
		{
			token = strtok(NULL, " \t");
			printf("Memory: %s ", token);
			token = strtok(NULL, " \n");
			printf("%s (Virtual Memory)\n", token);
		}
	}

	free(buf);
	free(path);
	free(statpath);
	free(stat);
	fclose(fp);
	return 1;
}

//clears the screen
int clear()
{
	printf("\033[H\033[J");
	return 1;
}

//function for cd command
int change_dir(char** args, char* Path)
{
	if(args[1] == NULL)
	{
		if(chdir(Path) == -1)
		{
			perror("Error changing directory");
			return 1;
		}
	}

	else if(chdir(args[1]) == -1)
	{
		perror("Error changing directory");
		return 1;
	}
	return 1;
}

//function for pwd command
int present_working_dir(char **args)
{
	char* directory = getcwd(NULL, 0);
	if(directory == NULL)
	{
		perror("Couldn't fetch current directory\n");
		return 1;
	}
	printf("%s\n", directory);
	free(directory);
	return 1;
}

//function for echo command
int display(char **args)
{
	int i = 1;
	while(args[i] != NULL)
		printf("%s ", args[i++]);
	printf("\n");
	return 1;
}

//execute the system functions
int SystemFunc(char** args)
{
	int i = 0, flag = 0;
	while(args[i] != NULL)
	{
		if(strcmp(args[i], "&") == 0)
		{
			args[i] = NULL;
			flag = 1;
		}
		else if(args[i][strlen(args[i]) - 1] == '&')
		{
			args[i][strlen(args[i]) - 1] = '\0';
			flag = 1;
		}
		i++;
	}

	pid_t pid;
	int status;
	pid = fork();
	if(pid < 0)													//child couldn't be spawned
	{
		perror("Child process couldn't be created");
		exit(EXIT_FAILURE);
	}

	else if (pid ==0)											//child goes here
	{
		if (execvp(args[0], args) == -1)
			perror("Couldn't execute command");
		exit(EXIT_FAILURE);
	}
	else if (flag == 0)											//foreground goes here
	{
		waitpid(pid, &status, WUNTRACED);
	}
	else														//background goes here
	{
		add_background_process(pid, args[0], status);
	}
	return 1;
}

//template for shell prompt
void template(char* Path)
{
	char currDirectory[FILENAME_MAX], HostName[HOSTNAME_MAX];				//full current path and hostname
	char *displayDirectory = malloc(sizeof(char) * FILENAME_MAX);			//relative path
	if(displayDirectory == NULL)
	{
		fprintf(stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}
	char *cd = getcwd(currDirectory, sizeof(currDirectory));				//current path
	if(cd == NULL)
	{
		perror("Working Directory Error");
		exit(EXIT_FAILURE);
	}

	int HostNameErr = gethostname(HostName, sizeof(HostName));				//check for hostname
	if(HostNameErr == -1)
	{
		perror("HostName Determination Error");
		exit(EXIT_FAILURE);
	}

	struct passwd *p;														//get username and related info
	p = getpwuid(getuid());
	if(p == NULL)
	{
		perror("UserName Determination Error");
		exit(EXIT_FAILURE);
	}
	char *UserName = p -> pw_name;											//get username from the passwd object

	int i, k=0;
	for(i=0; i < strlen(Path); i++)
		if(Path[i] != cd[i+k])
			break;

	if(i == strlen(Path))
	{
		displayDirectory[k++] = '~';
		for(; i < strlen(currDirectory); i++)
			displayDirectory[k++] = currDirectory[i];
		displayDirectory[k] = '\0';
	}
	else
	{
		strcpy(displayDirectory, currDirectory);
	}

	printf("<%s@%s:%s>", UserName, HostName, displayDirectory);
	free(displayDirectory);
}

//parse the arguments and commands
char** read_arguments(char* input)
{
	int argsize = BUFFER_SIZE, index = 0;
	char **args = malloc(sizeof(char *) * argsize);
	char *ptr;

	if(!args)
	{
		fprintf(stderr, "Memory Allocation Error!\n");
		exit(EXIT_FAILURE);
	}

	ptr = strtok(input, delim);
	while(ptr != NULL)
	{
		args[index] = ptr;
		index++;

		if(index == argsize)
		{
			argsize += BUFFER_SIZE;
			args = realloc(args, sizeof(char *) * argsize);

			if(!args)
			{
				fprintf(stderr, "Memory Allocation Error!\n");
			}
		}
		ptr = strtok(NULL, delim);
	}
	args[index] = NULL;
	return args;
}

//read the input
char* read_line(char* Path)
{
	int size = BUFFER_SIZE, index = 0;
	char ch, *buffer = malloc(sizeof(char) * size);

	if(buffer == NULL)
	{
		fprintf(stderr, "Error Allocating Memory!\n");
		exit(EXIT_FAILURE);
	}

	while(1)
	{
		ch = getchar();
		//if semicolon encountered, execute the parsed command
		if(ch == ';')
		{
			buffer[index] = '\0';
			char **args = read_arguments(buffer);
			int flag = execute(args, Path);
			free(buffer);
			buffer = malloc(sizeof(char) * size);
			index = 0;
		}
		//end of full line of commands
		else if(ch == '\n' || ch == EOF)
		{
			buffer[index] = '\0';
			return buffer;
		}
		else
		{
			buffer[index] = ch;
			index++;
		}
		//increase size of buffer
		if(index == size)
		{
			size += BUFFER_SIZE;
			buffer = realloc(buffer, size);

			if(buffer == NULL)
			{
				fprintf(stderr, "Memory Allocation Error!\n");
				exit(EXIT_FAILURE);
			}
		}
	}

}

//execute the commands with their flags
int execute(char** args, char* Path)
{
	int i;

	if (args[0] == NULL)
		//empty command
    	return 1;

	for(i = 0; i < BUILTIN_SIZE; i++)
		if(strcmp(BuiltIn[i], args[0]) == 0)
			return BuiltInFunc(i, args, Path);

	return SystemFunc(args);
}

//the loop to keep shell running
void shell_loop()
{
	char Path[FILENAME_MAX], *input, **args;
	int flag = 1;
	getcwd(Path, sizeof(Path));
	init_bg();
	do
	{
		clean_background_process();
		template(Path);
		input = read_line(Path);
		args = read_arguments(input);
		flag = execute(args, Path);
		free(input);
		free(args);
	}while(flag);
	
}

//main function
int main(int argc, char **argv)
{
	shell_loop();
	return EXIT_SUCCESS;
}
