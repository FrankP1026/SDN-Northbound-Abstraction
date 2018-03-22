all : wrapper

wrapper : main.o handleRequest.o controllerComm.o
	gcc -o wrapper main.o handleRequest.o controllerComm.o
	
main.o : main.c handleRequest.h
	gcc -c main.c
	
handleRequest : handleRequest.c handleRequest.h controllerComm.h
	gcc -c handleRequest.c
	
controllerComm : controllerComm.c controllerComm.h
	gcc -c controllerComm.c
	
clean :
	rm wrapper *.o