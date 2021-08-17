
SUBDIRS = scull

subdirs:
	for n in $(SUBDIRS); do $(MAKE) -C $$n; exit 0; done;
clean:
	for n in $(SUBDIRS); do $(MAKE) -C $$n clean; done;

