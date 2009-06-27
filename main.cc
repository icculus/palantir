/*!
  \file main.cc
  \brief Entry point for the SDL-based VNC client.
  \author John R. Hall
*/

#include <iostream>
#include <iomanip>
#include <cassert>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "vnc.h"
#include "vnc-sdl.h"
#include <SDL/SDL_thread.h>

#define CLIENT_VERSION 0.1f          //!< client release number

using namespace std;

bool g_quit = false;                 //!< global quit flag

/*!
  Displays command line usage information.
  \param path path to this executable, generally from argv[0]
*/
static void Usage( char const* path )
{
	cerr << "Edifying VNC Client of Ook, version " << setprecision(2) << CLIENT_VERSION << endl
		 << "Usage:" << path << " [-p port] [-a password] [-v] [-d encoding] hostname" << endl
		 << "    -p port          TCP port to connect with" << endl
		 << "    -a password      VNC authentication password" << endl
		 << "    -d encoding      disable a particular encoding by name" << endl;
}

/*!
  Entry point for the client's network processing thread.
  Processes network data until the connection dies or the
  g_quit global variable is set.
  \param _rfb pointer to the RFB protocol object to update.
*/
static int NetworkThread( void* _rfb )
{
	VNC::RFBProto& rfb = *(VNC::RFBProto*)_rfb;

	try
	{
		// update continuously, with a 100ms timeout
		while( !g_quit )
			rfb.Update( 100 );
	}
	catch( VNC::Exc const& e )		
	{
		cerr << "Flagrant network error: " << (char const*)e << endl;
		g_quit = true;
	}
		
	return 0;
}

int main( int argc, char* argv[] )
{
	// Parse command line args.
	int ch;
	char const* program_path = argv[0];
	char const* opt_password = "";
	int opt_port = VNC_DEFAULT_PORT;
	char const* opt_hostname = NULL;
	bool opt_verbose = false;
	bool opt_enable_hextile = true, opt_enable_corre = true, opt_enable_rre = true, opt_enable_zrle = true, opt_enable_copyrect = true, opt_enable_zlib = true;
	
	while( ( ch = getopt( argc, argv, "va:p:d:" ) ) != -1 )
	{
		switch( ch )
		{
		case 'a':
			opt_password = optarg;
			break;

		case 'p':
			{
				char const* tmp = optarg;
				opt_port = atoi( tmp );
				if( opt_port < 0 || opt_port > 65535 )
				{
					cerr << "Invalid port " << opt_port << " selected." << endl;
					return 1;
				}
			}
			break;

		case 'v':
			opt_verbose = true;
			break;

		case 'd':
			{
				if( !strcasecmp( optarg, "hextile" ) )        { opt_enable_hextile = false; }
				else if( !strcasecmp( optarg, "corre" ) )     { opt_enable_corre = false; }
				else if( !strcasecmp( optarg, "rre" ) )       { opt_enable_rre = false; }
				else if( !strcasecmp( optarg, "zrle" ) )      { opt_enable_zrle = false; }
				else if( !strcasecmp( optarg, "copyrect" ) )  { opt_enable_copyrect = false; }
				else if( !strcasecmp( optarg, "zlib" ) )      { opt_enable_zlib = false; }
				else { Usage( program_path ); return 1; }
			}
			break;
			
		default:
			Usage( program_path );
			return 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if( argc != 1 )
	{
		Usage( program_path );
		return 1;
	}
	
	opt_hostname = argv[0];
	assert( opt_hostname );
	
	// Initialize the SDL_net library.
	if( opt_verbose ) cerr << "Initializing SDL_net." << endl;
	if( SDLNet_Init() < 0 )
	{
		cerr << "Unable to initialize SDL_net." << endl;
		return -1;
	}	
	atexit( SDLNet_Quit );

	SDL_Thread* net_thread = NULL;     //<! pointe
	
	try
	{
		if( opt_verbose ) cerr << "Starting client." << endl;

		// Set up the network connection.
		if( opt_verbose ) cerr << "Connecting to " << opt_hostname << " on port " << opt_port << "..." << endl;
		VNC::SDLNetworkClient client( opt_hostname, (VNC::Uint16) opt_port );

		// Create decoders in order of preference.
		vector< VNC::Decoder* > decoders;
//		::VNC::VNC_DECODER( ZRLE ) dec_zrle( client ); if( opt_enable_zrle ) decoders.push_back( &dec_zrle );
		::VNC::VNC_DECODER( ZLIB ) dec_zlib( client ); if( opt_enable_zlib ) decoders.push_back( &dec_zlib );
		::VNC::VNC_DECODER( HEXTILE ) dec_hextile( client ); if( opt_enable_hextile ) decoders.push_back( &dec_hextile );
		::VNC::VNC_DECODER( CORRE ) dec_corre( client ); if( opt_enable_corre ) decoders.push_back( &dec_corre );
		::VNC::VNC_DECODER( RRE ) dec_rre( client ); if( opt_enable_rre ) decoders.push_back( &dec_rre );
		::VNC::VNC_DECODER( COPYRECT ) dec_copyrect( client ); if( opt_enable_copyrect ) decoders.push_back( &dec_copyrect );
		::VNC::VNC_DECODER( RAW ) dec_raw( client ); decoders.push_back( &dec_raw );

		// Verbose spew.
		if( opt_verbose )
		{
			cerr << "Supported encodings:" << endl;
			for( unsigned i = 0; i < decoders.size(); ++i )				
				cerr << "    " << decoders[i]->GetDesc() << endl;
		}

		// Ignore SIGPIPE.
		// SDL rather stupidly intercepts this and makes bad things happen.
		signal( SIGPIPE, SIG_IGN );
		
		// Set up the RFB protocol.
		VNC::RFBProto rfb( client, opt_password, true, decoders );
		if( opt_verbose )
		{
			cerr << "Connected to VNC server (using protocol version "
				 << rfb.GetMajorVersion() << "."
				 << rfb.GetMinorVersion() << ")." << endl;

			VNC::PixelFormat fmt;
			rfb.GetPixelFormat( fmt );
			cerr << "Native format of '" << rfb.GetDesktopName() << "':" << endl
				 << "    " << rfb.GetDesktopWidth() << "x" << rfb.GetDesktopHeight() << " pixels" << endl
				 << "    " << fmt.bits << " bits per pixel" << endl
				 << "    " << (fmt.big_endian ? "big endian" : "little endian") << endl;
		}

		// Create the display and attach it to the protocol handler.
		VNC::SDLDisplay display( rfb );
		rfb.SetDisplay( &display );
		
		// Create the network update thread.
 		net_thread = SDL_CreateThread( NetworkThread, (void*)&rfb );
 		if( net_thread == NULL )
			throw VNC::Exc( "unable to create network thread" );
		
		// Run!
		while( display.Update() ) { };
		
		// End the network thread.
 		if( opt_verbose ) cerr << "Shutting down." << endl;
		g_quit = true;

		// Wait for the thread to terminate.
		SDL_WaitThread( net_thread, NULL );

		// Print usage stats.
		if( opt_verbose )
		{
			cerr << "Decoder usage statistics:" << endl;
			for( unsigned i = 0; i < decoders.size(); ++i )
				cerr << "    " << decoders[i]->GetNumProcessed() << " " << decoders[i]->GetName() << " packets" << endl;
		}
	}
	catch ( VNC::Exc& e )
	{
		cerr << "Flagrant VNC error: " << (char const*)e << endl;
		if( net_thread )
			SDL_KillThread( net_thread );
		return 1;
	}
	
	if( opt_verbose ) cerr << "Client is exiting normally." << endl;
	
	return 0;
}
