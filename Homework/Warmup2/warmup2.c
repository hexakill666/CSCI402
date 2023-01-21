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

#include "my402list.h"
#include "mypacket.h"

double sToUs;
double msToUs;
double timeStampLimit;
int timeStampStrSize;

double lambda;
double mu;
double r;
long long B;
long long P;
long long num;

int numIndex;
int lambdaIndex;
int muIndex;
int rIndex;
int BIndex;
int PIndex;
int tsfileIndex;

FILE* fileInput;
long long lineNum;

long long packetId;
long long tokenId;
long long curTokenSize;
long long tokenDropSize;
long long inputQSize;

long long interTokenTime;
long long allInterPacketTime;
long long allPacketServiceTime;

My402List inputQ;
My402List outputQ;
My402List Q1;
My402List Q2;

pthread_t packet;
pthread_t token;
pthread_t s1;
pthread_t s2;

pthread_mutex_t myLock;
pthread_cond_t cv;

struct timeval emulationStartTime;
struct timeval emulationEndTime;
struct timeval prePacketArriveTime;

pthread_t sig;
sigset_t mask;

double myRound(double num, int keepDigitSize){
    double mul = 1;
    while(keepDigitSize > 0){
        mul *= 10;
        keepDigitSize--;
    }
    return (long long)(num * mul + 0.5) / mul;
}

double myMax(double num1, double num2){
    return num1 >= num2 ? num1 : num2;
}

double myMin(double num1, double num2){
    return num1 <= num2 ? num1 : num2;
}

long long calTimeDiff(struct timeval start, struct timeval end){
    return (end.tv_sec - start.tv_sec) * sToUs + (end.tv_usec - start.tv_usec);
}

void getTimeStampStr(char timeStampStr[], int strSize, long long timestamp){
    memset(timeStampStr, 0, strSize);
    double timeStampMS = timestamp / msToUs;
    if(timeStampMS >= timeStampLimit){
        for(int a = 0; a < strSize - 1; a++){
            timeStampStr[a] = '?';
        }
        timeStampStr[8] = '.';
    }
    else{
        sprintf(timeStampStr, "%012.3f", timeStampMS);
    }
}

void printUsageAndExit(){
    fprintf(stderr, "usage: warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\n");
    exit(1);
}

MyPacket* createPacket(){
    MyPacket* myPacket = (MyPacket*)malloc(sizeof(MyPacket));

    myPacket->serviceType = 0;
    myPacket->packetType = 0;
    myPacket->packetId = 0;

    myPacket->tokenNeed = 0;
    myPacket->interPacketTime = 0;
    myPacket->packetServiceTime = 0;

    myPacket->arriveTime = 0;
    myPacket->enterQ1Time = 0;
    myPacket->leaveQ1Time = 0;
    myPacket->enterQ2Time = 0;
    myPacket->leaveQ2Time = 0;
    myPacket->beginServiceTime = 0;
    myPacket->endServiceTime = 0;

    myPacket->realInterPacketArriveTime = 0;

    return myPacket;
}

void initPacket(MyPacket* myPacket, PacketData* packetData){
    packetId++;
    myPacket->packetId = packetId;
    myPacket->tokenNeed = packetData->tokenNeed;
    myPacket->interPacketTime = packetData->interPcketTime;
    myPacket->packetServiceTime = packetData->packetServiceTime;
}

PacketData* createPacketData(){
    PacketData* packetData = (PacketData*)malloc(sizeof(PacketData));

    packetData->tokenNeed = P;
    packetData->interPcketTime = allInterPacketTime;
    packetData->packetServiceTime = allPacketServiceTime;

    return packetData;
}

int isValidOption(char option[]){
    return strcmp("-lambda", option) == 0 || strcmp("-mu", option) == 0 || 
           strcmp("-r", option) == 0 || strcmp("-B", option) == 0 || 
           strcmp("-P", option) == 0 || strcmp("-n", option) == 0 || 
           strcmp("-t", option) == 0;
}

int isInteger(char optionValue[], int optionValueSize){
    for(int a = 0; a < optionValueSize; a++){
        if(!isdigit(optionValue[a])){
            return FALSE;
        }
    }
    return TRUE;
}

int isNumber(char optionValue[], int optionValueSize){
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
        printUsageAndExit();
    }
    if(fileLine[0] == ' ' || fileLine[0] == '\t' || fileLine[strSize - 2] == ' ' || fileLine[strSize - 2] == '\t'){
        fprintf(stderr, "malformed input, line %d has leading or trailing space or tab\n", lineNum);
        printUsageAndExit();
    }
}

void checkField(char fieldStr[], int strSize, int lineNum){
    if(!isInteger(fieldStr, strSize)){
        fprintf(stderr, "malformed input, line %d is not an integer\n", lineNum);
        printUsageAndExit();
    }
    long long fieldNum = atoll(fieldStr);
    if(fieldNum <= 0){
        fprintf(stderr, "malformed input, line %d is not an positive integer\n", lineNum);
        printUsageAndExit();
    }
}

void checkInput(int argc, char* argv[]){
    for(int a = 1; a < argc; a += 2){
        if(!isValidOption(argv[a])){
            fprintf(stderr, "malformed command, %s is not a valid commandline option\n", argv[a]);
            printUsageAndExit();
        }
        if(a + 1 >= argc || isValidOption(argv[a + 1])){
            fprintf(stderr, "malformed command, value for %s is not given\n", argv[a]);
            printUsageAndExit();
        }
        if(strcmp("-B", argv[a]) == 0 || strcmp("-P", argv[a]) == 0 || strcmp("-n", argv[a]) == 0){
            if(!isInteger(argv[a + 1], strlen(argv[a + 1]))){
                fprintf(stderr, "malformed command, %s value %s is not an integer\n", argv[a], argv[a + 1]);
                printUsageAndExit();
            }

            long long val = atoll(argv[a + 1]);
            if(val <= 0 || val > INT_MAX){
                fprintf(stderr, "malformed command, %s value %s is not in valid range [1, %d]\n", argv[a], argv[a + 1], INT_MAX);
                printUsageAndExit();
            }

            if(strcmp("-B", argv[a]) == 0){
                BIndex = a;
            }
            if(strcmp("-P", argv[a]) == 0){
                PIndex = a;
            }
            if(strcmp("-n", argv[a]) == 0){
                numIndex = a;
            }
        }
        if(strcmp("-lambda", argv[a]) == 0 || strcmp("-mu", argv[a]) == 0 || strcmp("-r", argv[a]) == 0){
            if(!isNumber(argv[a + 1], strlen(argv[a + 1]))){
                fprintf(stderr, "malformed command, %s value %s is not a number\n", argv[a], argv[a + 1]);
                printUsageAndExit();
            }

            if(strcmp("-lambda", argv[a]) == 0){
                lambdaIndex = a;
            }
            if(strcmp("-mu", argv[a]) == 0){
                muIndex = a;
            }
            if(strcmp("-r", argv[a]) == 0){
                rIndex = a;
            }
        }
        if(strcmp("-t", argv[a]) == 0){
            tsfileIndex = a;
            if((fileInput = fopen(argv[a + 1], "r")) == NULL){
                fprintf(stderr, "Error opening file %s\n", argv[a + 1]);
                printUsageAndExit();
            }
        }
    }
}

void readTsFileConfig(int argc, char* argv[]){
    int bufferSize = 1050;
    char fileLine[bufferSize];
    char packetSizeStr[bufferSize];
    memset(fileLine, 0, bufferSize);
    memset(packetSizeStr, 0, bufferSize);

    if(!feof(fileInput) && fgets(fileLine, bufferSize, fileInput) != NULL){
        lineNum++;
        checkFileLine(fileLine, strlen(fileLine), lineNum);

        sscanf(fileLine, "%s", packetSizeStr);
        checkField(packetSizeStr, strlen(packetSizeStr), lineNum);
        num = atoll(packetSizeStr);
    }

    if(lineNum == 0){
        fprintf(stderr, "Error opening file %s.\n", argv[tsfileIndex + 1]);
        printUsageAndExit();
    }
}

void readTsFileData(PacketData* packetData){
    int bufferSize = 1050;
    char fileLine[bufferSize];
    char interPacketTimeStr[bufferSize];
    char tokenNeedStr[bufferSize];
    char packetServiceTimeStr[bufferSize];
    memset(fileLine, 0, bufferSize);
    memset(interPacketTimeStr, 0, bufferSize);
    memset(tokenNeedStr, 0, bufferSize);
    memset(packetServiceTimeStr, 0, bufferSize);

    if(!feof(fileInput) && fgets(fileLine, bufferSize, fileInput)){
        lineNum++;
        checkFileLine(fileLine, strlen(fileLine), lineNum);

        sscanf(fileLine, "%s %s %s", interPacketTimeStr, tokenNeedStr, packetServiceTimeStr);
        checkField(interPacketTimeStr, strlen(interPacketTimeStr), lineNum);
        checkField(tokenNeedStr, strlen(tokenNeedStr), lineNum);
        checkField(packetServiceTimeStr, strlen(packetServiceTimeStr), lineNum);

        packetData->interPcketTime = atoll(interPacketTimeStr) * msToUs;
        packetData->tokenNeed = atoll(tokenNeedStr);
        packetData->packetServiceTime = atoll(packetServiceTimeStr) * msToUs;
    }
}

void readInput(int argc, char* argv[]){
    if(lambdaIndex >= 0){
        lambda = atof(argv[lambdaIndex + 1]);
    }
    if(muIndex >= 0){
        mu = atof(argv[muIndex + 1]);
    }
    if(rIndex >= 0){
        r = atof(argv[rIndex + 1]);
    }
    if(BIndex >= 0){
        B = atoll(argv[BIndex + 1]);
    }
    if(PIndex >= 0){
        P = atoll(argv[PIndex + 1]);
    }
    if(numIndex >= 0){
        num = atoll(argv[numIndex + 1]);
    }
    if(tsfileIndex >= 0){
        readTsFileConfig(argc, argv);
    }
}

void setDefault(){
    sToUs = 1e6;
    msToUs = 1e3;
    timeStampLimit = 1e8;
    timeStampStrSize = 13;

    lambda = 1;
    mu = 0.35;
    r = 1.5;
    B = 10;
    P = 3;
    num = 20;

    fileInput = NULL;
    lineNum = 0;

    numIndex = -1;
    lambdaIndex = -1;
    muIndex = -1;
    rIndex = -1;
    BIndex = -1;
    PIndex = -1;
    tsfileIndex = -1;

    packetId = 0;
    tokenId = 0;
    curTokenSize = 0;
    tokenDropSize = 0;

    My402ListInit(&inputQ);
    My402ListInit(&outputQ);
    My402ListInit(&Q1);
    My402ListInit(&Q2);

    sigemptyset(&mask);
}

void init(){
    allInterPacketTime = round(1.0 / lambda * msToUs) * msToUs;
    allPacketServiceTime = round(1.0 / mu * msToUs) * msToUs;
    interTokenTime = round(1.0 / r * msToUs) * msToUs;

    allInterPacketTime = myMin(allInterPacketTime, 10.0 * sToUs);
    allPacketServiceTime = myMin(allPacketServiceTime, 10.0 * sToUs);
    interTokenTime = myMin(interTokenTime, 10.0 * sToUs);

    inputQSize = num;
    
    pthread_mutex_init(&myLock, NULL);
    pthread_cond_init(&cv, NULL);

    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void printConfig(int argc, char* argv[]){
    fprintf(stdout, "Emulation Parameters:\n");
    fprintf(stdout, "\tnumber to arrive = %lld\n", num);
    if(tsfileIndex == -1){
        fprintf(stdout, "\tlambda = %.6g\n", lambda);
        fprintf(stdout, "\tmu = %.6g\n", mu);
    }
    fprintf(stdout, "\tr = %.6g\n", r);
    fprintf(stdout, "\tB = %lld\n", B);
    if(tsfileIndex == -1){
        fprintf(stdout, "\tP = %lld\n", P);
    }
    if(tsfileIndex >= 0){
        fprintf(stdout, "\ttsfile = %s\n", argv[tsfileIndex + 1]);
    }
    fprintf(stdout, "\n");
    // fprintf(stdout, "\tall inter-packet time = %lld\n", allInterPacketTime);
    // fprintf(stdout, "\tall inter-token time = %lld\n", interTokenTime);
    // fprintf(stdout, "\tall packet-service time = %lld\n", allPacketServiceTime);
}

void printStatics(){
    double totalRealInterPacketArriveTime = 0;
    double totalRealServiceTime = 0;
    double totalTimeInQ1 = 0;
    double totalTimeInQ2 = 0;
    double totalTimeInS1 = 0;
    double totalTimeInS2 = 0;
    double totalTimeInSystem = 0;

    long long packetServeSize = 0;
    long long packetDropSize = 0;

    for(My402ListElem* cur = My402ListFirst(&outputQ); cur != NULL; cur = My402ListNext(&outputQ, cur)){
        MyPacket* curPacket = (MyPacket*) cur->obj;

        totalRealInterPacketArriveTime += myRound(curPacket->realInterPacketArriveTime / msToUs, 3);
        
        if(curPacket->packetType == 1){
            packetServeSize++;

            double curRealServiceTime = myRound((curPacket->endServiceTime - curPacket->beginServiceTime) / msToUs, 3);
            totalRealServiceTime += curRealServiceTime;

            totalTimeInQ1 += myRound((curPacket->leaveQ1Time - curPacket->enterQ1Time) / msToUs, 3);
            totalTimeInQ2 += myRound((curPacket->leaveQ2Time - curPacket->enterQ2Time) / msToUs, 3);

            if(curPacket->serviceType == 1){
                totalTimeInS1 += curRealServiceTime;
            }
            else{
                totalTimeInS2 += curRealServiceTime;
            }

            totalTimeInSystem += myRound((curPacket->endServiceTime - curPacket->arriveTime) / msToUs, 3);
        }
        else if(curPacket->packetType == 2){
            packetDropSize++;
        }
    }

    double avgRealInterPacketArriveTime = -1;
    double avgRealServiceTime = -1;

    double avgNumPacketInQ1 = -1;
    double avgNumPacketInQ2 = -1;
    double avgNumPacketInS1 = -1;
    double avgNumPacketInS2 = -1;
    
    double avgPacketSystemTime = -1;
    double stdevSystemTime = -1;
    
    double tokenDropProb = -1;
    double packetDropProb = -1;
    
    long long totalEmulationTime = calTimeDiff(emulationStartTime, emulationEndTime);

    if(num > 0){
        avgRealInterPacketArriveTime = totalRealInterPacketArriveTime / num;
        packetDropProb = (double)packetDropSize / num;

        if(packetServeSize > 0){
            avgRealServiceTime = totalRealServiceTime / packetServeSize;
            avgPacketSystemTime = totalTimeInSystem / packetServeSize;

            double variance = 0;
            for(My402ListElem* cur = My402ListFirst(&outputQ); cur != NULL; cur = My402ListNext(&outputQ, cur)){
                MyPacket* curPacket = (MyPacket*) cur->obj;
                if(curPacket->packetType == 1){
                    double curSystemTime = myRound((curPacket->endServiceTime - curPacket->arriveTime) / msToUs , 3);
                    variance += pow(curSystemTime - avgPacketSystemTime, 2);
                }                
            }
            variance /= packetServeSize;
            stdevSystemTime = sqrt(variance);
        }
    }

    if(totalEmulationTime > 0){
        double totalEmulationTimeMS = myRound(totalEmulationTime / msToUs, 3);
        avgNumPacketInQ1 = totalTimeInQ1 / totalEmulationTimeMS;
        avgNumPacketInQ2 = totalTimeInQ2 / totalEmulationTimeMS;
        avgNumPacketInS1 = totalTimeInS1 / totalEmulationTimeMS;
        avgNumPacketInS2 = totalTimeInS2 / totalEmulationTimeMS;
    }

    if(tokenId > 0){
        tokenDropProb = (double)tokenDropSize / tokenId;
    }

    fprintf(stdout, "\nStatistics:\n");
    fprintf(stdout, "\n");

    if(num > 0){
        fprintf(stdout, "\taverage packet inter-arrival time = %.6g\n", avgRealInterPacketArriveTime / msToUs);
    }
    else{
        fprintf(stdout, "\taverage packet inter-arrival time = %s\n", "N/A, no packet was served");
    }

    if(packetServeSize > 0){
        fprintf(stdout, "\taverage packet service time = %.6g\n", avgRealServiceTime / msToUs);
    }
    else{
        fprintf(stdout, "\taverage packet service time = %s\n", "N/A, no packet was served");
    }

    fprintf(stdout, "\n");

    if(totalEmulationTime > 0){
        fprintf(stdout, "\taverage number of packets in Q1 = %.6g\n", avgNumPacketInQ1);
        fprintf(stdout, "\taverage number of packets in Q2 = %.6g\n", avgNumPacketInQ2);
        fprintf(stdout, "\taverage number of packets in S1 = %.6g\n", avgNumPacketInS1);
        fprintf(stdout, "\taverage number of packets in S2 = %.6g\n", avgNumPacketInS2);
    }
    else{
        fprintf(stdout, "\taverage number of packets in Q1 = %s\n", "N/A, no emulation time");
        fprintf(stdout, "\taverage number of packets in Q2 = %s\n", "N/A, no emulation time");
        fprintf(stdout, "\taverage number of packets in S1 = %s\n", "N/A, no emulation time");
        fprintf(stdout, "\taverage number of packets in S2 = %s\n", "N/A, no emulation time");
    }

    fprintf(stdout, "\n");

    if(packetServeSize > 0){
        fprintf(stdout, "\taverage time a packet spent in system = %.6g\n", avgPacketSystemTime / msToUs);
        fprintf(stdout, "\tstandard deviation for time spent in system = %.6g\n", stdevSystemTime / msToUs);
    }
    else{
        fprintf(stdout, "\taverage time a packet spent in system = %s\n", "N/A, no packet was served");
        fprintf(stdout, "\tstandard deviation for time spent in system = %s\n", "N/A, no packet was served");
    }

    fprintf(stdout, "\n");

    if(tokenId > 0){
        fprintf(stdout, "\ttoken drop probability = %.6g\n", tokenDropProb);
    }
    else{
        fprintf(stdout, "\ttoken drop probability = %s\n", "N/A, no token created");
    }

    if(num > 0){
        fprintf(stdout, "\tpacket drop probability = %.6g\n", packetDropProb);
    }
    else{
        fprintf(stdout, "\tpacket drop probability = %s\n", "N/A, no packet was served");
    }
}

void cleanUp(){
    My402ListUnlinkAll(&outputQ);
    if(tsfileIndex >= 0){
        fclose(fileInput);
    }
}

void* packetFunc(void* argv){
    while(inputQSize > 0){
        PacketData* packetData = createPacketData();
        if(tsfileIndex >= 0){
            readTsFileData(packetData);
        }

        if(packetData->interPcketTime > 0){
            usleep(packetData->interPcketTime);
        }
        
        pthread_mutex_lock(&myLock);

        if(inputQSize <= 0){
            pthread_mutex_unlock(&myLock);
            continue;
        }

        inputQSize--;
        MyPacket* inputPacket = createPacket();
        initPacket(inputPacket, packetData);

        struct timeval curArriveTime;
        gettimeofday(&curArriveTime, NULL);
        
        char timeStampStr[timeStampStrSize];
        getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curArriveTime));

        long long curArriveTimeDiff = calTimeDiff(prePacketArriveTime, curArriveTime);
        double curArriveTimeDiffMS = curArriveTimeDiff / msToUs;

        inputPacket->arriveTime = calTimeDiff(emulationStartTime, curArriveTime);
        inputPacket->realInterPacketArriveTime = curArriveTimeDiff;

        prePacketArriveTime.tv_sec = curArriveTime.tv_sec;
        prePacketArriveTime.tv_usec = curArriveTime.tv_usec;
        
        if(inputPacket->tokenNeed > B){
            inputPacket->packetType = 2;
            My402ListAppend(&outputQ, inputPacket);

            fprintf(stdout, "%sms: p%lld arrives, needs %lld tokens, inter-arrival time = %.3fms, dropped\n", timeStampStr, inputPacket->packetId, inputPacket->tokenNeed, curArriveTimeDiffMS);
        }
        else{
            fprintf(stdout, "%sms: p%lld arrives, needs %lld tokens, inter-arrival time = %.3fms\n", timeStampStr, inputPacket->packetId, inputPacket->tokenNeed, curArriveTimeDiffMS);

            My402ListAppend(&Q1, inputPacket);

            struct timeval curEnterQ1Time;
            gettimeofday(&curEnterQ1Time, NULL);

            getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curEnterQ1Time));

            long long curEnterQ1TimeDiff = calTimeDiff(emulationStartTime, curEnterQ1Time);
            inputPacket->enterQ1Time = curEnterQ1TimeDiff;
            fprintf(stdout, "%sms: p%lld enters Q1\n", timeStampStr, inputPacket->packetId);

            if(!My402ListEmpty(&Q1)){
                My402ListElem* elem = My402ListFirst(&Q1);
                MyPacket* q1Packet = (MyPacket*)elem->obj;

                if(curTokenSize >= q1Packet->tokenNeed){
                    My402ListUnlink(&Q1, elem);
                    curTokenSize -= q1Packet->tokenNeed;

                    struct timeval curLeaveQ1Time;
                    gettimeofday(&curLeaveQ1Time, NULL);

                    getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curLeaveQ1Time));

                    long long curLeaveQ1TimeDiff = calTimeDiff(emulationStartTime, curLeaveQ1Time);
                    q1Packet->leaveQ1Time = curLeaveQ1TimeDiff;
                    double timeInQ1 = (q1Packet->leaveQ1Time - q1Packet->enterQ1Time) / msToUs;

                    fprintf(stdout, "%sms: p%lld leaves Q1, time in Q1 = %.3fms, token bucket now has %lld tokens\n", timeStampStr, q1Packet->packetId, timeInQ1, curTokenSize);

                    My402ListAppend(&Q2, q1Packet);
                    
                    struct timeval curEnterQ2Time;
                    gettimeofday(&curEnterQ2Time, NULL);

                    getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curEnterQ2Time));

                    long long curEnterQ2TimeDiff = calTimeDiff(emulationStartTime, curEnterQ2Time);
                    q1Packet->enterQ2Time = curEnterQ2TimeDiff;

                    fprintf(stdout, "%sms: p%lld enters Q2\n", timeStampStr, q1Packet->packetId);

                    pthread_cond_broadcast(&cv);
                }
            }
        }
        
        pthread_mutex_unlock(&myLock);
    }
    // fprintf(stdout, "inputQSize: %lld, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, My402ListLength(&Q1), My402ListLength(&Q2), My402ListLength(&outputQ));
    // fprintf(stdout, "packet thread end!!!\n");
    return NULL;
}

void* tokenFunc(void* argv){
    while(inputQSize > 0 || !My402ListEmpty(&Q1)){
        if(interTokenTime > 0){
            usleep(interTokenTime);
        }

        pthread_mutex_lock(&myLock);

        if(inputQSize <= 0 && My402ListEmpty(&Q1)){
            pthread_mutex_unlock(&myLock);
            continue;
        }
        
        tokenId++;

        struct timeval tokenArriveTime;
        gettimeofday(&tokenArriveTime, NULL);

        char timeStampStr[timeStampStrSize];
        getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, tokenArriveTime));

        if(curTokenSize >= B){
            tokenDropSize++;
            fprintf(stdout, "%sms: token t%lld arrives, dropped\n", timeStampStr, tokenId);
        }
        else{
            curTokenSize++;
            fprintf(stdout, "%sms: token t%lld arrives, token bucket now has %lld tokens\n", timeStampStr, tokenId, curTokenSize);
        }
    
        if(!My402ListEmpty(&Q1)){
            My402ListElem* elem = My402ListFirst(&Q1);
            MyPacket* q1Packet = (MyPacket*)elem->obj;

            if(curTokenSize >= q1Packet->tokenNeed){
                My402ListUnlink(&Q1, elem);
                curTokenSize -= q1Packet->tokenNeed;

                struct timeval curLeaveQ1Time;
                gettimeofday(&curLeaveQ1Time, NULL);

                getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curLeaveQ1Time));

                long long curLeaveQ1TimeDiff = calTimeDiff(emulationStartTime, curLeaveQ1Time);
                q1Packet->leaveQ1Time = curLeaveQ1TimeDiff;
                double timeInQ1 = (q1Packet->leaveQ1Time - q1Packet->enterQ1Time) / msToUs;

                fprintf(stdout, "%sms: p%lld leaves Q1, time in Q1 = %.3fms, token bucket now has %lld tokens\n", timeStampStr, q1Packet->packetId, timeInQ1, curTokenSize);

                My402ListAppend(&Q2, q1Packet);
                
                struct timeval curEnterQ2Time;
                gettimeofday(&curEnterQ2Time, NULL);

                getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curEnterQ2Time));

                long long curEnterQ2TimeDiff = calTimeDiff(emulationStartTime, curEnterQ2Time);
                q1Packet->enterQ2Time = curEnterQ2TimeDiff;

                fprintf(stdout, "%sms: p%lld enters Q2\n", timeStampStr, q1Packet->packetId);

                pthread_cond_broadcast(&cv);
            }
        }

        pthread_mutex_unlock(&myLock);
    }
    // fprintf(stdout, "inputQSize: %lld, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, My402ListLength(&Q1), My402ListLength(&Q2), My402ListLength(&outputQ));
    // fprintf(stdout, "token thread end!!!\n");
    return NULL;
}

void* serverFunc(void* argv){
    char* name = (char*) argv;
    while(inputQSize > 0 || !My402ListEmpty(&Q1) || !My402ListEmpty(&Q2)){
        pthread_mutex_lock(&myLock);

        while(My402ListEmpty(&Q2) && inputQSize > 0 && !My402ListEmpty(&Q1)){
            pthread_cond_wait(&cv, &myLock);
        }

        MyPacket* q2Packet = NULL;
        char timeStampStr[timeStampStrSize];

        if(!My402ListEmpty(&Q2)){
            My402ListElem* elem = My402ListFirst(&Q2);
            q2Packet = (MyPacket*)elem->obj;

            My402ListUnlink(&Q2, elem);

            struct timeval curLeaveQ2Time;
            gettimeofday(&curLeaveQ2Time, NULL);

            getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curLeaveQ2Time));

            long long curLeaveQ2TimeDiff = calTimeDiff(emulationStartTime, curLeaveQ2Time);
            q2Packet->leaveQ2Time = curLeaveQ2TimeDiff;
            double timeInQ2 = (q2Packet->leaveQ2Time - q2Packet->enterQ2Time) / msToUs;

            fprintf(stdout, "%sms: p%lld leaves Q2, time in Q2 = %.3fms\n", timeStampStr, q2Packet->packetId, timeInQ2);

            q2Packet->packetType = 1;
            if(strcmp("S1", name) == 0){
                q2Packet->serviceType = 1;
            }
            else{
                q2Packet->serviceType = 2;
            }

            My402ListAppend(&outputQ, q2Packet);
            
            struct timeval curBeginServiceTime;
            gettimeofday(&curBeginServiceTime, NULL);

            getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curBeginServiceTime));

            long long curBeginServiceTimeDiff = calTimeDiff(emulationStartTime, curBeginServiceTime);
            q2Packet->beginServiceTime = curBeginServiceTimeDiff;
            double packetServiceTime = q2Packet->packetServiceTime / msToUs;

            fprintf(stdout, "%sms: p%lld begins service at %s, requesting %.0fms of service\n", timeStampStr, q2Packet->packetId, name, packetServiceTime);

            pthread_cond_broadcast(&cv);
        }
        
        pthread_mutex_unlock(&myLock);

        if(q2Packet != NULL){
            if(q2Packet->packetServiceTime > 0){
                usleep(q2Packet->packetServiceTime);
            }

            struct timeval curEndServiceTime;
            gettimeofday(&curEndServiceTime, NULL);

            getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curEndServiceTime));

            long long curEndServiceTimeDiff = calTimeDiff(emulationStartTime, curEndServiceTime);
            q2Packet->endServiceTime = curEndServiceTimeDiff;
            double curRealServiceTime = (q2Packet->endServiceTime - q2Packet->beginServiceTime) / msToUs;
            double curSystemTime = (q2Packet->endServiceTime - q2Packet->arriveTime) / msToUs;

            fprintf(stdout, "%sms: p%lld departs from %s, service time = %.3fms, time in system = %.3fms\n", timeStampStr, q2Packet->packetId, name, curRealServiceTime, curSystemTime);
        }
        
    }
    // fprintf(stdout, "inputQSize: %lld, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, My402ListLength(&Q1), My402ListLength(&Q2), My402ListLength(&outputQ));
    // fprintf(stdout, "%s thread end!!!\n", name);
    return NULL;
}

void* signalFunc(void* argv){
    int mySignal = -1;
    while(mySignal < 0){
        sigwait(&mask, &mySignal);
        
        pthread_mutex_lock(&myLock);
        
        struct timeval curSignalCatchTime;
        gettimeofday(&curSignalCatchTime, NULL);

        char timeStampStr[timeStampStrSize];
        getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curSignalCatchTime));

        fprintf(stdout, "\n%sms: SIGINT caught, no new packets or tokens will be allowed\n", timeStampStr);

        struct timeval curRemovePacketTime;
        for(My402ListElem* cur = My402ListFirst(&Q1); cur != NULL; cur = My402ListNext(&Q1, cur)){
            MyPacket* curPacket = (MyPacket*) cur->obj;
            curPacket->packetType = 3;

            My402ListAppend(&outputQ, curPacket);

            gettimeofday(&curRemovePacketTime, NULL);
            getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curRemovePacketTime));

            fprintf(stdout, "%sms: p%lld removed from Q1\n", timeStampStr, curPacket->packetId);
        }
        for(My402ListElem* cur = My402ListFirst(&Q2); cur != NULL; cur = My402ListNext(&Q2, cur)){
            MyPacket* curPacket = (MyPacket*) cur->obj;
            curPacket->packetType = 3;

            My402ListAppend(&outputQ, curPacket);

            gettimeofday(&curRemovePacketTime, NULL);
            getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, curRemovePacketTime));

            fprintf(stdout, "%sms: p%lld removed from Q2\n", timeStampStr, curPacket->packetId);
        }
        inputQSize = 0;
        My402ListUnlinkAll(&Q1);
        My402ListUnlinkAll(&Q2);
        
        pthread_cond_broadcast(&cv);
        
        pthread_mutex_unlock(&myLock);
    }
    // fprintf(stdout, "inputQSize: %lld, Q1: %d, Q2: %d, outputQ: %d\n", inputQSize, My402ListLength(&Q1), My402ListLength(&Q2), My402ListLength(&outputQ));
    // fprintf(stdout, "signal thread end!!!\n");
    return NULL;
}

int main(int argc, char* argv[]){
    setDefault();

    checkInput(argc, argv);

    readInput(argc, argv);

    init();

    printConfig(argc, argv);

    gettimeofday(&emulationStartTime, NULL);
    gettimeofday(&prePacketArriveTime, NULL);

    char timeStampStr[timeStampStrSize];
    getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, emulationStartTime));

    fprintf(stdout, "%sms: emulation begins\n", timeStampStr);

    pthread_create(&sig, NULL, signalFunc, "sig");
    pthread_create(&packet, NULL, packetFunc, "packet");
    pthread_create(&token, NULL, tokenFunc, "token");
    pthread_create(&s1, NULL, serverFunc, "S1");
    pthread_create(&s2, NULL, serverFunc, "S2");

    pthread_join(packet, NULL);
    pthread_join(token, NULL);
    pthread_join(s1, NULL);
    pthread_join(s2, NULL);

    gettimeofday(&emulationEndTime, NULL);

    getTimeStampStr(timeStampStr, timeStampStrSize, calTimeDiff(emulationStartTime, emulationEndTime));

    fprintf(stdout, "%sms: emulation ends\n", timeStampStr);

    printStatics();

    cleanUp();
}