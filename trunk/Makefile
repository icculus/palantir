DOXYGEN = doxygen

CLIENT_HEADERS += vnc.h vnctypes.h vnc-sdl.h d3des.h
CLIENT_OBJ += main.o vnc-rfb.o vnc-net-sdl.o d3des.o vnc-display.o vnc-display-sdl.o vnc-encoding-raw.o vnc-encoding-copyrect.o vnc-encoding-rre.o vnc-encoding-hextile.o
CLIENT_LIBS += `sdl-config --libs` -lSDL_net
CXXFLAGS += `sdl-config --cflags` -W -Wall -D_REENTRANT

# for debugging
CXXFLAGS += -g

# change this for the platform
# x86:     VNC_LITTLE_ENDIAN
# powerpc: VNC_BIG_ENDIAN
CXXFLAGS += -DVNC_BIG_ENDIAN

.PHONY: docs clean default

default:
	@echo "Edit the Makefile, set the VNC_xxx_ENDIAN flag correctly, and type 'make client' to build the VNC client."

client: $(CLIENT_OBJ) $(CLIENT_HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_OBJ) $(CLIENT_LIBS)

docs:
	mkdir -p doc/client
	$(DOXYGEN) client.dox

clean:
	rm -rf client *.o *~ doc/client
