cc = gcc
objects = eepoll.o log.o uuid4.o main.o
server : ${objects}
	 ${cc} -o server ${objects}

.PHONY : clean
clean :
	rm server ${objects}