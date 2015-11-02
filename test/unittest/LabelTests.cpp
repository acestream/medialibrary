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

#include "Tests.h"

#include "IMediaLibrary.h"
#include "Media.h"
#include "ILabel.h"

class Labels : public Tests
{
};

TEST_F( Labels, Add )
{
    auto f = ml->addFile( "media.avi", nullptr );
    auto l1 = ml->createLabel( "sea otter" );
    auto l2 = ml->createLabel( "cony the cone" );

    ASSERT_NE( l1, nullptr);
    ASSERT_NE( l2, nullptr);

    auto labels = f->labels();
    ASSERT_EQ( labels.size(), 0u );

    f->addLabel( l1 );
    f->addLabel( l2 );

    labels = f->labels();

    ASSERT_EQ( labels.size(), 2u );
    ASSERT_EQ( labels[0]->name(), "sea otter" );
    ASSERT_EQ( labels[1]->name(), "cony the cone" );
}

TEST_F( Labels, Remove )
{
    auto f = ml->addFile( "media.avi", nullptr );
    auto l1 = ml->createLabel( "sea otter" );
    auto l2 = ml->createLabel( "cony the cone" );

    f->addLabel( l1 );
    f->addLabel( l2 );

    auto labels = f->labels();
    ASSERT_EQ( labels.size(), 2u );

    bool res = f->removeLabel( l1 );
    ASSERT_TRUE( res );

    // Check for existing file first
    labels = f->labels();
    ASSERT_EQ( labels.size(), 1u );
    ASSERT_EQ( labels[0]->name(), "cony the cone" );

    // And now clean fetch another instance of the file & check again for DB replication
    auto f2 = ml->file( f->mrl() );
    labels = f2->labels();
    ASSERT_EQ( labels.size(), 1u );
    ASSERT_EQ( labels[0]->name(), "cony the cone" );

    // Remove a non-linked label
    res = f->removeLabel( l1 );
    ASSERT_FALSE( res );

    // Remove the last label
    res = f->removeLabel( l2 );
    ASSERT_TRUE( res );

    labels = f->labels();
    ASSERT_EQ( labels.size(), 0u );

    // Check again for DB replication
    f2 = ml->file( f->mrl() );
    labels = f2->labels();
    ASSERT_EQ( labels.size(), 0u );
}

TEST_F( Labels, Files )
{
    auto f = ml->addFile( "media.avi", nullptr );
    auto f2 = ml->addFile( "file.mp3", nullptr );
    auto f3 = ml->addFile( "otter.mkv", nullptr );

    auto l1 = ml->createLabel( "label1" );
    auto l2 = ml->createLabel( "label2" );

    f->addLabel( l1 );
    f2->addLabel( l2 );
    f3->addLabel( l1 );

    auto label1Files = l1->files();
    auto label2Files = l2->files();

    ASSERT_EQ( label1Files.size(), 2u );
    ASSERT_EQ( label2Files.size(), 1u );

    ASSERT_EQ( label2Files[0], f2 );

    for (auto labelFile : label1Files )
    {
        ASSERT_TRUE( labelFile == f || labelFile == f3 );
    }
}

TEST_F( Labels, Delete )
{
    auto f = ml->addFile( "media.avi", nullptr );
    auto l1 = ml->createLabel( "sea otter" );
    auto l2 = ml->createLabel( "cony the cone" );

    f->addLabel( l1 );
    f->addLabel( l2 );

    auto labels = f->labels();
    ASSERT_EQ( labels.size(), 2u );

    ml->deleteLabel( "sea otter" );
    labels = f->labels();
    ASSERT_EQ( labels.size(), 1u );

    ml->deleteLabel( l2 );
    labels = f->labels();
    ASSERT_EQ( labels.size(), 0u );

    // Nothing to delete anymore, this should fail gracefuly
    bool res = ml->deleteLabel( l1 );
    ASSERT_FALSE( res );
}
