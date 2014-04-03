SUBDIRS = SearchFile BingRequest BlackJack Oauth1Client

.PHONY: subdirs $(SUBDIRS) all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

all: subdirs
