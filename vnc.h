/*!
  \file vnc.h
  \brief Main definitions for the VNC system.
  \author John R. Hall
*/

#ifndef VNC_H
#define VNC_H

#include <string>
#include <vector>
#include <map>
#include "vnctypes.h"
#include "zlib-reader.h"

//-------------------------------------------------------------------------------------

#define VNC_DEFAULT_PORT       5901     //!< default TCP port for VNC displays

#define VNC_STRING_LENGTH_LIMIT  1000   //!< arbitrary sanity

#define RFB_AUTH_FAILED     0     //!< incompatible server version
#define RFB_AUTH_NONE       1     //!< no authentication required
#define RFB_AUTH_VNC        2     //!< DES hash authentication

#define RFB_AUTH_RESULT_OK      0 //!< authentication succeeded
#define RFB_AUTH_RESULT_FAILED  1 //!< authentication not accepted
#define RFB_AUTH_RESULT_TOOMANY 2 //!< ooh, not good

#define RFB_ENCODING_RAW      0   //!< raw pixel data encoding (mandatory support)
#define RFB_ENCODING_COPYRECT 1   //!< copy rectangle within framebuffer encoding
#define RFB_ENCODING_RRE      2   //!< rise and run length encoding
#define RFB_ENCODING_CORRE    4   //!< compact rise and run length encoding
#define RFB_ENCODING_HEXTILE  5   //!< 16x16 tile encoding
#define RFB_ENCODING_ZRLE     16  //!< zipped RLE encoding

#define RFB_ENCODING_NAME_RAW       "Raw"
#define RFB_ENCODING_NAME_COPYRECT  "CopyRect"
#define RFB_ENCODING_NAME_RRE       "RRE"
#define RFB_ENCODING_NAME_CORRE     "CoRRE"
#define RFB_ENCODING_NAME_HEXTILE   "Hextile"
#define RFB_ENCODING_NAME_ZRLE      "ZRLE"

#define RFB_ENCODING_DESC_RAW       "raw pixel data without compression"
#define RFB_ENCODING_DESC_COPYRECT  "fast copy within framebuffer"
#define RFB_ENCODING_DESC_RRE       "rise and run length encoded pixel data (RRE)"
#define RFB_ENCODING_DESC_CORRE     "compact rise and run length encoded pixel data (CoRRE)"
#define RFB_ENCODING_DESC_HEXTILE   "16x16 tile encoded pixel data (hextile)"
#define RFB_ENCODING_DESC_ZRLE      "zlib-compressed RLE pixel data (ZRLE)"

#define RFB_HEXTILE_RAW                    1     //!< tile sent as raw pixels; other bits irrelevant
#define RFB_HEXTILE_BG_SPECIFIED           2     //!< background color for this tile follows
#define RFB_HEXTILE_FG_SPECIFIED           4     //!< foreground color for all subrects follows
#define RFB_HEXTILE_ANY_SUBRECTS           8     //!< byte with # of subrects follows
#define RFB_HEXTILE_SUBRECTS_COLORED       16    //!< each subrect will be preceded by its own foreground color

//-------------------------------------------------------------------------------------

/*!
	\brief Namespace for VNC-related classes and functions.	
*/
namespace VNC
{

	class Client;
	class NetworkClient;

	/*!
	  \brief Simple network client for use by VNC clients.
	  Provides the ability to synchronously read and write
	  blocks of data, and nothing else.
	  Connects and disconnects in the ctor and dtor, respectively.
	*/
	class NetworkClient
	{
	public:

        // -------------------------------------------------------------
		// Exceptions
		
		CREATE_VNC_EXCEPTION( Read, "unable to read data" );
		CREATE_VNC_EXCEPTION( Write, "unable to write data" );

        // -------------------------------------------------------------
		// Construction and destruction
		
		//! Constructor.
		/*!
		  Should set up a connection, and throw an exception on failure.
		  Probably will need to take a hostname or similar as a parameter.
		*/
		NetworkClient() {};

		//! Destructor.
		/*!
		  Should bring down the connection.
		*/
		virtual ~NetworkClient() {};

		// -------------------------------------------------------------
		// Locking, to avoid having packets stepped on

		//! Obtains a data sending lock, so another thread can't step on an outgoing packet.
		/*!
		  Normal mutex lock behaviour. This function is necessary because the network thread
		  and the display thread both need to be able to send packets.		  
		  \sa SendBytes
		*/
		virtual void BeginWritePacket() = 0;

		//! Releases the data sending lock.
		/*!
		  Normal mutex unlock behaviour.
		  \sa SendBytes
		*/
		virtual void EndWritePacket() = 0;

		// may need to add read locks later
		
        // -------------------------------------------------------------
		// Other methods
		
		//! Subclass-provided method to send data to the server.
		/*!
		  This is a very simple synchronous function for sending data
		  to the server. Errors are considered fatal.
		  \param data block of data to send
		  \param count number of bytes to send
		  \sa ReceiveBytes
		 */
		virtual void SendBytes( Uint8 const* data, unsigned int count ) = 0;


		//! Subclass-provided method to receive data from the server.
		/*!
		  This is a very simple synchronous function for receiving data
		  from the server. Errors are considered fatal.
		  \param data buffer to read data into; must be at least \a count bytes
		  \param count number of bytes to read
		  \sa SendBytes
		 */
		virtual void ReceiveBytes( Uint8* data, unsigned int count ) = 0;

		//! Monitors the network for data.
		/*!
		  Returns when at least one byte can be read immediately, or after the
		  specified number of milliseconds.
		  \param ms timeout in milliseconds.
		  \returns true if data is available, false if the timeout expired.
		*/
		virtual bool WaitDataReady( Uint32 ms ) = 0;
	};

	//-------------------------------------------------------------------------------------
	
	class Display;
	class Decoder;
	
	/*!
 	    \brief Implementation of the Remote Framebuffer (RFB) protocol.
	*/	
	class RFBProto
	{
		
	public:
		/*! incorrect initial handshake, probably not RFB at all */
		CREATE_VNC_EXCEPTION( NotRFB, "this doesn't appear to be an RFB server" );

		/*! unacceptable major protocol version */
		CREATE_VNC_EXCEPTION( BadVersion, "incompatible RFB protocol version" );

		/*! a nonstandard authentication method that we don't yet grok */
		CREATE_VNC_EXCEPTION( UnknownAuth, "unknown authentication type requested" );

		/*! whatever authentication scheme we used did not succeed */
		CREATE_VNC_EXCEPTION( AuthFailed, "authentication failed" );

		/*! we've run out of chances to successfully authenticate */
		CREATE_VNC_EXCEPTION( AuthTooMany, "authentication failed too many times" );

		/*! some pixel format we can't deal with (rare) */
		CREATE_VNC_EXCEPTION( BadFormat, "bizarre pixel format" );

		/*! a display update packet we can't deal with (shouldn't happen, by protocol design) */
		CREATE_VNC_EXCEPTION( MissingDecoder, "no decoder for this packet type" );

		/*! a message type code we don't understand */
		CREATE_VNC_EXCEPTION( UnknownMessage, "unknown message type received" );
		
        // -------------------------------------------------------------
		// Construction and destruction

		//! Constructor.
		/*!
		  \param net network link to use
		  \param password VNC connection password; only used if needed
		  \param shared true if other clients should be allowed to share the VNC server
		  \param decoders supported update encodings
		*/
		RFBProto( NetworkClient& net, std::string const& password, bool shared, std::vector< Decoder* > decoders );

		//! Destructor.
		virtual ~RFBProto();

		// -------------------------------------------------------------
		// Updating

		//! Checks for new network traffic. Dispatches notifications to the display.
		/*!
		  Returns if the first byte of a new packet has not arrived after the given
		  number of milliseconds.
		  \param ms data timeout in milliseconds
		 */
		void Update( Uint32 ms );
		
		// -------------------------------------------------------------
		// Accessors and mutators

		//! Returns the currently active pixel format.
		/*!
		  \returns pixel format
		*/
		PixelFormat const& GetPixelFormat() const { return m_pixel_format; }
		
		//! Sets the display to update.
		/*!
		  Also sets the active pixel format.
		  \param display Display object to send updates to.
		*/
		void SetDisplay( Display* display );		
		
		// -------------------------------------------------------------
		// Client -> Server messages		
		
		//! Sends a key press or release message.
		/*!
		  \param key keycode to report; key constants are those defined by X11
		  \param down press or release flag
		*/
		void SendKeyEventMessage( Uint32 key, bool down );

		//! Sends a mouse status update.
		/*!
		  \param x x coordinate of the mouse
		  \param y y coordinate of the mouse
		  \param buttons bitmask of mouse buttons
		*/
		void SendMouseEventMessage( Uint16 x, Uint16 y, Uint8 buttons );		

		//! Requests an update of the given screen region.
		/*!
		  \param rect screen rectangle of interest
		  \param incremental false if a full refresh is needed, true otherwise
		*/
		void SendUpdateRequest( ScreenRect const& rect, bool incremental );

		//! Establishes a new pixel format for this session.
		/*!
		  \param format pixel format to set
		*/
		void SendPixelFormat( PixelFormat const& format );
		
		// -------------------------------------------------------------
		// State query methods
		
		//! Returns the server's major version number.
		int GetMajorVersion() const { return m_rfb_major_version; }

		//! Returns the server's minor version number.		
		int GetMinorVersion() const { return m_rfb_minor_version; }

		//! Returns the current desktop format.
		/*!
		  \param fmt PixelFormat structure to fill
		*/
		void GetPixelFormat( PixelFormat& fmt ) const { fmt = m_pixel_format; }

		//! Returns the desktop's name.
		std::string GetDesktopName() const { return m_desktop_name; }

		//! Returns the desktop's width.
		int GetDesktopWidth() const { return m_desktop_width; }

		//! Returns the desktop's height.
		int GetDesktopHeight() const { return m_desktop_height; }
		
		// -------------------------------------------------------------
		// Private variables
		
	private:

		//! Performs the opening version handshake.
		/*!
		  Throws an exception if the server's RFB version is not acceptable.
		*/
		void DoVersionHandshake();

		//! Performs the authentication handshake.
		/*!
		  Performs encrypted password authentication if necessary.
		*/
		void DoAuthHandshake();

		//! Responds to the server's DES authentication challenge.
		void DoDESChallenge();

		//! Generates a response by hashing the challenge with m_password.
		/*!
		  \param challenge 16-byte challenge from VNC server
		  \param response 16-byte buffer to receive hashed response
		  \sa m_password
		 */		  
		void GenerateDESResponse( Uint8 const* challenge, Uint8* response );

		//! Performs the post-authentication setup handshake.
		void DoInitHandshake();

		//! Advises the server of which encoding types we support.
		void DoSupportedEncodings();
		
		//! Retrieves a decoder for a packet type.
		/*!
		  Throws an exception if the decoder is missing.
		  \param type video update packet type
		  \returns reference to the requested decoder
		*/
		Decoder& GetDecoder( Uint32 type ) const;
		
		bool m_shared;              //!< allow other clients to share desktop
		NetworkClient& m_net;       //!< network connection
		std::string m_password;     //!< plaintext connection password

		int m_rfb_major_version;    //!< major version number (probably 3)
		int m_rfb_minor_version;    //!< minor version number (likely 3)

		PixelFormat m_pixel_format; //!< current desktop format
		int m_desktop_width;        //!< desktop width in pixels
		int m_desktop_height;       //!< desktop height in pixels
		std::string m_desktop_name; //!< desktop name

		Display* m_display;           //!< display to update
		std::map< Uint32, Decoder* > m_decoders;  //! packet type -> decoder
		std::vector< Decoder* > m_decoders_vec;   //! decoders in order of preference
	};

	//-------------------------------------------------------------------------------------

	/*!
	  \brief Generic VNC display.
	*/
	class Display
	{
	public:

        // -------------------------------------------------------------
		// Construction and destruction

		//! Constructor.
		/*!
		   Associates with an RFB protocol object.
		   \param rfb RFB protocol object to work with.
		*/
		Display( RFBProto& rfb );

		//! Destructor.
		virtual ~Display();

        // -------------------------------------------------------------
		// Other methods

		//! Prepares for a series of framebuffer updates.
		/*!
		  Sets up the display to accept a series of drawing commands.
		  It is an error to draw without calling this first.
		*/
		virtual void BeginDrawing() = 0;

		//! Ends a series of framebuffer updates.
		/*!
		  Updates the contents of the screen to reflect the recent drawing.
		  \param rect boundary rectangle of all drawing we performed
		*/		
		virtual void EndDrawing( ScreenRect const& rect ) = 0;
		
		//! Our basic drawing primitive.
		/*!
		  Writes a block of pixel data to the framebuffer at the given coordinate.
		  \param x x coordinate for pixel data
		  \param y y coordinate for pixel data
		  \param count number of pixels to write
		  \param data source pixel data
		*/
		virtual void WritePixels( int x, int y, int count, Uint8* data ) = 0;

		//! Another basic drawing primitive.
		/*!
		  Writes a block of identical pixels at the given coordinate.
		  \param x x coordinate for pixel data
		  \param y y coordinate for pixel data
		  \param count number of pixels to write
		  \param pixel pixel value to write (probably not 32-bit)
		*/
		virtual void WriteUniformPixels( int x, int y, int count, Uint32 pixel ) = 0;

		//! Another basic drawing primitive.
		/*!
		  Copies a block of pixels within the framebuffer. It is a reasonable
		  assumption that this will be a relatively fast operation.
		  \param sx source x coordinate
		  \param sy source y coordinate
		  \param dx destination x coordinate
		  \param dy destination y coordinate
		  \param w width of pixel block
		  \param h height of pixel block
		*/
		virtual void CopyPixels( int sx, int sy, int dx, int dy, int w, int h ) = 0;

		// note to hackers:
		// please avoid adding more drawing primitives if it can be avoided
		// I would like this interface to remain thin
		
		//! Processes events and updates the RFB object.
		/*!
		  This is the main update function. It is called in a tight loop by the app.
		  \returns false if the display has been terminated, true otherwise
		*/
		bool Update();

		//! Returns the pixel format expected by the drawing methods.
		/*!
		  \returns pixel format
		*/
		PixelFormat const& GetPixelFormat() const { return m_format; }
		
        // -------------------------------------------------------------
		// Private variables
		
	protected:

		//! Implementation-provided method to collect input and send RFB updates.
		/*!
		  This function must not block.
		  \returns false if the user has asked to close the display, true otherwise
		*/
		virtual bool UpdateInput() = 0;
		
		RFBProto& m_rfb;       //!< RFB protocol object to use for events
		PixelFormat m_format;  //!< pixel format accepted by this display
	};

	//-------------------------------------------------------------------------------------
	
	//! Functor for handling video update packets.
	class Decoder
	{
	public:

        // -------------------------------------------------------------
		// Construction and destruction
		
		//! Constructor.
		/*!
		  \param net network connection to read data from when invoked
		*/
		Decoder( NetworkClient& net ) : m_net( net ), m_processed( 0 ) {};

		//! Destructor.
		virtual ~Decoder() {};
        // -------------------------------------------------------------
		// Other methods
		
		//! Decodes an update packet from the network and applies it to the given display.
		/*!
		  \param rect affected rectangle
		  \param disp display to update
		*/
		virtual void operator() ( ScreenRect const& rect, Display& disp ) = 0;

		//! Retrieves the RFB type of this decoder.
		/*!
		  \returns RFB type ID
		*/
		virtual Uint32 GetType() = 0;
		
		//! Retrieves a human readable description of this decoder.
		/*!
		  \returns description string
		*/
		virtual char const* GetDesc() = 0;

		//! Retrieves a short name of this decoder.
		/*!
		  \returns short name string
		*/
		virtual char const* GetName() = 0;
		
		//! Retrieves the number of packets processed by this encoding.
		/*!
		  It is up to the individual encoding to increment this.
		  \returns number of packets processed by this encoding
		*/
		unsigned GetNumProcessed() const { return m_processed; }
		
		// -------------------------------------------------------------
		// Private variables
		
	protected:
		NetworkClient& m_net;   //!< network client to read data from
		unsigned m_processed;   //!< number of packets processed by this encoding
	};


	//! creates a decoder functor name for a particular packet type
#define VNC_DECODER( type ) Decoder##type

	//! declares the interface for a decoder
#define VNC_DECODER_INTERFACE( type )		   							\
	public:																\
	VNC_DECODER( type )( NetworkClient& net ) : Decoder( net ) {}		\
	virtual void operator() ( ScreenRect const& rect, Display& disp );	\
	virtual Uint32 GetType() { return RFB_ENCODING_##type; }			\
	virtual char const* GetName() { return RFB_ENCODING_NAME_##type; }  \
	virtual char const* GetDesc() { return RFB_ENCODING_DESC_##type; }  \
    private:		  													\

	//! begins the definition of a decoder 
#define DEFINE_VNC_DECODER( type ) \
	void VNC_DECODER( type )::operator() ( ScreenRect const& rect, Display& disp )

	class VNC_DECODER( RAW ) : public Decoder
	{
		VNC_DECODER_INTERFACE( RAW );
	};

	class VNC_DECODER( COPYRECT ) : public Decoder
	{
		VNC_DECODER_INTERFACE( COPYRECT );
	};
	
	class VNC_DECODER( RRE ) : public Decoder
	{
		VNC_DECODER_INTERFACE( RRE );
	};

	class VNC_DECODER( CORRE ) : public Decoder
	{
		VNC_DECODER_INTERFACE( CORRE );
	};

	class VNC_DECODER( HEXTILE ) : public Decoder
	{
		VNC_DECODER_INTERFACE( HEXTILE );
	};

//	class VNC_DECODER( ZRLE ) : public Decoder
//	{
//		VNC_DECODER_INTERFACE( ZRLE );
//		ZlibReader m_zlib_reader;   //!< zlib input stream
//	};
	
};

#endif
