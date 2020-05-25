
#include "betsy/CpuImage.h"
#include "betsy/EncoderBC6H.h"
#include "betsy/EncoderETC1.h"

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

	betsy::CpuImage cpuImage( "/home/matias/untitled.png" );
	//betsy::CpuImage cpuImage( "/home/matias/Projects/BC6H_BC7/rg-etc1/test0.png" );
	betsy::EncoderETC1 encoderBC6H;

	encoderBC6H.initResources( cpuImage );

	int repeat = 0;

	while( repeat < 2 )
	{
		encoderBC6H.execute01();
		encoderBC6H.execute02();
		betsy::pollPlatformWindow();
		++repeat;
	}

	encoderBC6H.deinitResources();

	printf( "Shutting down\n" );
	betsy::shutdownBetsyPlatform();

	return 0;
}
