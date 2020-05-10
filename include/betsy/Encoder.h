
#pragma once

#include <stdint.h>
#include <stddef.h>

namespace betsy
{
	/// Aligns the input 'offset' to the next multiple of 'alignment'.
	/// Alignment can be any value except 0. Some examples:
	///
	/// alignToNextMultiple( 0, 4 ) = 0;
	/// alignToNextMultiple( 1, 4 ) = 4;
	/// alignToNextMultiple( 2, 4 ) = 4;
	/// alignToNextMultiple( 3, 4 ) = 4;
	/// alignToNextMultiple( 4, 4 ) = 4;
	/// alignToNextMultiple( 5, 4 ) = 8;
	///
	/// alignToNextMultiple( 0, 3 ) = 0;
	/// alignToNextMultiple( 1, 3 ) = 3;
	inline uint32_t alignToNextMultiple( uint32_t offset, uint32_t alignment )
	{
		return ( (offset + alignment - 1u) / alignment ) * alignment;
	}
	inline size_t alignToNextMultiple( size_t offset, size_t alignment )
	{
		return ( (offset + alignment - 1u) / alignment ) * alignment;
	}

	class Encoder
	{
	};
}  // namespace betsy
