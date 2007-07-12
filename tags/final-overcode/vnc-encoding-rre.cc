/*!
  \file vnc-encoding-rre.cc
  \brief Implementation of the rise and run length encoding for VNC.
  \author John R. Hall
*/

#include <SDL/SDL.h>
#include <iostream>
#include "vnc.h"

using namespace std;


#define GET_UINT32( var ) m_net.ReceiveBytes( (Uint8*)&var, 4 ); var = VNC_SWAP_BE_32( var );
#define GET_UINT32_NOSWAP( var ) m_net.ReceiveBytes( (Uint8*)&var, 4 );

#define NET_UINT32( var ) Uint32 var; GET_UINT32( var );
#define NET_UINT32_NOSWAP( var ) Uint32 var; GET_UINT32_NOSWAP( var );

#define GET_UINT16( var ) m_net.ReceiveBytes( (Uint8*)&var, 2 ); var = VNC_SWAP_BE_16( var );
#define GET_UINT16_NOSWAP( var ) m_net.ReceiveBytes( (Uint8*)&var, 2 );

#define NET_UINT16( var ) Uint16 var; GET_UINT16( var );
#define NET_UINT16_NOSWAP( var ) Uint16 var; GET_UINT16_NOSWAP( var );

#define NET_UINT8( var ) Uint8 var; m_net.ReceiveBytes( &var, 1 );

namespace VNC
{

	static void FillSolidRect( Display& disp, ScreenRect const& rect, Uint32 pixel )
	{
		for( unsigned y = 0; y < rect.h; ++y )
		{
			disp.WriteUniformPixels( rect.x, rect.y+y, rect.w, pixel );
		}
	}

	static Uint32 NetPixel( NetworkClient& net, int bpp )
	{
		NetworkClient& m_net = net;
		switch( bpp )
		{
		case 1:
			{
				NET_UINT8( val );
				return (Uint32)val;
			}
			
		case 2:
			{
				NET_UINT16_NOSWAP( val );
				return (Uint32)val;
			}
			
		case 4:
			{
				NET_UINT32_NOSWAP( val );
				return (Uint32)val;
			}
			
		default:
			throw Exc( "invalid color depth for RRE decoder" );
		}
	}
	
	DEFINE_VNC_DECODER( RRE )
	{
		++m_processed;

		int bpp = disp.GetPixelFormat().bytes;
		NET_UINT32( num_subrects );
		Uint32 bg_pixel = NetPixel( m_net, bpp ); 
		
		disp.BeginDrawing();
		FillSolidRect( disp, rect, bg_pixel );
		for( unsigned i = 0; i < num_subrects; ++i )
		{
			Uint32 pixel = NetPixel( m_net, bpp );
			NET_UINT16( x );
			NET_UINT16( y );
			NET_UINT16( w );
			NET_UINT16( h );
			ScreenRect subrect( rect.x + x, rect.y + y, w, h );
			FillSolidRect( disp, subrect, pixel );
		}
		disp.EndDrawing( rect );
	}

	DEFINE_VNC_DECODER( CORRE )
	{
		++m_processed;
		
		int bpp = disp.GetPixelFormat().bytes;
		NET_UINT32( num_subrects );
		Uint32 bg_pixel = NetPixel( m_net, bpp ); 
		
		disp.BeginDrawing();
		FillSolidRect( disp, rect, bg_pixel );
		for( unsigned i = 0; i < num_subrects; ++i )
		{
			Uint32 pixel = NetPixel( m_net, bpp );
			NET_UINT8( x );
			NET_UINT8( y );
			NET_UINT8( w );
			NET_UINT8( h );
			ScreenRect subrect( rect.x + x, rect.y + y, w, h );
			FillSolidRect( disp, subrect, pixel );
		}
		disp.EndDrawing( rect );
	}
	
};
