/*!
  \file vnc-display-sdl.cc
  \brief SDL display and user interface implementation.
  \author John R. Hall
*/

#include <iostream>
#include "vnc-sdl.h"

using namespace std;

//! Translates an SDL keysym to an X11 keysym.
/*!
  This function is imperfect and should be improved over time.
  I don't know of any specific errors right now, but I'll add
  additional cases as I run into them.
  \param keysym SDL keysym (SDLK_xxxx) to translate
  \returns equivalent standard X11 keysym
*/
static Uint32 XlateSDLtoX11( int keysym )
{
	switch( keysym )
	{
	case SDLK_RETURN:     return 0xFF0D;
	case SDLK_BACKSPACE:  return 0xFF08;
	case SDLK_TAB:        return 0xFF09;
	case SDLK_NUMLOCK:    return 0xFF7F;
	case SDLK_CAPSLOCK:   return 0xFFE5;
	case SDLK_SCROLLOCK:  return 0xFF14;
	case SDLK_RSHIFT:     return 0xFFE2;
	case SDLK_LSHIFT:     return 0xFFE1;
	case SDLK_RCTRL:      return 0xFFE4;
	case SDLK_LCTRL:      return 0xFFE3;
	case SDLK_RALT:       return 0xFFEA;
	case SDLK_LALT:       return 0xFFE9;
	case SDLK_RMETA:      return 0xFFE8;
	case SDLK_LMETA:      return 0xFFE7;
	case SDLK_LSUPER:     return 0xFFEB;
	case SDLK_RSUPER:     return 0xFFEC;
	case SDLK_MODE:       return 0xFF7E;
	case SDLK_COMPOSE:    return 0xFF20;
	case SDLK_INSERT:     return 0xFF63;
	case SDLK_DELETE:     return 0xFFFF;
	case SDLK_HOME:       return 0xFF50;
	case SDLK_END:        return 0xFF57;
	case SDLK_PAGEUP:     return 0xFF55;
	case SDLK_PAGEDOWN:   return 0xFF56;
	case SDLK_UP:         return 0xFF52;
	case SDLK_DOWN:       return 0xFF54;
	case SDLK_LEFT:       return 0xFF51;
	case SDLK_RIGHT:      return 0xFF53;
	case SDLK_F1:         return 0xFFBE;
	case SDLK_F2:         return 0xFFBF;
	case SDLK_F3:         return 0xFFC0;
	case SDLK_F4:         return 0xFFC1;
	case SDLK_F5:         return 0xFFC2;
	case SDLK_F6:         return 0xFFC3;
	case SDLK_F7:         return 0xFFC4;
	case SDLK_F8:         return 0xFFC5;
	case SDLK_F9:         return 0xFFC6;
	case SDLK_F10:        return 0xFFC7;
	case SDLK_F11:        return 0xFFC8;
	case SDLK_F12:        return 0xFFC9;
	case SDLK_F13:        return 0xFFCA;
	case SDLK_F14:        return 0xFFCB;
	case SDLK_F15:        return 0xFFCC;
	default:
		{
			// most keys are just themselves
			// need to manually apply shift modifier, though
			SDLMod mod = SDL_GetModState();
			if( mod & KMOD_SHIFT )
			{
				// there's gotta be a better way to do this
				if( keysym >= 'a' && keysym <= 'z' )
				{
					keysym += 'A' - 'a';
				}
				else if( keysym >= '0' && keysym <= '9' )
				{
					char const numshift[] = {')','!','@','#','$','%','^','&','*','('};
					keysym = numshift[keysym - '0'];
				}
				else
				{
					// there's REALLY gotta be a better way to do this
					switch( keysym )
					{
					case '`': keysym = '~'; break;
					case ',': keysym = '<'; break;
					case '.': keysym = '>'; break;
					case '/': keysym = '?'; break;
					case ';': keysym = ':'; break;
					case '\'': keysym = '"'; break;
					case '[': keysym = '{'; break;
					case ']': keysym = '}'; break;
					case '\\': keysym = '|'; break;
					case '-': keysym = '_'; break;
					case '=': keysym = '+'; break;
					}
				}
			}
			
			return (Uint32)keysym;
		}
	}
}

namespace VNC
{

	SDLDisplay::SDLDisplay( RFBProto& rfb )
		: Display( rfb ),
		  m_display( NULL ),
		  m_quit( false )
	{
		if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
			throw ExcSDLInit();
		atexit( SDL_Quit );
		m_display = SDL_SetVideoMode( m_rfb.GetDesktopWidth(),
									  m_rfb.GetDesktopHeight(),
									  m_format.bits,
									  0 );
		if( m_display == NULL )
			throw ExcSDLVideo();

		ReconcilePixelFormat();
		
		SDL_UpdateRect( m_display, 0, 0, 0, 0 );
		SDL_WM_SetCaption( m_rfb.GetDesktopName().c_str(),
						   m_rfb.GetDesktopName().c_str() );
		SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY,
							 SDL_DEFAULT_REPEAT_INTERVAL );

		ScreenRect rect( 0, 0, m_rfb.GetDesktopWidth(), m_rfb.GetDesktopHeight() );
		rfb.SendUpdateRequest( rect, false );
	}

	SDLDisplay::~SDLDisplay()
	{
		SDL_Quit();
	}

	bool SDLDisplay::CheckKeyCombos()
	{
		SDLMod mod = SDL_GetModState();
		if( (mod & KMOD_LSHIFT) && (mod & KMOD_LCTRL) )
		{
			Uint8* keys = SDL_GetKeyState( NULL );
			
			// toggle fullscreen
			if( keys[SDLK_f] )
			{
				SDL_WM_ToggleFullScreen( m_display );
			}
			
			// exit
			if( keys[SDLK_ESCAPE] )
			{
				m_quit = true;
			}

			// toggle cursor
			if( keys[SDLK_c] )
			{
				SDL_ShowCursor( SDL_ShowCursor( SDL_QUERY ) == SDL_ENABLE ? SDL_DISABLE : SDL_ENABLE );
			}
			
			return true;
		}
		return false;
	}
	
	bool SDLDisplay::UpdateInput()
	{
		SDL_Event event;
		static Uint8 mouse_buttons = 0;

		if( SDL_WaitEvent( &event ) )
		{
			switch( event.type )
			{
			case SDL_MOUSEBUTTONUP:
				{
					/* swap buttons 2 and 3 (middle and right). */
					Uint8 button = event.button.button;
					if( button == 2 )
						button = 3;
					else if( button == 3 )
						button = 2;

					button--;
					mouse_buttons &= ~(1 << button);
					m_rfb.SendMouseEventMessage( event.button.x, event.button.y, mouse_buttons );
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
				{
					/* swap buttons 2 and 3 (middle and right). */
					Uint8 button = event.button.button;
					if( button == 2 )
						button = 3;
					else if( button == 3 )
						button = 2;

					button--;
					mouse_buttons |= (1 << button);
					m_rfb.SendMouseEventMessage( event.button.x, event.button.y, mouse_buttons );
				}
				break;

			case SDL_MOUSEMOTION:
				{
					m_rfb.SendMouseEventMessage( event.motion.x, event.motion.y, mouse_buttons );
				}
				break;

			case SDL_KEYUP:
			case SDL_KEYDOWN:
				{
					if( CheckKeyCombos() )
						break;
					bool down = event.key.state == SDL_PRESSED ? true : false;
					Uint32 keysym = XlateSDLtoX11( event.key.keysym.sym );
					m_rfb.SendKeyEventMessage( keysym, down );
				}
				break;
				
			case SDL_QUIT:
				m_quit = true;
				break;
			}
		}
		
		return m_quit ? false : true;
	}

	void SDLDisplay::ReconcilePixelFormat()
	{
		switch( m_display->format->BytesPerPixel )
		{
		case 1:   Setup8bpp();     break;
		case 2:   Setup16or32bit();  break;
		case 3:   Setup16or32bit();  break;
		case 4:   Setup16or32bit(); break;
		default:  throw Exc( "strange color depth returned from SDL" );
		}		
	}

	// returns the number of bits in a solid bitmask	
	static int MaskSize( Uint32 mask )
	{
		int i = 0;
		while( mask )
		{
			++i;
			mask >>= 1;
		}
		return i;
	}
	
	void SDLDisplay::Setup8bpp()
	{
		// default to a 332 colormap
		int rbits = 3;
		int gbits = 3;
		int bbits = 2;
		int rshift = 5;
		int gshift = 2;
		int bshift = 0;
		
		if( m_format.bytes == 1 )
		{
			// take the server's preferred values if possible
			rbits = MaskSize( m_format.red_mask );
			gbits = MaskSize( m_format.green_mask );
			bbits = MaskSize( m_format.blue_mask );
			rshift = m_format.red_shift;
			gshift = m_format.green_shift;
			bshift = m_format.blue_shift;
		}

		int rmask = (1 << rbits) - 1;
		int gmask = (1 << rbits) - 1;
		int bmask = (1 << rbits) - 1;
		
		SDL_Color colormap[256];
		for( Uint32 i = 0; i < 256; ++i )
		{
			colormap[i].r = ((i >> rshift) & rmask) << (8 - rbits);
			colormap[i].g = ((i >> gshift) & gmask) << (8 - gbits);
			colormap[i].b = ((i >> bshift) & bmask) << (8 - bbits);
		}
		SDL_SetColors( m_display, colormap, 0, 256 );

		m_format.bytes = 1;
		m_format.bits = 8;
		m_format.red_mask = rmask;
		m_format.green_mask = gmask;
		m_format.blue_mask = bmask;
		m_format.red_shift = rshift;
		m_format.green_shift = gshift;
		m_format.blue_shift = bshift;
		m_format.big_endian = 0;
	}

	void SDLDisplay::Setup16or32bit()
	{
		
		SDL_PixelFormat* fmt = m_display->format;
		m_format.red_mask = fmt->Rmask >> fmt->Rshift;
		m_format.green_mask = fmt->Gmask >> fmt->Gshift;
		m_format.blue_mask = fmt->Bmask >> fmt->Bshift;
		m_format.red_shift = fmt->Rshift;
		m_format.green_shift = fmt->Gshift;
		m_format.blue_shift = fmt->Bshift;
#ifdef VNC_BIG_ENDIAN
		m_format.big_endian = true;
#else
		m_format.big_endian = false;
#endif
	}
	
	void SDLDisplay::BeginDrawing()
	{
		// prepare the surface for drawing
		SDL_LockSurface( m_display );
	}
		
	void SDLDisplay::EndDrawing( ScreenRect const& rect )
	{
		// release the surface
		SDL_UnlockSurface( m_display );
		SDL_UpdateRect( m_display, rect.x, rect.y, rect.w, rect.h );
	}
	
	void SDLDisplay::WritePixels( int x, int y, int count, Uint8* data )
	{
		int bpp = m_display->format->BytesPerPixel;
		if (bpp == 3) 
		{
			Uint8* pixels = (Uint8*)m_display->pixels + m_display->pitch * y + x * bpp;
			while (count >= 0) {		
				*pixels++ = *data++;
				*pixels++ = *data++;
				*pixels++ = *data++;
				*data++;
			    count--;
			}
		}
		else
		{
			Uint8* pixels = (Uint8*)m_display->pixels;
			memcpy( pixels + m_display->pitch * y + x * bpp, data, count * bpp );
		}
	}

	
	void SDLDisplay::WriteUniformPixels( int x, int y, int count, Uint32 pixel )
	{
		int bpp = m_display->format->BytesPerPixel;
		switch( bpp )
		{
		case 1:
			{
				Uint8 val = (Uint8)pixel;
				Uint8* pixels = (Uint8*)m_display->pixels;
				memset( pixels + m_display->pitch * y + x * bpp, val, count );				
			}
			break;
			
		case 2:
			{
				Uint16 val = (Uint16)pixel;
				Uint16* pixels = (Uint16*)((Uint8*)m_display->pixels + m_display->pitch * y) + x;
				Uint16* end = pixels + count;
				while( pixels < end )
				{
					*pixels = val;
					++pixels;
				}
			}
			break;
		case 3:
			{
				Uint8* pixels = (Uint8*)((Uint8*)m_display->pixels + m_display->pitch * y) + x * bpp;
				while( count > 0 )
				{
					*pixels++ = pixel & 0xff;
		            *pixels++ = (pixel >> 8) & 0xff;
		            *pixels++ = (pixel >> 16) & 0xff;
		            count--;
				}
			}
			break;
			
		case 4:
			{
				Uint32* pixels = (Uint32*)((Uint8*)m_display->pixels + m_display->pitch * y) + x;
				Uint32* end = pixels + count;
				while( pixels < end )
				{
					*pixels = pixel;
					++pixels;
				}				
			}
			break;
			
		default:
			throw Exc( "invalid color depth for WriteUniformPixels" );
		}
	}
	
	void SDLDisplay::CopyPixels( int sx, int sy, int dx, int dy, int w, int h )
	{
		int bpp = m_display->format->BytesPerPixel;
		Uint8* pixels = (Uint8*)m_display->pixels;
		if( sy > dy )
		{
			for( int y = 0; y < h; ++y )
			{
				memcpy( pixels + m_display->pitch * (dy + y) + (dx * bpp),
						pixels + m_display->pitch * (sy + y) + (sx * bpp),
						w * bpp );
			}
		}
		else
		{
			for( int y = h-1; y >= 0; --y )
			{
				memcpy( pixels + m_display->pitch * (dy + y) + (dx * bpp),
						 pixels + m_display->pitch * (sy + y) + (sx * bpp),
						 w * bpp );
			}
		}
	}
	
};
