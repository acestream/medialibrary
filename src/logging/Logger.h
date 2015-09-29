#pragma once

#include <atomic>
#include <memory>
#include <sstream>

#include "ILogger.h"

class Log
{
private:
    enum class Level
    {
        Error,
        Warning,
        Info,
    };

    template <typename T>
    static void createMsg( std::stringstream& s, T&& t )
    {
        s << t;
    }

    template <typename T, typename... Args>
    static void createMsg( std::stringstream& s, T&& t, Args&&... args )
    {
        s << std::forward<T>( t );
        createMsg( s, std::forward<Args>( args )... );
    }

    template <typename... Args>
    static std::string createMsg( Args&&... args )
    {
        std::stringstream stream;
        createMsg( stream, std::forward<Args>( args )... );
        stream << "\n";
        return stream.str();
    }

    template <typename... Args>
    static void log(Level lvl, Args&&... args)
    {
        auto msg = createMsg( std::forward<Args>( args )... );
        auto l = s_logger.load( std::memory_order_consume );
        if ( l == nullptr )
           l = s_defaultLogger.get();
        switch ( lvl )
        {
        case Level::Error:
            l->Error( msg );
            break;
        case Level::Warning:
            l->Warning( msg );
            break;
        case Level::Info:
            l->Info( msg );
            break;
        }
    }

public:
    static void SetLogger( ILogger* logger )
    {
        s_logger.store( logger, std::memory_order_relaxed );
    }

    template <typename... Args>
    static void Error( Args... args )
    {
        log( Level::Error, std::forward<Args>( args )... );
    }

    template <typename... Args>
    static void Warning( Args... args )
    {
        log( Level::Warning, std::forward<Args>( args )... );
    }

    template <typename... Args>
    static void Info( Args... args )
    {
        log( Level::Info, std::forward<Args>( args )... );
    }

private:

private:
    static std::unique_ptr<ILogger> s_defaultLogger;
    static std::atomic<ILogger*> s_logger;
};