SUBDIRS = SearchFile BingRequest BlackJack

.PHONY: subdirs $(SUBDIRS) all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

all: subdirs
