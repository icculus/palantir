/*!
  \file vnc-encoding-copyrect.cc
  \brief Implementation of the CopyRect update encoding type.
  \author John R. Hall
*/

#include <iostream>
#include "vnc.h"

using namespace std;

namespace VNC
{

	DEFINE_VNC_DECODER( COPYRECT )
	{
		++m_processed;
		
		Uint16 src_x, src_y;
		m_net.ReceiveBytes( (Uint8*)&src_x, 2 ); src_x = VNC_SWAP_BE_16( src_x );
		m_net.ReceiveBytes( (Uint8*)&src_y, 2 ); src_y = VNC_SWAP_BE_16( src_y );
		disp.BeginDrawing();
		disp.CopyPixels( src_x, src_y, rect.x, rect.y, rect.w, rect.h );
		disp.EndDrawing( rect );
	}

};
