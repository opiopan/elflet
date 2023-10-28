DEBUGFLAGS	= -g
CFLAGS		= -I../firmware/main $(DEBUGFLAGS) -std=c99
INSTALLDIR	= /usr/local/bin
PLIBDIR     	= elfletlib
TARGETS 	= irtx elflet-config elflet-shadow
OBJS		= irtx.o irslib.o

all: $(TARGETS)

irtx: $(OBJS)
	$(CC) -o $(LFLAGS) $@ $(OBJS)

install: all
	cp $(TARGETS) $(INSTALLDIR)
	cp -R $(PLIBDIR) $(INSTALLDIR)

uninstall:
	for f in $(TARGETS);do rm -f $(INSTALLDIR)/$$f;done
	rm -rf $(INSTALLDIR)/$(PLIBDIR)

clean:
	rm -f irtx $(OBJS)
