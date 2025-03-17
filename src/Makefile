CFLAGS = -O2 -g -Wno-unused-result -Wunused
LDLIBS = -lreadline -lncurses

BIN			= ../bin
SHARED	= chipconfig.o efsio.o hdlc.o memio.o qcio.o ptable.o sahara.o
TOOLS		= mibibsplit qbadblock qblinfo qcommand qdload qefs qflashparm qident \
				qnvram qrflash qrmem qwdirect qwflash qterminal
LIST		= $(addprefix $(BIN)/, $(TOOLS))

.PHONY: all clean

all: $(LIST)

dir:
	mkdir -p $(BIN)

clean:
	rm -f *.o
	rm -f $(LIST)

%: %.c
	$(CC) $(INC) $< $(CFLAGS) -o $@ $(LDLIBS)

$(BIN)/%: %.c $(SHARED) dir
	$(CC) $(INC) $< $(CFLAGS) -o $@ $(LDLIBS) $(SHARED)
