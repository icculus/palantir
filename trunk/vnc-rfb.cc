/*!
  \file vnc-rfb.cc
  \brief Implementation of the RFB protocol.
  \author John R. Hall
*/

#include "vnc.h"
#include <string>
#include <iostream>

extern "C" {
#include "d3des.h"
};
	
using namespace std;

// Some macros to make life easier (and the code cleaner).
#define GET_UINT32( var ) m_net.ReceiveBytes( (Uint8*)&var, 4 ); var = VNC_SWAP_BE_32( var );
#define NET_UINT32( var ) Uint32 var; GET_UINT32( var );

#define GET_UINT16( var ) m_net.ReceiveBytes( (Uint8*)&var, 2 ); var = VNC_SWAP_BE_16( var );
#define NET_UINT16( var ) Uint16 var; GET_UINT16( var );

#define NET_UINT8( var ) Uint8 var; m_net.ReceiveBytes( &var, 1 );

#define NET_STRING( var, limit ) string var; { NET_UINT32(_v); if( _v > limit ) throw Exc( "received unreasonably long string" ); char* _tmp = new char[_v + 1]; m_net.ReceiveBytes( (Uint8*)_tmp, _v ); _tmp[_v] = '\0'; var = _tmp; delete _tmp; }
#define SEND_UINT8( val )  { Uint8 _t = val; m_net.SendBytes( (Uint8 const*)&_t, 1 ); }
#define SEND_UINT16( val ) { Uint16 _t = val; _t = VNC_SWAP_BE_16( _t ); m_net.SendBytes( (Uint8 const*)&_t, 2 ); }
#define SEND_UINT32( val ) { Uint32 _t = val; _t = VNC_SWAP_BE_32( _t ); m_net.SendBytes( (Uint8 const*)&_t, 4 ); }

// #define GET_UINT32( var ) m_net.ReceiveBytes( (Uint8*)&var, 4 ); var = VNC_SWAP_BE_32( var );
// #define NET_UINT32( var ) cerr << #var << " (uint32)" << endl; Uint32 var; GET_UINT32( var ); cerr << "--> " << var << endl;
// #define GET_UINT16( var ) m_net.ReceiveBytes( (Uint8*)&var, 2 ); var = VNC_SWAP_BE_16( var );
// #define NET_UINT16( var ) cerr << #var << " (uint16)" << endl; Uint16 var; GET_UINT16( var ); cerr << "--> " << var << endl;
// #define NET_UINT8( var ) cerr << #var << " (uint8)" << endl; Uint8 var; m_net.ReceiveBytes( &var, 1 );  cerr << "--> " << (int)var << endl;


// Client -> server message types
#define RFB_CLIENT_SETPIXELFORMAT        0
#define RFB_CLIENT_FIXCOLORMAPENTRIES    1
#define RFB_CLIENT_SETENCODINGS          2
#define RFB_CLIENT_FBUPDATEREQUEST       3
#define RFB_CLIENT_KEYEVENT              4
#define RFB_CLIENT_POINTEREVENT          5
#define RFB_CLIENT_CUTTEXT               6

// Server -> client message types
#define RFB_SERVER_FBUPDATE              0
#define RFB_SERVER_SETCOLORMAPENTRIES    1
#define RFB_SERVER_BELL                  2
#define RFB_SERVER_CUTTEXT               3


namespace VNC
{

	RFBProto::RFBProto( NetworkClient& net, std::string const& password, bool shared, std::vector< Decoder* > decoders )
		: m_shared( shared ),
		  m_net( net ),
		  m_password( password ),
		  m_rfb_major_version( -1 ),
		  m_rfb_minor_version( -1 ),
		  m_desktop_width( -1 ),
		  m_desktop_height( -1 ),
		  m_desktop_name( "not connected" ),
		  m_decoders_vec( decoders )
	{
		for( unsigned i = 0; i < decoders.size(); ++i )
			m_decoders[ decoders[i]->GetType() ] = decoders[i];
		
		DoVersionHandshake();
		DoAuthHandshake();
		DoInitHandshake();
		DoSupportedEncodings();
	}

	RFBProto::~RFBProto()
	{
		// disconnect
	}

	void RFBProto::DoVersionHandshake()
	{
		char buf[13];

		// receive server version string
		m_net.ReceiveBytes( (Uint8*)buf, 12 );
		buf[11] = '\0';  // chop off \n
		string version = buf;		

		// grab version numbers
		string rfb_id = version.substr( 0, 3 );
		string rfb_major = version.substr( 4, 3 );
		string rfb_minor = version.substr( 8, 3 );
		m_rfb_major_version = atoi( rfb_major.c_str() );
		m_rfb_minor_version = atoi( rfb_minor.c_str() );

		// check version
		if( rfb_id != "RFB" )
			throw ExcNotRFB();
		if( m_rfb_major_version != 3 )
			throw ExcBadVersion();
		if( m_rfb_minor_version < 0 )
			throw ExcBadVersion();

		// send our version
		// for now, we always override the server's version
		// and specify version 3.3.
		m_rfb_major_version = 3;
		m_rfb_minor_version = 3;

		sprintf( buf, "RFB 003.003\n" );
		m_net.SendBytes( (Uint8*)buf, 12 );
	}

	void RFBProto::DoAuthHandshake()
	{
		// receive server's desired authentication scheme
		NET_UINT32( scheme );
		switch( scheme )
		{
		case RFB_AUTH_FAILED:
			{
				string msg = "RFB handshake failed: ";
				NET_STRING( reason, VNC_STRING_LENGTH_LIMIT );
				throw Exc( msg + reason );
			}
		case RFB_AUTH_NONE:
			return;
		case RFB_AUTH_VNC:
			DoDESChallenge();
			return;
		default:
			throw ExcUnknownAuth();
		}
	}

	void RFBProto::DoDESChallenge()
	{
		// receive challenge
		Uint8 challenge[16];
		m_net.ReceiveBytes( challenge, 16 );

		// hash it with the password
		Uint8 response[16];
		GenerateDESResponse( challenge, response );

		// reply
		m_net.SendBytes( response, 16 );

		// see if that worked
		NET_UINT32( result );
		switch( result )
		{
		case RFB_AUTH_RESULT_OK:
			return;
		case RFB_AUTH_RESULT_FAILED:
			throw ExcAuthFailed();
		case RFB_AUTH_RESULT_TOOMANY:
			throw ExcAuthTooMany();
		default:
			throw ExcAuthFailed();
		}
	}

	void RFBProto::GenerateDESResponse( Uint8 const* challenge, Uint8* response )
	{
		Uint8 key[8];

		// generate null-padded key
		for( unsigned i = 0; i < 8; ++i )
		{
			if( i >= m_password.length() )
				key[i] = '\0';
			else
				key[i] = m_password[i];
		}

		// generate a DES key
		deskey( key, 0 );

		// encrypt the challenge
		des( (unsigned char*)challenge, (unsigned char*)response );
		des( (unsigned char*)challenge+8, (unsigned char*)response+8 );
	}

	void RFBProto::DoInitHandshake()
	{
		// client -> server init handshake
		Uint8 shared_flag = m_shared ? 1 : 0;
		m_net.SendBytes( &shared_flag, 1 );

		// server -> client init handshake
		NET_UINT16( fb_width );
		NET_UINT16( fb_height );

		NET_UINT8( bits_per_pixel );
		NET_UINT8( color_depth );
		NET_UINT8( big_endian_flag );
		NET_UINT8( true_color_flag );
		NET_UINT16( red_max );
		NET_UINT16( green_max );
		NET_UINT16( blue_max );
		NET_UINT8( red_shift );
		NET_UINT8( green_shift );
		NET_UINT8( blue_shift );

		char padding[3]; m_net.ReceiveBytes( (Uint8*)padding, 3 );

		NET_STRING( desktop_name, VNC_STRING_LENGTH_LIMIT );
		m_desktop_name = desktop_name;

		// set dimensions
		m_desktop_width = fb_width;
		m_desktop_height = fb_height;
		
		// set pixel format structure
		m_pixel_format.bytes = bits_per_pixel / 8;
		if( m_pixel_format.bytes < 1 ) throw ExcBadFormat();
		m_pixel_format.bits = color_depth;
		m_pixel_format.red_mask = red_max;
		m_pixel_format.green_mask = green_max;
		m_pixel_format.blue_mask = blue_max;
		m_pixel_format.red_shift = red_shift;
		m_pixel_format.green_shift = green_shift;
		m_pixel_format.blue_shift = blue_shift;
		m_pixel_format.big_endian = big_endian_flag ? true : false;
	}

	void RFBProto::SendPixelFormat( PixelFormat const& format )
	{
		m_net.BeginWritePacket();
		SEND_UINT8( RFB_CLIENT_SETPIXELFORMAT );
		SEND_UINT8( 0 );
		SEND_UINT16( 0 );
		SEND_UINT8( format.bytes * 8 );
		SEND_UINT8( format.bits );
		SEND_UINT8( format.big_endian ? 1 : 0 );
		SEND_UINT8( 1 );  // indexing not supported yet
		SEND_UINT16( format.red_mask );
		SEND_UINT16( format.green_mask );
		SEND_UINT16( format.blue_mask );
		SEND_UINT8( format.red_shift );
		SEND_UINT8( format.green_shift );
		SEND_UINT8( format.blue_shift );
		SEND_UINT8( 0 );
		SEND_UINT16( 0 );
		m_net.EndWritePacket();
	}

	void RFBProto::DoSupportedEncodings()
	{
		m_net.BeginWritePacket();
		SEND_UINT8( RFB_CLIENT_SETENCODINGS );
		SEND_UINT8( 0 );   // padding

 		SEND_UINT16( m_decoders_vec.size() );
		for( unsigned i = 0; i < m_decoders_vec.size(); ++i )
 		{
 			SEND_UINT32( m_decoders_vec[i]->GetType() );
 		}
		
		m_net.EndWritePacket();
	}
	
	void RFBProto::SetDisplay( Display* display )
	{		
		m_display = display;
		m_pixel_format = display->GetPixelFormat();
		SendPixelFormat( m_pixel_format );
	}
	
	void RFBProto::Update( Uint32 ms )
	{
		if( m_net.WaitDataReady( ms ) == false )
			return;
		NET_UINT8( type );
		switch( type )
		{
		case RFB_SERVER_FBUPDATE:
			{				
				NET_UINT8( padding );
				NET_UINT16( num_rects );
				for( unsigned i = 0; i < num_rects; ++i )
				{
					NET_UINT16( x );
					NET_UINT16( y );
					NET_UINT16( w );
					NET_UINT16( h );
					ScreenRect rect( x, y, w, h );
					NET_UINT32( type );					
					GetDecoder( type )( rect, *m_display );
				}
				//! \todo mechanism for repainting lost areas of the display
				SendUpdateRequest( ScreenRect( 0, 0, m_desktop_width, m_desktop_height ), true );
			}
			break;

		case RFB_SERVER_SETCOLORMAPENTRIES:
			{
				throw Exc( "server tried to set color map entries, but this is currently unsupported" );
			}
			break;
			
		case RFB_SERVER_BELL:
			{
				cerr << "Ding!" << endl;
				//! \todo produce console bell or similar effect
			}
			break;

		case RFB_SERVER_CUTTEXT:
			{
				NET_UINT16( padding1 );
				NET_UINT8( padding2 );
				NET_UINT32( length );
				char* buf = new char[ length + 1  ];
				m_net.ReceiveBytes( (Uint8*)buf, length );
				buf[length] = '\0';
				cerr << "New cut text: " << buf << endl;
				delete buf;
				//! \todo actually handle this
			}
			break;
			
		default:
			cerr << "got unknown message type " << (int)type << endl;
			throw ExcUnknownMessage();
		}
	}

	void RFBProto::SendKeyEventMessage( Uint32 key, bool down )
	{
		m_net.BeginWritePacket();
		SEND_UINT8( RFB_CLIENT_KEYEVENT );
		SEND_UINT8( (down ? 1 : 0) );
		SEND_UINT16( 0 );
		SEND_UINT32( key );
		m_net.EndWritePacket();
	}

	void RFBProto::SendMouseEventMessage( Uint16 x, Uint16 y, Uint8 buttons )
	{
		m_net.BeginWritePacket();
		SEND_UINT8( RFB_CLIENT_POINTEREVENT );
		SEND_UINT8( buttons );
		SEND_UINT16( x );
		SEND_UINT16( y );
		m_net.EndWritePacket();
	}

	void RFBProto::SendUpdateRequest( ScreenRect const& rect, bool incremental )
	{
		m_net.BeginWritePacket();
		SEND_UINT8( RFB_CLIENT_FBUPDATEREQUEST );
		SEND_UINT8( incremental ? 1 : 0 );
		SEND_UINT16( rect.x );
		SEND_UINT16( rect.y );
		SEND_UINT16( rect.w );
		SEND_UINT16( rect.h );
		m_net.EndWritePacket();
	}

	Decoder& RFBProto::GetDecoder( Uint32 type ) const
	{
		map< Uint32, Decoder* >::const_iterator it = m_decoders.find( type );
		if( it == m_decoders.end() )
		{
			cerr << "unable to find decoder for packet type " << type << endl;
			throw ExcMissingDecoder();
		}
		return *(it->second);
	}
	
};
