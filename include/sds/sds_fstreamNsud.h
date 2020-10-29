
#pragma once

#include <stdint.h>
#include <algorithm>
#include <string>

namespace sds
{
	/** @ingroup sds
	@class fstreamNsud
		SDS stands for "super duper standard library replacement". Yes, silly name.

		fstreamNsud is an sds::fstream implementation using NSUserDefaults storage as a backend.

		This is required on AppleTV which doesn't offer other forms of persistent storage.

		Beware the storage capacity of NSUserDefaults is very limited (500kb).
		This implementation keeps everything in memory, thus don't use it for large files.
	*/
	class fstreamNsud
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
			eof = 1u << 0u,
			badbit = 1u << 1u,
			failbit = 1u << 2u,
		};
		enum Whence
		{
			beg,
			cur,
			end,
		};

	protected:
		uint8_t *m_data;
		size_t m_offset;
		size_t m_fileSize;
		size_t m_size; // It's really capacity

		std::string m_filename;

		uint8_t m_statusBits;
		bool m_canRead;
		bool m_canWrite;

		/** Checks if we have enough space to write the requested bytes.
			If not, makes the memory grow by reallocating.
		@remarks
			Each time the container grows, it grows by 50% + requestedLength
		*/
		void growToFit( size_t requestedLength );

		void readFromNSUserDefaults( bool seekEnd, bool createIfNotFound );
		void writeToNSUserDefaults();

	public:
		fstreamNsud();
		fstreamNsud( const std::string &fullpath, FileOpenMode mode );
		fstreamNsud( const char *fullpath, FileOpenMode mode );
		~fstreamNsud();

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

		/// Changes the name of the file (i.e. where we're saving to)
		/// Caller is responsible for ensuring this happens before readFromNSUserDefaults /
		/// writeToNSUserDefaults, or that the side effects are irrelant.
		void _changeFilename( const std::string &newFilename );

		template <typename T>
		size_t read( T &outValue )
		{
			return read( reinterpret_cast<char *>( &outValue ), sizeof( T ) );
		}
		template <typename T>
		T read()
		{
			T value = (T)0;
			read( reinterpret_cast<char *>( &value ), sizeof( T ) );
			return value;
		}
		template <typename T>
		size_t write( T inValue )
		{
			return write( reinterpret_cast<const char *>( &inValue ), sizeof( T ) );
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

	template <>
	size_t fstreamNsud::read<bool>( bool &outValue );
	template <>
	size_t fstreamNsud::write<bool>( bool inValue );
}  // namespace sds
