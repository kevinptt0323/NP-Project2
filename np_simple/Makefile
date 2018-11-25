include ../common/make.common

EXEC = np_simple
OBJS = ${COMMON}/builtin.o \
       ${COMMON}/command.o \
       ${COMMON}/job.o \
       ${COMMON}/number_pipe.o \
       ${COMMON}/signal_handler.o \
       ${COMMON}/type.o \
       ${COMMON}/utils.o

.PHONY: all
all: $(EXEC)

$(EXEC): main.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -I${COMMON}

clean:
	$(RM) $(EXEC)
