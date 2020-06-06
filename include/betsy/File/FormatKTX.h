
#pragma once

#include "betsy/CpuImage.h"

namespace betsy
{
	class FormatKTX
	{
	public:
		static void save( const char *fullpath, const CpuImage &cpuImage );
	};
}
