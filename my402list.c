#include <stdio.h>
#include <stdlib.h>
#include "my402list.h"
#include "cs402.h"

int My402ListLength(My402List* list){    
    return list->num_members;
}

int My402ListEmpty(My402List* list){
    if (list->num_members == 0){
        return 1;
    }
    return 0;
}

int My402ListAppend(My402List* list, void* objc){
    My402ListElem * newEle = (My402ListElem*) malloc(sizeof(My402ListElem));
    newEle->obj = objc;
    int initial_members = list->num_members;

    if(My402ListEmpty(list) != 0){
        newEle->next = &(list->anchor);
        newEle->prev = &(list->anchor);
        list->anchor.next = newEle;
        list->anchor.prev = newEle;
        list->num_members++;
    }
    else{
        newEle->prev = list->anchor.prev;
        newEle->next = &(list->anchor);
        list->anchor.prev->next = newEle;
        list->anchor.prev = newEle;
        list->num_members++;
    }
    
    //Verifying Post-Condition to check if the operation was successful or not.
    if (initial_members == list->num_members - 1){
        return 1;
    }

    return 0;
}

int  My402ListPrepend(My402List* list, void* objc){
    My402ListElem * newEle = (My402ListElem*) malloc(sizeof(My402ListElem));
    newEle->obj = objc;
    int initial_members = list->num_members;

    if(My402ListEmpty(list) != 0){
        newEle->next = &(list->anchor);
        newEle->prev = &(list->anchor);
        list->anchor.next = newEle;
        list->anchor.prev = newEle;
        list->num_members++;
    }

    else{
        newEle->prev = &(list->anchor);
        newEle->next = list->anchor.next;
        list->anchor.next->prev = newEle;
        list->anchor.next = newEle;
        list->num_members++;
    } 

    //Verifying Post-Condition to check if the operation was successful or not.
    if (initial_members == list->num_members - 1){
        return 1;
    }

    return 0;
}

void My402ListUnlink(My402List* list, My402ListElem* elem){
    //Creating temporary pointers for better code redability
    My402ListElem* point_prev = elem->prev;
    My402ListElem* point_next = elem->next;
    point_prev->next = elem->next;
    point_next->prev = elem->prev;
    free(elem);
    list->num_members--;
}

void My402ListUnlinkAll(My402List* list){
    while (list->num_members != 0)
    {
        My402ListUnlink(list, list->anchor.next);
    }
}

int  My402ListInsertAfter(My402List* list, void* objc, My402ListElem* elem){
    
    int initial_members = list->num_members;

    if (elem == NULL){
        My402ListAppend(list, objc);
    }
    else{
        My402ListElem* newEle = (My402ListElem*) malloc(sizeof(My402ListElem));
        newEle->obj = objc;
        My402ListElem* point_next = elem->next;
        elem->next = newEle;
        newEle->prev = elem;
        newEle->next = point_next;
        point_next->prev = newEle;
        list->num_members++;
    }

    if (initial_members == list->num_members-1){
        return 1;
    }
    return 0;
}

int  My402ListInsertBefore(My402List* list, void* objc, My402ListElem* elem){

    int initial_members = list->num_members;

    if (elem == NULL){
        My402ListPrepend(list, objc);
    }
    else{
        My402ListElem* newEle = (My402ListElem*) malloc(sizeof(My402ListElem));
        newEle->obj = objc;
        My402ListElem* point_prev = elem->prev;
        elem->prev = newEle;
        newEle->prev = point_prev;
        newEle->next = elem;
        point_prev->next = newEle;
        list->num_members++;
    }

    if (initial_members == list->num_members-1){
        return 1;
    }
    return 0;
}

My402ListElem *My402ListFirst(My402List* list){

    if(My402ListEmpty(list) != 0){
        return NULL;
    }
    return list->anchor.next;
}

My402ListElem *My402ListLast(My402List* list){

    if(My402ListEmpty(list) != 0){
        return NULL;
    }    
    return list->anchor.prev;
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem){
 
    if(list->anchor.prev == elem){
        return NULL;
    }
    return elem->next;
}

My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem){
   
    if(list->anchor.next == elem){
        return NULL;
    }
    return elem->prev;
}

My402ListElem *My402ListFind(My402List* list, void* objc){
    My402ListElem* pointer = list->anchor.next;
    while(list->anchor.prev != pointer){
        if(pointer->obj == objc){
            return pointer;
        }
        pointer = pointer->next;
    }

    // If it exits the while loop without returning that means either the objc we are finding is
    // linked to the last element, or the objc just does not exists in the list.
    
    if (pointer->obj == objc){
        return pointer;
    }

    return NULL;
}


int My402ListInit(My402List* list){
    list->num_members = 0;
    list->anchor.next = &(list->anchor);
    list->anchor.prev = &(list->anchor);
    
    if ((list->num_members == 0) && (list->anchor.next == list->anchor.prev)){
        return 1;
    }
    return 0;
}


