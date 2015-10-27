
all:
	@make clean -s
	@make mandel -s
	@make run -s
	@rm -f *.o 

mandel: mandel.c
	gcc -c mandel.c -lm -lpng -pthread -Werror -Wall -O3
	gcc mandel.o -o mandel -lm -lpng -pthread -O3
	-rm -f mandel.o

run: mandel run.c
	gcc -c run.c -Werror -Wall -O3
	gcc run.o -o run -O3
	-rm -f run.o

clean:
	-@rm -f *~ *.o mandel run
