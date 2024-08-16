#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAX_INPUT_LENGTH 2048
#define HISTORY_SIZE 2048
#define MAXLIST 100

char history[HISTORY_SIZE][MAX_INPUT_LENGTH];
int history_index = 0;
char prev_directory[MAX_INPUT_LENGTH] = "";

void print_history(int size){
    if(size == -1)
        size = HISTORY_SIZE;
    int start =  0;
    if(history_index > size)
        start = history_index - size;
    if(start<0)
        start = 0;
    for (int i = start; i < history_index; i++) {
        printf(" %s\n",history[i]);
    }
}

void split(char* str, char** parsed, char* separator, int length){
    char *token;
    int i = 0;
    while ((token = strsep(&str, separator)) != NULL && i < length) {
        if (strlen(token) > 0) {
            parsed[i++] = token;
        }
    }
    parsed[i] = NULL;
}

int countPipeCharacters(const char *str) {
    int count = 0;
    while (*str) {
        if (*str == '|')
            count++;
        str++;
    }
    return count;
}

int inbuilt_command_handler(char** parsed){

    if(parsed[0] == NULL)
        return 0;

    if(strcmp(parsed[0], "cd") == 0) {
        char cwd[MAX_INPUT_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            printf("Invalid Command\n");
            return 1;
        }   

        if(parsed[1] == NULL || strcmp(parsed[1], "~") == 0) {
            parsed[1] = getenv("HOME");
        } else if(strcmp(parsed[1], "-") == 0) {
            if (strlen(prev_directory) == 0) {
                printf("cd: OLDPWD not set\n");
                return 1;
            }
            printf("%s\n", prev_directory);
            parsed[1] = prev_directory;
        }

        if (chdir(parsed[1]) != 0) {
            perror("cd error");
        } else {
            strcpy(prev_directory, cwd); // Update the previous directory
        }
        return 1;
    }
    else if(strcmp(parsed[0], "history") == 0) {
        int n = -1;
        if(parsed[1] != NULL) {
            sscanf(parsed[1], "%d", &n);
        }
        print_history(n);
        return 1;
    }
    else if (strcmp(parsed[0], "echo") == 0) {
        int first_arg = 1;
        while (parsed[first_arg] != NULL) {
            char *arg = parsed[first_arg];
            // if (arg[0] == '"' && arg[strlen(arg) - 1] == '"') {
            //     arg[strlen(arg) - 1] = '\0'; // Remove trailing quote
            //     arg++; // Remove leading quote
            // }
            printf("%s", arg);
            if (parsed[first_arg + 1] != NULL) {
                printf(" ");
            }
            first_arg++;
        }
        printf("\n");
        return 1;
    }
    else if(strcmp(parsed[0], "exit") == 0) {
        exit(0);
    }
    return 0;
}

void ExecuteNonpipedCommands(char** command){
    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("Fork error");
        exit(1);
    } 
    else if (child_pid == 0) {
        execvp(command[0], command);
        printf("Invalid Command\n");
        exit(1);
    } 
    else {
        int status;
        waitpid(child_pid, &status, 0);
        return;
    }
}

void ExecutePipedCommands(int n, char* args[n][MAXLIST]){
    pid_t process;
    int READ = 0;
    int WRITE = 1;
    int fd[2];
    int previous = STDIN_FILENO; 

    for (int i=0;i<n-1;i++) {
        pipe(fd);
        pid_t child = fork();
        if (child == 0) {
            if (previous != STDIN_FILENO) {
                dup2(previous, STDIN_FILENO);
                close(previous);
            }

            dup2(fd[WRITE], STDOUT_FILENO);
            close(fd[WRITE]);
            if(!inbuilt_command_handler(args[i])){
                execvp(args[i][0], args[i]);
                printf("Invalid Command\n");
                exit(1);
            }
            exit(1); 
        }
        close(previous);
        close(fd[WRITE]);
        previous = fd[READ];
    }

    process = fork();
    if (process == 0) {
        if (previous != STDIN_FILENO) {
            dup2(previous, STDIN_FILENO);
            close(previous);
        }
        if(!inbuilt_command_handler(args[n-1])){
            execvp(args[n-1][0], args[n-1]);
            printf("Invalid Command\n");
            exit(1);
        }
        exit(1);
    }
    else{
        int status;
        close(fd[READ]);
        close(fd[WRITE]);
        waitpid(process, &status, 0);
    }     
    return;
}

int main(int argc, char const *argv[]) {

  char  *args_nonpiped[MAXLIST];
  int original_stdin = dup(STDIN_FILENO);

  if (getcwd(prev_directory, sizeof(prev_directory)) == NULL) {
        printf("Invalid Command\n");
        exit(1);
    }
  
  
  while(1){
    dup2(original_stdin, STDIN_FILENO);
    char user_input[MAX_INPUT_LENGTH];
    printf("MTL458 > ");
    fflush(stdout); 
    fflush(stdin);

    if(fgets(user_input, MAX_INPUT_LENGTH, stdin)==NULL){
        printf("logout\n");   
        break;
    }

    user_input[strcspn(user_input, "\n")] = '\0';
    if(user_input[0]!='\0'){
        strncpy(history[history_index % HISTORY_SIZE], user_input, MAX_INPUT_LENGTH);
        history_index++;
    }
    
    int num_pipes = countPipeCharacters(user_input);
    char* strpiped[num_pipes+1];

    char * pipe = "|";
    char * space = " ";
    
    split(user_input, strpiped, pipe ,num_pipes+1);
    if(num_pipes==0){
        split(user_input,args_nonpiped, space,MAXLIST);
        if (inbuilt_command_handler(args_nonpiped)){
            continue;
        }   
        else{
            ExecuteNonpipedCommands(args_nonpiped);
        }
    }
    else{
        char *args [num_pipes+1][MAXLIST];
        for(int i = 0;i<num_pipes+1;i++)
            split(strpiped[i],args[i],space,MAXLIST);
        ExecutePipedCommands(num_pipes+1, args);
    }
  }
  return 0;
}