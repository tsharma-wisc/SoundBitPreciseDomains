bvsfdAnalysis: 
	$(MAKE) -C $(CURDIR)/src/analysis

install:
	$(MAKE) -C $(CURDIR)/src/analysis install

.PHONY: clean
clean:
	-$(MAKE) -C $(CURDIR)/src/analysis clean
