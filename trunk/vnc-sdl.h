/*!
  \file vnc-sdl.h
  \brief SDL implementation of VNC classes.
  \author John R. Hall
*/

#ifndef VNC_SDL_H
#define VNC_SDL_H

#include <string>

#include <SDL/SDL.h>
#include <SDL/SDL_net.h>
#include <SDL/SDL_mutex.h>

#include "vnc.h"

namespace VNC
{

	/*!
	  \brief SDL_net implementation of NetworkClient.
	*/
	class SDLNetworkClient : public NetworkClient
	{
	public:

		//! hostname lookup did not succeed
		CREATE_VNC_EXCEPTION( Resolve, "unable to resolve hostname" );

		//! TCP connection did not succeed
		CREATE_VNC_EXCEPTION( Connect, "unable to connect to host" );

		//! socket event wait did not succeed
		CREATE_VNC_EXCEPTION( Select,  "socket select failed" );
		
		//! Constructor.
		/*!
		  Establishes a connection with a server.
		  Attempts to connect to the given hostname and port.
		  Throws an exception on failure.
		  \param host Hostname or dotted IP address to connect to.
		  \param port Port number.
		*/
		SDLNetworkClient( std::string host, Uint16 port );

		//! Destructor.
		virtual ~SDLNetworkClient();

		// inherited from NetworkClient class
		virtual void BeginWritePacket();
		virtual void EndWritePacket();		
		virtual void SendBytes( Uint8 const* data, unsigned int count );
		virtual void ReceiveBytes( Uint8* data, unsigned int count );
		virtual bool WaitDataReady( Uint32 ms );
		
	private:

		//! Returns whenever data is ready to be read, or after 100 ms.
		/*!
		  \returns true if data is ready, false if the 100 ms timeout expired.
		*/
		bool WaitDataReady();
		
		TCPsocket m_socket;   //!< SDL_net TCP socket
		SDL_mutex* m_mutex;   //!< lock to keep from reading and writing at the same time
		
	};

	/*!
	  \brief SDL implementation of Display class.
	*/
	class SDLDisplay : public Display
	{
	public:

		//! SDL_Init() call failed
		CREATE_VNC_EXCEPTION( SDLInit, "SDL initialization failed" );

		//! SDL_SetVideoMode() call failed
		CREATE_VNC_EXCEPTION( SDLVideo, "SDL mode set failed" );

		//! Constructor.
		/*!
		  \param rfb RFB protocol object to associate with.
		*/
		SDLDisplay( RFBProto& rfb );

		//! Destructor.
		virtual ~SDLDisplay();

		// inherited from Display class
		virtual void BeginDrawing();
		virtual void EndDrawing( ScreenRect const& rect );
		virtual void WritePixels( int x, int y, int count, Uint8* data );
		virtual void WriteUniformPixels( int x, int y, int count, Uint32 pixel );
		virtual void CopyPixels( int sx, int sy, int dx, int dy, int w, int h );

	protected:

		//! Decides on the best compromise between the server's preference and our capabilities.
		/*!
		  Updates m_format to the final arbitrated pixel format.
		  Currently we try to set the server's requested bit depth.
		  We always accept the server's bit layout for 8bpp (since we just have to compose a palette
		  for this), but in 16bpp and 32bpp modes we use whatever bit layout is easiest for us.
		  The VNC protocol guarantees that this is acceptable.
		*/
		void ReconcilePixelFormat();

		//! Sets up the palette to handle incoming 8bpp pixel data in the current format.
		void Setup8bpp();

		//! Sets the current format to reflect our truecolor or hicolor bit layout.
		void Setup16or32bit();

		//! Checks for special key combinations.
		/*!
		  Reads the current keyboard state and acts on special key combinations.
		  \returns true if a key combo was recognized, false otherwise
		 */
		bool CheckKeyCombos();

		//! Processes one user interface event.
		/*!
		  This is a blocking function. It waits for keyboard or mouse activity,
		  checks for special key combos, and sends appropriate events to the
		  RFB protocol object.
		  \returns false if a QUIT event has been processed, true otherwise
		*/
		virtual bool UpdateInput();
		
		SDL_Surface* m_display;   //!< pointer to the main SDL display
		bool m_quit;              //!< quit flag
	};	


};

#endif
