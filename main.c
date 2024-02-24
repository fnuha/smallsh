#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>

struct command { //command struct to easily store and access a parsed command
    char *command_name; //name for passing into exec function
    char *args[512]; //512 arguments max, 512 char pointers
    int argnum; //how many arguments, good for checking if there are any arguments
    char *inputfile; //storing these separate to the argument
    char *outputfile;
    int amp; //0 - no &, run in foreground, 1 - has &, run in bg
};

int foregroundMode = 0; //0 for no foreground mode, 1 for foreground mode

void catchSIGTSTP (int signo) { //TSTP signal handler function, used for toggling foreground mode
    if (foregroundMode == 0) { //if foreground mode is off...
        char* msg1 = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, msg1, 50);
        foregroundMode = 1; //turn it on
    }
    else { //else, if it's on...
        char* msg2 = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, msg2, 30);
        foregroundMode = 0; //turn it off
    }


}

struct command *parseCommand(char *commToParse) { //taking string and making command struct

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
    newCommand->args[i] = calloc(strlen(newCommand->command_name) + 1, sizeof(char));
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

    //make sigaction struct(s)
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    //set first value to SIG_IGN
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;

    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    
    char input[2048];
    memset(input, '\0', 2048);
    int lastStatus = 0; // need the data (exit status which is an int or terminating signal) 
    //boolean for exit status vs terminating signal
    int sigStatus = 0; //0 for terminating normally, 1 for signal termination
    int backgroundPIDs[512] = { -5 };
    int f = 0;
    for (f = 0; f < 512; f++) {
            backgroundPIDs[f] = -5;
        }

    while(1) {

        int j = 0;
        
        int childPID_actual = 0;
        int* childExitMethod;
        
        for (j = 0; j < 512; j++) {
            if (backgroundPIDs[j] != -5) {
                childPID_actual = waitpid(backgroundPIDs[j],&childExitMethod, WNOHANG);
                if (childPID_actual != 0) {
                    if (WIFEXITED(childExitMethod) != 0) {
                        printf("background pid %d is done: exited with status %d\n", backgroundPIDs[j],WEXITSTATUS(childExitMethod));

                    }
                    else {
                        printf("background pid %d is done: terminated by signal %d\n", backgroundPIDs[j],WTERMSIG(childExitMethod));
                    }
                    backgroundPIDs[j] = -5;
                }
                //waitpid WNOHANG
                fflush(stdout);
            }
        }

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
            
            if (foregroundMode == 1) {
                newCommand->amp = 0;
            }

            if(strcmp(newCommand->command_name, "exit") == 0) {
                //kill any processes started by shell
                int i = 0;
                for (i = 0; i < 512; i++) {
                    if (backgroundPIDs[i] != -5) {
                        kill(backgroundPIDs[i], SIGKILL);
                    }
                }
                exit(0);
            }

            else if (strcmp(newCommand->command_name, "cd") == 0) {
                //navigate to directory given
                if (newCommand->argnum == 0) {
                    chdir(getenv("HOME"));
                }
                else {
                    chdir(newCommand->args[1]);
                }

            }

            else if(strcmp(newCommand->command_name, "status") == 0) {
                if (sigStatus == 0) {
                    printf("terminated with status %d\n", lastStatus);
                }
                else {
                    printf("terminated by signal %d\n", lastStatus);
                }
            }

            else {// execute command
            pid_t spawnpid = -5;
            int inputResult, outputResult, targetInput, targetOutput;
            
            spawnpid = fork();
            switch (spawnpid) {
                case -1:
                    perror("Failed to fork.\n");
                    exit(1);
                    break;
                case 0: //child
                    //i/o redirection here
                    SIGINT_action.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &SIGINT_action, NULL);
                    SIGTSTP_action.sa_handler = SIG_IGN;
                    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

                    if (newCommand->inputfile != NULL) {
                        targetInput = open(newCommand->inputfile, O_RDONLY, S_IRUSR | S_IWUSR);//
                        if (targetInput == -1) {    perror("Input open failed"); exit(1);}
                        inputResult = dup2(targetInput, 0);
                    }
                    if (newCommand->outputfile != NULL) {
                        targetOutput = open(newCommand->outputfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);//
                        if (targetOutput == -1) {    perror("Output open failed"); exit(1);}
                        outputResult = dup2(targetOutput, 1);
                    }

                    if (newCommand->amp == 1) {
                        if (newCommand->inputfile != NULL) {
                            targetInput = open("/dev/null", O_RDONLY);//r
                            if (targetInput == -1) {    perror("Input open failed"); exit(1);}
                            inputResult = dup2(targetInput, 0);
                        }

                        if (newCommand->outputfile != NULL) {
                            targetOutput = open("/dev/null", O_WRONLY);//
                            if (targetOutput == -1) {    perror("Output open failed"); exit(1);}
                            outputResult = dup2(targetOutput, 0);
                        }
                    }

                    if (execvp(newCommand->command_name, newCommand->args) != 0) {
                        perror("Exec failure");
                        exit(1);
                    }

                    exit(0);
                default:
                    if (newCommand->amp == 1) {
                        printf("background pid is %d\n", spawnpid);
                        int i = 0;
                        while (backgroundPIDs[i] != -5) {
                            i++;
                        }
                        backgroundPIDs[i] = spawnpid;
                    }

                    else {
                        waitpid(spawnpid, &childExitMethod, 0);

                        if (WIFEXITED(childExitMethod) != 0) {
                            lastStatus = WEXITSTATUS(childExitMethod);
                            sigStatus = 0;

                        }
                        else {
                            lastStatus = WTERMSIG(childExitMethod);
                            sigStatus = 1;
                            printf("\nterminated by signal %d\n", lastStatus);
                            fflush(stdout);
                        }

                        //exit status function

                    }
                    break;
            }

            }


        }

        

    }

}