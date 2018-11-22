include ../common/make.common

OBJS = builtin.o command.o job.o number_pipe.o signal_handler.o type.o utils.o

.PHONY: all
all: $(OBJS)

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -o $@ $< -c

clean:
	$(RM) $(OBJS)
