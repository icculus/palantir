#include <iostream>
#include "zlib-reader.h"

using namespace std;

namespace VNC
{

	static voidpf ZAlloc( voidpf, uInt items, uInt size )
	{
		return malloc( items * size );
	}

	static void ZFree( voidpf, voidpf address )
	{
		free( address );
	}
	
	ZlibReader::ZlibReader()
	{
		memset( &m_zs, 0, sizeof( m_zs ) );
		m_zs.zalloc = ZAlloc;
		m_zs.zfree = ZFree;
		if( inflateInit( &m_zs ) != Z_OK )
			throw Exc( "unable to initialize zlib" );
	}
	
	ZlibReader::~ZlibReader()
	{
		inflateEnd( &m_zs );
	}

	void ZlibReader::SetStream( Uint8* input, int size )
	{
		cerr << "stream has " << size << " bytes" << endl;
		m_zs.next_in = (Bytef*)input;
		m_zs.avail_in = size;
	}

	void ZlibReader::ReadBytes( Uint8* buf, int length )
	{
		m_zs.next_out = (Bytef*)buf;
		m_zs.avail_out = length;
		do
		{
			if( inflate( &m_zs, Z_SYNC_FLUSH ) != Z_OK )
			{
				throw Exc( "unable to decompress data" );
			}
		} while( m_zs.next_out - (Uint8*)buf < length );
	}
	
};
