
#pragma once

#include <string>
#include <filesystem>

namespace betsy
{
	class IncludeParser
	{
		std::string m_source;

		static bool loadFromFileInto( const std::filesystem::path& path, std::string &outSource );

		bool parseIncludeFiles( const std::filesystem::path& relativePath );

	public:
		bool loadFromFile( const std::filesystem::path& filename);
		bool loadFromFile( const std::filesystem::path& filename, const std::filesystem::path& relativePath );

		const char *getFinalSource() const;
	};
}  // namespace betsy
