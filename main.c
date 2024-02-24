/* Author: Faaizah Nuha
   Description: Shell program that executes basic commands
   Date: 2/23/2024
   Class: CS374

   Citation for the original code snipped for trailing newline removal:
   Date retrieved: 2/12/2024
   Copied from: https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
   

*/
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
    newCommand->inputfile = NULL; //set all values to NULL/initial starting value so later on, defaults can be checked
    newCommand->outputfile = NULL;
    newCommand->argnum = 0;
    newCommand->amp = 0;

    char *saveptr; //pointer for strtok_r

    char *token = strtok_r(commToParse, " ", &saveptr); //using strtok_r to parse first part of string, the command
    newCommand->command_name = calloc(strlen(token) + 1, sizeof(char)); //allocating data that is the size of the command + 1 for /0
    strcpy(newCommand->command_name, token); //saving command in struct

    int i = 0; //initializing int for loop
    newCommand->args[i] = calloc(strlen(newCommand->command_name) + 1, sizeof(char)); //adding main command to args list
    strcpy(newCommand->args[i], newCommand->command_name);
    i++; //incrementing i for while loop
    token = strtok_r(NULL, " ", &saveptr); //finding next argument string

    while (token != NULL) { //while there are more arguments to parse

        if(strcmp(token, "<") == 0) { //input file
            token = strtok_r(NULL, " ", &saveptr); //skip to next argument
            newCommand->inputfile = calloc(strlen(token) + 1, sizeof(char)); //store in struct under inputfile
            strcpy(newCommand->inputfile, token);
        }

        else if(strcmp(token, ">") == 0) { //output file
            token = strtok_r(NULL, " ", &saveptr); //skip to next argument
            newCommand->outputfile = calloc(strlen(token) + 1, sizeof(char)); //store in struct under inputfile
            strcpy(newCommand->outputfile, token);
        }


        else if(strcmp(token, "&") == 0) { //ampersand at the end
            token = strtok_r(NULL, " ", &saveptr); //check next argument
            if (token == NULL) { //if nothing after ampersand
                newCommand->amp = 1; //mark as background command
                break;
            }
            else { //if something is after ampersand argument,
                newCommand->args[i] = calloc(strlen("&") + 1, sizeof(char)); //add ampersand...
                strcpy(newCommand->args[i], "&");
                i++; //increment i by one to keep up with number of args added...

                newCommand->args[i] = calloc(strlen(token) + 1, sizeof(char)); //and next argument to args array
                strcpy(newCommand->args[i], token);
                i++;
                newCommand->argnum = i; //and increment number of arguments to match new number

            }

        }

        //all others are added normally
        else {
            newCommand->args[i] = calloc(strlen(token) + 1, sizeof(char)); //create space for argument in array
            strcpy(newCommand->args[i], token); //copy to array
            i++; //increment number of args tracker
            newCommand->argnum = i; //store new number in struct
        }
        
        token = strtok_r(NULL, " ", &saveptr); //look for next argument

    }

    return newCommand; //return to main


}

int main (int argc, char *argv[]) {

    //make sigaction struct(s)
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    //set first value to SIG_IGN
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask); //fill the mask so nothing interrupts signal action
    SIGINT_action.sa_flags = 0; //no additional instructions

    //repeat for TSTP
    SIGTSTP_action.sa_handler = catchSIGTSTP; //except link TSTP to void function set up
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;


    //initialize both signal handlers
    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    
    char input[2048]; //input from user
    memset(input, '\0', 2048); //initialize it so it doesn't have garbage data
    int lastStatus = 0; // need int for program exit data (exit status which could be an int or terminating signal) 
    
    int sigStatus = 0; //boolean for exit status vs terminating signal: 0 for exiting, 1 for signal termination
    int backgroundPIDs[512] = { -5 }; //make array for tracking background command PIDs
    int f = 0; //loop int
    for (f = 0; f < 512; f++) {
            backgroundPIDs[f] = -5; //initialize PID array to have numbers that are not valid as PIDs
        }

    while(1) { //main command loop

        int j = 0; //another loop int
        
        int childPID_actual = 0; //for waitpid return value, either will be 0 or the pid of the child
        int* childExitMethod; //pointer for the exit value of the program
        
        for (j = 0; j < 512; j++) { //loop through all PIDs
            if (backgroundPIDs[j] != -5) { //if PID is valid value
                childPID_actual = waitpid(backgroundPIDs[j],&childExitMethod, WNOHANG); //ping the program
                if (childPID_actual != 0) { //if it returns with a non-zero aka is done
                    if (WIFEXITED(childExitMethod) != 0) { //check to see if exited
                        printf("background pid %d is done: exited with status %d\n", backgroundPIDs[j],WEXITSTATUS(childExitMethod));
                        //and print the exit status if it exited
                    }
                    else { //otherwise that means it was killed by signal,
                        printf("background pid %d is done: terminated by signal %d\n", backgroundPIDs[j],WTERMSIG(childExitMethod));
                        //print that signal out
                    }
                    backgroundPIDs[j] = -5; //and free up space in the array for new PIDs
                }
                fflush(stdout);
            }
        }

        printf(": "); //actual command prompt
        fflush(stdout);
        fgets(input, 2048, stdin); //get the user input

        //code from https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
        input[strcspn(input, "\n")] = 0; //remove trailing newline
    
        if (strlen(input) != 0 && input[0] != '#' && input[0] != ' ') { //if not blank or comment or space

                while (strstr(input, "$$") != NULL) { //variable expansion, look for $$ using strstr

                char buffer[2048]; //store chars that come before the $$
                char buffer2[2048]; //store chars that come after
                memset(buffer, '\0', 2048); //and initialize them both
                memset(buffer2, '\0', 2048);
                int i; //basically a rudimentary pointer, tracks which character we're looking at

                int lengthuntil = strcspn(input, "$$"); //find where the $$ starts
                for (i = 0; i < lengthuntil; i++) { 
                    buffer[i] = input[i]; // and store all characters that come before into buffer
                }

                lengthuntil = lengthuntil + 2; //then increment the "pointer" to be after the $$

                for (i = 0; i < strlen(input); i++) { 
                    buffer2[i] = input[i+lengthuntil]; //and store all chars that come after buffer
                }

                sprintf(input, "%s%d%s", buffer, getpid(), buffer2); //now concatenate them all and put into input

            }

            struct command *newCommand = parseCommand(input); //call command to parse the expanded input
            
            if (foregroundMode == 1) { //turn off background commands if foregroundmode is on
                newCommand->amp = 0;
            }

            if(strcmp(newCommand->command_name, "exit") == 0) { //if command is exit
                //kill any processes started by shell
                int i = 0;
                for (i = 0; i < 512; i++) { //by going through all PIDs stored
                    if (backgroundPIDs[i] != -5) {
                        kill(backgroundPIDs[i], SIGKILL); //and killing if they're valid PIDs
                    }
                }
                exit(0); //then exit shell program itself
            }

            else if (strcmp(newCommand->command_name, "cd") == 0) { //if command is cd
                //navigate to directory given
                if (newCommand->argnum == 0) {
                    chdir(getenv("HOME")); //move to home if no args given
                }
                else {
                    chdir(newCommand->args[1]); //move to location of first arg
                }

            }

            else if(strcmp(newCommand->command_name, "status") == 0) { //if command is status
                if (sigStatus == 0) { //check the last status to see if it was signal or exit to print correct msg
                    printf("terminated with status %d\n", lastStatus); //print for exiting
                }
                else {
                    printf("terminated by signal %d\n", lastStatus); //print for status
                }
            }

            else {// executing command
            pid_t spawnpid = -5; //setting spawnpid to some valud that is garbage 
            int inputResult, outputResult, targetInput, targetOutput; //making variables since they can't be declared in switch
            
            spawnpid = fork();
            switch (spawnpid) {
                case -1: //error thrown by fork itself
                    perror("Failed to fork.\n"); //somehow failed to fork the shell, so
                    exit(1); //we exit the shell
                    break;
                case 0: //child
                    //turning on/off relevant signals for child and updating handlers
                    SIGINT_action.sa_handler = SIG_DFL;
                    sigaction(SIGINT, &SIGINT_action, NULL);
                    SIGTSTP_action.sa_handler = SIG_IGN;
                    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

                    if (newCommand->inputfile != NULL) { //opening input file only if it exits
                        targetInput = open(newCommand->inputfile, O_RDONLY, S_IRUSR | S_IWUSR); //giving read and write perms
                        if (targetInput == -1) {    perror("Input open failed"); exit(1);} //error catching
                        inputResult = dup2(targetInput, 0); //redirecting stdin (which is 0) to file
                    }
                    if (newCommand->outputfile != NULL) {
                        targetOutput = open(newCommand->outputfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                        //gives read and write perms, also creates if file does not exit and truncates if file does exist
                        if (targetOutput == -1) {    perror("Output open failed"); exit(1);} //error catching 
                        outputResult = dup2(targetOutput, 1); //redirecting stdout (which is 1) to file
                    }

                    if (newCommand->amp == 1) { //background command flag raised

                        //replacing input/output with dev/null
                        if (newCommand->inputfile == NULL) { //only if input file not specified do i replace with dev/null
                            targetInput = open("/dev/null", O_RDONLY);
                            if (targetInput == -1) {    perror("Input open failed"); exit(1);}
                            inputResult = dup2(targetInput, 0);
                        }

                        if (newCommand->outputfile == NULL) { //if output file not specified i also replace with dev/null
                            targetOutput = open("/dev/null", O_WRONLY);
                            if (targetOutput == -1) {    perror("Output open failed"); exit(1);}
                            outputResult = dup2(targetOutput, 0);
                        }
                    }

                    if (execvp(newCommand->command_name, newCommand->args) != 0) { //execute! within an if statement so that it...
                        perror("Exec failure"); //throws error if it can't execute
                        exit(1); //and exits with a status of 1
                    }

                    exit(0); //for safety
                default:
                    if (newCommand->amp == 1) { //background commands
                        printf("background pid is %d\n", spawnpid); //prints out pid and
                        int i = 0;
                        while (backgroundPIDs[i] != -5) {
                            i++;
                        }
                        backgroundPIDs[i] = spawnpid; //stores it in first available slot in background pids array
                    }

                    else {
                        waitpid(spawnpid, &childExitMethod, 0); //wait for it to finish

                        if (WIFEXITED(childExitMethod) != 0) { //if the child exited
                            lastStatus = WEXITSTATUS(childExitMethod); //store the status of the exit
                            sigStatus = 0; //and set the bool to mark that it was exit and not signal

                        }
                        else { //child was terminated by signal
                            lastStatus = WTERMSIG(childExitMethod); //check to see what signal killed it
                            sigStatus = 1; //and set bool to mark that it was signal
                            printf("\nterminated by signal %d\n", lastStatus); //print out status
                            fflush(stdout);
                        }

                        //exit status function

                    }
                    break; //go back to start of loop
            }

            }


        }

        

    }

}