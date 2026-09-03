# include "Logging.h"
# include "SharedDefinitions.h"

#include <fstream>

#ifdef __linux__
#include <syslog.h>
#include <getopt.h>
#endif

void Logger::log(eLoggingFlags level, const std::string& msg)
{
    if (m_LogLevelFilter & level)
    {
        if (m_LogOutputFilter & LOG_FILE)
        {
            if (m_LogOutputFile.size() > 0 && m_LogOutputFile != "")
            {
                std::ofstream log(m_LogOutputFile, std::ios::app);

                if (!log) {
                    std::cerr << "Cannot open log file for writing\n";
                }
                else
                {
                    log << msg << '\n';

                    if (!log) {
                        std::cerr << "Failed to write to log file\n";
                    }
                }
            }
        }

        if (m_LogOutputFilter & LOG_SYSTEM)
        {
            #ifdef __linux__
                unsigned short int syslogLevel = LOG_INFO;
                switch (level)
                {
                    case LOG_LEVEL_DEBUG: syslogLevel = LOG_DEBUG; break;
                    case LOG_LEVEL_INFO: syslogLevel = LOG_INFO; break;
                    case LOG_LEVEL_WARN: syslogLevel = LOG_WARNING; break;
                    case LOG_LEVEL_ERROR: syslogLevel = LOG_ERR; break;
                }
                openlog(SERVER_DAEMON_ID.c_str(), LOG_NOWAIT | LOG_PID, LOG_DAEMON);
                syslog(syslogLevel, msg.c_str());
                closelog();
            #endif
        }

        if (m_LogOutputFilter & LOG_CONSOLE)
        {
            if (level & LOG_FLAG_ERROR)
            {
                std::cerr << msg << std::endl;
            }
            else
            {
                std::cout << msg << std::endl;
            }
        }
    }
}
