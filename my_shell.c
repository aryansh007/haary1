#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

int numberOfArgs = 0;
int pipe_exists = 0;
int numberOfPipes = -1;

char **make_pipe(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for(i =0; i < strlen(line); i++)
	{
		char readChar = line[i];

		if (readChar == '|'|| readChar == '\n')
		{
			numberOfPipes++;
			token[tokenIndex] = '\0';
			if (tokenIndex != 0)
			{
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} 
		else 
		{
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL ;
	return tokens;
}

char **tokenize(char *line)
{
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i, tokenIndex = 0, tokenNo = 0;

	for(i =0; i < strlen(line); i++)
	{
		char readChar = line[i];
		if(readChar == '|')
		{
			pipe_exists = 1;
		}

		if (readChar == ' ' || readChar == '\n' || readChar == '\t')
		{
			token[tokenIndex] = '\0';
			if (tokenIndex != 0)
			{
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} 
		else 
		{
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL ;
	numberOfArgs = tokenNo;
	return tokens;
}


int main(int argc, char* argv[]) 
{
	char  line[MAX_INPUT_SIZE];
	char **tokens;
	int i;

	FILE* fp;
	if(argc == 2) 
	{
		fp = fopen(argv[1],"r");
		if(fp < 0) 
		{
			printf("File doesn't exists.");
			return -1;
		}
	}

	while(1) 
    {			
		/* BEGIN: TAKING INPUT */
		memset(line, 0, sizeof(line));
		if(argc == 2) 
		{ // batch mode
			if(fgets(line, sizeof(line), fp) == NULL) 
			{ // file reading finished
				break;	
			}
			line[strlen(line) - 1] = '\0';
		} 
		else 
		{ // interactive mode
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
		}

		line[strlen(line)] = '\n'; //terminate with new line
		tokens = tokenize(line);
		
		//pipe implementation
		if(pipe_exists == 1)
		{
			char **pipe_cmd = make_pipe(line);
			int *pipes = (int *)malloc(2*numberOfPipes*sizeof(int));
			int i,j,stat;
			for(i=0;i<numberOfPipes;i++)
			{
				pipe(pipes + 2*i);
			}
			for(i=0;i<=numberOfPipes;i++)
			{
                pid_t pid = fork();
                if(pid<0)
                {
                    printf("Cannot be forked");
					continue;
                }
				if(pid==0)
				{
					//replace read(from stdin) of ith command to the read from previous pipe(i-1) 
					if(i!=0) dup2(pipes[2*(i-1)],0);
					//replace write(to stdout) of ith command to its write
					if(i!=numberOfPipes) dup2(pipes[2*i + 1], 1);

					for(j=0;j<numberOfPipes;j++)
					{
						close(pipes[2*j]);
						close(pipes[2*j + 1]);
					}

					pipe_cmd[i][strlen(pipe_cmd[i])] = '\n';
					tokens = tokenize(pipe_cmd[i]);

					execvp(tokens[0], tokens);
					printf("Invalid command");
				}
			}
			
			for(j=0;j<numberOfPipes;j++)
			{
				close(pipes[2*j]);
				close(pipes[2*j + 1]);
			}
			
			for(i=0;i<=numberOfPipes;i++)
				wait(&stat);
			
			for(i=0;pipe_cmd[i]!=NULL;i++)
			{
				free(pipe_cmd[i]);
			}
			free(pipes);
			pipe_exists = 0;
			numberOfPipes = -1;
		}
		else
		{
			tokens = tokenize(line);
			int background = 0;
			if(strcmp(tokens[numberOfArgs-1],"&") == 0)
			{
				background = 1;
				tokens[numberOfArgs-1] = '\0';
			}
			if(strcmp(tokens[0],"exit")==0)
			{
				exit(0);
			}
			else if(strcmp(tokens[0],"cd")==0)
			{
				chdir(tokens[1]);
			}
			else if(strcmp(tokens[0],"kill")==0)
			{
				int stat;
				kill((pid_t)atoi(tokens[1]),SIGUSR1);
				wait(&stat);
			}
			else
			{
				pid_t child_pid = fork();
				if(child_pid<0)
				{
					printf("Cannot be forked");
					continue;
				}
				if(child_pid==0)
				{
					execvp(tokens[0],tokens);
					printf("Invalid command\n");
				}
				else
				{ 
					if(background!=1)waitpid(child_pid, NULL, 0);
                    else signal(SIGCHLD, signal_handler);
				}
			}
		}
        
        for(i=0;tokens[i]!=NULL;i++)
		{
			free(tokens[i]);
		}
		free(tokens);

	}
	return 0;
}