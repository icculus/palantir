/*!
  \file vnc-encoding-hextile.cc
  \brief Implementation of the hextile encoding for VNC.
  \author John R. Hall
*/

#include <SDL/SDL.h>
#include <iostream>
#include "vnc.h"

using namespace std;

#define GET_UINT32( var ) m_net.ReceiveBytes( (Uint8*)&var, 4 ); var = VNC_SWAP_BE_32( var );
#define NET_UINT32( var ) Uint32 var; GET_UINT32( var );
#define GET_UINT16( var ) m_net.ReceiveBytes( (Uint8*)&var, 2 ); var = VNC_SWAP_BE_16( var );
#define NET_UINT16( var ) Uint16 var; GET_UINT16( var );
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
				NET_UINT16( val );
				return (Uint32)val;
			}
			
		case 4:
			{
				NET_UINT32( val );
				return (Uint32)val;
			}
			
		default:
			throw Exc( "invalid color depth for RRE decoder" );
		}
	}
	
	DEFINE_VNC_DECODER( HEXTILE )
	{
		++m_processed;
		
		Uint32 tile_bg_color = 0;    // these are running values that can
		Uint32 subtile_fg_color = 0; //   be shared across tiles
		int bpp = disp.GetPixelFormat().bytes;
		Uint8* raw_pixel_buf = new Uint8[ bpp * 16 * 16 ];  // buffer for raw 

		disp.BeginDrawing();

		// iterate through each 16x16 tile in this rect
		for( int tile_y = 0; tile_y < rect.h; tile_y += 16 )
		{
			int tile_height = (rect.h - tile_y) < 16 ? (rect.h - tile_y) : 16;
			for( int tile_x = 0; tile_x < rect.w; tile_x += 16 )
			{
				int tile_width = (rect.w - tile_x) < 16 ? (rect.w - tile_x) : 16;
				int num_subrects = 0;
				bool subrects_colored;
				
				NET_UINT8( encoding );
				if( encoding & RFB_HEXTILE_RAW )
				{
					// the other bits don't matter; process a raw tile
					m_net.ReceiveBytes( raw_pixel_buf, tile_width * tile_height * bpp );
					for( int y = 0; y < tile_height; ++y )
					{
						disp.WritePixels( rect.x + tile_x, rect.y + tile_y + y, tile_width, raw_pixel_buf + bpp * tile_width * y );
					}
				}
				else
				{
					// process a complex tile
					
					if( encoding & RFB_HEXTILE_BG_SPECIFIED )
					{
						// new background color for the entire tile
						tile_bg_color = NetPixel( m_net, bpp );
					}

					if( encoding & RFB_HEXTILE_FG_SPECIFIED )
					{
						// new foreground color for all subrects in this tile
						subtile_fg_color = NetPixel( m_net, bpp );
					}

					if( encoding & RFB_HEXTILE_ANY_SUBRECTS )
					{
						// this tile contains subrectangels
						NET_UINT8( tmp );
						num_subrects = tmp;
					}
					else
					{
						// this tile contains no subrects, just the solid background
						num_subrects = 0;
					}

					if( encoding & RFB_HEXTILE_SUBRECTS_COLORED )
						// each subrect has its own foreground color
						subrects_colored = true;
					else
						// all subrects share subtile_fg_color
						subrects_colored = false;

					// fill the background
					ScreenRect tile_rect( tile_x + rect.x, tile_y + rect.y, tile_width, tile_height );
					FillSolidRect( disp, tile_rect, tile_bg_color );

					// draw subrects
					for( int subrect = 0; subrect < num_subrects; ++subrect )
					{
						Uint32 subrect_pixel;

						// if subtiles have their own FG colors, read a color
						if( subrects_colored )
							subrect_pixel = NetPixel( m_net, bpp );
						else
							subrect_pixel = subtile_fg_color;

						// read the dimensions of this tile
						NET_UINT8( packed_xy );
						NET_UINT8( packed_wh );
						ScreenRect subtile_rect( tile_rect.x + ((packed_xy >> 4) & 0x0F), tile_rect.y + (packed_xy & 0x0F),
												 1 + ((packed_wh >> 4) & 0x0F), 1 + (packed_wh & 0x0F) );

						// draw it
						FillSolidRect( disp, subtile_rect, subrect_pixel );
					}
				}
			}
		}

		disp.EndDrawing( rect );
		
		delete raw_pixel_buf;
	}
	
};
