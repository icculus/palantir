/*!
  \file zlib-reader.h
  \brief Simple wrapper class for reading from ZLIB.
  \author John R. Hall
*/

#ifndef ZLIB_READER_H
#define ZLIB_READER_H

#include <zlib.h>
#include "vnctypes.h"

namespace VNC
{

	class ZlibReader
	{
	
	public:
	
		ZlibReader();
		~ZlibReader();

		void SetStream( Uint8* input, int size );
	
		template< typename T >
		void Read( T& val );
		
		void ReadBytes( Uint8* buf, int length );
	
	private:

		z_stream  m_zs;    //!< zlib stream
	};

	template< typename T >
	void ZlibReader::Read( T& val )
	{
		ReadBytes( &val, sizeof( val ) );
	}

};

#endif
