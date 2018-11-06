CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -O2
EXEC = npshell
OBJS = builtin.o command.o job.o signal_handler.o utils.o

.PHONY: all
all: $(EXEC)

$(EXEC): main.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -o $@ $< -c

install: $(EXEC)
	mkdir -p $(TARGET_DIR)
	cp $(EXEC) $(TARGET_DIR)

clean:
	$(RM) $(EXEC) $(OBJS)
