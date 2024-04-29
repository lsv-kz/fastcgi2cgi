CFLAGS = -Wall -O2 -g -std=c++11

CC = c++
#CC = clang++

OBJSDIR = objs

OBJS = $(OBJSDIR)/fcgi2cgi.o \
	$(OBJSDIR)/cgi.o \
	$(OBJSDIR)/socket.o \

fcgi2cgi: $(OBJS)
	$(CC) $(CFLAGS) -o $@  $(OBJS) -lpthread

$(OBJSDIR)/fcgi2cgi.o: fcgi2cgi.cpp main.h fcgi_server.h
	$(CC) $(CFLAGS) -c fcgi2cgi.cpp -o $@

$(OBJSDIR)/cgi.o: cgi.cpp main.h fcgi_server.h
	$(CC) $(CFLAGS) -c cgi.cpp -o $@

$(OBJSDIR)/socket.o: socket.cpp main.h
	$(CC) $(CFLAGS) -c socket.cpp -o $@

clean:
	rm -f fcgi2cgi
	rm -f $(OBJSDIR)/*.o
