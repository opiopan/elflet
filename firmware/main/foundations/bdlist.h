#pragma once

#include <esp_log.h>

template <class T> class BDList {
private:
    const char* tag;
    T* first;
    T* last;

public:
    BDList(const char* t): tag(t), first(NULL), last(NULL){};

    ~BDList(){
	T* object;
	while ((object = getLast()) != NULL){
	    remove(object);
	}
    };
    
    T* getFirst(){
	return first;
    };
    
    T* getLast(){
	return last;
    };
    
    void addAfterObject(T* object, T* at){
	if (object->next != NULL || object->prev != NULL){
	    ESP_LOGE(tag, "object relation corrupted");
	    abort();
	}
	if (at){
	    object->next = at->next;
	    object->prev = at;
	    at->next = object;
	    if (object->next){
		object->next->prev = object;
	    }else{
		last = object;
	    }
	}else{
	    if (first || last){
		ESP_LOGE(tag, "list structurecorrupted");
		abort();
	    }
	    first = object;
	    last = object;
	}
    }

    void addBeforeObject(T* object, T* at){
	if (object->next != NULL || object->prev != NULL){
	    ESP_LOGE(tag, "object relation corrupted");
	    abort();
	}
	if (at){
	    object->next = at;
	    object->prev = at->prev;
	    at->prev = object;
	    if (object->prev){
		object->prev->next = object;
	    }else{
		first = object;
	    }
	}else{
	    if (first || last){
		ESP_LOGE(tag, "list structurecorrupted");
		abort();
	    }
	    first = object;
	    last = object;
	}
    };

    void addAtFirst(T* object){
	addBeforeObject(object, first);
    };
    
    void addAtLast(T* object){
	addAfterObject(object, last);
    };

    void remove(T* object){
	if (object->prev){
	    object->prev->next = object->next;
	}else{
	    first = object->next;
	}
	if (object->next){
	    object->next->prev = object->prev;
	}else{
	    last = object->prev;
	}
	object->next = NULL;
	object->prev = NULL;
    };
};
