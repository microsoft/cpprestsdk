SUBDIRS = SearchFile BingRequest

.PHONY: subdirs $(SUBDIRS) all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

all: subdirs
