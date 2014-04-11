SUBDIRS = SearchFile BingRequest BlackJack Oauth1Client Oauth2Client

.PHONY: subdirs $(SUBDIRS) all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

all: subdirs
