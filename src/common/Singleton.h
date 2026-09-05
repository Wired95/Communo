#ifndef _SINGLETON_H_
#define _SINGLETON_H_

// Singleton of a class with a special constructor
template <typename T> class Singleton
{
public:
    Singleton(T a) : m_instance(a) {}
public:
    T& getInstance()
    {
        static T instance(m_instance);
        return instance;
    }

private:
    T m_instance;
};

// Singleton of a regular class
template <typename T> class Singleton2
{
public:
    static T& getInstance()
    {
        static T instance;
        return instance;
    }

public:
    static T* m_instance;
};

#endif // _SINGLETON_H_
