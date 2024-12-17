CC = g++
CFLAGS = -g

objects = ib.o nm.o dmc_utils.o

all : client server

server : $(objects) dmc_conf.h server.cc ib.h
	$(CC) $(CFLAGS) -o $@ $(objects) server.cc -libverbs -lpthread

client : $(objects) dmc_conf.h client.cc ib.h
	$(CC) $(CFLAGS) -o $@ $(objects) client.cc -libverbs -lpthread

ib.o : dmc_utils.h
	$(CC) -c $(CFLAGS) -o $@ ib.cc
nm.o : dmc_utils.h
	$(CC) -c $(CFLAGS) -o $@ nm.cc
dmc_utils.o : debug.h
	$(CC) -c $(CFLAGS) -o $@ dmc_utils.cc

.PHONY : clean
clean :
	-rm client server $(objects)

