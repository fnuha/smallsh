#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

struct command {
    char *command_name;
    char *args[512]; //512 arguments max, 512 char pointers
    int argnum;
    char *inputfile;
    char *outputfile;
    int amp; //0 - no &, run in foreground, 1 - has &, run in bg
};

struct command *parseCommand(char *commToParse) {

    //create struct to store information
    struct command *newCommand = malloc(sizeof(struct command));
    newCommand->inputfile = NULL;
    newCommand->outputfile = NULL;
    newCommand->argnum = 0;
    newCommand->amp = 0;

    char *saveptr;

    char *token = strtok_r(commToParse, " ", &saveptr);
    newCommand->command_name = calloc(strlen(token) + 1, sizeof(char));
    strcpy(newCommand->command_name, token);

    int i = 0;
    strcpy(newCommand->args[i], newCommand->command_name);
    i++;
    token = strtok_r(NULL, " ", &saveptr);

    while (token != NULL) {

        if(strcmp(token, "<") == 0) { //input file
            token = strtok_r(NULL, " ", &saveptr);
            newCommand->inputfile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(newCommand->inputfile, token);
        }

        else if(strcmp(token, ">") == 0) { //output file
            token = strtok_r(NULL, " ", &saveptr);
            newCommand->outputfile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(newCommand->outputfile, token);
        }


        else if(strcmp(token, "&") == 0) { //ampersand
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL) {
                newCommand->amp = 1;
                break;
            }
            else {
                newCommand->args[i] = calloc(strlen("&") + 1, sizeof(char));
                strcpy(newCommand->args[i], "&");
                i++;

                newCommand->args[i] = calloc(strlen(token) + 1, sizeof(char));
                strcpy(newCommand->args[i], token);
                i++;
                newCommand->argnum = i;

            }

        }

        //all others
        else {
            newCommand->args[i] = calloc(strlen(token) + 1, sizeof(char));
            strcpy(newCommand->args[i], token);
            i++;
            newCommand->argnum = i;
        }
        
        token = strtok_r(NULL, " ", &saveptr);

    }

    return newCommand;


}

int main (int argc, char *argv[]) {

    char input[2048];
    memset(input, '\0', 2048);

    while(1) {
        printf(": ");
        fflush(stdout);
        fgets(input, 2048, stdin);

        //code from https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        input[strcspn(input, "\n")] = 0;
    
        if (strlen(input) != 0 && input[0] != '#') {

                while (strstr(input, "$$") != NULL) { //variable expansion

                char buffer[2048];
                char buffer2[2048];
                memset(buffer, '\0', 2048);
                memset(buffer2, '\0', 2048);
                int i;

                int lengthuntil = strcspn(input, "$$");
                for (i = 0; i < lengthuntil; i++) {
                    buffer[i] = input[i];
                }

                lengthuntil = lengthuntil + 2;

                for (i = 0; i < strlen(input); i++) {
                    buffer2[i] = input[i+lengthuntil];
                }

                sprintf(input, "%s%d%s", buffer, getpid(), buffer2);

            }

            struct command *newCommand = parseCommand(input);

            printf("command: %s\narg num: %d\n", newCommand->command_name,newCommand->argnum);
            fflush(stdout);
        
            if (newCommand->argnum >= 0) {
                int i;
                for (i = 0; i < newCommand->argnum; i++) {
                    printf("argument %d: %s\n", i, newCommand->args[i]);
                }
            }
            if (newCommand->inputfile != NULL) {
                printf("input file: %s\n", newCommand->inputfile);

            }
            if (newCommand->outputfile != NULL) {
                printf("output file: %s\n", newCommand->outputfile);

            }
            fflush(stdout);

            if(strcmp(newCommand->command_name, "exit") == 0) {
                //add kill any processes started by shell
                exit(0);
            }

            else if (strcmp(newCommand->command_name, "cd") == 0) {
                //navigate to directory given
                if (newCommand->argnum == 0) {
                    chdir(getenv("HOME"));
                }
                else {
                    chdir(newCommand->args[0]);
                }

            }

            else if(strcmp(newCommand->command_name, "status") == 0) {
                //add kill any processes started by shell
                exit(0);
            }

            else {// execute command
            pid_t spawnpid = -5;
            int* childExitMethod;
            int inputResult, outputResult, targetInput, targetOutput;
            
            spawnpid = fork();
            switch (spawnpid) {
                case -1:
                    perror("Failed to fork.\n");
                    exit(1);
                    break;
                case 0: //child
                    //i/o redirection here
                    if (newCommand->inputfile != NULL) {
                        targetInput = open(newCommand->inputfile, O_RDONLY | O_CREAT);//change these values later
                        if (targetInput == -1) {    perror("Input open failed"); exit(1);}
                        inputResult = dup2(targetInput, 0);
                    }
                    if (newCommand->outputfile != NULL) {
                        targetOutput = open(newCommand->outputfile, O_WRONLY | O_CREAT | O_TRUNC);//change these values later
                        if (targetOutput == -1) {    perror("Output open failed"); exit(1);}
                        outputResult = dup2(targetOutput, 0);
                    }

                    if (newCommand->amp == 1) {
                        targetInput = open("/dev/null", O_RDONLY);//change these values later
                        if (targetInput == -1) {    perror("Input open failed"); exit(1);}
                        inputResult = dup2(targetInput, 0);

                        targetOutput = open("/dev/null", O_WRONLY);//change these values later
                        if (targetOutput == -1) {    perror("Output ooen failed"); exit(1);}
                        outputResult = dup2(targetOutput, 0);
                    }

                    if (execvp(newCommand->command_name, newCommand->args) != 0) {
                        perror("Exec failure");
                        exit(1);
                    }

                    exit(0);
                default:
                    if (newCommand->amp == 1) {
                        printf("background pid is %d\n", spawnpid);
                    }

                    waitpid(spawnpid, &childExitMethod, 0);
                    break;
            }

            }


        }

        

    }

}