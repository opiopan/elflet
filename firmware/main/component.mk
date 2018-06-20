#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
CFLAGS		+= -DMG_ENABLE_HTTP_STREAMING_MULTIPART
CXXFLAGS	+= -DMG_ENABLE_HTTP_STREAMING_MULTIPART

COMPONENT_SRCDIRS                = . foundations
COMPONENT_ADD_INCLUDEDIRS        = . foundations

COMPONENT_EMBED_TXTFILES	:= html/index.htmlz \
				   html/jquery-3.3.1.min.jsz \
				   html/elflet.cssz html/wizard.htmlz \
				   html/wizard.cssz html/wizard.jsz

COMPONENT_EXTRA_CLEAN		:= $(COMPONENT_EMBED_TXTFILES)

.SUFFIXES: .html .htmlz .js .jsz .css .cssz

.html.htmlz .js.jsz .css.cssz:
	echo ZIP $@
	gzip -c $< > $@

extra-clean:
	rm -f $(COMPONENT_EXTRA_CLEAN)
