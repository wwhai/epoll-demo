cc = gcc
out = sserver
objects = sserver.o log.o uuid4.o thpool.o main.o
server : ${objects}
	 ${cc} -o ${out} ${objects} -pthread

.PHONY : clean
clean :
	rm ${out} ${objects}