/*!
  \file vnc-encoding-zrle.cc
  \brief Implementation of the ZRLE update encoding type.
  \author John R. Hall
*/

#include <iostream>
#include "vnc.h"

using namespace std;

#define GET_UINT32( var ) m_net.ReceiveBytes( (Uint8*)&var, 4 ); var = VNC_SWAP_BE_32( var );
#define NET_UINT32( var ) Uint32 var; GET_UINT32( var );
#define GET_UINT16( var ) m_net.ReceiveBytes( (Uint8*)&var, 2 ); var = VNC_SWAP_BE_16( var );
#define NET_UINT16( var ) Uint16 var; GET_UINT16( var );
#define NET_UINT8( var ) Uint8 var; m_net.ReceiveBytes( &var, 1 );

#define ZIP_UINT32( var ) Uint32 var; m_zlib_reader.Read( var );
#define ZIP_UINT16( var ) Uint16 var; m_zlib_reader.Read( var );
#define ZIP_UINT8( var )  Uint8 var; m_zlib_reader.Read( var );

namespace VNC
{

	DEFINE_VNC_DECODER( ZRLE )
	{
		++m_processed;

		// read the length of the compressed data
		//! \todo sanity check this length
		NET_UINT32( compressed_length );

		// receive the data and hand it to the decoder
		Uint8* compressed_buf = new Uint8[compressed_length];
		m_net.ReceiveBytes( compressed_buf, compressed_length );
		m_zlib_reader.SetStream( compressed_buf, compressed_length );

		ZIP_UINT8( subencoding );
		cerr << "Subencoding: " << (int)subencoding << endl;
		
		delete compressed_buf;		
	}

};
