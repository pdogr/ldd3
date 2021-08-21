
SUBDIRS = scull scullp scullc 

subdirs:
	for n in $(SUBDIRS); do $(MAKE) -C $$n || exit 1; done;
clean:
	for n in $(SUBDIRS); do $(MAKE) -C $$n clean; done;

