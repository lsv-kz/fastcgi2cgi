#ifndef CLASS_ARRAY_H_
#define CLASS_ARRAY_H_
#include <iostream>
#include <cstring>
#include <sstream>

//======================================================================
template <typename T>
class Array
{
protected:
    const int ADDITION = 8;
    T *t = NULL;
    unsigned int sizeBuf = 0;
    unsigned int lenBuf = 0;
    const char *err = "Success";
    
    int append(const T& val)
    {
        if (lenBuf >= sizeBuf)
            if (reserve(sizeBuf + ADDITION)) throw ENOMEM;
        t[lenBuf++] = val;
        return 0;
    }
    
public:
    Array(const Array&) = delete;
    Array() { }
    Array(int n) { if (reserve(n)) throw ENOMEM; }
    ~Array() {/*std::cout << "~Array()\n";*/ if (t) { delete [] t; } }
    Array<T> & operator << (const T& val) { append(val); return *this; }

    int reserve(unsigned int n)
    {
        if (n <= lenBuf)
            return 1;
        T *tmp = new(std::nothrow) T [n];
        if (!tmp)
            return 1;
        for (unsigned int i = 0; i < lenBuf; ++i)
            tmp[i] = t[i];
        if (t)
            delete [] t;
        t = tmp;
        sizeBuf = n;
        return 0;
    }
    
    const char *error()
    {
        const char *p = err;
        err = "Success";
        return p;
    }  
    
    T *get(unsigned int i)
    {
        if (i < lenBuf)
            return t+i;
        else
            return NULL;
    }
    
    int len() { return lenBuf; }
    int capacity() { return sizeBuf; }
};

#endif
