//----------------------------------------------------------------------
// SmartPtr.h: smart pointer template
//----------------------------------------------------------------------
#pragma once

#include <iostream>

namespace HSL{ namespace Base {

template <class T> class SmartPtr{
protected:
    class ObjectHandle{
    protected:
        T*	object;
        int	refCount;
        public:
        ObjectHandle(T* obj) : object(obj), refCount(1) {};
        void addRef(){refCount++;};
        void release()
        {
            refCount--;
            if (refCount <= 0){
                delete object;
                delete this;
            }
        };
        T* pointer() const{return object;};
    };

    ObjectHandle* handle;

public:
    explicit SmartPtr(T* obj = NULL)
    {
        if (obj){
            handle = new ObjectHandle(obj);
        }else{
	    handle = NULL;
        }
    };

    SmartPtr(const SmartPtr<T>& src)
    {
        handle = src.handle;
        if (handle){
            handle->addRef();
        }
    };

    ~SmartPtr()
    {
        if (handle){
            handle->release();
        }
    };

    SmartPtr<T>& operator = (const SmartPtr<T>& src)
    {
        if (handle)
            handle->release();
        handle = src.handle;
        if (handle)
            handle->addRef();
        return *this;
    };

    T* operator -> (void) const
    {
        return handle ? handle->pointer() : NULL;
    }

    void setNull()
    {
        if (handle)
            handle->release();
        handle = NULL;
    }

    bool isNull() const
    {
        return handle == NULL;
    }
};


}}; // end of name spaece

using namespace HSL::Base;
