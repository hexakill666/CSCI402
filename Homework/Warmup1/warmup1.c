#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "my402list.h"
#include "my402listobj.h"

void checkLine(char line[], int lineLen, int lineNum){
    if(lineLen > 1024){
        fprintf(stderr, "line%d, field: line, the line length is larger than 1024.\n", lineNum);
        exit(1);
    }
    int tabCount = 0;
    for(int a = 0; a < lineLen; a++){
        if(line[a] == '\t'){
            tabCount++;
        }
    }
    if(tabCount != 3){
        fprintf(stderr, "line%d, field: line, the line does not have exactly 4 fields.\n", lineNum);
        exit(1);
    }
}

void checkType(char type[], int typeLen, int lineNum){
    if(typeLen != 1){
        fprintf(stderr, "line%d, field: type, the type does not have only 1 char.\n", lineNum);
        exit(1);
    }
    if(type[0] != '+' && type[0] != '-'){
        fprintf(stderr, "line%d, field: type, the type should be + or -.\n", lineNum);
        exit(1);
    }
}

long checkTime(char myTime[], int myTimeLen, int lineNum){
    if(myTimeLen < 1 || myTimeLen >= 11){
        fprintf(stderr, "line%d, field: time, the time length should be >= 1 and <= 10.\n", lineNum);
        exit(1);
    }
    for(int a = 0; a < myTimeLen; a++){
        if(myTime[a] < '0' || myTime[a] > '9'){
            fprintf(stderr, "line%d, field: time, the time has non-digit char.\n", lineNum);
            exit(1);
        }
    }
    if(myTime[0] == '0'){
        fprintf(stderr, "line%d, field: time, the time first digit is 0.\n", lineNum);
        exit(1);
    }
    long long inputTime = atoll(myTime);
    if(inputTime < 0 || inputTime > time(NULL)){
        fprintf(stderr, "line%d, field: time, the time should not < 0 or > curTime.\n", lineNum);
        exit(1);
    }
    return atol(myTime);
}

long long checkAmount(char amount[], int amountLen, int lineNum){
    if(amountLen < 1){
        fprintf(stderr, "line%d, field: amount, the amount length should be > 0.\n", lineNum);
        exit(1);
    }
    int dotIndex = 0;
    int dotCount = 0;
    for(int a = 0; a < amountLen; a++){
        if(amount[a] == '.'){
            dotIndex = a;
            dotCount++;
        }
    }
    if(dotCount != 1){
        fprintf(stderr, "line%d, field: amount, the amount does not have exactly only 1 dot.\n", lineNum);
        exit(1);
    }
    for(int a = 0; a < dotIndex; a++){
        if(amount[a] < '0' || amount[a] > '9'){
            fprintf(stderr, "line%d, field: amount, the amount has non-digit char.\n", lineNum);
            exit(1);
        }
    }
    for(int a = dotIndex + 1; a < amountLen; a++){
        if(amount[a] < '0' || amount[a] > '9'){
            fprintf(stderr, "line%d, field: amount, the amount has non-digit char.\n", lineNum);
            exit(1);
        }
    }
    if(dotIndex > 7){
        fprintf(stderr, "line%d, field: amount, the amount number before dot has more than 7 digits.\n", lineNum);
        exit(1);
    }
    if(amountLen - 1 - dotIndex != 2){
        fprintf(stderr, "line%d, field: amount, the amount number after dot does not have exactly only 2 digits.\n", lineNum);
        exit(1);
    }
    char numPrevDot[16];
    char numNextDot[16];
    memset(numPrevDot, 0, 16);
    memset(numNextDot, 0, 16);
    for(int a = 0; a < dotIndex; a++){
        numPrevDot[a] = amount[a];
    }
    for(int a = dotIndex + 1, b = 0; a < amountLen && b < 16; a++, b++){
        numNextDot[b] = amount[a];
    }
    long long numPrev = atoll(numPrevDot);
    long long numNext = atoll(numNextDot);
    if(numPrev > 0 || numNext > 0){
        if(dotIndex - 1 != 0 && amount[0] == '0'){
            fprintf(stderr, "line%d, field: amount, the amount non-zero has leading 0.\n", lineNum);
            exit(1);
        }
    }
    else{
        fprintf(stderr, "line%d, field: amount, the amount cannot be 0.\n", lineNum);
        exit(1);
    }
    return numPrev * 100 + numNext;
}

void checkDesc(char desc[], int descLen, int lineNum){
    if(descLen < 1){
        fprintf(stderr, "line%d, field: desc, the description length should be > 0.\n", lineNum);
        exit(1);
    }
    int nonZeroIndex = 0;
    for(int a = 0; a < descLen; a++){
        if(desc[a] != ' '){
            nonZeroIndex = a;
            break;
        }
    }
    if(nonZeroIndex >= descLen){
        fprintf(stderr, "line%d, field: desc, the description cannot be empty.\n", lineNum);
        exit(1);
    }
    char temp[25];
    memset(temp, 0, sizeof(temp));
    for(int a = nonZeroIndex, b = 0; a < descLen && b < 25; a++, b++){
        temp[b] = desc[a];
    }
    strcpy(desc, temp);
}

void BubbleForward(My402List *pList, My402ListElem **pp_elem1, My402ListElem **pp_elem2)
    /* (*pp_elem1) must be closer to First() than (*pp_elem2) */
{
    My402ListElem* elem1 = (*pp_elem1), *elem2 = (*pp_elem2);
    void* obj1 = elem1->obj, *obj2 = elem2->obj;
    My402ListElem* elem1prev = My402ListPrev(pList, elem1);
/*  My402ListElem *elem1next=My402ListNext(pList, elem1); */
/*  My402ListElem *elem2prev=My402ListPrev(pList, elem2); */
    My402ListElem* elem2next = My402ListNext(pList, elem2);

    My402ListUnlink(pList, elem1);
    My402ListUnlink(pList, elem2);
    if (elem1prev == NULL) {
        (void)My402ListPrepend(pList, obj2);
        *pp_elem1 = My402ListFirst(pList);
    } 
    else {
        (void)My402ListInsertAfter(pList, obj2, elem1prev);
        *pp_elem1 = My402ListNext(pList, elem1prev);
    }
    if (elem2next == NULL) {
        (void)My402ListAppend(pList, obj1);
        *pp_elem2 = My402ListLast(pList);
    } 
    else {
        (void)My402ListInsertBefore(pList, obj1, elem2next);
        *pp_elem2 = My402ListPrev(pList, elem2next);
    }
}

void BubbleSortForwardList(My402List *pList, int num_items){
    My402ListElem* elem = NULL;
    int i = 0;

    if (My402ListLength(pList) != num_items) {
        fprintf(stderr, "List length is not %1d in BubbleSortForwardList().\n", num_items);
        exit(1);
    }
    for (i = 0; i < num_items; i++) {
        int j = 0, something_swapped = FALSE;
        My402ListElem* next_elem = NULL;

        for (elem = My402ListFirst(pList), j = 0; j < num_items-i - 1; elem = next_elem, j++) {
            My402ListElemObj* curObj = (My402ListElemObj*)(elem->obj);
            long long cur_val = curObj->timestamp;

            next_elem = My402ListNext(pList, elem);
            My402ListElemObj* nextObj = (My402ListElemObj*)(next_elem->obj);
            long long next_val = nextObj->timestamp;

            if(cur_val == next_val){
                fprintf(stderr, "line%d and %d, field: time, there are two identical timestamps.\n", curObj->lineNum, nextObj->lineNum);
                exit(1);
            }

            if (cur_val > next_val) {
                BubbleForward(pList, &elem, &next_elem);
                something_swapped = TRUE;
            }
        }
        if(!something_swapped){
            break;
        }
    }
}

void transferFormat(long long money, char temp[]){
    memset(temp, 0, 16);
    temp[0] = ' ';
    temp[13] = ' ';
    if(money < 0){
        temp[0] = '(';
        temp[13] = ')';
    }
    double moneyLimit = 1e9;
    if(money <= -moneyLimit || money >= moneyLimit){
        for(int a = 1; a < 13; a++){
            temp[a] = '?';
        }
        temp[10] = '.';
        temp[2] = ',';
        temp[6] = ',';
        return;
    }
    money = abs(money);
    int index = 12;
    while(money > 0 && index >= 1){
        int remainder = money % 10;
        temp[index] = '0' + remainder;
        money /= 10;
        if(index == 11 && money > 0){
            temp[--index] = '.';
        }
        else if(index == 7 && money > 0){
            temp[--index] = ',';
        }
        else if(index == 3 && money > 0){
            temp[--index] = ',';
        }
        index--;
    }
    while(index >= 9){
        temp[index--] = '0';
    }
    temp[10] = '.';
    for(int a = index; a >= 1; a--){
        temp[a] = ' ';
    }
}

void PrintTestList(My402List *pList, int num_items){
    My402ListElem* elem = NULL;

    if (My402ListLength(pList) != num_items) {
        fprintf(stderr, "List length is not %1d in PrintTestList().\n", num_items);
        exit(1);
    }
    for (elem = My402ListFirst(pList); elem != NULL; elem = My402ListNext(pList, elem)) {
        My402ListElemObj* curObj = (My402ListElemObj*)(elem->obj);
        fprintf(stdout, "%s %s %s %s %s %ld %lld\n", curObj->type, curObj->time, curObj->amount, curObj->desc, curObj->timeStr, curObj->timestamp, curObj->amountNum);
    }
    fprintf(stdout, "\n");
}

void printTable(My402List* myList){
    int bufferSize = 81;
    char dashLine[bufferSize];
    char secondLine[bufferSize];
    memset(dashLine, 0, bufferSize);
    memset(secondLine, 0, bufferSize);
    
    int indexArray[] = {0, 18, 45, 62, 79};
    for(int a = 0; a < bufferSize - 1; a++){
        dashLine[a] = '-';
    }
    for(int a = 0; a < 5; a++){
        int index = indexArray[a];
        dashLine[index] = '+';
    }

    for(int a = 0; a < bufferSize - 1; a++){
        secondLine[a] = ' ';
    }
    for(int a = 0; a < 5; a++){
        int index = indexArray[a];
        secondLine[index] = '|';
    }
    
    char temp1[] = {"Date"};
    char temp2[] = {"Description"};
    char temp3[] = {"Amount"};
    char temp4[] = {"Balance"};
    for(int a = 8, b = 0; a < 12 && b < strlen(temp1); a++, b++){
        secondLine[a] = temp1[b];
    }
    for(int a = 20, b = 0; a < 31 && b < strlen(temp2); a++, b++){
        secondLine[a] = temp2[b];
    }
    for(int a = 55, b = 0; a < 61 && b < strlen(temp3); a++, b++){
        secondLine[a] = temp3[b];
    }
    for(int a = 71, b = 0; a < 78 && b < strlen(temp4); a++, b++){
        secondLine[a] = temp4[b];
    }

    fprintf(stdout, "%s\n", dashLine);
    fprintf(stdout, "%s\n", secondLine);
    fprintf(stdout, "%s\n", dashLine);

    long long balance = 0;
    for (My402ListElem* elem = My402ListFirst(myList); elem != NULL; elem = My402ListNext(myList, elem)) {
        char line[81];
        memset(line, 0, 81);

        for(int a = 0; a < 80; a++){
            line[a] = ' ';
        }
        for(int a = 0; a < 5; a++){
            int index = indexArray[a];
            line[index] = '|';
        }

        char amountStr[16];
        char balanceStr[16];

        My402ListElemObj* curObj = (My402ListElemObj*)(elem->obj);
        transferFormat(curObj->amountNum, amountStr);
        balance += curObj->amountNum;
        transferFormat(balance, balanceStr);

        for(int a = 2, b = 0; a < indexArray[1] - 1 && b < strlen(curObj->timeStr); a++, b++){
            line[a] = curObj->timeStr[b];
        }
        for(int a = 20, b = 0; a < indexArray[2] - 1 && b < strlen(curObj->desc); a++, b++){
            line[a] = curObj->desc[b];
        }
        for(int a = 47, b = 0; a < indexArray[3] - 1 && b < strlen(amountStr); a++, b++){
            line[a] = amountStr[b];
        }
        for(int a = 64, b = 0; a < indexArray[4] - 1 && b < strlen(balanceStr); a++, b++){
            line[a] = balanceStr[b];
        }
        fprintf(stdout, "%s\n", line);
    }
    fprintf(stdout, "%s\n", dashLine);
}

int main(int argc, char *argv[]){
    FILE* input;
    if(argc == 1){
        fprintf(stderr, "malformed command\n");
        fprintf(stderr, "useage: ./warmup1 sort [tfile]\n");
        exit(1);
    }
    else if(argc == 2){
        if(strcmp("sort", argv[1]) != 0){
            fprintf(stderr, "malformed command, %s is not a valid commandline option\n", argv[1]);
            fprintf(stderr, "useage: ./warmup1 sort [tfile]\n");
            exit(1);
        }
        input = stdin;
    }
    else if(argc == 3){
        if(strcmp("sort", argv[1]) != 0){
            fprintf(stderr, "malformed command, %s is not a valid commandline option\n", argv[1]);
            fprintf(stderr, "useage: ./warmup1 sort [tfile]\n");
            exit(1);
        }
        if((input = fopen(argv[2], "r")) == NULL){
            fprintf(stderr, "Error opening file %s.\n", argv[2]);
            fprintf(stderr, "useage: ./warmup1 sort [tfile]\n");
            exit(1);
        }
    }
    else{
        fprintf(stderr, "malformed command\n");
        fprintf(stderr, "useage: ./warmup1 sort [tfile]\n");
        exit(1);
    }

    int bufferSize = 1050;
    char inputLine[bufferSize];
    char type[bufferSize];
    char time[bufferSize];
    char amout[bufferSize];
    char desc[bufferSize];

    My402List myList;
    memset(&myList, 0, sizeof(myList));
    My402ListInit(&myList);

    int lineNum = 0;
    while(!feof(input) && fgets(inputLine, bufferSize, input) != NULL){
        memset(type, 0, bufferSize);
        memset(time, 0, bufferSize);
        memset(amout, 0, bufferSize);
        memset(desc, 0, bufferSize);

        sscanf(inputLine, "%s %s %s %[^\n]", type, time, amout, desc);

        lineNum++;
        checkLine(inputLine, strlen(inputLine), lineNum);
        checkType(type, strlen(type), lineNum);
        long inputTime = checkTime(time, strlen(time), lineNum);
        long long myAmount = checkAmount(amout, strlen(amout), lineNum);
        checkDesc(desc, strlen(desc), lineNum);

        My402ListElemObj* obj = (My402ListElemObj*)malloc(sizeof(My402ListElemObj));
        strcpy(obj->type, type);
        strcpy(obj->time, time);
        strcpy(obj->amount, amout);
        strcpy(obj->desc, desc);
        
        struct tm* timeInfo = localtime(&inputTime);
        strftime(obj->timeStr, 16, "%a %b %d %Y", timeInfo);
        if(obj->timeStr[8] == '0'){
            obj->timeStr[8] = ' ';
        }
        obj->timestamp = inputTime;
        obj->amountNum = myAmount;
        if(type[0] == '-'){
            obj->amountNum *= -1;
        }
        obj->lineNum = lineNum;

        My402ListAppend(&myList, obj);
    }

    if(lineNum == 0){
        fprintf(stderr, "there is no transaction.\n");
        exit(1);
    }

    BubbleSortForwardList(&myList, myList.num_members);
    
    printTable(&myList);

    My402ListUnlinkAll(&myList);
    fclose(input);

    return 0;
}