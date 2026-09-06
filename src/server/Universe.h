#ifndef _UNIVERSE_H_
#define _UNIVERSE_H_

#include "Singleton.h"
#include <cstdint>

class Universe
{
public:
    Universe() : m_Counter(0) {}

    uint64_t const getCounter()
    {
        return m_Counter;
    }

    void incrementCounter()
    {
        m_Counter++;
    }

private:
    uint64_t m_Counter;
};

// Define Universe singleton
static Singleton2<Universe> __Universe;
#define sUniverse           __Universe.getInstance()

#endif // _UNIVERSE_H_