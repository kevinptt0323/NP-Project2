.PHONY: np_simple np_single_proc np_multi_proc

all: np_simple np_single_proc np_multi_proc

np_simple:
	$(MAKE) -C np_simple

np_single_proc:
	$(MAKE) -C np_single_proc

np_multi_proc:
	$(MAKE) -C np_multi_proc

clean:
	$(MAKE) -C common clean
	$(MAKE) -C np_simple clean
	$(MAKE) -C np_single_proc clean
	$(MAKE) -C np_multi_proc clean
