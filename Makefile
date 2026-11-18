MODULES := kernel_scheduler kernel_memory cpu io memory_stick swap

.PHONY: all clean utils

all: utils
	for mod in $(MODULES); do \
		$(MAKE) -C $$mod clean STATIC_LIBPATHS=; \
		$(MAKE) -C $$mod all; \
	done

utils:
	$(MAKE) -C utils clean all

clean:
	$(MAKE) -C utils clean
	for mod in $(MODULES); do \
		$(MAKE) -C $$mod clean; \
	done
