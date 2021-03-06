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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "VLCThumbnailer.h"

#include "AlbumTrack.h"
#include "Album.h"
#include "Artist.h"
#include "Media.h"
#include "File.h"
#include "logging/Logger.h"
#include "MediaLibrary.h"
#include "utils/VLCInstance.h"
#include "utils/ModificationsNotifier.h"

#ifdef HAVE_JPEG
#include "imagecompressors/JpegCompressor.h"
#elif defined(HAVE_EVAS)
#include "imagecompressors/EvasCompressor.h"
#else
#error No image compressor available
#endif

namespace medialibrary
{

VLCThumbnailer::VLCThumbnailer()
    : m_instance( VLCInstance::get() )
    , m_thumbnailRequired( false )
    , m_width( 0 )
    , m_height( 0 )
    , m_prevSize( 0 )
{
#ifdef HAVE_JPEG
    m_compressor.reset( new JpegCompressor );
#elif defined(HAVE_EVAS)
    m_compressor.reset( new EvasCompressor );
#endif
}

bool VLCThumbnailer::initialize()
{
    return true;
}

bool VLCThumbnailer::isCompleted( const parser::Task& task ) const
{
    return task.isStepCompleted( parser::Task::ParserStep::Thumbnailer );
}

parser::Task::Status VLCThumbnailer::run( parser::Task& task )
{
    auto media = task.media.get();
    auto file = task.file.get();

    LOG_INFO( "Generating ", file->mrl(), " thumbnail..." );

    if ( task.vlcMedia.isValid() == false )
    {
        task.vlcMedia = VLC::Media( m_instance, file->mrl(), VLC::Media::FromType::FromLocation );
    }

    task.vlcMedia.addOption( ":no-audio" );
    task.vlcMedia.addOption( ":no-osd" );
    task.vlcMedia.addOption( ":no-spu" );
    task.vlcMedia.addOption( ":input-fast-seek" );
    task.vlcMedia.addOption( ":avcodec-hw=none" );
    task.vlcMedia.addOption( ":no-mkv-preload-local-dir" );
    auto duration = task.vlcMedia.duration();
    if ( duration > 0 && media->type() != IMedia::Type::Audio )
    {
        std::ostringstream ss;
        // Duration is in ms, start-time in seconds, and we're aiming at 1/4th of the media
        ss << ":start-time=" << duration / 4000;
        task.vlcMedia.addOption( ss.str() );
    }

    VLC::MediaPlayer mp( task.vlcMedia );

    setupVout( mp );

    auto res = startPlayback( task, mp );
    if ( res != parser::Task::Status::Success )
    {
        // If the media became an audio file, it's not an error
        if ( media->type() == Media::Type::Audio )
        {
            task.markStepCompleted( parser::Task::ParserStep::Thumbnailer );
            task.saveParserStep();
            LOG_INFO( file->mrl(), " type has changed to Audio. Skipping thumbnail generation" );
            return parser::Task::Status::Success;
        }
        // Otherwise, we failed to start the playback and this is an error indeed
        LOG_WARN( "Failed to generate ", file->mrl(), " thumbnail: Can't start playback" );
        return res;
    }
    // Yet another special case:
    // We could have run the thumbnailer already as a fallback for some weird video with no
    // preparse detected tracks. If so, we don't want to spend more time computing a thumbnail, but
    // we do need to run the metadata extraction again.
    if ( media->type() == Media::Type::Unknown && media->thumbnail().empty() == false )
    {
        task.markStepCompleted( parser::Task::ParserStep::Thumbnailer );
        // startPlayback will return an error in case the media is an audio file
        media->setType( IMedia::Type::Video );
        // And now let the metadata extraction run again
        return parser::Task::Status::Success;
    }

    if ( duration <= 0 )
    {
        // Seek ahead to have a significant preview
        res = seekAhead( mp );
        if ( res != parser::Task::Status::Success )
        {
            LOG_WARN( "Failed to generate ", file->mrl(), " thumbnail: Failed to seek ahead" );
            return res;
        }
    }
    res = takeThumbnail( media, file, mp );
    if ( res != parser::Task::Status::Success )
        return res;

    LOG_INFO( "Done generating ", file->mrl(), " thumbnail" );
    if ( task.file->isDeleted() == true || task.media->isDeleted() == true )
        return parser::Task::Status::Fatal;

    task.markStepCompleted( parser::Task::ParserStep::Thumbnailer );
    m_notifier->notifyMediaModification( task.media );

    auto t = m_ml->getConn()->newTransaction();
    if ( media->save() == false || task.saveParserStep() == false )
        return parser::Task::Status::Fatal;
    t->commit();
    return parser::Task::Status::Success;
}

parser::Task::Status VLCThumbnailer::startPlayback( parser::Task& task, VLC::MediaPlayer &mp )
{
    // Use a copy of the event manager to automatically unregister all events as soon
    // as we leave this method.
    bool hasVideoTrack = false;
    bool failedToStart = false;
    bool hasAnyTrack = false;
    bool success = false;
    auto em = mp.eventManager();
    em.onESAdded([this, &hasVideoTrack, &hasAnyTrack]( libvlc_track_type_t type, int ) {
        std::lock_guard<compat::Mutex> lock( m_mutex );
        if ( type == libvlc_track_video )
            hasVideoTrack = true;
        hasAnyTrack = true;
        m_cond.notify_all();
    });
    em.onEncounteredError([this, &failedToStart]() {
        std::lock_guard<compat::Mutex> lock( m_mutex );
        failedToStart = true;
        m_cond.notify_all();
    });

    bool metaArtworkChanged = false;
    auto mem = task.vlcMedia.eventManager();
    if ( task.media->type() == Media::Type::Audio )
    {
        mem.onMetaChanged([this, &metaArtworkChanged, &task]( libvlc_meta_t meta ) {
            if ( meta != libvlc_meta_ArtworkURL
                 || metaArtworkChanged == true
                 || task.vlcMedia.meta( libvlc_meta_ArtworkURL ) == task.media->thumbnail() )
                return;
            std::lock_guard<compat::Mutex> lock( m_mutex );
            metaArtworkChanged = true;
            m_cond.notify_all();
        });
    }

    {
        std::unique_lock<compat::Mutex> lock( m_mutex );
        mp.play();
        success = m_cond.wait_for( lock, std::chrono::seconds( 3 ), [&failedToStart, &hasAnyTrack]() {
            return failedToStart == true || hasAnyTrack == true;
        });

        // In case the playback failed, we probably won't fetch anything interesting anyway.
        if ( failedToStart == true || success == false )
            return parser::Task::Status::Fatal;

        // If we have any kind of track, but not a video track, we don't have to wait long, tracks are usually
        // being discovered together.
        if ( hasVideoTrack == false )
        {
            if ( task.media->type() == Media::Type::Audio )
            {
                m_cond.wait_for( lock, std::chrono::milliseconds( 500 ), [&metaArtworkChanged]() {
                    return metaArtworkChanged == true;
                });
            }
            else
            {
                m_cond.wait_for( lock, std::chrono::seconds( 1 ), [&hasVideoTrack]() {
                    return hasVideoTrack == true;
                });
            }
        }
    }

    // Now that we waited long enough for a potential video track, if we have one, we keep generating
    // the thumbnail.
    // If we don't
    if ( hasVideoTrack == true )
        return parser::Task::Status::Success;

    if ( task.media->type() == Media::Type::Audio )
        updateAudioArtwork( task );

    // We are now in the case of a timeout: No failure, but no video track either.
    // The file might be an audio file we haven't detected yet:
    else if ( task.media->type() == Media::Type::Unknown )
    {
        task.media->setType( Media::Type::Audio );
        task.media->save();
        // We still return an error since we don't want to attempt the thumbnail generation for a
        // file without video tracks
    }
    return parser::Task::Status::Fatal;
}

void VLCThumbnailer::updateAudioArtwork( parser::Task& task )
{
    auto artwork = task.vlcMedia.meta( libvlc_meta_ArtworkURL );
    if ( artwork.empty() == true )
        return;

    task.media->setThumbnail( artwork );
    task.media->save();
    auto rel = AlbumTrack::fromMedia( m_ml, task.media->id() );
    if ( rel == nullptr )
        return;

    auto album = rel->album();
    if ( album->artworkMrl() == artwork )
        return;

    // We no not have any other IAlbum implementation, so the downcast is safe here
    auto a = static_cast<Album*>( album.get() );
    a->setArtworkMrl( artwork );

    // If no artwork was set for the AlbumArtist, use this new one
    auto artist = album->albumArtist();
    if ( artist->artworkMrl().empty() == true )
    {
        auto artistPtr = static_cast<Artist*>( artist.get() );
        artistPtr->setArtworkMrl( artwork );
    }

}

parser::Task::Status VLCThumbnailer::seekAhead( VLC::MediaPlayer& mp )
{
    float pos = .0f;
    auto event = mp.eventManager().onPositionChanged([this, &pos](float p) {
        std::unique_lock<compat::Mutex> lock( m_mutex );
        pos = p;
        m_cond.notify_all();
    });
    auto success = false;
    {
        std::unique_lock<compat::Mutex> lock( m_mutex );
        mp.setPosition( .4f );
        success = m_cond.wait_for( lock, std::chrono::seconds( 3 ), [&pos]() {
            return pos >= .1f;
        });
    }
    // Since we're locking a mutex for each position changed, let's unregister ASAP
    event->unregister();
    if ( success == false )
        return parser::Task::Status::Fatal;
    return parser::Task::Status::Success;
}

void VLCThumbnailer::setupVout( VLC::MediaPlayer& mp )
{
    mp.setVideoFormatCallbacks(
        // Setup
        [this, &mp](char* chroma, unsigned int* width, unsigned int *height, unsigned int *pitches, unsigned int *lines) {
            strcpy( chroma, m_compressor->fourCC() );

            const float inputAR = (float)*width / *height;

            m_width = DesiredWidth;
            m_height = (float)m_width / inputAR + 1;
            if ( m_height < DesiredHeight )
            {
                // Avoid downscaling too much for really wide pictures
                m_width = inputAR * DesiredHeight;
                m_height = DesiredHeight;
            }
            auto size = m_width * m_height * m_compressor->bpp();
            // If our buffer isn't enough anymore, reallocate a new one.
            if ( size > m_prevSize )
            {
                m_buff.reset( new uint8_t[size] );
                m_prevSize = size;
            }
            *width = m_width;
            *height = m_height;
            *pitches = m_width * m_compressor->bpp();
            *lines = m_height;
            return 1;
        },
        // Cleanup
        nullptr);
    mp.setVideoCallbacks(
        // Lock
        [this](void** pp_buff) {
            *pp_buff = m_buff.get();
            return nullptr;
        },
        //unlock
        nullptr,

        //display
        [this](void*) {
            bool expected = true;
            if ( m_thumbnailRequired.compare_exchange_strong( expected, false ) )
            {
                m_cond.notify_all();
            }
        }
    );
}

parser::Task::Status VLCThumbnailer::takeThumbnail( Media* media, File* file, VLC::MediaPlayer &mp )
{
    // lock, signal that we want a thumbnail, and wait.
    {
        std::unique_lock<compat::Mutex> lock( m_mutex );
        m_thumbnailRequired = true;
        bool success = m_cond.wait_for( lock, std::chrono::seconds( 15 ), [this]() {
            // Keep waiting if the vmem thread hasn't restored m_thumbnailRequired to false
            return m_thumbnailRequired == false;
        });
        if ( success == false )
        {
            LOG_WARN( "Timed out while computing ", file->mrl(), " snapshot" );
            return parser::Task::Status::Fatal;
        }
    }
    mp.stop();
    return compress( media, file );
}

parser::Task::Status VLCThumbnailer::compress( Media* media, File* file )
{
    auto path = m_ml->thumbnailPath();
    path += "/";
    path += std::to_string( media->id() ) + "." + m_compressor->extension();

    auto hOffset = m_width > DesiredWidth ? ( m_width - DesiredWidth ) / 2 : 0;
    auto vOffset = m_height > DesiredHeight ? ( m_height - DesiredHeight ) / 2 : 0;

    if ( m_compressor->compress( m_buff.get(), path, m_width, m_height, DesiredWidth, DesiredHeight,
                            hOffset, vOffset ) == false )
        return parser::Task::Status::Fatal;

    media->setThumbnail( path );
    return parser::Task::Status::Success;
}

const char*VLCThumbnailer::name() const
{
    return "Thumbnailer";
}

uint8_t VLCThumbnailer::nbThreads() const
{
    return 1;
}

}
