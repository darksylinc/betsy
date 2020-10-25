#define NOMINMAX

#include "betsy/CpuImage.h"
#include "betsy/EncoderBC1.h"
#include "betsy/EncoderBC4.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderEAC.h"
#include "betsy/EncoderETC1.h"
#include "betsy/EncoderETC2.h"
#include "betsy/File/FormatKTX.h"

#include "CmdLineParams.h"
#include "CmdLineParamsEnum.h"

#include <algorithm>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

namespace betsy
{
	extern void initBetsyPlatform();
	extern void shutdownBetsyPlatform();
	extern void pollPlatformWindow();
}  // namespace betsy

void printHelp()
{
	printf( "Usage:\n" );
	printf( "	betsy input.hdr --codec=etc2_rgb --quality=2 output.ktx\n\n" );

	printf( "Supported input formats:\n" );
	printf( "	Whatever FreeImage supports (png, jpg, hdr, exr, bmp, ...):\n" );
	printf( "	Does not read KTX nor DDS\n" );

	printf( "Supported output formats:\n" );
	printf( "	KTX\n" );

	printf( "Supported codecs:\n" );
	printf(
		"	etc1            0.5 bpp - ETC1 RGB, Works on ETC1 HW. Backwards compatible w/ ETC2 HW\n"
		"	etc2_rgba_etc1  1.0 bpp - ETC1 RGB + EAC Alpha. Requires ETC2 HW\n"
		"	etc2_rgb        0.5 bpp - ETC2 RGB\n"
		"	etc2_rgba       1.0 bpp - ETC2 RGB + EAC Alpha\n"
		"	eac_r11         0.5 bpp - EAC Red unorm (source 11-bits per pixel)\n"
		"	eac_rg11        1.0 bpp - EAC RG unorm (11-bits each, useful for normal maps)\n"
		"	bc6h            1.0 bpp - BC6 Unsigned half-floating point format, RGB\n"
		"	bc1             0.5 bpp - BC1 RGB aka DXT1\n"
		"	bc3             1.0 bpp - BC3 RGBA aka DXT5\n"
		"	bc4             0.5 bpp - BC4 Red unorm\n"
		"	bc4_snorm       0.5 bpp - BC4 Red snorm\n"
		"	bc5             1.0 bpp - BC5 RG unorm\n"
		"	bc5_snorm       1.0 bpp - BC5 RG snorm. Ideal for normal maps\n" );

	printf( "\nbpp here stands for 'bytes per pixel', rather than bits per pixel.\n"
			"\nOther options:\n" );
	printf(
		"	--quality       Value in range [0; 2] where 0 is lowest quality.\n"
		"	                Not all codecs support it.\n"
		"	--dither        Use Floyd-steinberg dithering. Anti-banding method.\n"
		"	                Not recommended on images with flat colours (e.g. charts, normal maps)\n"
		"	                Supported by BC1 & ETC1/2\n"
		"	--renderdoc     Use this to ensure RenderDoc capture works (slower)\n" );
}

/** Returns true if 'str' starts with the text contained in 'what'
@param str
@param what
@param outStartIdx
	str[outStartIdx] points to the first letter (including null terminator)
	that diverged from 'what', even if we return false
@return
	True if there's a match, false otherwise
*/
bool startsWith( const char *str, const char *what, size_t &outStartIdx )
{
	const char *origStart = str;

	while( *str && *what && *str == *what )
	{
		++str;
		++what;
	}

	outStartIdx = static_cast<size_t>( str - origStart );

	return *what == '\0';
}

bool parseCmdLine( int nargs, char *const argv[], CmdLineParams &outParams )
{
	if( nargs < 3 )
		return false;

	size_t currFilename = 0u;
	for( int i = 1; i < nargs; ++i )
	{
		size_t startIdx;
		if( startsWith( argv[i], "--codec=", startIdx ) )
		{
			std::string codecName( argv[i] + startIdx );
			if( !Codec::CodecEnum::find( codecName, outParams.codec ) )
			{
				printf( "No such codec: '%s'\n", argv[i] + startIdx );
				return false;
			}
		}
		else if( startsWith( argv[i], "--quality=", startIdx ) )
		{
			outParams.quality = static_cast<uint8_t>( atoi( argv[i] + startIdx ) );
			outParams.quality = std::min<uint8_t>( outParams.quality, 2u );
		}
		else if( startsWith( argv[i], "--dither", startIdx ) )
		{
			outParams.dither = true;
		}
		else if( startsWith( argv[i], "--renderdoc", startIdx ) )
		{
			outParams.usingRenderDoc = true;
		}
		else if( startsWith( argv[i], "--help", startIdx ) )
		{
			return false;
		}
		else if( startsWith( argv[i], "--", startIdx ) )
		{
			printf( "Unknown parameter '%s'\n", argv[i] );
			return false;
		}
		else
		{
			if( currFilename >= 2u )
			{
				printf( "Unknown parameter '%s'\n", argv[i] );
				return false;
			}
			outParams.filename[currFilename] = argv[i];
			++currFilename;
		}
	}

	if( currFilename != 2u )
	{
		printf( "Input and output filenames not given" );
		return false;
	}

	return true;
}

template <typename T>
void saveToDisk( T &encoder, const CmdLineParams params )
{
	betsy::CpuImage downloadImg;
	encoder.startDownload();
	encoder.downloadTo( downloadImg );
	betsy::FormatKTX::save( params.filename[1].c_str(), downloadImg );
	downloadImg.data = 0;  // This pointer is not owned by CpuImage and must not be freed
}

int main( int nargs, char *const argv[] )
{
	CmdLineParams params;
	params.quality = 2;

	if( !parseCmdLine( nargs, argv, params ) )
	{
		printHelp();
		return -1;
	}

	printf( "Initializing API\n" );
	betsy::initBetsyPlatform();

	size_t repeat = params.usingRenderDoc ? 2u : 1u;

	betsy::CpuImage cpuImage( params.filename[0].c_str() );

	switch( params.codec )
	{
	case Codec::etc1:
	case Codec::etc2_rgba_etc1:
	{
		betsy::EncoderETC1 encoder;
		encoder.initResources( cpuImage, params.codec == Codec::etc2_rgba_etc1, params.dither );
		while( repeat-- )
		{
			encoder.execute00();
			encoder.execute01( static_cast<betsy::EncoderETC1::Etc1Quality>( params.quality ) );
			encoder.execute02();
			if( params.usingRenderDoc )
				encoder.execute03();  // Not needed in offline mode
			betsy::pollPlatformWindow();
		}
		saveToDisk( encoder, params );
		encoder.deinitResources();
	}
	break;
	case Codec::etc2_rgb:
	case Codec::etc2_rgba:
	{
		betsy::EncoderETC2 encoder;
		encoder.initResources( cpuImage, params.codec == Codec::etc2_rgba, params.dither );
		while( repeat-- )
		{
			encoder.execute00();
			encoder.execute01( static_cast<betsy::EncoderETC1::Etc1Quality>( params.quality ) );
			encoder.execute02();
			if( params.usingRenderDoc )
				encoder.execute03();  // Not needed in offline mode
			betsy::pollPlatformWindow();
		}
		saveToDisk( encoder, params );
		encoder.deinitResources();
	}
	break;
	case Codec::eac_r11:
	case Codec::eac_rg11:
	{
		betsy::EncoderEAC encoder;
		encoder.initResources( cpuImage, params.codec == Codec::eac_rg11 );
		encoder.execute01();
		encoder.execute02();
		betsy::pollPlatformWindow();
		saveToDisk( encoder, params );
		encoder.deinitResources();
	}
	break;
	case Codec::bc1:
	case Codec::bc3:
	{
		betsy::EncoderBC1 encoder;
		encoder.initResources( cpuImage, params.codec == Codec::bc3, params.dither );
		while( repeat-- )
		{
			encoder.execute01();
			encoder.execute02();
			if( params.usingRenderDoc )
				encoder.execute03();  // Not needed in offline mode
			betsy::pollPlatformWindow();
		}
		saveToDisk( encoder, params );
		encoder.deinitResources();
	}
	break;
	case Codec::bc4:
	case Codec::bc4_snorm:
	case Codec::bc5:
	case Codec::bc5_snorm:
	{
		betsy::EncoderBC4 encoder;
		encoder.initResources( cpuImage, params.codec == Codec::bc5 || params.codec == Codec::bc5_snorm,
							   params.codec == Codec::bc4_snorm || params.codec == Codec::bc5_snorm );
		while( repeat-- )
		{
			encoder.execute01();
			encoder.execute02();
			if( params.usingRenderDoc )
				encoder.execute03();  // Not needed in offline mode
			betsy::pollPlatformWindow();
		}
		saveToDisk( encoder, params );
		encoder.deinitResources();
	}
	break;
	case Codec::bc6h:
	{
		betsy::EncoderBC6H encoder;
		encoder.initResources( cpuImage );
		encoder.execute01();
		// encoder.execute02(); Not needed in offline mode
		betsy::pollPlatformWindow();
		saveToDisk( encoder, params );
		encoder.deinitResources();
	}
	break;
	}

	betsy::pollPlatformWindow();

	printf( "Shutting down\n" );
	betsy::shutdownBetsyPlatform();

	return 0;
}
