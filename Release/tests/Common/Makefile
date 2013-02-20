SUBDIRS = TestRunner UnitTestpp utilities

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

TestRunner: UnitTestpp
