#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
CFLAGS		+= -DMG_ENABLE_HTTP_STREAMING_MULTIPART
CXXFLAGS	+= -DMG_ENABLE_HTTP_STREAMING_MULTIPART

COMPONENT_SRCDIRS                = . foundations
COMPONENT_ADD_INCLUDEDIRS        = . foundations

COMPONENT_EMBED_TXTFILES	:= html/index.html html/jquery-3.3.1.min.js \
				   html/elflet.css
