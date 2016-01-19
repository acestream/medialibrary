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

#include "Types.h"
#include "database/DatabaseHelpers.h"
#include "IHistoryEntry.h"

#include <vector>
#include <string>

class History;
class Media;

namespace policy
{
struct HistoryTable
{
    static const std::string Name;
    static const std::string PrimaryKeyColumn;
    static unsigned int History::* const PrimaryKey;
};
}

class History : public IHistoryEntry, public DatabaseHelpers<History, policy::HistoryTable>
{
public:
    History( DBConnection dbConn, sqlite::Row& row );
    static bool createTable( DBConnection dbConnection );
    static bool insert( DBConnection dbConn, const IMedia& media );
    static bool insert( DBConnection dbConn, const std::string& mrl );
    static std::vector<std::shared_ptr<IHistoryEntry>> fetch(DBConnection dbConn);

    virtual MediaPtr media() const override;
    virtual const std::string& mrl() const override;
    virtual unsigned int insertionDate() const override;

    static constexpr unsigned int MaxEntries = 100u;

private:
    unsigned int m_id;
    std::string m_mrl;
    unsigned int m_mediaId;
    unsigned int m_date;
    std::shared_ptr<Media> m_media;

    friend policy::HistoryTable;
};