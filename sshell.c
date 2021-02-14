#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <dirent.h>

#define CMDLINE_MAX 512
#define MAX_ARGU 16
#define MAX_TOKNUM 32

/* Object to store command line and its properties */
struct command {
        char **cmd_arr;
        int num_cmd;
        bool redir;
        bool pipe;
        bool append;
        int *statuses;
	int statuses_num;
        struct command* next;
};

/* Links a command object to the end of the head */
struct command* add_command(struct command *head, struct command *cmd) {
        struct command *current = (struct command*)malloc(sizeof(struct command));

	/* Go to the last node and add cmd as the next node */
        current = head;
        while(current->next) {
                current = current->next;
        }
        current->next = cmd;
        cmd->next = NULL;

        return head;
}

/* Breaks each individual command by the spaces and puts into command object */
bool parse_command(struct command *c, char command[]) {
        char *comd = malloc(CMDLINE_MAX * sizeof(char*));
        strcpy(comd, command);
        int len = strlen(comd);
        char *index;
        c->cmd_arr = malloc(len * sizeof(char *));
        int i=0;
        bool success = true;
        index = strtok(comd, " ");

	/* Breaks strings up by spaces */
        while(index != NULL) {
                c->cmd_arr[i++] = index;
                index = strtok(NULL, " ");
        }

	if(i>MAX_ARGU) {
		fprintf(stderr, "Error: too many process arguments\n");
		success = false;
		return success;
	}

        c->cmd_arr[i++] = NULL;
	return success;
}

/* Checks if there are commands after the last meta character */
bool contains_char(char tok[]) {
	bool flag = true;

	/* Iterate through the string check if all are whitespace */
	for(unsigned int i=1; i<strlen(tok); i++) {
		if(!isspace(tok[i]))
			flag = false;
	}
	return flag;
}

/* Checks to for any errors in the command input by user */
bool parse_meta(char command[]) {
        char comd[CMDLINE_MAX], cmd_copy[CMDLINE_MAX], tok_copy[CMDLINE_MAX];
        char *tok1, *tok2;
	bool status = false;
	bool com_flag=1, out_flag=1;

        strcpy(comd, command);

	/* Checks for any '>' before '|' */
        tok1 = strchr(comd, '>');
        tok2 = strchr(comd, '|');
        if((tok1 && tok2) && ((tok1-comd+1) < (tok2-comd+1))) {
                fprintf(stderr, "Error: mislocated output redirection\n");
                return(status);
        }

	/* Checks for any characters before the meta chars */
	strcpy(cmd_copy, comd);
	tok1 = strtok(cmd_copy, " ");
	strcpy(tok_copy, tok1);
	if(tok_copy[0] == '|' || tok_copy[0] == '>') {
                fprintf(stderr, "Error: missing command\n");
                return(status);
        }

	/* Checks for any characters after '|' */
	memset(&cmd_copy[0], 0, sizeof(cmd_copy));
	strcpy(cmd_copy, comd);
	tok2 = strrchr(comd, '|');
	if(tok2) {
		memset(&cmd_copy[0], 0, sizeof(cmd_copy));
		strcpy(cmd_copy, tok2);
		com_flag = contains_char(cmd_copy);
		if(com_flag) {
			fprintf(stderr, "Error: missing command\n");
			return(status);
		}
	}

	memset(&cmd_copy[0], 0, sizeof(cmd_copy));
	strcpy(cmd_copy, comd);

	/* Checks for any characters after '>' */
	tok1 = strrchr(cmd_copy, '>');
	if(tok1) {
		strcpy(cmd_copy, tok1);
		out_flag = contains_char(cmd_copy);
		if(out_flag) {
			fprintf(stderr, "Error: no output file\n");
			return(status);
		}
	}

	status = true;
	return status;
}

/* Breaks the strings up by the meta character and stores in array of strings */
bool parse(struct command *c, char command[]) {
	char comd[CMDLINE_MAX];
	char **comd_arr = malloc(CMDLINE_MAX * sizeof(char*));	
	char *tok1;
	bool status;
	c->cmd_arr = NULL;
	
	/* Checks to see if any piping ends with redirect or append */
	strcpy(comd, command);
        if(strstr(comd, "|")) {
                c->pipe = true;
                if(strstr(comd, ">>")) {
                        c->append = true;
                } else if(strstr(comd, ">")) {
                        c->redir = true;
                }
        } else if(strstr(comd, ">>")) {
                c->append = true;
	} else if(strstr(comd, ">")) {
		c->redir = true;
	}

	/* Break command line up by '|' and '' */
	status = true;
        tok1 = strtok(comd, ">|");
	c->num_cmd = 0;
        while(tok1) {
		comd_arr[c->num_cmd] = malloc(MAX_TOKNUM * sizeof(char));
                comd_arr[c->num_cmd] = tok1;
		c->num_cmd++;
                tok1 = strtok(NULL, ">|");
        }

	/* Parse the first command into its struct */
        struct command *next_comd = NULL;
        status = parse_command(c, comd_arr[0]);
        c->next = NULL;
	
	/* If first node is initilized, continue adding nodes to the end of the head */
	if(status) {
        	for(int i=1; i<c->num_cmd; i++) {
                	next_comd = (struct command*)malloc(sizeof(struct command));
                	status = parse_command(next_comd, comd_arr[i]);
                	c = add_command(c, next_comd);
		} 
        } else {
		return false;
	}	
	

	/* Allocate shared memory for both parent and child to store process's sucess */
	if(c->append || c->redir) {
		c->statuses = mmap(NULL, (c->num_cmd-1)*sizeof(int), PROT_READ|PROT_WRITE, 
			       	MAP_SHARED|MAP_ANONYMOUS, -1, 0);
		c->statuses_num = c->num_cmd-1;
	} else {
		c->statuses = mmap(NULL, (c->num_cmd)*sizeof(int), PROT_READ|PROT_WRITE,
                                MAP_SHARED|MAP_ANONYMOUS, -1, 0);
		c->statuses_num = c->num_cmd;
	}
	return status;
}

/* Executes normal commands that doesn't include meta characters */
bool execute(struct command *cmd) {
	int status;
        pid_t pid;

	/* Child executes command, parent waits */
        pid = fork();
        if(pid == 0) {
                execvp(cmd->cmd_arr[0], cmd->cmd_arr);
                fprintf(stderr,"Error: command not found\n");
		exit(1);
        } else if(pid > 0) {
                waitpid(pid, &status, 0);
		cmd->statuses[0] = WEXITSTATUS(status);
        } else {
		perror("fork failed\n");
		exit(1);
	}
	/* Function exits successfully */
	return false;
}

/* Piping function that can include one redirection at the end */
bool pipe_comd(struct command *head) {
	int i=0;
	int status;
	struct command *previous = (struct command*)malloc(sizeof(struct command));
	struct command *current = (struct command*)malloc(sizeof(struct command));
	pid_t pid_child, pid_parent;
	int pip[2];
	current = head;

	pid_parent = fork();
	if(pid_parent==0) {
        	while(current->next) {
                	pipe(pip);
               		pid_child = fork();
			/* Child connects stdout to write end and executes cmd */
               		if(pid_child==0) {
				close(pip[0]);
                       		dup2(pip[1], STDOUT_FILENO);
                       		close(pip[1]);
                       		execvp(current->cmd_arr[0], current->cmd_arr);
				fprintf(stderr, "Error: command not found\n");
				previous = current;
				current = current->next;
				exit(1);
			/* Parent waits for child, stores status, and goes to next node */
			/* Parent connects stdout to read pipe */
               		} else if(pid_child > 0) {
				waitpid(pid_child, &status, 0);
				head->statuses[i] = WEXITSTATUS(status);
				close(pip[1]);
               			dup2(pip[0], STDIN_FILENO);
               			close(pip[0]);
				previous = current;
               			current = current->next;
				i++;
			/* If piping fails, exit */
			} else {
				perror("fork failed\n");
				exit(1);
			}
       		}

		/* At the last node, if there is redirect or append, open file desc. */
		int fd;
		if(head->redir) {
			fd = open(current->cmd_arr[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
		} else if (current->append) {
			fd = open(current->cmd_arr[0], O_WRONLY | O_CREAT | O_APPEND, 0644);
		}

		if( (head->redir || head->append) && fd<0) {
			fprintf(stderr, "Error: cannot open output file\n");
			exit(1);
		}

		/* If redir or append, connect stdout to file desc. */
		/*   If pipe, execute last command */

		if(head->redir || head->append) {
			dup2(fd, STDOUT_FILENO);
			close(fd);
			execvp(previous->cmd_arr[0], previous->cmd_arr);
		} else {
			execvp(current->cmd_arr[0], current->cmd_arr);
			fprintf(stderr, "Error: command not found\n");
			exit(1);
		}
	/* Parent waits for child to execute the whole command and saves status */
	} else if (pid_parent > 0) {
		waitpid(pid_parent, &status, 0);
		head->statuses[head->statuses_num-1] = WEXITSTATUS(status);
	/* Exit if fork fails */
	} else {
		perror("fork failed\n");
		exit(1);
	}
	/* Returning false means functions exits successfully */
		return false;
}

/* Redirect or appending function to execute command and send output to file */
bool redir(struct command *comnd) {
	int fd, status;

	struct command *curr_cmd = (struct command*)malloc(sizeof(struct command));
	struct command *next_cmd = (struct command*)malloc(sizeof(struct command));
	curr_cmd = comnd;
	next_cmd = comnd->next;

	struct stat sb;
	stat(next_cmd->cmd_arr[0], &sb);

	/* Check if redirection or append and open file accordingly */
	if(comnd->redir) {
		fd = open(next_cmd->cmd_arr[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	} else if(comnd->append) {
		fd = open(next_cmd->cmd_arr[0], O_WRONLY | O_CREAT | O_APPEND, 0644);
	}

	/* If cannot open file or it's a directory, error */
        if( ((sb.st_mode & S_IFMT) == S_IFDIR) || fd<0 ) {
                        fprintf(stderr, "Error: cannot open output file\n");
			comnd->statuses[0] = 1;
			return false;
        }

	/* Child connects file desc with stdout and parent waits */
	pid_t pid;
	pid = fork();
        if(pid == 0) {
		dup2(fd, STDOUT_FILENO);
		close(fd);
                execvp(curr_cmd->cmd_arr[0], curr_cmd->cmd_arr);
        } else if(pid > 0) {
                waitpid(pid, &status, 0);
		comnd->statuses[0] = WEXITSTATUS(status);
	/* Exit if forks fails */
        } else {
		perror("fork failed\n");
		exit(1);
	}
	return true;
}

/* Changes directory into the second node */
bool changedir(struct command *comd) {
        struct stat stats;
        stat(comd->cmd_arr[1], &stats);

	/* If it's a directory change to it and return */
        if (S_ISDIR(stats.st_mode)) {
                chdir(comd->cmd_arr[1]);
		comd->statuses[0] = 0;
        } else {
		fprintf(stderr, "Error: cannot cd into directory\n");
		comd->statuses[0] = 1;
	}

	return true;
}

bool pwd(struct command *comd) {
	char cwd[CMDLINE_MAX];
	getcwd(cwd, sizeof(cwd));
	
	printf("%s\n", cwd);
	comd->statuses[0] = 0;

	return true;
}

/* Prints current directory's contents and its size */
bool sls(struct command *cmd) {
        DIR *dirp;
        struct dirent *dp;
        struct stat sb;

	/* Open directory, list all contents not beginning with "." */
	/* and its bytes */
        dirp = opendir(".");
        while ((dp = readdir(dirp)) != NULL) {
                stat(dp->d_name, &sb);
		if(dp->d_name[0] != '.')
			printf("%s (%ld bytes)\n", dp->d_name, sb.st_size);
        }

        closedir(dirp);

        cmd->statuses[0] = 0;
	return true;
}

/* Print completion message and the status of each command success status */
void print_status(char cmd[], struct command *comd) {
	fprintf(stderr, "+ completed '%s' ", cmd);
	for(int i=0; i<comd->statuses_num; i++) {
		fprintf(stderr, "[%d]", comd->statuses[i]);
	}
	fprintf(stderr, "\n");
}

/* Gets command and passes the line into the function accordingly */
int main(void)
{
        char cmd[CMDLINE_MAX];
	int status = 0;

        while (1) {
                char *nl;
		bool parse_success;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

		if(!strcmp(cmd, "\n"))
			continue;

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl) 
                        *nl = '\0';

                /* Builtin command */
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n+ completed '%s' [%d]\n", cmd, status);
                        break;
                }

                /* Regular command */
		struct command *c = (struct command*)malloc(sizeof(struct command));
		parse_success = parse_meta(cmd);
		if(parse_success)
			parse_success = parse(c, cmd);

		if (parse_success && (c->pipe && c->cmd_arr)) {
			parse_success = !pipe_comd(c);
		} else if (parse_success && ((c->append||c->redir) && c->cmd_arr)) {
			parse_success = redir(c);
		} else if (parse_success && (!strcmp(c->cmd_arr[0], "cd"))) {
			parse_success = changedir(c);
		} else if (parse_success && (!strcmp(c->cmd_arr[0], "pwd"))) {
			parse_success = pwd(c);
		} else if (parse_success && (!strcmp(c->cmd_arr[0], "sls"))) {
			parse_success = sls(c);
		} else if (parse_success && c->cmd_arr) {
                	parse_success = !execute(c);
		}
		

		if(parse_success && c->cmd_arr) {
			print_status(cmd,c);
		}

        }

        return status;
}
