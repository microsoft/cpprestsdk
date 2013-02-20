SUBDIRS = Common Functional

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

Functional: Common

all: Functional
