#Makefile
CC = gcc
CFLAGS = -Wall -g -std=c99 -pthread
CWD= $(pwd)
CLIENTDIR = Client
SERVERDIR = Server

SRCS = $(wildcard $(SERVERDIR)/*.c)
SRCC = $(wildcard $(CLIENTDIR)/*.c)

OBJS := $(notdir $(SRCS:%.c=%.o))
OBJC := $(notdir $(SRCC:%.c=%.o))


.PHONY: all clean test1

all: server client

server: $(SERVERDIR)/$(OBJS)
	$(CC) $(CFLAGS) $< -o $@ 
	@mv server $(SERVERDIR)/server_project

client : $(CLIENTDIR)/$(OBJC)
	$(CC) $(CFLAGS) $< -o $@
	@mv client $(CLIENTDIR)/client_project


%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@


test1: server client
	sh test1.sh

test2: server client
	sh test2.sh

test3: server client
	sh test3.sh

stats: server client
	sh statistiche.sh

clean: 
	rm $(SERVERDIR)/$(OBJS) $(CLIENTDIR)/$(OBJC)

