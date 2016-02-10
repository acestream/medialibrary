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

#include "Album.h"
#include "AlbumTrack.h"
#include "Artist.h"
#include "Genre.h"
#include "Media.h"
#include "IMediaLibrary.h"

class Albums : public Tests
{
};

TEST_F( Albums, Create )
{
    auto a = ml->createAlbum( "album" );
    ASSERT_NE( a, nullptr );

    auto a2 = ml->album( a->id() );
    ASSERT_EQ( a, a2 );
    ASSERT_EQ( a2->title(), "album" );
}

TEST_F( Albums, Fetch )
{
    auto a = ml->createAlbum( "album" );

    // Clear the cache
    Reload();

    auto a2 = ml->album( a->id() );
    // The shared pointer are expected to point to a different instance
    ASSERT_NE( a, a2 );

    ASSERT_EQ( a->id(), a2->id() );
}

TEST_F( Albums, AddTrack )
{
    auto a = ml->createAlbum( "albumtag" );
    auto f = ml->addFile( "track.mp3" );
    auto track = a->addTrack( f, 10, 0 );
    f->save();
    ASSERT_NE( track, nullptr );

    auto tracks = a->tracks();
    ASSERT_EQ( tracks.size(), 1u );

    Reload();

    a = std::static_pointer_cast<Album>( ml->album( a->id() ) );
    tracks = a->tracks();
    ASSERT_EQ( tracks.size(), 1u );
    ASSERT_EQ( tracks[0]->albumTrack()->trackNumber(), track->trackNumber() );
}

TEST_F( Albums, NbTracks )
{
    auto a = ml->createAlbum( "albumtag" );
    for ( auto i = 1u; i <= 10; ++i )
    {
        auto f = ml->addFile( "track" + std::to_string(i) + ".mp3" );
        auto track = a->addTrack( f, i, i );
        f->save();
        ASSERT_NE( track, nullptr );
    }
    auto tracks = a->tracks();
    ASSERT_EQ( tracks.size(), a->nbTracks() );

    Reload();

    a = std::static_pointer_cast<Album>( ml->album( a->id() ) );
    tracks = a->tracks();
    ASSERT_EQ( tracks.size(), a->nbTracks() );
}

TEST_F( Albums, TracksByGenre )
{
    auto a = ml->createAlbum( "albumtag" );
    auto g = ml->createGenre( "genre" );

    for ( auto i = 1u; i <= 10; ++i )
    {
        auto f = ml->addFile( "track" + std::to_string(i) + ".mp3" );
        auto track = a->addTrack( f, i, i );
        f->save();
        ASSERT_NE( track, nullptr );
        if ( i <= 5 )
            track->setGenre( g );
    }
    auto tracks = a->tracks( g );
    ASSERT_EQ( 5u, tracks.size() );

    Reload();

    a = std::static_pointer_cast<Album>( ml->album( a->id() ) );
    tracks = a->tracks( g );
    ASSERT_NE( tracks.size(), a->nbTracks() );
    ASSERT_EQ( 5u, tracks.size() );
}

TEST_F( Albums, SetReleaseDate )
{
    auto a = ml->createAlbum( "album" );

    ASSERT_EQ( 0u, a->releaseYear() );

    a->setReleaseYear( 1234, false );
    ASSERT_EQ( a->releaseYear(), 1234u );

    a->setReleaseYear( 4321, false );
    // We now have conflicting dates, it should be restored to 0.
    ASSERT_EQ( 0u, a->releaseYear() );

    // Check that this is not considered initial state anymore, and that pretty much any other date will be ignored.
    a->setReleaseYear( 666, false );
    ASSERT_EQ( 0u, a->releaseYear() );

    // Now check that forcing a date actually forces it
    a->setReleaseYear( 9876, true );
    ASSERT_EQ( 9876u, a->releaseYear() );

    Reload();

    auto a2 = ml->album( a->id() );
    ASSERT_EQ( a->releaseYear(), a2->releaseYear() );
}

TEST_F( Albums, SetShortSummary )
{
    auto a = ml->createAlbum( "album" );

    a->setShortSummary( "summary" );
    ASSERT_EQ( a->shortSummary(), "summary" );

    Reload();

    auto a2 = ml->album( a->id() );
    ASSERT_EQ( a->shortSummary(), a2->shortSummary() );
}

TEST_F( Albums, SetArtworkMrl )
{
    auto a = ml->createAlbum( "album" );

    a->setArtworkMrl( "artwork" );
    ASSERT_EQ( a->artworkMrl(), "artwork" );

    Reload();

    auto a2 = ml->album( a->id() );
    ASSERT_EQ( a->artworkMrl(), a2->artworkMrl() );
}

TEST_F( Albums, FetchAlbumFromTrack )
{
    auto a = ml->createAlbum( "album" );
    auto f = ml->addFile( "file.mp3" );
    auto t = a->addTrack( f, 1, 0 );
    f->save();

    Reload();

    f = ml->media( f->id() );
    auto t2 = f->albumTrack();
    auto a2 = t2->album();
    ASSERT_NE( a2, nullptr );
    ASSERT_EQ( a2->title(), "album" );
}

TEST_F( Albums, Artists )
{
    auto album = ml->createAlbum( "album" );
    auto artist1 = ml->createArtist( "john" );
    auto artist2 = ml->createArtist( "doe" );

    ASSERT_NE( album, nullptr );
    ASSERT_NE( artist1, nullptr );
    ASSERT_NE( artist2, nullptr );

    auto res = album->addArtist( artist1 );
    ASSERT_EQ( res, true );

    res = album->addArtist( artist2 );
    ASSERT_EQ( res, true );

    auto artists = album->artists();
    ASSERT_EQ( artists.size(), 2u );

    Reload();

    album = std::static_pointer_cast<Album>( ml->album( album->id() ) );
    artists = album->artists();
    ASSERT_EQ( album->albumArtist(), nullptr );
    ASSERT_EQ( artists.size(), 2u );
}

TEST_F( Albums, AlbumArtist )
{
    auto album = ml->createAlbum( "test" );
    ASSERT_EQ( album->albumArtist(), nullptr );
    auto artist = ml->createArtist( "artist" );
    album->setAlbumArtist( artist.get() );
    ASSERT_NE( album->albumArtist(), nullptr );

    Reload();

    album = std::static_pointer_cast<Album>( ml->album( album->id() ) );
    auto albumArtist = album->albumArtist();
    ASSERT_NE( albumArtist, nullptr );
    ASSERT_EQ( albumArtist->name(), artist->name() );
}

TEST_F( Albums, SearchByTitle )
{
    ml->createAlbum( "sea otters" );
    ml->createAlbum( "pangolins of fire" );

    auto albums = ml->searchAlbums( "otte" );
    ASSERT_EQ( 1u, albums.size() );
}

TEST_F( Albums, SearchByArtist )
{
    auto a = ml->createAlbum( "sea otters" );
    auto artist = ml->createArtist( "pangolins" );
    a->setAlbumArtist( artist.get() );

    auto albums = ml->searchAlbums( "pangol" );
    ASSERT_EQ( 1u, albums.size() );
}

TEST_F( Albums, SearchNoDuplicate )
{
    auto a = ml->createAlbum( "sea otters" );
    auto artist = ml->createArtist( "otters" );
    a->setAlbumArtist( artist.get() );

    auto albums = ml->searchAlbums( "otters" );
    ASSERT_EQ( 1u, albums.size() );
}

TEST_F( Albums, SearchNoUnknownAlbum )
{
    auto artist = ml->createArtist( "otters" );
    auto album = artist->unknownAlbum();
    ASSERT_NE( nullptr, album );

    auto albums = ml->searchAlbums( "otters" );
    ASSERT_EQ( 0u, albums.size() );
    // Can't search by name since there is no name set for unknown albums
}

TEST_F( Albums, SearchAfterDeletion )
{
    auto a = ml->createAlbum( "sea otters" );
    auto albums = ml->searchAlbums( "sea" );
    ASSERT_EQ( 1u, albums.size() );

    ml->deleteAlbum( a->id() );

    albums = ml->searchAlbums( "sea" );
    ASSERT_EQ( 0u, albums.size() );
}

TEST_F( Albums, SearchAfterArtistUpdate )
{
    auto a = ml->createAlbum( "sea otters" );
    auto artist = ml->createArtist( "pangolin of fire" );
    auto artist2 = ml->createArtist( "pangolin of ice" );
    a->setAlbumArtist( artist.get() );

    auto albums = ml->searchAlbums( "fire" );
    ASSERT_EQ( 1u, albums.size() );

    albums = ml->searchAlbums( "ice" );
    ASSERT_EQ( 0u, albums.size() );

    a->setAlbumArtist( artist2.get() );

    albums = ml->searchAlbums( "fire" );
    ASSERT_EQ( 0u, albums.size() );

    albums = ml->searchAlbums( "ice" );
    ASSERT_EQ( 1u, albums.size() );
}

TEST_F( Albums, AutoDelete )
{
    auto a = ml->createAlbum( "album" );
    auto m = ml->addFile( "media.mp3" );
    auto t = a->addTrack( m, 1, 1 );

    auto album = ml->album( a->id() );
    ASSERT_NE( nullptr, album );

    ml->deleteTrack( t->id() );

    album = ml->album( a->id() );
    ASSERT_EQ( nullptr, album );
}
