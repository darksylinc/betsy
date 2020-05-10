
#pragma once

#include <string>

namespace betsy
{
	class IncludeParser
	{
		std::string m_source;

		static bool loadFromFileInto( const char *fullpath, std::string &outSource );

		bool parseIncludeFiles( const char *relativePath );

	public:
		bool loadFromFile( const char *filename, const char *relativePath );

		const char *getFinalSource() const;
	};
}  // namespace betsy
