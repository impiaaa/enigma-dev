include ../../Config.mk

SHARED_SRC_DIR := ../../shared

PROTO_DIR := $(SHARED_SRC_DIR)/protos/.eobjs

ifeq ($(OS), Linux)
	OS_LIBS=-lboost_program_options -Wl,--no-as-needed -Wl,-rpath,./ -lpthread
else ifeq ($(OS), Darwin)
	OS_LIBS=-lboost_program_options -lpthread
else
	OS_LIBS=-lboost_system-mt -Wl,--no-as-needed -Wl,-rpath,./ -lboost_program_options-mt -lpthread
endif

CXXFLAGS  += -I../../CompilerSource -I$(PROTO_DIR)
LDFLAGS   += $(OS_LIBS) -L../../ -lcompileEGMf -lProtocols -lENIGMAShared -lprotobuf -lyaml-cpp -lpng

ifeq ($(TESTS), TRUE)
	TARGET=../../emake-tests
	SOURCES := $(call rwildcard, ../emake-tests,*.cpp)
	LDFLAGS += -lpthread -lgtest_main -lgtest
else
	TARGET = ../../emake
	SOURCES := $(call rwildcard,$(SRC_DIR),*.cpp)
endif

#ON BY DEFAULT
ifneq ($(CLI_ENABLE_EGM), FALSE)
	CXXFLAGS += -DCLI_ENABLE_EGM
	CXXFLAGS += -I../libEGM -I../libEGM
	LDFLAGS += -lEGM
endif

#OFF BY DEFAULT
ifeq ($(CLI_ENABLE_SERVER), TRUE)
	CXXFLAGS += -DCLI_ENABLE_SERVER
	LDFLAGS += -lgrpc++
else
	SOURCES := $(filter-out ./Server.cpp, $(SOURCES))
endif

include ../../Default.mk
