#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my402list.h"

int My402ListLength(My402List* my402List){
    return my402List->num_members;
}

int My402ListEmpty(My402List* my402List){
    return my402List->num_members == 0;
}

int My402ListAppend(My402List* my402List, void* obj){
    My402ListElem* newElem = (My402ListElem*)malloc(sizeof(My402ListElem));
    if(newElem == NULL){
        fprintf(stderr, "Error malloc in append.\n");
        return FALSE;
    }
    newElem->obj = obj;
    if(My402ListEmpty(my402List)){
        my402List->num_members++;
        my402List->anchor.next = newElem;
        my402List->anchor.prev = newElem;
        newElem->prev = &my402List->anchor;
        newElem->next = &my402List->anchor;
        return TRUE;
    }
    my402List->num_members++;
    My402ListElem* last = My402ListLast(my402List);
    newElem->prev = last;
    newElem->next = &my402List->anchor;
    last->next = newElem;
    my402List->anchor.prev = newElem;
    return TRUE;
}

int My402ListPrepend(My402List* my402List, void* obj){
    My402ListElem* newElem = (My402ListElem*)malloc(sizeof(My402ListElem));
    if(newElem == NULL){
        fprintf(stderr, "Error malloc in prepend.\n");
        return FALSE;
    }
    newElem->obj = obj;
    if(My402ListEmpty(my402List)){
        my402List->num_members++;
        my402List->anchor.next = newElem;
        my402List->anchor.prev = newElem;
        newElem->prev = &my402List->anchor;
        newElem->next = &my402List->anchor;
        return TRUE;
    }
    my402List->num_members++;
    My402ListElem* first = My402ListFirst(my402List);
    newElem->prev = &my402List->anchor;
    newElem->next = first;
    my402List->anchor.next = newElem;
    first->prev = newElem;
    return TRUE;
}

void My402ListUnlink(My402List* my402List, My402ListElem* elem){
    My402ListElem* prev = elem->prev;
    My402ListElem* next = elem->next;
    prev->next = next;
    next->prev = prev;
    elem->prev = NULL;
    elem->next = NULL;
    free(elem);
    my402List->num_members--;
}

void My402ListUnlinkAll(My402List* my402List){
    My402ListElem* cur = My402ListFirst(my402List);
    My402ListElem* curNext = My402ListNext(my402List, cur);
    while(cur != NULL){
        My402ListUnlink(my402List, cur);
        cur = curNext;
        curNext = My402ListNext(my402List, curNext);
    }
    My402ListInit(my402List);
}

int My402ListInsertAfter(My402List* my402List, void* obj, My402ListElem* elem){
    if(elem == NULL){
        return My402ListAppend(my402List, obj);
    }
    my402List->num_members++;
    My402ListElem* newElem = (My402ListElem*)malloc(sizeof(My402ListElem));
    newElem->obj = obj;
    My402ListElem* next = elem->next;
    newElem->prev = elem;
    newElem->next = next;
    elem->next = newElem;
    next->prev = newElem;
    return TRUE;
}

int My402ListInsertBefore(My402List* my402List, void* obj, My402ListElem* elem){
    if(elem == NULL){
        return My402ListPrepend(my402List, obj);
    }
    my402List->num_members++;
    My402ListElem* newElem = (My402ListElem*)malloc(sizeof(My402ListElem));
    newElem->obj = obj;
    My402ListElem* prev = elem->prev;
    newElem->prev = prev;
    newElem->next = elem;
    prev->next = newElem;
    elem->prev = newElem;
    return TRUE;
}

My402ListElem* My402ListFirst(My402List* my402List){
    if(My402ListEmpty(my402List)){
        return NULL;
    }
    return my402List->anchor.next;
}

My402ListElem* My402ListLast(My402List* my402List){
    if(My402ListEmpty(my402List)){
        return NULL;
    }
    return my402List->anchor.prev;
}

My402ListElem* My402ListNext(My402List* my402List, My402ListElem* cur){
    if(My402ListLast(my402List) == cur){
        return NULL;
    }
    return cur->next;
}

My402ListElem* My402ListPrev(My402List* my402List, My402ListElem* cur){
    if(My402ListFirst(my402List) == cur){
        return NULL;
    }
    return cur->prev;
}

My402ListElem* My402ListFind(My402List* my402List, void* obj){
    for(My402ListElem* cur = My402ListFirst(my402List); cur != NULL; cur = My402ListNext(my402List, cur)){
        if(cur->obj == obj){
            return cur;
        }
    }
    return NULL;
}

int My402ListInit(My402List* my402List){
    memset(my402List, 0, sizeof(My402List));
    my402List->num_members = 0;
    my402List->anchor.prev = NULL;
    my402List->anchor.next = NULL;
    return TRUE;
}