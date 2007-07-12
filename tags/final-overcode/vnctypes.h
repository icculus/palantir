/*!
  \file vnctypes.h
  \brief Basic types used by the VNC implementation.
  \author John R. Hall
*/

#ifndef VNCTYPES_H
#define VNCTYPES_H

#include <string>

namespace VNC
{
	typedef signed char Int8;      //!< Signed 8-bit integer
	typedef unsigned char Uint8;   //!< Unsigned 8-bit integer
	typedef signed short Int16;    //!< Signed 16-bit integer
	typedef unsigned short Uint16; //!< Unsigned 16-bit integer
	typedef signed int Int32;	   //!< Signed 32-bit integer
	typedef unsigned int Uint32;   //!< Unsigned 32-bit integer

	/*!
	  \brief Pixel format class. Defines RGB pixel formats.
	*/
	struct PixelFormat
	{
		unsigned int bytes;         //!< bytes per pixel (1, 2, or 4 currently)
		unsigned int bits;          //!< bits actually used (usually 8, 15, 16, 24, 32)
		unsigned int red_mask;      //!< unshifted bitmask of red bits (2^n - 1)
		unsigned int green_mask;    //!< unshifted bitmask of blue bits (2^n - 1)
		unsigned int blue_mask;     //!< unshifted bitmask green bits (2^n - 1)
		unsigned int red_shift;     //!< offset of red bits in pixel
		unsigned int green_shift;   //!< offset of green bits in pixel
		unsigned int blue_shift;    //!< offset of blue bits in pixel
		bool big_endian;            //!< use the one true byte order?
	};

	// byte swapping macros
#if defined(VNC_LITTLE_ENDIAN)

	/*! swap 16-bit big endian to native byte order */
# define VNC_SWAP_BE_16(u16) (((u16 & 0xFF00) >> 8) + ((u16 & 0xFF) << 8))

	/*! swap 16-bit little endian to native byte order */	
# define VNC_SWAP_LE_16(u16) (u16)

	/*! swap 32-bit big endian to native byte order */
# define VNC_SWAP_BE_32(u32) (((u32 & 0xFF000000) >> 24) + ((u32 & 0x00FF0000) >> 8) + ((u32 & 0x0000FF00) << 8) + ((u32 & 0x000000FF) << 24))
	
	/*! swap 32-bit little endian to native byte order */
# define VNC_SWAP_LE_32(u32) (u32)

#else  //--------------------------------------------------------

	/*! swap 16-bit little endian to native byte order */
# define VNC_SWAP_LE_16(u16) (((u16 & 0xFF00) >> 8) + ((u16 & 0xFF) << 8))

	/*! swap 16-bit big endian to native byte order */
# define VNC_SWAP_BE_16(u16) (u16)

	/*! swap 32-bit little endian to native byte order */
# define VNC_SWAP_LE_32(u32) (((u32 & 0xFF000000) >> 24) + ((u32 & 0x00FF0000) >> 8) + ((u32 & 0x0000FF00) << 8) + ((u32 & 0x000000FF) << 24))

	/*! swap 32-bit big endian to native byte order */
# define VNC_SWAP_BE_32(u32) (u32)

#endif
    	
		
	/*!
	  \brief Screen rectangle class. Defines screen regions.
	*/
	struct ScreenRect
	{
		ScreenRect() {}
		ScreenRect( Uint16 _x, Uint16 _y, Uint16 _w, Uint16 _h )
			: x( _x ), y( _y ), w( _w ), h( _h ) {}
		Uint16 x, y;          //!< upper left corner of region
		Uint16 w, h;          //!< width and height of region
	};
	
	/*!
	  \brief Simple exception reporting class.
	  Exc objects are used by all VNC implementation classes to
	  report unrecoverable errors. They are never used for non-error
	  conditions.
	*/
	class Exc
	{
	public:
		/*! Creates an exception with the given message. */
		Exc( char const* msg )
			: m_msg( msg ) {};

		/*! Creates an exception with the given message. */
		Exc( std::string msg )
			: m_msg( msg ) {};		

		/*! Retrieves a flat string for this exception. */
		operator char const*() const { return m_msg.c_str(); }
		
	private:
		std::string m_msg;  //!< exception message
	};

	//! Creates an exception reporting class.
#define CREATE_VNC_EXCEPTION( name, msg ) \
	class Exc##name : public Exc { public: Exc##name() : Exc( msg ) {} };
	
};

#endif
