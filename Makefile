# yoe game makefile
EXE  			:= game

CXX				:= g++
CXXFLAGS  		:= -O1 -Wall -Wextra -g -Wno-missing-braces -std=c++20
# -mwindows		no terminal window.

LDFLAGS 		:= -L lib/ -lglfw3 -lgdi32 -lopengl32
INCLUDE  		:= -I include/

SRCS 			:= $(wildcard src/*.cpp src/*.c)
OBJS 			:= $(patsubst %, %.o, $(patsubst src%, out%, $(SRCS)))

# compile + run
$(EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(INCLUDE) -o $@
	$(EXE)

# create object files from .cpp
out/%.cpp.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< $(INCLUDE) -o $@
	@echo .cpp.o created

# create object files from .c
out/%.c.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< $(INCLUDE) -o $@
	@echo .c.o created

.PHONY: clean
clean:
	del *.o $(EXE).exe /s
	@echo finished cleaning!