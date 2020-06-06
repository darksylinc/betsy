
#pragma once

#include <limits>
#include <string>

namespace sds
{
	/// Returns true if s1 > s2, checks for wrap-around.
	template <typename T>
	static bool isSequenceMoreRecent( T s1, T s2 )
	{
		// clang-format off
		const T max = std::numeric_limits<T>::max();
		return
			( ( s1 > s2 ) &&
			  ( s1 - s2 <= (max >> 1u) ) )
			   ||
			( ( s2 > s1 ) &&
			  ( s2 - s1  > (max >> 1u) ) );
		// clang-format on
	}
}  // namespace sds
