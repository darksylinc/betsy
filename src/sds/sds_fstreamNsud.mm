
#include "sds/sds_fstreamNsud.h"

#include <limits>

#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <Foundation/NSUserDefaults.h>

namespace sds
{
	static const size_t c_defaultBufferSize = 512u;

	fstreamNsud::fstreamNsud() :
		m_data( 0 ),
		m_offset( 0 ),
		m_size( 0 ),
		m_statusBits( 0 ),
		m_canRead( false ),
		m_canWrite( false )
	{
	}
	//-------------------------------------------------------------------------
	fstreamNsud::fstreamNsud( const char *fullpath, FileOpenMode mode ) :
		m_data( 0 ),
		m_offset( 0 ),
		m_size( 0 ),
		m_statusBits( 0 ),
		m_canRead( false ),
		m_canWrite( false )
	{
		open( fullpath, mode );
	}
	//-------------------------------------------------------------------------
	fstreamNsud::fstreamNsud( const std::string &fullpath, FileOpenMode mode ) :
		m_data( 0 ),
		m_offset( 0 ),
		m_size( 0 ),
		m_statusBits( 0 ),
		m_canRead( false ),
		m_canWrite( false )
	{
		open( fullpath.c_str(), mode );
	}
	//-------------------------------------------------------------------------
	fstreamNsud::~fstreamNsud() { close(); }
	//-------------------------------------------------------------------------
	void fstreamNsud::growToFit( size_t requestedLength )
	{
		if( m_offset + requestedLength > m_size )
		{
			// Reallocate
			size_t newSize = m_offset + ( m_offset >> 1 ) + requestedLength;
			uint8_t *tmpBuffer = reinterpret_cast<uint8_t *>( malloc( newSize ) );
			memcpy( tmpBuffer, m_data, m_size );
			free( m_data );
			m_data = tmpBuffer;
			m_size = newSize;
		}
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::readFromNSUserDefaults( bool seekEnd, bool createIfNotFound )
	{
		NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

		NSString *filename = [NSString stringWithUTF8String:m_filename.c_str()];
		NSData *data = [userDefaults dataForKey:filename];
		if( data )
		{
			m_fileSize = static_cast<size_t>( data.length );
			growToFit( m_fileSize );
			memcpy( m_data, data.bytes, data.length );
			if( seekEnd )
				m_offset = m_fileSize;
		}
		else if( createIfNotFound )
		{
			growToFit( c_defaultBufferSize );
		}
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::writeToNSUserDefaults()
	{
		NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
		NSData *data = [NSData dataWithBytesNoCopy:m_data length:m_fileSize freeWhenDone:NO];
		NSString *filename = [NSString stringWithUTF8String:m_filename.c_str()];
		[userDefaults setObject:data forKey:filename];
		[userDefaults synchronize];
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::open( const std::string &fullpath, FileOpenMode mode )
	{
		open( fullpath.c_str(), mode );
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::open( const char *fullpath, FileOpenMode mode )
	{
		close();

		m_filename = fullpath;

		switch( mode )
		{
		case fstreamNsud::Input:
		case fstreamNsud::InputEnd:
			readFromNSUserDefaults( mode == fstreamNsud::InputEnd, false );
			m_canRead = true;
			break;
		case fstreamNsud::OutputKeep:
		case fstreamNsud::OutputKeepEnd:
			readFromNSUserDefaults( mode == fstreamNsud::OutputKeepEnd, true );
			m_canWrite = true;
			break;
		case fstreamNsud::OutputDiscard:
			growToFit( c_defaultBufferSize );
			m_canWrite = true;
			break;
		case fstreamNsud::InOutKeep:
		case fstreamNsud::InOutEnd:
			readFromNSUserDefaults( mode == fstreamNsud::InOutEnd, true );
			m_canRead = true;
			m_canWrite = true;
			break;
		}
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::close()
	{
		if( m_data && m_canWrite )
			writeToNSUserDefaults();

		if( m_data )
		{
			free( m_data );
			m_data = 0;
		}

		m_offset = 0u;
		m_fileSize = 0u;
		m_size = 0u;

		m_statusBits = 0u;
		m_canRead = false;
		m_canWrite = false;
	}
	//-------------------------------------------------------------------------
	bool fstreamNsud::is_open() const { return m_data != 0; }
	//-------------------------------------------------------------------------
	bool fstreamNsud::good()
	{
		if( m_data == 0 )
			return false;
		return m_statusBits == 0u || m_statusBits == fstreamNsud::eof;
	}
	//-------------------------------------------------------------------------
	size_t fstreamNsud::read( char *outData, size_t sizeBytes )
	{
		if( m_canRead && good() )
		{
			assert( m_offset <= m_fileSize );
			const size_t clampedSizeBytes = std::min( sizeBytes, m_fileSize - m_offset );
			memcpy( outData, m_data + m_offset, clampedSizeBytes );
			m_offset += clampedSizeBytes;

			if( m_offset >= m_fileSize )
				m_statusBits |= fstreamNsud::eof;

			return clampedSizeBytes;
		}
		else
		{
			m_statusBits |= fstreamNsud::failbit;
			return std::numeric_limits<size_t>::max();
		}
	}
	//-------------------------------------------------------------------------
	size_t fstreamNsud::write( const char *inData, size_t sizeBytes )
	{
		if( m_canWrite && good() )
		{
			growToFit( sizeBytes );
			memcpy( m_data + m_offset, inData, sizeBytes );
			m_offset += sizeBytes;
			m_fileSize = std::max( m_fileSize, m_offset );
			return sizeBytes;
		}
		else
		{
			m_statusBits |= fstreamNsud::failbit;
			return std::numeric_limits<size_t>::max();
		}
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::seek( ssize_t dir, Whence whence )
	{
		if( !good() )
		{
			m_statusBits |= fstreamNsud::failbit;
			return;
		}

		int fileWhence = 0;
		switch( whence )
		{
		case fstreamNsud::beg:
			m_offset = static_cast<size_t>( dir );
			break;
		case fstreamNsud::cur:
			m_offset += static_cast<size_t>( dir );
			break;
		case fstreamNsud::end:
			m_offset = m_fileSize + static_cast<size_t>( dir );
			fileWhence = SEEK_END;
			break;
		}

		if( m_offset == m_fileSize )
			m_statusBits |= fstreamNsud::eof;
		else if( m_offset > m_fileSize )
			m_statusBits |= fstreamNsud::failbit;
	}
	//-------------------------------------------------------------------------
	size_t fstreamNsud::tell()
	{
		if( !good() )
		{
			m_statusBits |= fstreamNsud::failbit;
			return std::numeric_limits<size_t>::max();
		}
		return m_offset;
	}
	//-------------------------------------------------------------------------
	int fstreamNsud::flush()
	{
		if( m_canWrite )
		{
			writeToNSUserDefaults();
			return 0;
		}
		else
		{
			m_statusBits |= fstreamNsud::failbit;
			return -1;
		}
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::fsync( bool preferDataSync )
	{
		if( !m_canWrite )
		{
			m_statusBits |= fstreamNsud::failbit;
			return;
		}

		flush();
	}
	//-------------------------------------------------------------------------
	void fstreamNsud::_changeFilename( const std::string &newFilename ) { m_filename = newFilename; }
	//-------------------------------------------------------------------------
	template <>
	size_t fstreamNsud::read<bool>( bool &outValue )
	{
		uint8_t value = 0;
		const size_t retVal = read<uint8_t>( value );
		outValue = value != 0;
		return retVal;
	}
	//-------------------------------------------------------------------------
	template <>
	size_t fstreamNsud::write<bool>( bool inValue )
	{
		uint8_t value = inValue ? 1u : 0u;
		return write<uint8_t>( value );
	}
}
