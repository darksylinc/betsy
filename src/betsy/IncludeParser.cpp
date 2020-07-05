
#include "betsy/IncludeParser.h"

#include <algorithm>
#include <fstream>

namespace betsy
{
	bool IncludeParser::loadFromFileInto( const char *fullpath, std::string &outSource )
	{
		std::ifstream file;
		file.open( fullpath, std::ios_base::binary | std::ios_base::in );
		if( file.fail() )
		{
			printf( "Error opening %s\n", fullpath );
			return false;
		}

		file.seekg( 0, std::ios_base::end );
		const size_t fileSize = static_cast<size_t>( file.tellg() );
		outSource.resize( fileSize );

		file.seekg( 0, std::ios_base::beg );
		file.read( &outSource[0], static_cast<std::streamsize>( fileSize ) );

		// Remove "#version" from first newline
		const size_t firstNewlinePos = outSource.find_first_of( '\n' );
		if( firstNewlinePos != std::string::npos )
		{
			const size_t versionPos = outSource.substr( 0u, firstNewlinePos ).find( "#version ", 0u );
			if( versionPos != std::string::npos )
				outSource.erase( 0u, firstNewlinePos + 1u );
		}

		return true;
	}
	//-------------------------------------------------------------------------
	bool IncludeParser::loadFromFile( const char *filename, const char *relativePath )
	{
		std::string fullpath = relativePath;
		fullpath += filename;

		std::ifstream file;
		file.open( fullpath.c_str(), std::ios_base::binary | std::ios_base::in );
		if( file.fail() )
		{
			printf( "Error opening %s\n", fullpath.c_str() );
			return false;
		}

		file.seekg( 0, std::ios_base::end );
		const size_t fileSize = static_cast<size_t>( file.tellg() );
		m_source.resize( fileSize );

		file.seekg( 0, std::ios_base::beg );
		file.read( &m_source[0], static_cast<std::streamsize>( fileSize ) );

		return parseIncludeFiles( relativePath );
	}
	//-------------------------------------------------------------------------
	bool IncludeParser::parseIncludeFiles( const char *relativePath )
	{
		size_t startPos = 0;
		std::string includeKeyword = "#include ";

		startPos = m_source.find( includeKeyword, startPos );
		while( startPos != std::string::npos )
		{
			// Backtrack to see if have to skip commented lines like "// #include".
			// We do not support block comments /**/
			const size_t lineStartPos = m_source.rfind( '\n', startPos );
			if( lineStartPos == std::string::npos || lineStartPos + 2u >= m_source.size() ||
				m_source[lineStartPos + 1] != '/' || m_source[lineStartPos + 2] != '/' )
			{
				size_t pos = startPos + includeKeyword.length() + 1u;
				size_t eolMarkerPos = m_source.find( '\n', pos );
				size_t endPos0 = m_source.find( '"', pos );
				size_t endPos1 = m_source.find( '>', pos );
				size_t endPos = std::min( endPos0, endPos1 );

				if( endPos == std::string::npos ||
					( endPos >= eolMarkerPos && eolMarkerPos != std::string::npos ) )
				{
					printf(
						"Invalid #include syntax near %s",
						m_source
							.substr( startPos, std::min( startPos + 10u, m_source.size() - startPos ) )
							.c_str() );
					return false;
				}

				std::string file = m_source.substr( pos, endPos - pos );

				std::string includeContent;
				if( loadFromFileInto( ( relativePath + file ).c_str(), includeContent ) )
					m_source.replace( startPos, ( endPos + 1u ) - startPos, includeContent );
				else
				{
					// Leave the included header, fallback to the compiler
					// (e.g. Metal can include system headers)
					startPos = endPos;
				}
			}
			else
			{
				startPos += sizeof( "#include " ) - 1u;
			}

			startPos = m_source.find( includeKeyword, startPos );
		}

		return true;
	}
	//-------------------------------------------------------------------------
	const char *IncludeParser::getFinalSource() const { return m_source.c_str(); }
}  // namespace betsy
