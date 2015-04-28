LIN_CXX=clang++
LIN_WXCONFIG=wx-config
LIN_STRIP=strip

# You'll have to adjust the settings below to point to your cross compilers.

WIN_CXX=i686-w64-mingw32-g++
WIN_WXCONFIG=/usr/local/i686-w64-mingw32/bin/wx-config
WIN_STRIP=i686-w64-mingw32-strip
WINDRES=i686-w64-mingw32-windres

OSX_CXX=x86_64-apple-darwin14-clang++-libc++ -I/usr/local/x86_64-apple-darwin14/include
OSX_WXCONFIG=/usr/local/x86_64-apple-darwin14/bin/wx-config
osx_strip=x86_64-apple-darwin14-strip

SOURCES=gui.cpp ihex.cpp usb.cpp

CXXFLAGS=-O2 -std=c++11

tek.lin: $(patsubst %.cpp,%-lin.o,$(SOURCES))
	$(LIN_CXX) $(CXXFLAGS) $^ `$(LIN_WXCONFIG) --libs` -lusb-1.0 -o $@
	$(LIN_STRIP) -s $@

tek.mac: $(patsubst %.cpp,%-mac.o,$(SOURCES))
	$(OSX_CXX) $(CXXFLAGS) $^ `$(OSX_WXCONFIG) --libs --static` -lusb-1.0 -o $@ -v
	$(OSX_STRIP) -S $@

tek.exe: $(patsubst %.cpp,%-win.o,$(SOURCES)) rc-win.o
	$(WIN_CXX) $(CXXFLAGS) $^ `$(WIN_WXCONFIG) --libs --static` -static -lusb-1.0 -o $@
	$(WIN_STRIP) -s $@

%-lin.o: %.cpp
	$(LIN_CXX) -c $(CXXFLAGS) `$(LIN_WXCONFIG) --cxxflags` $(filter %.cpp,$^) -o $@

%-win.o: %.cpp
	$(WIN_CXX) -c $(CXXFLAGS) `$(WIN_WXCONFIG) --cxxflags` $(filter %.cpp,$^) -o $@

%-mac.o: %.cpp
	$(OSX_CXX) -c $(CXXFLAGS) `$(OSX_WXCONFIG) --cxxflags` $(filter %.cpp,$^) -o $@

background.inc: background.png
	xxd -p $< | sed -e 's/../0x&,/g' > $@

gui-lin.o: background.inc
gui-lin.o gui-win.o gui-mac.o: ihex.hpp usb.hpp
usb-lin.o usb-win.o usb-mac.o: usb.hpp

rc-win.o: info.rc icon.ico background.png
	$(WINDRES) $< -O coff -o $@

.PHONY: dist
dist: tek-linux.tar.gz tek-windows.zip tek-macosx.zip

tek-linux.tar.gz: tek.lin linux-readme linux-udev-rules
	tar \
		--transform 's|tek.lin|tek-linux/tek|' \
		--transform 's|linux-readme|tek-linux/README|' \
		--transform 's|linux-udev-rules|tek-linux/40-tek.rules|' \
		-czvf $@ $^

tek-macosx.zip: tek.mac Info.plist background.png
	rm -rf tek.app; \
	mkdir -p tek.app/Contents && \
	cd tek.app/Contents && \
	echo 'APPL????' > PkgInfo && \
	cp ../../Info.plist . && \
	mkdir Resources && \
	mkdir MacOS && \
	cp ../../tek.mac MacOS/tek && \
	cp ../../background.png Resources
	zip $@ -r tek.app
	rm -rf tek.app

tek-windows.zip: tek.exe
	zip $@ $^

.PHONY: clean
clean:
	rm -rf tek.app
	rm -f *.o tek.* tek-*.tar.gz tek-*.zip background.inc
