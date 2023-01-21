#ifndef _MY402LISTOBJ_H_
#define _MY402LISTOBJ_H_

typedef struct tagMy402ListElemObj{
    char type[2];
    char time[16];
    char amount[16];
    char desc[25];
    char timeStr[16];
    long timestamp;
    long long amountNum;
    int lineNum;
}My402ListElemObj;

#endif