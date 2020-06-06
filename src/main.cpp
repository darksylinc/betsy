
#include "betsy/CpuImage.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderETC1.h"
#include "betsy/File/FormatKTX.h"

#include <stdio.h>

namespace betsy
{
	extern void initBetsyPlatform();
	extern void shutdownBetsyPlatform();
	extern void pollPlatformWindow();
}  // namespace betsy

int main()
{
	printf( "Initializing API\n" );
	betsy::initBetsyPlatform();

	betsy::CpuImage cpuImage( "../Data/test_pic.png" );
	// betsy::CpuImage cpuImage( "/home/matias/Projects/BC6H_BC7/rg-etc1/test0.png" );
	betsy::EncoderETC1 encoderBC6H;

	encoderBC6H.initResources( cpuImage, true );

	int repeat = 0;

	while( repeat < 1 )
	{
		encoderBC6H.execute01( betsy::EncoderETC1::cHighQuality );
		encoderBC6H.execute02();
		betsy::pollPlatformWindow();
		++repeat;
	}

	{
		betsy::CpuImage downloadImg;
		encoderBC6H.startDownload();
		encoderBC6H.downloadTo( downloadImg );
		betsy::FormatKTX::save( "./output.ktx", downloadImg );
		downloadImg.data = 0; // This pointer is not owned by CpuImage and must not be freed
	}

	encoderBC6H.deinitResources();

	printf( "Shutting down\n" );
	betsy::shutdownBetsyPlatform();

	return 0;
}
