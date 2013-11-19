include ./findcudalib.mk

# Location of the CUDA Toolkit
CUDA_PATH       ?= /usr/local/cuda-5.5

# internal flags
NVCCFLAGS   := -m${OS_SIZE}
CCFLAGS     :=
NVCCLDFLAGS :=
LDFLAGS     :=

# Extra user flags
EXTRA_NVCCFLAGS   ?=
EXTRA_NVCCLDFLAGS ?=
EXTRA_LDFLAGS     ?=
EXTRA_CCFLAGS     ?=

# OS-specific build flags
ifneq ($(DARWIN),) 
  LDFLAGS += -Xlinker -rpath $(CUDA_PATH)/lib
  CCFLAGS += -arch $(OS_ARCH) $(STDLIB)  
else
  ifeq ($(OS_ARCH),armv7l)
    ifeq ($(abi),gnueabi)
      CCFLAGS += -mfloat-abi=softfp
    else
      # default to gnueabihf
      override abi := gnueabihf
      LDFLAGS += --dynamic-linker=/lib/ld-linux-armhf.so.3
      CCFLAGS += -mfloat-abi=hard
    endif
  endif
endif

ifeq ($(ARMv7),1)
NVCCFLAGS += -target-cpu-arch ARM
ifneq ($(TARGET_FS),) 
CCFLAGS += --sysroot=$(TARGET_FS)
LDFLAGS += --sysroot=$(TARGET_FS)
LDFLAGS += -rpath-link=$(TARGET_FS)/lib
LDFLAGS += -rpath-link=$(TARGET_FS)/usr/lib
LDFLAGS += -rpath-link=$(TARGET_FS)/usr/lib/arm-linux-$(abi)
endif
endif

# Debug build flags
ifeq ($(dbg),1)
      NVCCFLAGS += -g -G
      TARGET := debug
else
      TARGET := release
endif

ALL_CCFLAGS :=
ALL_CCFLAGS += $(NVCCFLAGS)
ALL_CCFLAGS += $(addprefix -Xcompiler ,$(CCFLAGS))
ALL_CCFLAGS += $(EXTRA_NVCCFLAGS)
ALL_CCFLAGS += $(addprefix -Xcompiler ,$(EXTRA_CCFLAGS))

ALL_LDFLAGS :=
ALL_LDFLAGS += $(ALL_CCFLAGS)
ALL_LDFLAGS += $(NVCCLDFLAGS)
ALL_LDFLAGS += $(addprefix -Xlinker ,$(LDFLAGS))
ALL_LDFLAGS += $(EXTRA_NVCCLDFLAGS)
ALL_LDFLAGS += $(addprefix -Xlinker ,$(EXTRA_LDFLAGS))

# Common includes and paths for CUDA
INCLUDES  := -I/usr/local/cuda/common/inc
LIBRARIES :=

################################################################################

# MPI check and binaries
MPICXX ?= $(shell which mpicxx)
EXEC   ?=

ifneq ($(shell uname -m | sed -e "s/i386/i686/"), ${OS_ARCH})
      $(info -----------------------------------------------------------------------------------------------)
      $(info WARNING - MPI not supported when cross compiling.)
      MPICXX :=
endif

ifeq ($(MPICXX),)
      $(info -----------------------------------------------------------------------------------------------)
      $(info WARNING - No MPI compiler found.)
      $(info -----------------------------------------------------------------------------------------------)
      $(info   Cannot be built without an MPI Compiler.)
      $(info   This will be a dry-run of the Makefile.)
      $(info   For more information on how to set up your environment to build and run this )
      $(info   sample, please refer the CUDA Samples documentation and release notes)
      $(info -----------------------------------------------------------------------------------------------)
      MPICXX=mpicxx
      EXEC=@echo "[@]"
endif

# CUDA code generation flags
ifneq ($(OS_ARCH),armv7l)
GENCODE_SM10    := -gencode arch=compute_10,code=sm_10
endif
GENCODE_SM20    := -gencode arch=compute_20,code=sm_20
GENCODE_SM30    := -gencode arch=compute_30,code=sm_30 -gencode arch=compute_35,code=\"sm_35,compute_35\"
GENCODE_FLAGS   := $(GENCODE_SM10) $(GENCODE_SM20) $(GENCODE_SM30)

LIBSIZE := 
ifeq ($(DARWIN),)
ifeq ($(OS_SIZE),64)
LIBSIZE := 64
endif
endif

LIBRARIES += -L$(CUDA_PATH)/lib${LIBSIZE} -lcudart

################################################################################

# Target rules
all: build

build: vector_add

vector_add_cuda.o: vector_add.cu
	$(EXEC) $(NVCC) $(INCLUDES) $(ALL_CCFLAGS) $(GENCODE_FLAGS) -o $@ -c $<

vector_add.o: vector_add.cpp
	$(EXEC) $(MPICXX) $(CCFLAGS) $(INCLUDES) -o $@ -c $<

vector_add: vector_add.o vector_add_cuda.o
	$(EXEC) $(MPICXX) $(CCFLAGS) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	$(EXEC) rm -f vector_add vector_add.o vector_add.o

clobber: clean

