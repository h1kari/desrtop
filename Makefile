TARGET = op
USER_SOURCES =  main.c op.c lookup.c des.c redlfsr.c
CFLAGS += -I../include -ggdb
LDFLAGS += -lcrypto

include $(PICOBASE)/software/Makefile.common
