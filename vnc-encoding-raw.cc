/*!
  \file vnc-encoding-raw.cc
  \brief Implementation of the Raw update encoding type.
  \author John R. Hall
*/

#include <iostream>
#include "vnc.h"

using namespace std;

namespace VNC
{

	DEFINE_VNC_DECODER( RAW )
	{
		++m_processed;

		unsigned int count = rect.w*rect.h*disp.GetPixelFormat().bytes;
		Uint8* buf = new Uint8[count];
		m_net.ReceiveBytes( buf, count );
		disp.BeginDrawing();
		for( unsigned y = 0; y < rect.h; ++y )
		{
			disp.WritePixels( rect.x, rect.y + y, rect.w, buf + disp.GetPixelFormat().bytes * rect.w * y );
		}
		disp.EndDrawing( rect );
		delete buf;
	}

};
