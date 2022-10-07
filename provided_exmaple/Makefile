CC=gcc
EXE=add segfault loopf forever cmdarg inproc outproc relay

all: $(EXE)

add: add-loop.c
	$(CC) add-loop.c -o $@

segfault: segfault.c
	$(CC) segfault.c -o $@

loopf: loop.c
	$(CC) $< -o $@
    
forever: loopever.c
	$(CC) $< -o $@
    
cmdarg: cmdarg.c
	$(CC) $< -o $@
  
inproc: inProc.c
	$(CC) $< -o $@

outproc: outProc.c
	$(CC) $< -o $@

Relay: relay.c
	$(CC) $< -o $@

clean:
	rm $(EXE)
