#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>

#include "cs402.h"

int numIndex;
int lambdaIndex;
int muIndex;
int rIndex;
int BIndex;
int PIndex;
int tsfileIndex;
FILE* fileInput;

int num;
int inputQSize;
int outputQSize;
int Q1Size;
int Q2Size;

pthread_mutex_t myLock;
pthread_cond_t cv;

pthread_t packet;
pthread_t token;
pthread_t s1;
pthread_t s2;
pthread_t sig;

sigset_t mask;

double myRound(double x, int n){
    double mul = 1;
    while(n > 0){
        mul *= 10;
        n--;
    }
    return (long long)(x * mul + 0.5) / mul;
}

void* packetFunc(void* argv){
    char* name = (char*) argv;
    while(inputQSize > 0){
        usleep(1 * 1e6);
        pthread_mutex_lock(&myLock);
        if(inputQSize > 0){
            inputQSize--;
            Q1Size++;
            fprintf(stdout, "%s: inputQSize: %d\n", name, inputQSize);
            fprintf(stdout, "%s: Q1Size: %d\n", name, Q1Size);
        }
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&myLock);
    }
    fprintf(stdout, "%s线程结束\n", name);
    fprintf(stdout, "inputQ: %d, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, Q1Size, Q2Size, outputQSize);
    return NULL;
}

void* tokenFunc(void* argv){
    char* name = (char*) argv;
    while(inputQSize > 0 || Q1Size > 0){
        usleep(1 * 1e6);
        pthread_mutex_lock(&myLock);
        if(inputQSize > 0 || Q1Size > 0){
            fprintf(stdout, "%s: 生产token\n", name);
        }
        if(Q1Size > 0){
            Q1Size--;
            Q2Size++;
            fprintf(stdout, "%s: 将packet从Q1移到Q2\n", name);
            pthread_cond_broadcast(&cv);
        }
        pthread_mutex_unlock(&myLock);
    }
    fprintf(stdout, "%s线程结束\n", name);
    fprintf(stdout, "inputQ: %d, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, Q1Size, Q2Size, outputQSize);
    return NULL;
}

void* serverFunc(void* argv){
    char* name = (char*) argv;
    while(inputQSize > 0 || Q1Size > 0 || Q2Size > 0){
        pthread_mutex_lock(&myLock);
        while(Q2Size == 0 && outputQSize < num){
            pthread_cond_wait(&cv, &myLock);
        }
        if(Q2Size > 0){
            Q2Size--;
            fprintf(stdout, "%s服务Q2中的packet\n", name);
            pthread_cond_broadcast(&cv);
        }
        pthread_mutex_unlock(&myLock);
    }
    fprintf(stdout, "%s线程结束\n", name);
    fprintf(stdout, "inputQ: %d, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, Q1Size, Q2Size, outputQSize);
    return NULL;
}

void* signalFunc(void* argv){
    char* name = (char*) argv;
    int mySignal = -1;
    while(mySignal < 0){
        sigwait(&mask, &mySignal);
        pthread_mutex_lock(&myLock);
        fprintf(stdout, "%s收到信号, 信号 = %d\n", name, mySignal);
        inputQSize = 0;
        fprintf(stdout, "%s移除所有inputQ的packet\n", name);
        Q1Size = 0;
        fprintf(stdout, "%s移除所有Q1的packet\n", name);
        Q2Size = 0;
        fprintf(stdout, "%s移除所有Q2的packet\n", name);
        outputQSize = num;
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&myLock);
    }
    fprintf(stdout, "%s线程结束\n", name);
    fprintf(stdout, "inputQ: %d, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, Q1Size, Q2Size, outputQSize);
    return NULL;
}

void init(){
    num = 100;
    inputQSize = num;
    outputQSize = 0;
    Q1Size = 0;
    Q2Size = 0;

    pthread_mutex_init(&myLock, NULL);
    pthread_cond_init(&cv, NULL);

    sigemptyset(&mask);
}

int isValidOption(char option[]){
    return strcmp("-lambda", option) == 0 || strcmp("-mu", option) == 0 || 
           strcmp("-r", option) == 0 || strcmp("-B", option) == 0 || 
           strcmp("-P", option) == 0 || strcmp("-n", option) == 0 || 
           strcmp("-t", option) == 0;
}

int isInteger(char optionValue[], int optionValueSize){
    fprintf(stdout, "%s %d\n", optionValue, optionValueSize);
    for(int a = 0; a < optionValueSize; a++){
        if(!isdigit(optionValue[a])){
            return FALSE;
        }
    }
    return TRUE;
}

int isNumber(char optionValue[], int optionValueSize){
    fprintf(stdout, "%s %d\n", optionValue, optionValueSize);
    int dotCount = 0;
    for(int a = 0; a < optionValueSize; a++){
        if(optionValue[a] == '.'){
            dotCount++;
            continue;
        }
        if(!isdigit(optionValue[a])){
            return FALSE;
        }
    }
    if(dotCount > 1 || optionValue[optionValueSize - 1] == '.'){
        return FALSE;
    }
    return TRUE;
}

void checkFileLine(char fileLine[], int strSize, int lineNum){
    if(strSize > 1024){
        fprintf(stderr, "malformed input, line %d has more than 1024 char\n", lineNum);
        fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
        exit(1);
    }
    if(fileLine[0] == ' ' || fileLine[0] == '\t' || fileLine[strSize - 2] == ' ' || fileLine[strSize - 2] == '\t'){
        fprintf(stderr, "malformed input, line %d has leading or trailing space or tab\n", lineNum);
        fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
        exit(1);
    }
}

void checkField(char fieldStr[], int strSize, int lineNum){
    if(!isInteger(fieldStr, strSize)){
        fprintf(stderr, "malformed input, line %d is not an integer\n", lineNum);
        fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
        exit(1);
    }
    long long fieldNum = atoll(fieldStr);
    if(fieldNum <= 0){
        fprintf(stderr, "malformed input, line %d is not an positive integer\n", lineNum);
        fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
        exit(1);
    }
}

void checkInput(int argc, char* argv[]){
    for(int a = 1; a < argc; a += 2){
        if(!isValidOption(argv[a])){
            fprintf(stderr, "malformed command, %s is not a valid commandline option\n", argv[a]);
            fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
            exit(1);
        }
        int nextIndex = a + 1;
        if(nextIndex >= argc || isValidOption(argv[nextIndex])){
            fprintf(stderr, "malformed command, value for %s is not given\n", argv[a]);
            fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
            exit(1);
        }
        if(strcmp("-B", argv[a]) == 0 || strcmp("-P", argv[a]) == 0 || strcmp("-num", argv[a]) == 0){
            if(!isInteger(argv[nextIndex], strlen(argv[nextIndex]))){
                fprintf(stderr, "malformed command, %s value %s is not an integer\n", argv[a], argv[nextIndex]);
                fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
                exit(1);
            }

            long long val = atoll(argv[nextIndex]);
            if(val <= 0 || val > INT_MAX){
                fprintf(stderr, "malformed command, %s value %s is not in valid range [1, %d]\n", argv[a], argv[nextIndex], INT_MAX);
                fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
                exit(1);
            }
        }
        if(strcmp("-lambda", argv[a]) == 0 || strcmp("-mu", argv[a]) == 0 || strcmp("-r", argv[a]) == 0){
            if(!isNumber(argv[nextIndex], strlen(argv[nextIndex]))){
                fprintf(stderr, "malformed command, %s value %s is not a number\n", argv[a], argv[nextIndex]);
                fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
                exit(1);
            }
        }
        if(strcmp("-t", argv[a]) == 0){
            if((fileInput = fopen(argv[a + 1], "r")) == NULL){
                fprintf(stderr, "Error opening file %s.\n", argv[a + 1]);
                fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
                exit(1);
            }

            int bufferSize = 1050;
            char fileLine[bufferSize];
            char packetSizeStr[bufferSize];
            char interPacketTimeStr[bufferSize];
            char tokenNeedStr[bufferSize];
            char packetServiceTimeStr[bufferSize];
            memset(fileLine, 0, bufferSize);
            memset(packetSizeStr, 0, bufferSize);
            memset(interPacketTimeStr, 0, bufferSize);
            memset(tokenNeedStr, 0, bufferSize);
            memset(packetServiceTimeStr, 0, bufferSize);

            int lineNum = 0;
            while(!feof(fileInput) && fgets(fileLine, bufferSize, fileInput) != NULL){
                lineNum++;
                checkFileLine(fileLine, strlen(fileLine), lineNum);

                if(lineNum == 1){
                    sscanf(fileLine, "%s", packetSizeStr);
                    checkField(packetSizeStr, strlen(packetSizeStr), lineNum);
                    num = atoll(packetSizeStr);
                }
                else{
                    sscanf(fileLine, "%s %s %s", interPacketTimeStr, tokenNeedStr, packetServiceTimeStr);
                    fprintf(stdout, "%s %s %s\n", interPacketTimeStr, tokenNeedStr, packetServiceTimeStr);
                    checkField(interPacketTimeStr, strlen(interPacketTimeStr), lineNum);
                    checkField(tokenNeedStr, strlen(tokenNeedStr), lineNum);
                    checkField(packetServiceTimeStr, strlen(packetServiceTimeStr), lineNum);
                }
            }

            if(lineNum == 0){
                fprintf(stderr, "Error opening file %s.\n", argv[a + 1]);
                fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
                exit(1);
            }
        }
    }
    fprintf(stdout, "有效\n");
}

int main(int argc, char* argv[]){

    // checkInput(argc, argv);

    // init();

    // sigaddset(&mask, SIGINT);
    // sigprocmask(SIG_BLOCK, &mask, NULL);

    // pthread_create(&sig, NULL, signalFunc, "signal");
    // pthread_create(&packet, NULL, packetFunc, "packet");
    // pthread_create(&token, NULL, tokenFunc, "token");
    // pthread_create(&s1, NULL, serverFunc, "s1");
    // pthread_create(&s2, NULL, serverFunc, "s2");

    // pthread_join(packet, NULL);
    // pthread_join(token, NULL);
    // pthread_join(s1, NULL);
    // pthread_join(s2, NULL);
    // pthread_join(sig, NULL);

    tsfileIndex = 1;
    int aSize = 200;
    int bSize = 0;
    int temp1 = tsfileIndex == -1 ? aSize > 0 : bSize > 0;

    fprintf(stdout, "%d\n", temp1);

    return 0;
}