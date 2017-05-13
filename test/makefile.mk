include $(MODULE).pro

include $(PROJECT_ROOT_PATH)/makerules/common_header.mk

# local flag define
INCLUDE += 
CXX_OPTS +=
CC_OPTS +=
OPTI_OPTS +=
DEFINE +=
LD_OPTS += 
AR_OPTS +=

include $(PROJECT_ROOT_PATH)/makerules/common_footer.mk

build: 
	@echo \# Building $(MODULE)...
	$(MAKE) -fmakefile.mk libs MODULE=$(MODULE) TARGET=depend
	$(MAKE) -fmakefile.mk libs MODULE=$(MODULE)
	$(MAKE) -fmakefile.mk libs MODULE=$(MODULE) TARGET=exe

