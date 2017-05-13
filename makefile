include Rule.make

.PHONY: numa
.PHONY: numa_clean
.PHONY: test
.PHONY: test_clean

all : numa test

distclean : clean
	-rm -rf bin
	-rm -rf objs
	-rm -rf lib

clean : numa_clean test_clean

numa:
	$(MAKE) -fmakefile.mk -C$(PROJECT_ROOT_PATH)/src build MODULE=numa-eg

numa_clean:
	$(MAKE) -fmakefile.mk -C$(PROJECT_ROOT_PATH)/src clean MODULE=numa-eg

test:
	$(MAKE) -fmakefile.mk -C$(PROJECT_ROOT_PATH)/test build MODULE=test

test_clean:
	$(MAKE) -fmakefile.mk -C$(PROJECT_ROOT_PATH)/test clean MODULE=test

