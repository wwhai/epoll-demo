cc = gcc
out = sserver
objects = sserver.o log.o uuid4.o main.o
server : ${objects}
	 ${cc} -o ${out} ${objects}

.PHONY : clean
clean :
	rm ${out} ${objects}