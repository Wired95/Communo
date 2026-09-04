#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "Singleton.h"

#include <string>
#include <iostream>

enum eLoggingOutputFlags
{
    LOG_NONE    = 0x000,
    LOG_FILE    = 0x001,
    LOG_SYSTEM  = 0x002,
    LOG_CONSOLE = 0x004,
};

enum eLoggingFlags
{
    LOG_FLAG_DEBUG  = 0x001,
    LOG_FLAG_INFO   = 0x002,
    LOG_FLAG_WARN   = 0x004,
    LOG_FLAG_ERROR  = 0x008,
};

enum eLoggingLevelFlags
{
    LOG_LEVEL_DEBUG = LOG_FLAG_DEBUG | LOG_FLAG_INFO | LOG_FLAG_WARN | LOG_FLAG_ERROR,
    LOG_LEVEL_INFO  = LOG_FLAG_INFO | LOG_FLAG_WARN | LOG_FLAG_ERROR,
    LOG_LEVEL_WARN  = LOG_FLAG_WARN | LOG_FLAG_ERROR,
    LOG_LEVEL_ERROR = LOG_FLAG_ERROR,
};

class Logger
{
public:
    Logger() :
        m_LogLevelFilter(LOG_LEVEL_INFO),
        m_LogOutputFilter(LOG_CONSOLE),
        m_LogOutputFile("") {}

    void ToConsole(const char* msg)
    {
        ToConsole(std::string(msg));
    }

    void ToConsole(const std::string& msg)
    {
        std::cout << msg << std::endl;
    }

    void log(eLoggingFlags level, const char* msg)
    {
        log(level, std::string(msg));
    }

    void log(eLoggingFlags level, const std::string& msg);

    void setLogLevel(eLoggingLevelFlags level) { m_LogLevelFilter = level; }
    void setLogOutput(eLoggingOutputFlags outFlags) { m_LogOutputFilter = outFlags; }

private:
    unsigned short int m_LogLevelFilter;
    unsigned short int m_LogOutputFilter;
    std::string m_LogOutputFile;
};




static Singleton2<Logger>    __Logger;
#define sLog                 __Logger.getInstance()

#endif // _LOGGING_H_