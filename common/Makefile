include ../common/make.common

OBJS = builtin.o command.o job.o signal_handler.o utils.o

.PHONY: all
all: $(OBJS)

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -o $@ $< -c

clean:
	$(RM) $(OBJS)
