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

#include "Movie.h"
#include "Media.h"
#include "database/SqliteTools.h"

const std::string policy::MovieTable::Name = "Movie";
const std::string policy::MovieTable::PrimaryKeyColumn = "id_movie";
unsigned int Movie::* const policy::MovieTable::PrimaryKey = &Movie::m_id;

Movie::Movie(DBConnection dbConnection, sqlite::Row& row )
    : m_dbConnection( dbConnection )
{
    row >> m_id
        >> m_title
        >> m_releaseDate
        >> m_summary
        >> m_artworkMrl
        >> m_imdbId;
}

Movie::Movie( const std::string& title )
    : m_id( 0 )
    , m_title( title )
    , m_releaseDate( 0 )
{
}


unsigned int Movie::id() const
{
    return m_id;
}

const std::string&Movie::title() const
{
    return m_title;
}

time_t Movie::releaseDate() const
{
    return m_releaseDate;
}

bool Movie::setReleaseDate( time_t date )
{
    static const std::string req = "UPDATE " + policy::MovieTable::Name
            + " SET release_date = ? WHERE id_movie = ?";
    if ( sqlite::Tools::executeUpdate( m_dbConnection, req, date, m_id ) == false )
        return false;
    m_releaseDate = date;
    return true;
}

const std::string&Movie::shortSummary() const
{
    return m_summary;
}

bool Movie::setShortSummary( const std::string& summary )
{
    static const std::string req = "UPDATE " + policy::MovieTable::Name
            + " SET summary = ? WHERE id_movie = ?";
    if ( sqlite::Tools::executeUpdate( m_dbConnection, req, summary, m_id ) == false )
        return false;
    m_summary = summary;
    return true;
}

const std::string&Movie::artworkMrl() const
{
    return m_artworkMrl;
}

bool Movie::setArtworkMrl( const std::string& artworkMrl )
{
    static const std::string req = "UPDATE " + policy::MovieTable::Name
            + " SET artwork_mrl = ? WHERE id_movie = ?";
    if ( sqlite::Tools::executeUpdate( m_dbConnection, req, artworkMrl, m_id ) == false )
        return false;
    m_artworkMrl = artworkMrl;
    return true;
}

const std::string& Movie::imdbId() const
{
    return m_imdbId;
}

bool Movie::setImdbId( const std::string& imdbId )
{
    static const std::string req = "UPDATE " + policy::MovieTable::Name
            + " SET imdb_id = ? WHERE id_movie = ?";
    if ( sqlite::Tools::executeUpdate( m_dbConnection, req, imdbId, m_id ) == false )
        return false;
    m_imdbId = imdbId;
    return true;
}

std::vector<MediaPtr> Movie::files()
{
    static const std::string req = "SELECT * FROM " + policy::MediaTable::Name
            + " WHERE movie_id = ?";
    return Media::fetchAll<IMedia>( m_dbConnection, req, m_id );
}

bool Movie::createTable( DBConnection dbConnection )
{
    static const std::string req = "CREATE TABLE IF NOT EXISTS " + policy::MovieTable::Name
            + "("
                "id_movie INTEGER PRIMARY KEY AUTOINCREMENT,"
                "title TEXT UNIQUE ON CONFLICT FAIL,"
                "release_date UNSIGNED INTEGER,"
                "summary TEXT,"
                "artwork_mrl TEXT,"
                "imdb_id TEXT"
            ")";
    return sqlite::Tools::executeRequest( dbConnection, req );
}

std::shared_ptr<Movie> Movie::create(DBConnection dbConnection, const std::string& title )
{
    auto movie = std::make_shared<Movie>( title );
    static const std::string req = "INSERT INTO " + policy::MovieTable::Name
            + "(title) VALUES(?)";
    if ( insert( dbConnection, movie, req, title ) == false )
        return nullptr;
    movie->m_dbConnection = dbConnection;
    return movie;
}
