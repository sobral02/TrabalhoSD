#/**
# * ----------------------------------------------------------------------*
# * ----------------------GRUPO 18 - 2023/24-----------------------------*
# * ----------------------SISTEMAS DISTRIBUÍDOS---------------------------*
# * 
# *      PROJETO 4ª FASE
# * 
# *      FC56269 - Frederico Prazeres		
# *      FC56332 - Ricardo Sobral
# *      FC56353 - João Osório
# *
# * ----------------------------------------------------------------------*
# * ----------------------------------------------------------------------*


CC = gcc
CFLAGS = -g -Wall -I $(INCDIR) 
LIBS = -lrt -lpthread -lprotobuf-c -lzookeeper_mt

SERVER_TARGET = table-server
CLIENT_TARGET = table-client
LIB_TARGET = lib/libtable.a

BINDIR = binary
INCDIR = include
LIBDIR = lib
OBJDIR = object
SRCDIR = source

SERVER_OBJ = object/table_server.o object/data.o object/entry.o object/message.o object/table_skel.o object/network_server.o object/sdmessage.pb-c.o object/client_stub.o object/network_client.o 
CLIENT_OBJ = object/table_client.o object/network_client.o object/sdmessage.pb-c.o object/client_stub.o object/message.o

all: libtable table-client table-server

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

libtable: $(OBJDIR)/data.o $(OBJDIR)/entry.o $(OBJDIR)/list.o $(OBJDIR)/table.o
	@echo "Creating $@"
	@$(AR) -rcs $(LIB_TARGET) $(OBJDIR)/data.o $(OBJDIR)/entry.o $(OBJDIR)/list.o $(OBJDIR)/table.o

$(CLIENT_TARGET): $(CLIENT_OBJ) $(LIB_TARGET)
	@echo "Linking $@"
	@$(CC) $(CFLAGS) $^ -o $(BINDIR)/$@ $(LIBS)

$(SERVER_TARGET): $(SERVER_OBJ) $(LIB_TARGET)
	@echo "Linking $@"
	@$(CC) $(CFLAGS) $^ -o $(BINDIR)/$@ $(LIBS)

$(OBJDIR)/sdmessage.pb-c.o: sdmessage.proto
	@mkdir -p $(OBJDIR)
	@echo "Compiling Proto file $@"
	@protoc-c --c_out=. sdmessage.proto
	@mv sdmessage.pb-c.c $(SRCDIR)
	@mv sdmessage.pb-c.h $(INCDIR)
	@$(CC) $(CFLAGS) -c -o $@ $(SRCDIR)/sdmessage.pb-c.c

clean:
	@rm -rf ./object/* ./lib/* ./binary/table-client ./binary/table-server

