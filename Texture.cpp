/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

/*
    MaSzyna EU07 locomotive simulator
    Copyright (C) 2001-2004  Marcin Wozniak and others

*/

#include "stdafx.h"
#include "texture.h"

#include <ddraw.h>
#include "GL/glew.h"

#include "usefull.h"
#include "globals.h"
#include "logs.h"
#include "sn_utils.h"

texture_manager::texture_manager() {

    // since index 0 is used to indicate no texture, we put a blank entry in the first texture slot
    m_textures.emplace_back( opengl_texture() );
}

// loads texture data from specified file
// TODO: wrap it in a workitem class, for the job system deferred loading
void
opengl_texture::load() {

    if( name.size() < 3 ) { goto fail; }

    WriteLog( "Loading texture data from \"" + name + "\"" );

    data_state = resource_state::loading;
    {
        std::string const extension = name.substr( name.size() - 3, 3 );

             if( extension == "dds" ) { load_DDS(); }
        else if( extension == "tga" ) { load_TGA(); }
        else if( extension == "bmp" ) { load_BMP(); }
        else if( extension == "tex" ) { load_TEX(); }
        else { goto fail; }
    }

    // data state will be set by called loader, so we're all done here
    if( data_state == resource_state::good ) {

        has_alpha = (
            data_components == GL_RGBA ?
                true :
                false );

        size = data.size() / 1024;

        return;
    }

fail:
    data_state = resource_state::failed;
    ErrorLog( "Failed to load texture \"" + name + "\"" );
    // NOTE: temporary workaround for texture assignment errors
    id = 0;
    return;
}

void
opengl_texture::load_BMP() {

    std::ifstream file( name, std::ios::binary ); file.unsetf( std::ios::skipws );

    BITMAPFILEHEADER header;

    file.read( (char *)&header, sizeof( BITMAPFILEHEADER ) );
    if( file.eof() ) {

        data_state = resource_state::failed;
        return;
    }

    // Read in bitmap information structure
    BITMAPINFO info;
    unsigned int infosize = header.bfOffBits - sizeof( BITMAPFILEHEADER );
    if( infosize > sizeof( info ) ) {
        WriteLog( "Warning - BMP header is larger than expected, possible format difference." );
    }
    file.read( (char *)&info, std::min( (size_t)infosize, sizeof( info ) ) );

    data_width = info.bmiHeader.biWidth;
    data_height = info.bmiHeader.biHeight;

    if( info.bmiHeader.biCompression != BI_RGB ) {

        ErrorLog( "Compressed BMP textures aren't supported." );
        data_state = resource_state::failed;
        return;
    }

    unsigned long datasize = info.bmiHeader.biSizeImage;
    if( 0 == datasize ) {
        // calculate missing info
        datasize = ( data_width * info.bmiHeader.biBitCount + 7 ) / 8 * data_height;
    }

    data.resize( datasize );
    file.read( &data[0], datasize );

    // fill remaining data info
    if( info.bmiHeader.biBitCount == 32 ) {

        data_format = GL_BGRA;
        data_components = GL_RGBA;
    }
    else {

        data_format = GL_BGR;
        data_components = GL_RGB;
    }
    data_mapcount = 1;
    data_state = resource_state::good;

    return;
}

DDCOLORKEY opengl_texture::deserialize_ddck(std::istream &s)
{
	DDCOLORKEY ddck;

	ddck.dwColorSpaceLowValue = sn_utils::ld_uint32(s);
	ddck.dwColorSpaceHighValue = sn_utils::ld_uint32(s);

	return ddck;
}

DDPIXELFORMAT opengl_texture::deserialize_ddpf(std::istream &s)
{
	DDPIXELFORMAT ddpf;

	ddpf.dwSize = sn_utils::ld_uint32(s);
	ddpf.dwFlags = sn_utils::ld_uint32(s);
	ddpf.dwFourCC = sn_utils::ld_uint32(s);
	ddpf.dwRGBBitCount = sn_utils::ld_uint32(s);
	ddpf.dwRBitMask = sn_utils::ld_uint32(s);
	ddpf.dwGBitMask = sn_utils::ld_uint32(s);
	ddpf.dwBBitMask = sn_utils::ld_uint32(s);
	ddpf.dwRGBAlphaBitMask = sn_utils::ld_uint32(s);

	return ddpf;
}

DDSCAPS2 opengl_texture::deserialize_ddscaps(std::istream &s)
{
	DDSCAPS2 ddsc;

	ddsc.dwCaps = sn_utils::ld_uint32(s);
	ddsc.dwCaps2 = sn_utils::ld_uint32(s);
	ddsc.dwCaps3 = sn_utils::ld_uint32(s);
	ddsc.dwCaps4 = sn_utils::ld_uint32(s);

	return ddsc;
}

DDSURFACEDESC2 opengl_texture::deserialize_ddsd(std::istream &s)
{
	DDSURFACEDESC2 ddsd;

	ddsd.dwSize = sn_utils::ld_uint32(s);
	ddsd.dwFlags = sn_utils::ld_uint32(s);
	ddsd.dwHeight = sn_utils::ld_uint32(s);
	ddsd.dwWidth = sn_utils::ld_uint32(s);
	ddsd.lPitch = sn_utils::ld_uint32(s);
	ddsd.dwBackBufferCount = sn_utils::ld_uint32(s);
	ddsd.dwMipMapCount = sn_utils::ld_uint32(s);
	ddsd.dwAlphaBitDepth = sn_utils::ld_uint32(s);
	ddsd.dwReserved = sn_utils::ld_uint32(s);
	sn_utils::ld_uint32(s);
	ddsd.lpSurface = nullptr;
	ddsd.ddckCKDestOverlay = deserialize_ddck(s);
	ddsd.ddckCKDestBlt = deserialize_ddck(s);
	ddsd.ddckCKSrcOverlay = deserialize_ddck(s);
	ddsd.ddckCKSrcBlt = deserialize_ddck(s);
	ddsd.ddpfPixelFormat = deserialize_ddpf(s);
	ddsd.ddsCaps = deserialize_ddscaps(s);
	ddsd.dwTextureStage = sn_utils::ld_uint32(s);

	return ddsd;
}

void
opengl_texture::load_DDS() {

    std::ifstream file( name, std::ios::binary | std::ios::ate ); file.unsetf( std::ios::skipws );
    std::size_t filesize = static_cast<size_t>(file.tellg());   // ios::ate already positioned us at the end of the file
    file.seekg( 0, std::ios::beg ); // rewind the caret afterwards

    char filecode[5];
    file.read(filecode, 4);
    filesize -= 4;
    filecode[4] = 0;

    if( filecode != std::string( "DDS " ) )
    {
        data_state = resource_state::failed;
        return;
    }

	DDSURFACEDESC2 ddsd = deserialize_ddsd(file);
	filesize -= 124;

    //
    // This .dds loader supports the loading of compressed formats DXT1, DXT3
    // and DXT5.
    //

    switch (ddsd.ddpfPixelFormat.dwFourCC)
    {
    case FOURCC_DXT1:
        // DXT1's compression ratio is 8:1
        data_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;

    case FOURCC_DXT3:
        // DXT3's compression ratio is 4:1
        data_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;

    case FOURCC_DXT5:
        // DXT5's compression ratio is 4:1
        data_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;

    default:
        data_state = resource_state::failed;
        return;
    }

    data_width = ddsd.dwWidth;
    data_height = ddsd.dwHeight;
    data_mapcount = ddsd.dwMipMapCount;

    int blockSize = ( data_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ? 8 : 16 );
    int offset = 0;

    while( ( data_width > Global::iMaxTextureSize ) || ( data_height > Global::iMaxTextureSize ) ) {
        // pomijanie zbyt dużych mipmap, jeśli wymagane jest ograniczenie rozmiaru
        offset += ( ( data_width + 3 ) / 4 ) * ( ( data_height + 3 ) / 4 ) * blockSize;
        data_width /= 2;
        data_height /= 2;
        --data_mapcount;
        WriteLog( "Texture size exceeds specified limits, skipping mipmap level" );
    };

    if( data_mapcount <= 0 ) {
        // there's a chance we've discarded the provided mipmap(s) as too large
        WriteLog( "Texture \"" + name + "\" has no mipmaps which can fit currently set texture size limits." );
        data_state = resource_state::failed;
        return;
    }

    size_t datasize = filesize - offset;
/*
    // this approach loads only the first mipmap and relies on graphics card to fill the rest
    data_mapcount = 1;
    int datasize = ( ( data_width + 3 ) / 4 ) * ( ( data_height + 3 ) / 4 ) * blockSize;
*/
/*
    // calculate size of accepted data
    // NOTE: this is a fallback, as we should be able to just move the file caret by calculated offset and read the rest
    int datasize = 0;
    int mapcount = data_mapcount,
        width = data_width,
        height = data_height;
    while( mapcount ) {

        datasize += ( ( width + 3 ) / 4 ) * ( ( height + 3 ) / 4 ) * blockSize;
        width = std::max( width / 2, 4 );
        height = std::max( height / 2, 4 );
        --mapcount;
    }
*/
    if( datasize == 0 ) {
        // catch malformed .dds files
        WriteLog( "File \"" + name + "\" is malformed and holds no texture data." );
        data_state = resource_state::failed;
        return;
    }
    // reserve space and load texture data
    data.resize( datasize );
    if( offset != 0 ) {
        // skip data for mipmaps we don't need
        file.seekg( offset, std::ios_base::cur );
        filesize -= offset;
    }
    file.read((char *)&data[0], datasize);
    filesize -= datasize;

    data_components =
        ( ddsd.ddpfPixelFormat.dwFourCC == FOURCC_DXT1 ?
            GL_RGB :
            GL_RGBA );

    data_state = resource_state::good;

    return;
}

void
opengl_texture::load_TEX() {

    std::ifstream file( name, std::ios::binary ); file.unsetf( std::ios::skipws );

    char head[ 5 ];
    file.read( head, 4 );
    head[ 4 ] = 0;

    bool hasalpha;
    if( std::string( "RGB " ) == head ) {
        hasalpha = false;
    }
    else if( std::string( "RGBA" ) == head ) {
        hasalpha = true;
    }
    else {
        ErrorLog( "Unrecognized TEX texture sub-format: " + std::string(head) );
        data_state = resource_state::failed;
        return;
    };

    file.read( (char *)&data_width, sizeof( int ) );
    file.read( (char *)&data_height, sizeof( int ) );

    std::size_t datasize = data_width * data_height * ( hasalpha ? 4 : 3 );

    data.resize( datasize );
    file.read( reinterpret_cast<char *>( &data[0] ), datasize );

    // fill remaining data info
    if( true == hasalpha ) {

        data_format = GL_RGBA;
        data_components = GL_RGBA;
    }
    else {

        data_format = GL_RGB;
        data_components = GL_RGB;
    }
    data_mapcount = 1;
    data_state = resource_state::good;

    return;
}

void
opengl_texture::load_TGA() {

    std::ifstream file( name, std::ios::binary ); file.unsetf( std::ios::skipws );

    // Read the header of the TGA, compare it with the known headers for compressed and uncompressed TGAs
    unsigned char tgaheader[ 18 ];
    file.read( (char *)tgaheader, sizeof( unsigned char ) * 18 );

    while( tgaheader[ 0 ] > 0 ) {
        --tgaheader[ 0 ];

        unsigned char temp;
        file.read( (char *)&temp, sizeof( unsigned char ) );
    }

    data_width = tgaheader[ 13 ] * 256 + tgaheader[ 12 ];
    data_height = tgaheader[ 15 ] * 256 + tgaheader[ 14 ];
    int const bytesperpixel = tgaheader[ 16 ] / 8;

    // check whether width, height an BitsPerPixel are valid
    if( ( data_width <= 0 )
     || ( data_height <= 0 )
     || ( ( bytesperpixel != 1 ) && ( bytesperpixel != 3 ) && ( bytesperpixel != 4 ) ) ) {

        data_state = resource_state::failed;
        return;
    }

    // allocate the data buffer
    int const datasize = data_width * data_height * 4;
    data.resize( datasize );

    // call the appropriate loader-routine
    if( tgaheader[ 2 ] == 2 ) {
        // uncompressed TGA
        if( bytesperpixel == 4 ) {
            // read the data directly
            file.read( reinterpret_cast<char*>( &data[ 0 ] ), datasize );
        }
        else {
            // rgb or greyscale image, expand to bgra
            unsigned char buffer[ 4 ] = { 255, 255, 255, 255 }; // alpha channel will be white

            unsigned int *datapointer = (unsigned int*)&data[ 0 ];
            unsigned int *bufferpointer = (unsigned int*)&buffer[ 0 ];

            int const pixelcount = data_width * data_height;

            for( int i = 0; i < pixelcount; ++i ) {

                file.read( (char *)&buffer[ 0 ], sizeof( unsigned char ) * bytesperpixel );
                if( bytesperpixel == 1 ) {
                    // expand greyscale data
                    buffer[ 1 ] = buffer[ 0 ];
                    buffer[ 2 ] = buffer[ 0 ];
                }
                // copy all four values in one operation
                ( *datapointer ) = ( *bufferpointer );
                ++datapointer;
            }
        }
    }
    else if( tgaheader[ 2 ] == 10 ) {
        // compressed TGA
        int currentpixel = 0;

        unsigned char buffer[ 4 ] = { 255, 255, 255, 255 };
        const int pixelcount = data_width * data_height;

        unsigned int *datapointer = (unsigned int *)&data[ 0 ];
        unsigned int *bufferpointer = (unsigned int *)&buffer[ 0 ];

        do {
            unsigned char chunkheader = 0;

            file.read( (char *)&chunkheader, sizeof( unsigned char ) );

            if( (chunkheader & 0x80 ) == 0 ) {
                // if the high bit is not set, it means it is the number of RAW color packets, plus 1
                for( int i = 0; i <= chunkheader; ++i ) {

                    file.read( (char *)&buffer[ 0 ], bytesperpixel );

                    if( bytesperpixel == 1 ) {
                        // expand greyscale data
                        buffer[ 1 ] = buffer[ 0 ];
                        buffer[ 2 ] = buffer[ 0 ];
                    }
                    // copy all four values in one operation
                    ( *datapointer ) = ( *bufferpointer );

                    ++datapointer;
                    ++currentpixel;
                }
            }
            else {
                // rle chunk, the color supplied afterwards is reapeated header + 1 times (not including the highest bit)
                chunkheader &= ~0x80;
                // read the current color
                file.read( (char *)&buffer[ 0 ], bytesperpixel );

                if( bytesperpixel == 1 ) {
                    // expand greyscale data
                    buffer[ 1 ] = buffer[ 0 ];
                    buffer[ 2 ] = buffer[ 0 ];
                }
                // copy the color into the image data as many times as dictated 
                for( int i = 0; i <= chunkheader; ++i ) {

                    ( *datapointer ) = ( *bufferpointer );
                    ++datapointer;
                    ++currentpixel;
                }
            }

        } while( currentpixel < pixelcount );
    }
    else {
        // unrecognized TGA sub-type
        data_state = resource_state::failed;
        return;
    }

    downsize( GL_BGRA );
    if( ( data_width > Global::iMaxTextureSize ) || ( data_height > Global::iMaxTextureSize ) ) {
        // for non-square textures there's currently possibility the scaling routine will have to abort
        // before it gets all work done
        data_state = resource_state::failed;
        return;
    }

    // TODO: add horizontal/vertical data flip, based on the descriptor (18th) header byte

    // fill remaining data info
    data_mapcount = 1;
    data_format = GL_BGRA;
    data_components =
        ( bytesperpixel == 4 ?
            GL_RGBA :
            GL_RGB );
    data_state = resource_state::good;

    return;
}

resource_state
opengl_texture::bind() {

    if( false == is_ready ) {

        create();
        if( data_state != resource_state::good ) {
            return data_state;
        }
    }
    ::glBindTexture( GL_TEXTURE_2D, id );
    return data_state;
}

resource_state
opengl_texture::create() {

    if( data_state != resource_state::good ) {
        // don't bother until we have useful texture data
        return data_state;
    }

    // TODO: consider creating and storing low-res version of the texture if it's ever unloaded from the gfx card,
    // as a placeholder until it can be loaded again
    if( id == -1 ) {

        ::glGenTextures( 1, &id );
        ::glBindTexture( GL_TEXTURE_2D, id );

        // analyze specified texture traits
        bool wraps{ true };
        bool wrapt{ true };
        for( auto const &trait : traits ) {

            switch( trait ) {

                case 's': { wraps = false; break; }
                case 't': { wrapt = false; break; }
            }
        }

        ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ( wraps == true ? GL_REPEAT : GL_CLAMP_TO_EDGE ) );
        ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ( wrapt == true ? GL_REPEAT : GL_CLAMP_TO_EDGE ) );

        set_filtering();

        if( data_mapcount == 1 ) {
            // fill missing mipmaps if needed
            ::glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
        }
        // upload texture data
        int dataoffset = 0,
            datasize = 0,
            datawidth = data_width,
            dataheight = data_height;
        for( int maplevel = 0; maplevel < data_mapcount; ++maplevel ) {

            if( ( data_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT )
                || ( data_format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT )
                || ( data_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ) ) {
                // compressed dds formats
                int const datablocksize =
                    ( data_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ?
                    8 :
                    16 );

                datasize = ( ( std::max( datawidth, 4 ) + 3 ) / 4 ) * ( ( std::max( dataheight, 4 ) + 3 ) / 4 ) * datablocksize;

                ::glCompressedTexImage2D(
                    GL_TEXTURE_2D, maplevel, data_format,
                    datawidth, dataheight, 0,
                    datasize, (GLubyte *)&data[ dataoffset ] );

                dataoffset += datasize;
                datawidth = std::max( datawidth / 2, 1 );
                dataheight = std::max( dataheight / 2, 1 );
            }
            else {
                // uncompressed texture data. have the gfx card do the compression as it sees fit
                ::glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA,
                    data_width, data_height, 0,
                    data_format, GL_UNSIGNED_BYTE, (GLubyte *)&data[ 0 ] );
            }
        }

        data.resize( 0 ); // TBD, TODO: keep the texture data if we start doing some gpu data cleaning down the road
/*
    data_state = resource_state::none;
*/
        data_state = resource_state::good;
        is_ready = true;
    }

    return data_state;
}

void
opengl_texture::set_filtering() {

    // default texture mode
    ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    ::glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

    if( GLEW_EXT_texture_filter_anisotropic ) {
        // anisotropic filtering
        ::glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, Global::AnisotropicFiltering );
    }

    bool sharpen{ false };
    for( auto const &trait : traits ) {

        switch( trait ) {

            case '#': { sharpen = true; break; }
/*
            // legacy filter modes. TODO, TBD: get rid of them?
            // let's just turn them off and see if anyone notices.
            case '4': {
                // najbliższy z tekstury
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
                break;
            }
            case '5': {
                //średnia z tekstury
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
                break;
            }
            case '6': {
                // najbliższy z mipmapy
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
                break;
            }
            case '7': {
                //średnia z mipmapy
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
                break;
            }
            case '8': {
                // najbliższy z dwóch mipmap
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR );
                break;
            }
            case '9': {
                //średnia z dwóch mipmap
                glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
                break;
            }
*/
        }
    }

    if( true == sharpen ) {
        // #: sharpen more
        ::glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -2.0 );
    }
    else {
        // regular texture sharpening
        ::glTexEnvf( GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, -1.0 );
    }
}

void
opengl_texture::downsize( GLuint const Format ) {

    while( ( data_width > Global::iMaxTextureSize ) || ( data_height > Global::iMaxTextureSize ) ) {
        // scale down the base texture, if it's larger than allowed maximum
        // NOTE: scaling is uniform along both axes, meaning non-square textures can drop below the maximum
        // TODO: replace with proper scaling function once we have image middleware in place
        if( ( data_width < 2 ) || ( data_height < 2 ) ) {
            // can't go any smaller
            break;
        }

        WriteLog( "Texture size exceeds specified limits, downsampling data" );
        switch( Format ) {

            case GL_RGB:  { downsample< glm::tvec3<std::uint8_t> >( data_width, data_height, data.data() ); break; }
            case GL_BGRA:
            case GL_RGBA: { downsample< glm::tvec4<std::uint8_t> >( data_width, data_height, data.data() ); break; }
            default:      { break; }
        }
        data_width /= 2;
        data_height /= 2;
        data.resize( data.size() / 4 ); // not strictly needed, but, eh
    };
}

// ustalenie numeru tekstury, wczytanie jeśli jeszcze takiej nie było
texture_handle
texture_manager::create( std::string Filename, std::string const &Dir, int const Filter, bool const Loadnow ) {

    if( Filename.find( '|' ) != std::string::npos )
        Filename.erase( Filename.find( '|' ) ); // po | może być nazwa kolejnej tekstury

    std::string traits;
    auto const traitpos = Filename.find( ':' );
    if( traitpos != std::string::npos ) {
        // po dwukropku mogą być podane dodatkowe informacje niebędące nazwą tekstury
        if( Filename.size() > traitpos + 1 )
            traits = Filename.substr( traitpos + 1 );
        Filename.erase( traitpos );
    }

    if( Filename.rfind( '.' ) != std::string::npos )
        Filename.erase( Filename.rfind( '.' ) ); // trim extension if there's one

    for( char &c : Filename ) {
        // change forward slashes to windows ones. NOTE: probably not strictly necessary, but eh
        c = ( c == '/' ? '\\' : c );
    }
/*
    std::transform(
        Filename.begin(), Filename.end(),
        Filename.begin(),
        []( char Char ){ return Char == '/' ? '\\' : Char; } );
*/
    if( Filename.find( '\\' ) == std::string::npos ) {
        // jeśli bieżaca ścieżka do tekstur nie została dodana to dodajemy domyślną
        Filename = szTexturePath + Filename;
    }

    std::vector<std::string> extensions{ { ".dds" }, { ".tga" }, { ".bmp" }, { ".ext" } };

    // try to locate requested texture in the databank
    auto lookup = find_in_databank( Filename + Global::szDefaultExt );
    if( lookup != npos ) {
        // start with the default extension...
        return lookup;
    }
    else {
        // ...then try recognized file extensions other than default
        for( auto const &extension : extensions ) {

            if( extension == Global::szDefaultExt ) {
                // we already tried this one
                continue;
            }
            lookup = find_in_databank( Filename + extension );
            if( lookup != npos ) {

                return lookup;
            }
        }
    }
    // if we don't have the texture in the databank, check if it's on disk
    std::string filename = find_on_disk( Filename + Global::szDefaultExt );
    if( true == filename.empty() ) {
        // if the default lookup fails, try other known extensions
        for( auto const &extension : extensions ) {

            if( extension == Global::szDefaultExt ) {
                // we already tried this one
                continue;
            }
            filename = find_on_disk( Filename + extension );
            if( false == filename.empty() ) {
                // we found something, don't bother with others
                break;
            }
        }
    }

    if( true == filename.empty() ) {
        // there's nothing matching in the databank nor on the disk, report failure
        ErrorLog( "Texture file missing: \"" + Filename + "\"" );
        return npos;
    }

    opengl_texture texture;
    texture.name = filename;
    if( ( Filter > 0 ) && ( Filter < 10 ) ) {
        // temporary. TODO, TBD: check how it's used and possibly get rid of it
        traits += std::to_string( ( Filter  < 4 ? Filter + 4 : Filter ) );
    }
    if( Filename.find('#') !=std::string::npos ) {
        // temporary code for legacy assets -- textures with names beginning with # are to be sharpened
        traits += '#';
    }
    texture.traits = traits;
    auto const textureindex = (texture_handle)m_textures.size();
    m_textures.emplace_back( texture );
    m_texturemappings.emplace( filename, textureindex );

    WriteLog( "Created texture object for \"" + filename + "\"" );

    if( true == Loadnow ) {

        texture_manager::texture( textureindex ).load();
#ifndef EU07_DEFERRED_TEXTURE_UPLOAD
        texture_manager::texture( textureindex ).create();
        // texture creation binds a different texture, force a re-bind on next use
        m_activetexture = 0;
#endif
    }

    return textureindex;
};

void
texture_manager::bind( texture_handle const Texture ) {

    if( Texture == m_activetexture ) {
        // don't bind again what's already active
        return;
    }
    // TODO: do binding in texture object, add support for other types
    if( Texture != 0 ) {
#ifndef EU07_DEFERRED_TEXTURE_UPLOAD
        // NOTE: we could bind dedicated 'error' texture here if the id isn't valid
        ::glBindTexture( GL_TEXTURE_2D, texture(Texture).id );
        m_activetexture = texture(Texture).id;
#else
        if( texture( Texture ).bind() == resource_state::good ) {
            m_activetexture = Texture;
        }
        else {
            // TODO: bind a special 'error' texture on failure
        }
#endif
    }
    else {

        ::glBindTexture( GL_TEXTURE_2D, 0 );
        m_activetexture = 0;
    }
    // all done
    return;
}

void
texture_manager::delete_textures() {
    for( auto const &texture : m_textures ) {
        // usunięcie wszyskich tekstur (bez usuwania struktury)
        if( ( texture.id > 0 )
         && ( texture.id != -1 ) ) {
            ::glDeleteTextures( 1, &texture.id );
        }
    }
}

// debug performance string
std::string
texture_manager::info() const {

    // TODO: cache this data and update only during resource sweep
    std::size_t totaltexturecount{ m_textures.size() - 1 };
    std::size_t totaltexturesize{ 0 };
#ifdef EU07_DEFERRED_TEXTURE_UPLOAD
    std::size_t readytexturecount{ 0 };
    std::size_t readytexturesize{ 0 };
#endif

    for( auto const& texture : m_textures ) {

        totaltexturesize += texture.size;
#ifdef EU07_DEFERRED_TEXTURE_UPLOAD

        if( texture.is_ready ) {

            ++readytexturecount;
            readytexturesize += texture.size;
        }
#endif
    }

    return
        "Textures: "
#ifdef EU07_DEFERRED_TEXTURE_UPLOAD
        + std::to_string( readytexturecount )
        + " ("
        + to_string( readytexturesize / 1024.0f, 2 ) + " mb)"
        + " in vram, "
#endif
        + std::to_string( totaltexturecount )
        + " ("
        + to_string( totaltexturesize / 1024.0f, 2 ) + " mb)"
        + " total";
}

// checks whether specified texture is in the texture bank. returns texture id, or npos.
texture_handle
texture_manager::find_in_databank( std::string const &Texturename ) const {

    auto lookup = m_texturemappings.find( Texturename );
    if( lookup != m_texturemappings.end() ) {
        return lookup->second;
    }
    // jeszcze próba z dodatkową ścieżką
    lookup = m_texturemappings.find( szTexturePath + Texturename );

    return (
        lookup != m_texturemappings.end() ?
            lookup->second :
            npos );
}

// checks whether specified file exists.
std::string
texture_manager::find_on_disk( std::string const &Texturename ) const {

    {
        std::ifstream file( Texturename );
        if( true == file.is_open() ) {
            // success
            return Texturename;
        }
    }
    // if we fail make a last ditch attempt in the default textures directory
    {
        std::ifstream file( szTexturePath + Texturename );
        if( true == file.is_open() ) {
            // success
            return szTexturePath + Texturename;
        }
    }
    // no results either way, report failure
    return "";
}
