SUBDIRS = BlackJack_Server BlackJack_Client

.PHONY: subdirs $(SUBDIRS) all

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

all: subdirs
