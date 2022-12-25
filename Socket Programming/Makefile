
CXX = gcc
CXXFLAGS = -c 
LDFLAGS = -lm

all: server client servicemap

server: server.o 
	$(CXX) server.o -o server $(LDFLAGS) 

server.o: server.c 
	$(CXX) $(CXXFLAGS) server.c 
	
client: client.o 
	$(CXX) client.o -o client $(LDFLAGS) 

client.o: client.c 
	$(CXX) $(CXXFLAGS) client.c 
	
servicemap: servicemap.o 
	$(CXX) servicemap.o -o servicemap $(LDFLAGS) 

servicemap.o: servicemap.c 
	$(CXX) $(CXXFLAGS) servicemap.c 
	
clean:
	rm -rf *o server client servicemap \
