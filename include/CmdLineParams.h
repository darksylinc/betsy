
#pragma once

#include <stdint.h>
#include <string>

namespace Codec
{
	enum Codec
	{
		etc1,
		etc2_rgb,
		etc2_rgba,
		eac_r11,
		eac_rg11,
		bc1,
		bc6h
	};
}

struct CmdLineParams
{
	uint8_t quality;
	Codec::Codec codec;
	std::string filename[2];
};
