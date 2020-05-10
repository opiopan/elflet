COMPONENT_OBJS			 = mongoose/mongoose.o
COMPONENT_SRCDIRS                = mongoose
COMPONENT_ADD_INCLUDEDIRS        = mongoose

CFLAGS += -DMG_ENABLE_HTTP_STREAMING_MULTIPART  -DMG_ENABLE_FILESYSTEM=1 \
	  -Wformat-truncation=0
