
#pragma once

#include <stdint.h>
#include <string>

namespace Codec
{
	enum Codec
	{
		etc1,
		etc2_rgba_etc1,
		etc2_rgb,
		etc2_rgba,
		eac_r11,
		eac_rg11,
		bc1,
		bc3,
		bc4,
		bc4_snorm,
		bc5,
		bc5_snorm,
		bc6h
	};
}

struct CmdLineParams
{
	uint8_t      quality;
	bool         dither;
	bool         usingRenderDoc;
	Codec::Codec codec;
	std::string  filename[2];

	CmdLineParams() : quality( 0 ), dither( false ), usingRenderDoc( false ), codec( Codec::etc1 ) {}
};
