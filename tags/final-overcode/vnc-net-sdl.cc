/*!
  \file vnc-net-sdl.cc
  \brief SDL implementation of network client class.
  \author John R. Hall
*/

#include <string>
#include <iostream>
#include <SDL/SDL_net.h>
#include <SDL/SDL_mutex.h>
#include <string.h>
#include <errno.h>
#include "vnc-sdl.h"

using namespace std;

namespace VNC
{

	SDLNetworkClient::SDLNetworkClient( std::string host, Uint16 port )
		: m_socket( NULL ),
		  m_mutex( NULL )
	{
		IPaddress addr;

		if( SDLNet_ResolveHost( &addr, host.c_str(), port ) < 0 )
			throw ExcResolve();

		m_socket = SDLNet_TCP_Open( &addr );

		if( m_socket == NULL )
			throw ExcConnect();

		m_mutex = SDL_CreateMutex();
	}

	SDLNetworkClient::~SDLNetworkClient()
	{
		if( m_socket != NULL )
			SDLNet_TCP_Close( m_socket );
		if( m_mutex != NULL )
			SDL_DestroyMutex( m_mutex );
	}

	void SDLNetworkClient::SendBytes( Uint8 const* data, unsigned int count )
	{
		int amt = SDLNet_TCP_Send( m_socket, (void*)data, (int)count );
		if( amt < (int)count )
			throw ExcWrite();		
	}
	
	void SDLNetworkClient::ReceiveBytes( Uint8* data, unsigned int count )
	{
		int received = 0;
		
		while( received < (int)count )
		{
			int amt = SDLNet_TCP_Recv( m_socket, data, (int)count-received );
			if( amt <= 0 )
			{
				throw ExcRead();
			}
			received += amt;
			data += amt;
		}
	}

	bool SDLNetworkClient::WaitDataReady( Uint32 ms )
	{
		SDLNet_SocketSet ss = SDLNet_AllocSocketSet( 1 );
		SDLNet_TCP_AddSocket( ss, m_socket );
		int result = SDLNet_CheckSockets( ss, ms );
		if( result <  0 )
			throw ExcSelect();
		if( result == 0 )
			return false;
		else
			return true;
	}

	void SDLNetworkClient::BeginWritePacket()
	{
		SDL_mutexP( m_mutex );
	}
	
	void SDLNetworkClient::EndWritePacket()
	{
		SDL_mutexV( m_mutex );
	}	
	
};
