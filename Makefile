

TARGET=receiver
OBJ=receiver.o test.c conf.o utils.o load.o hash.o log.o
CC=gcc  -Wall -Werror -Wcast-align 
DEBUG=-g

${TARGET} : ${OBJ}
	${CC} ${DEBUG} -o ${TARGET} ${OBJ}

receiver.o : receiver.c  conf.h utils.h hash.h
	${CC} ${DEBUG} -c $<

conf.o : conf.c utils.h
	${CC} ${DEBUG} -c $<
	
load.o: load.c conf.h
	${CC} ${DEBUG} -c $<
	
utils.o : utils.c
	${CC} ${DEBUG} -c $<
hash.o: hash.c
	${CC} ${DEBUG} -c $<
	
log.o: log.c
	${CC} ${DEBUG} -c $<
	
.PHONY :clean
clean :
	-rm *.o ${TARGET}  
