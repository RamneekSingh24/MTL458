#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

char *commandCD = "cd";
char *commandHistory = "history";

char *initialPath;
char *currentDirectory = "";
int behindRoot = 0;

char *prevCommands[5];
int commandCount = 0;
int sPointer = 0;

#define MAX_PATH_SIZE sizeof(char)*PATH_MAX

char* sh_readLine(void);
char** sh_parse(char* line);
char** sh_new_parse(char* line);
int sh_execute(char** args);
int sh_run_default(char** args);
int sh_run_cd(char** args);
int sh_run_history(void);


int is_ancestor(char* currDir, char* initDir) 
{
	if(strlen(currDir) < strlen(initDir) ) return 0;

	if(strcmp(currDir,initDir) == 0) return 2;

	for(int i = 0; i < strlen(initDir); i++) 
	{
		if(initDir[i] != currDir[i]) return 0;
	}

	return 1;
}

void sh_print_dir(void) 
{
	char* currDir = malloc(MAX_PATH_SIZE);
	getcwd(currDir,MAX_PATH_SIZE);


	int f = is_ancestor(currDir, initialPath);
	
	if(f == 2) 
	{
		printf("MTL458:~$ ");
	}
	else if(f == 1) 
	{
		printf("MTL458:~%s$ ", currDir+strlen(initialPath));
	}
	else 
	{	
		printf("MTL458:%s$ ", currDir);
	}

}

void sh_loop(void)
{
	char* line;
	char **args;
	while(1)
	{

		sh_print_dir();

		line = sh_readLine();

		if(line == NULL)
		{	
			continue;
		}

		args = sh_new_parse(line);

		int result = sh_execute(args);

	} 
}

void update_history(char* line) 
{
	if(commandCount < 5) {
		history[commandCount] = malloc(sizeof(char) * (strlen(line) + 1));
		strcpy(history[commandCount], line);
		commandCount++;
	}
	else{
		history[sPointer] = malloc(sizeof(char) * (strlen(line) + 1));
		strcpy(history[sPointer], line);
		sPointer++;
		sPointer %= 5;
	}
}


char* sh_readLine(void) 
{
	char *line = NULL;
	ssize_t bufsize = 0;

	int read = getline(&line, &bufsize, stdin);

	if(read == -1) 
	{
		if(feof(stdin)) {
			exit(0);
		}
		else {
			perror("readline");
			exit(0);
		}
	}

	if(line == NULL) return NULL;

	int onlySpaces = 1;

	int len = strlen(line);

	line[len-1] = ' ';
	
	for(int i = 0; i < (int)strlen(line); i++) 
	{	
		if(line[i] != ' ') onlySpaces = 0;
	}

	if(onlySpaces) return NULL;


	sh_update_history(line);

	return line;

}


char **sh_new_parse(char *line) 
{
	int buffersize = 130;
	int wordsize = 130;

	char **tokens = malloc(buffersize *sizeof(char*));

	int wordnum = 0;
	int letternum = 0;
	int wordStarted = 0;
	int ignoreSpace = 0;	


	for(int i = 0; i <strlen(line); i++) 
	{
		if(line[i] != ' ' || ignoreSpace)
		{	


			if(line[i] == '"') 
			{	

				ignoreSpace ^= 1;

			}


			else if(wordStarted)
			{
				if(letternum == wordsize) 
				{
					wordsize += 64;
					tokens[wordnum] = realloc(tokens[wordnum], wordsize * sizeof(char));
				}

				tokens[wordnum][letternum++] = line[i];
			}

			else
			{	

				wordsize = 64;
				letternum = 0;
				wordStarted = 1;
				tokens[wordnum] = malloc(wordsize * sizeof(char));
				tokens[wordnum][letternum++] = line[i];
			}
		}

		else
		{
			if(wordStarted) 
			{
				tokens[wordnum++][letternum] = NULL;
				ignoreSpace = 0;
				wordStarted = 0;

				if(wordnum == buffersize) 
				{	
					buffersize += 32;
					tokens = realloc(tokens, buffersize * sizeof(char*));
				}

			}
		}
	}	


	tokens[wordnum] = NULL;
	return tokens;

}



int sh_execute(char **args) 
{
	if( args[0] == NULL) 
	{	
		perror("No command");
		return 1;
	}

	else if(strcmp(args[0], commandCD) == 0)
	{
		return sh_run_cd(args);
	}

	else if(strcmp(args[0], commandHistory) == 0) 
	{
		return sh_run_history();
	}

	return sh_run_default(args);
}

int sh_run_default(char **args) 
{
	pid_t pid = fork();

	if(pid < 0) 
	{
		perror("fork error");
		exit(0);
	}
	else if(pid == 0)
	{
		if(execvp(args[0], args) == -1) 
		{	
			printf("%s: command not found\n", args[0]);
		}
		
	}
	else
	{	int status;
		waitpid(pid, &status, 0);
	}
	return 1;
}

char* remove_quotes(char* line) 
{
	int len = strlen(line);

	char *rtn = malloc(sizeof(char) * (len+1));
	int at = 0;
	for(int i = 0; i < len; i++) {
		if(line[i] != '"') rtn[at++] = line[i];
	}
	rtn[at] = NULL;
	return rtn;
}

int sh_run_cd(char **args)  
{
	if(args[1] == NULL) 
	{	

		return 1;
	}

	char *path = remove_quotes(args[1]);

	if(path == NULL) return 1;

	int onlySpaces = 1;
	for(int i = 0; i < strlen(path); i++) {
		if(path[i] != ' ') onlySpaces = 0;
	}

	if(onlySpaces) return 1;


	if(strcmp(path, "~") == 0)
	{	
		int result = chdir(initialPath);
		if(result) perror("Error in changing directory");
		return 1;
	}


	if(chdir(path) != 0)
	{
		perror("Error in changing directory");
		return 1;
	}



	return 1;

}

int sh_run_history(void) 
{

	for(int i = 0; i < commandCount; i++) 
	{
		printf("%d %s\n", i+1, prevCommands[i]);
	}

	return 1;
}





int main(int argc, char *argv[]) 
{	

	
	initialPath = malloc(MAX_PATH_SIZE);
	getcwd(initialPath, MAX_PATH_SIZE);

	sh_loop();


	return 0;
}

