PROJECT = chat
SOURCE = chat.c #drugi.c treci.c
#HEADERS = glavni.h pomocni.h
#CC = gcc ... ako zelimo kompajler gcc; default je cc
CFLAGS = -Wall -g
LDFLAGS =
OBJECTS = ${SOURCE:.c=.o}

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(PROJECT)

$(OBJECTS): $(HEADERS)

#test: $(PROJECT)
#	hcp riptool host5:/root
#	himage host5 /root/riptool -r 10.0.7.1
#	himage host5 /root/riptool -w 10.0.7.1
#	himage host5 /root/riptool -w 10.0.7.1 \
							20.1.2.0 255.255.255.0 2 \
							30.3.3.0 255.255.255.0 4 \
							40.4.4.0 255.255.255.0 5

clean:
	@rm -f $(PROJECT) $(OBJECTS) *.core


