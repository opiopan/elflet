DEBUGFLAGS	= -g
CFLAGS		= -I../firmware/main $(DEBUGFLAGS) -std=c99
INSTALLDIR	= /usr/local/bin
PLIBDIR     	= elfletlib
PLIBCACHE	= $(PLIBDIR)/__pycache__
TARGETS 	= irtx elflet-config elflet-shadow
OBJS		= irtx.o irslib.o

all: $(TARGETS)

irtx: $(OBJS)
	$(CC) -o $(LFLAGS) $@ $(OBJS)

install: all
	rm -rf $(PLIBCACHE)
	cp $(TARGETS) $(INSTALLDIR)
	cp -R $(PLIBDIR) $(INSTALLDIR)

uninstall:
	for f in $(TARGETS);do rm -f $(INSTALLDIR)/$$f;done
	rm -rf $(INSTALLDIR)/$(PLIBDIR)

clean:
	rm -rf irtx $(OBJS) $(PLIBCACHE)
