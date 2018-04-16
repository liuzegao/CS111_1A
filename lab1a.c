//
//  lab1.c
//  Project_1A
//  CS 111
//  Created by Zegao on 4/12/18.
//  Copyright © 2018 pro. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

char isThereShell = 'N';

int fpid;

static struct termios saved_attributes;

//GNU FUNCTION, EXAM!
void restoreInputMode(void)
{
    tcsetattr (0, TCSANOW, &saved_attributes);
    if (isThereShell == 'Y') {
        int childstatus;
        if (waitpid(fpid, &childstatus, 0) == -1) {
            fprintf(stderr, "error: waitpid failed");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(childstatus)) {
            const int higher = WEXITSTATUS(childstatus);
            const int lower = WTERMSIG(childstatus);
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", lower, higher);
            exit(0);
        }
    }
}

void changeInputMode(void)
{
    //Make sure stdin is in terminal mode
    if (!isatty(0))
    {
        fprintf (stderr, "Error: stdin is not in terminal mode.\n");
        exit(1);
    }
    
    //Save the original terminal mode
    tcgetattr(0, &saved_attributes);
    atexit(restoreInputMode);
    
    struct termios tattr;
    //Change to new terminal mode
    tcgetattr (0, &tattr);
    tattr.c_iflag = ISTRIP;
    tattr.c_oflag = 0;
    tattr.c_lflag = 0; //Vmin is by default 1, Vtime is by default 0
    tcsetattr (0, TCSANOW, &tattr);
}

int main(int argc, char * argv[]) {
    
    static struct option long_options[] = {
        {"shell", no_argument, 0, 'S'},
    };
    
    int ch;
    while((ch = getopt_long(argc, argv,"",long_options,NULL))!=-1){
        switch(ch){
            case 'S': isThereShell = 'Y'; break;
            default:  exit(1); break;
        }
    }
    
    if(isThereShell == 'N'){
        changeInputMode();
        //Read and write
        char buffer = '\0';
        char temp_buffer[2] = "\0";
        while((buffer != '\03')&&(buffer != '\04')){
            //Deal with the line change problem
            if(buffer == '\r' || buffer == '\n'){
                temp_buffer[0] = '\r';
                temp_buffer[1] = '\n';
                write(1,temp_buffer,sizeof(temp_buffer));
            }
            read(0,&buffer, sizeof(char));
            write(1,&buffer,sizeof(char));
        }
    }else{//Shell Mode
        //Create two fd
        int toChildPip[2],toParentPip[3];
        if(pipe(toChildPip) == -1){
            fprintf(stderr,"Error: failed to create toChildpip \r\n");
            exit(1); //EXAM: check if we should exit wiht 1 or others
        }
        if(pipe(toParentPip) == -1){
            fprintf(stderr,"Error: failed to create toParentpipe \r\n");
            exit(1); //EXAM: check if we should exit wiht 1 or others
        }
        //Create a new procee with fork()
        fpid =fork();
        if(fpid < 0){ //fails fork()
            fprintf(stderr, "Error: fail to fork \r\n");
            exit(1);  //EXAM: check if we should exit wiht 1 or others
        }else if (fpid == 0){//Child process
            //Close useless fd
            close(toChildPip[1]);
            close(toParentPip[0]);
            //Dup fd to stdin and stdout
            dup2(toChildPip[0],0);
            close(toChildPip[0]);
            dup2(toParentPip[1],1);
            dup2(toParentPip[1],2);
            close(toParentPip[1]);
            char *execvp_argv[2];
            char execvp_filename[] = "/bin/bash";
            execvp_argv[0] = execvp_filename;
            execvp_argv[1] = NULL;
            if (execvp(execvp_filename,execvp_argv)<0){
                fprintf(stderr, "Error: failed execvp() \r\n");
                exit(1); //EXAM: check if we should exit wiht 1 or others
            }
        }else{//parent process
            //Close useless fd
            close(toChildPip[0]);
            close(toParentPip[1]);
            
            changeInputMode();
            char parentbuffer;
            //char longparentbuffer[256] = "";
            //char shellbuffer[2048] = "\0";
            char newbuffer[2048] = "\0";

            //int count = 0;
            int returnValue;
            struct pollfd pollFdGroup[2];
            pollFdGroup[0].fd = 0;
            pollFdGroup[1].fd = toParentPip[0];
            pollFdGroup[0].events = POLLIN | POLLHUP | POLLERR;
            pollFdGroup[1].events = POLLIN | POLLHUP | POLLERR;
            //Poll
            while (1) {
                // do a poll and check for errors
                returnValue = poll(pollFdGroup, 2, 0);
                if (returnValue < 0) {
                    fprintf(stderr, "Error: failed poll \r\n");
                    exit(1);
                }
                if ((pollFdGroup[0].revents & POLLIN)){
                    if(read(0,&parentbuffer, 1)<0){
                        fprintf(stderr, "Error: failed parent stdin \r\n");
                        exit(1);
                    }
                    if(parentbuffer == '\04'){
                        close(toChildPip[1]);
                        //exit(0);
                    }else if(parentbuffer == '\03'){
                        kill(fpid, SIGINT);
                        //exit(0);
                    }else if(parentbuffer == '\r' || parentbuffer  == '\n'){
                        char tempBuffer = {'\n'};
                        int writeRC =  write(toChildPip[1],&tempBuffer,sizeof(tempBuffer));
                        int errorNote = errno;
                        if( writeRC < 0 ){
                            fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                            exit(1);
                        }
                        char templineBuffer[2] = {'\r','\n'};
                        writeRC = write(1,templineBuffer,sizeof(templineBuffer));
                        errorNote = errno;
                        if( writeRC < 0 ){
                            fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                            exit(1);
                        }
                    }else{
                        int writeRC = write(toChildPip[1], &parentbuffer, sizeof(parentbuffer));
                        int errorNote = errno;
                        if( writeRC < 0 ){
                            fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                            exit(1);
                        }
                        writeRC = write(1,&parentbuffer,sizeof(parentbuffer));
                        errorNote = errno;
                        if( writeRC < 0 ){
                            fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                            exit(1);
                        }
                    }
                }
                if ((pollFdGroup[1].revents & POLLIN)) {
                    int count = read(toParentPip[0], newbuffer, 2048); // read from shell pipe
                    int errorNote = errno;
                    if( count < 0 ){
                        fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                        exit(1);
                    }
                    int i;
                    for(i = 0;i<count;i++){
                        if(newbuffer[i] == '\n'){
                            char tempshellBuffer[2] = {'\r','\n'};
                            int writeRC  = write(1,&tempshellBuffer,sizeof(tempshellBuffer));
                            errorNote = errno;
                            if( writeRC < 0 ){
                                fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                                exit(1);
                            }
                            writeRC = write(1,tempshellBuffer,sizeof(tempshellBuffer));
                            errorNote = errno;
                            if( writeRC < 0 ){
                                fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                                exit(1);
                            }
                        }else{
                            int writeRC = write(1,&newbuffer[i],1);
                            errorNote = errno;
                            if( writeRC < 0 ){
                                fprintf(stderr, "Error:%s.\r\n", strerror(errorNote));
                                exit(1);
                            }
                        }
                    }
                }
                if ((pollFdGroup[1].revents & (POLLHUP | POLLERR))) {
                    exit(0);
                }
            }
        }
    }
    exit(0);
}

