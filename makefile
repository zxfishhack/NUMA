include Rule.make

.PHONY: numa
.PHONY: numa_clean

all : numa

distclean : clean
	-rm -rf bin
	-rm -rf objs
	-rm -rf lib

clean : numa_clean

numa:
	$(MAKE) -fmakefile.mk -C$(PROJECT_ROOT_PATH)/src build MODULE=numa

numa_clean:
	$(MAKE) -fmakefile.mk -C$(PROJECT_ROOT_PATH)/src clean MODULE=numa

