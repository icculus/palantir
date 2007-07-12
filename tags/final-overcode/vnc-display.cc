/*!
  \file vnc-display.cc
  \brief Implementation of the Display base class.
  \author John R. Hall
*/

#include "vnc.h"

namespace VNC
{

	Display::Display( RFBProto& rfb )
		: m_rfb( rfb ),
		  m_format( rfb.GetPixelFormat() )
	{
	}

	Display::~Display()
	{
	}

	bool Display::Update()
	{
		return this->UpdateInput();
	}
	
};
