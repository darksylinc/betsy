
#pragma once

#include <stdint.h>
#include <algorithm>
#include <string>

#ifdef _MSC_VER
#	include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

namespace sds
{
	/** @ingroup sds
	@class fstream
		SDS stands for "super duper standard library replacement". Yes, silly name.

		fstream is a near-drop-in replacement for std::ifstream & ofstream which
		relies on C fstreams as a backend.

		The main reason for this development is to provide easier read() and write() functions
		and to provide fsync which is an important missing function from the STL.
	*/
	class fstream
	{
	public:
		enum FileOpenMode
		{
			Input,
			/// Same as Input, but starts the cursor at the end of the file
			InputEnd,
			/// File is kept, must exist
			OutputKeep,
			/// Same as OutputKeep, but starts the cursor at the end of the file
			OutputKeepEnd,
			OutputDiscard,
			/// Same as OutputKeep, but allows reading
			InOutKeep,
			/// Same as OutputEnd, but allows reading
			InOutEnd,
		};

		enum StatusBits
		{
			eof		= 1u << 0u,
			badbit	= 1u << 1u,
			failbit	= 1u << 2u,
		};
		enum Whence
		{
			beg,
			cur,
			end,
		};
	protected:
		FILE	*m_handle;
		uint8_t	m_statusBits;
		bool	m_canRead;
		bool	m_canWrite;

	public:
		fstream();
		fstream( const std::string &fullpath, FileOpenMode mode );
		fstream( const char *fullpath, FileOpenMode mode );
		~fstream();

		void open( const std::string &fullpath, FileOpenMode mode );
		void open( const char *fullpath, FileOpenMode mode );
		void close();

		bool is_open() const;
		bool good();

		size_t read( char *outData, size_t sizeBytes );
		size_t write( const char *inData, size_t sizeBytes );

		void seek( ssize_t dir, Whence whence );
		size_t tell();

		int flush();

		/// Implies calling flush()
		void fsync( bool preferDataSync );

		template <typename T>
		size_t read( T &outValue )
		{
			return read( reinterpret_cast<char*>( &outValue ), sizeof( T ) );
		}
		template <typename T>
		T read()
		{
			T value = (T)0;
			read( reinterpret_cast<char*>( &value ), sizeof( T ) );
			return value;
		}
		template <typename T>
		size_t write( T inValue )
		{
			return write( reinterpret_cast<const char*>( &inValue ), sizeof( T ) );
		}

		/// Same as readString, but assumes the length is an 8-bit value
		std::string readString8()
		{
			const uint8_t strLength = read<uint8_t>();
			std::string string;
			string.resize( strLength );

			if( strLength )
				read( reinterpret_cast<char *>( &string[0] ), strLength );

			return string;
		}

		/// Same as writeString, but assumes the length is an 8-bit value
		void writeString8( const std::string &inValue )
		{
			const uint8_t strSize = static_cast<uint8_t>( std::min<size_t>( inValue.size(), 255u ) );
			write<uint8_t>( strSize );
			write( inValue.c_str(), strSize );
		}
	};

	template <> size_t fstream::read<bool>( bool &outValue );
	template <> size_t fstream::write<bool>( bool inValue );
}
