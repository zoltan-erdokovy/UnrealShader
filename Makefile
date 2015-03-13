# Copyright (c) 2008-2013 Luxology LLC

LXSDK = ../LXSDK
LXSDK_INC = $(LXSDK)/include
LXSDK_BUILD = $(LXSDK)/samples/Makefiles/build
SRC_DIR=UnrealShader
OBJ_DIR=Linux/obj
TARGET_DIR=Linux/build
MYCXX = g++
LINK = g++
CXXFLAGS = -std=c++0x -g -c -I$(LXSDK_INC) -fPIC -m64 -msse
LDFLAGS = -L$(LXSDK_BUILD) -L/usr/lib -lcommon -lpthread -shared

OBJS = $(OBJ_DIR)/initializer.o $(OBJ_DIR)/UnrealShader.o
TARGET = $(TARGET_DIR)/unrealShader.lx

all: $(TARGET)

lxsdk:
	cd $(LXSDK)/samples/Makefiles/common; make

$(OBJ_DIR):
	mkdir $@

.PRECIOUS :$(OBJ_DIR)/%.o 
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(MYCXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJ_DIR) lxsdk $(OBJS)
	$(LINK) -o $@ $(OBJS) $(LDFLAGS)

clean:
	cd $(LXSDK)/samples/Makefiles/common; make clean
	rm -rf $(TARGET) $(OBJ_DIR)
