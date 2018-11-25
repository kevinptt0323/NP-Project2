include ../common/make.common

EXEC = np_multi_proc
COMMON_OBJS = ${COMMON}/builtin.o \
              ${COMMON}/command.o \
              ${COMMON}/job.o \
              ${COMMON}/number_pipe.o \
              ${COMMON}/signal_handler.o \
              ${COMMON}/type.o \
              ${COMMON}/utils.o
OBJS = user.o

.PHONY: all
all: $(EXEC)

$(EXEC): main.cpp $(OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -I${COMMON}

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -o $@ $< -I${COMMON} -c

clean:
	$(RM) $(EXEC) $(OBJS)
