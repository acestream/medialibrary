/*****************************************************************************
 * Media Library
 *****************************************************************************
 * Copyright (C) 2015 Hugo Beauzée-Luyssen, Videolabs
 *
 * Authors: Hugo Beauzée-Luyssen<hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <sstream>

#include "ILogger.h"
#include "Types.h"

class Log
{
private:
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
    static void log(LogLevel lvl, Args&&... args)
    {
        if ( lvl < s_logLevel.load( std::memory_order_relaxed ) )
            return;

        auto msg = createMsg( std::forward<Args>( args )... );
        auto l = s_logger.load( std::memory_order_consume );
        if ( l == nullptr )
           l = s_defaultLogger.get();

        switch ( lvl )
        {
        case LogLevel::Error:
            l->Error( msg );
            break;
        case LogLevel::Warning:
            l->Warning( msg );
            break;
        case LogLevel::Debug:
        case LogLevel::Info:
            l->Info( msg );
            break;
        }
    }

public:
    static void SetLogger( ILogger* logger )
    {
        s_logger.store( logger, std::memory_order_relaxed );
    }

    static void setLogLevel( LogLevel level )
    {
        s_logLevel.store( level, std::memory_order_relaxed );
    }

    template <typename... Args>
    static void Error( Args... args )
    {
        log( LogLevel::Error, std::forward<Args>( args )... );
    }

    template <typename... Args>
    static void Warning( Args... args )
    {
        log( LogLevel::Warning, std::forward<Args>( args )... );
    }

    template <typename... Args>
    static void Info( Args... args )
    {
        log( LogLevel::Info, std::forward<Args>( args )... );
    }

private:

private:
    static std::unique_ptr<ILogger> s_defaultLogger;
    static std::atomic<ILogger*> s_logger;
    static std::atomic<LogLevel> s_logLevel;
};

#if defined(__clang__) || defined(__GNUG__)
# define LOG_ORIGIN __PRETTY_FUNCTION__, ':', __LINE__
#elif defined(_MSC_VER)
# define LOG_ORIGIN __FUNCDNAME__
#else
# define LOG_ORIGIN __FILE__, ":", __LINE__, ' ', __func__
#endif

#define LOG_ERROR( ... ) Log::Error( LOG_ORIGIN, ' ', __VA_ARGS__ )
#define LOG_WARN( ... ) Log::Warning( LOG_ORIGIN, ' ', __VA_ARGS__ )
#define LOG_INFO( ... ) Log::Info( LOG_ORIGIN, ' ', __VA_ARGS__ )
