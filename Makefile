CC = clang++
FLAGS = -g -O0 `pkg-config --cflags --libs cairo`
#FLAGS = -O3

testParser: testParser.o circuitParser.o gate.o circuit.o utility.o draw.o
	${CC} ${FLAGS} $^ -o $@
testParser.o: testParser.cpp
	${CC} ${FLAGS} -c $<
circuitParser.o: circuitParser.cpp
	${CC} ${FLAGS} -c $<
gate.o: gate.cpp
	${CC} ${FLAGS} -c $<
circuit.o: circuit.cpp
	${CC} ${FLAGS} -c $<
utility.o: utility.cpp
	${CC} ${FLAGS} -c $<
draw.o: draw.cpp
	${CC} ${FLAGS} -c $<

clean:
	rm *.o
