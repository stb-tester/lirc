
# Post-installation generation of plugin docs and programs.html.
# Uses hardcore GNU Make addons not likely to run on any other
# make implementation
#

HERE         = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
THERE        = $(abspath $(HERE)/../lirc.org/html)
SRC          = $(filter-out $(HERE)/programs.html, $(wildcard $(HERE)/*.html))
DOCS         = $(subst $(HERE), $(THERE), $(SRC))
INDEX        = $(THERE)/programs.html
STYLESHEETS  = $(HERE)/plugpage.xsl \
               $(HERE)/page.xsl \
               $(HERE)/driver-toc.xsl \
               $(HERE)/ext-driver-toc.xsl


update: $(DOCS) $(INDEX)


clean-docs:
	rm -f $(DOCS)

$(HERE)/ext-driver-toc.xsl: $(HERE)/make-ext-driver-toc.sh $(SRC)
	cd $(HERE); /bin/bash make-ext-driver-toc.sh $(SRC)> $@

$(DOCS): $(SRC) $(STYLESHEETS)
	cd $(HERE); \
	    xsltproc --html plugpage.xsl $(subst $(THERE),$(HERE),$@) > $@

$(INDEX):$(HERE)/programs.html $(STYLESHEETS) $(SRC)
	cd $(HERE); xsltproc --html plugpage.xsl programs.html > $@
