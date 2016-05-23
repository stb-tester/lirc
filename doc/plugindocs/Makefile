
# Installation generation of plugin docs and programs.html.
# Uses hardcore GNU Make addons not likely to run on any other
# make implementation.
#

SRC            = $(filter-out programs.html, $(wildcard *.html))
DOCS           = $(addprefix ../lirc.org/html/, $(SRC))
INDEX          = ../lirc.org/html/programs.html

all: $(DOCS) $(INDEX)

clean:
	rm -f $(DOCS)

docpage.xsl: docpage.xsl.in
	sed 's|@.*@/||' < $? > $@

driver-toc.xsl: make-driver-toc.sh
	/usr/bin/env bash make-driver-toc.sh $(SRC)> $@

$(DOCS): $(SRC) docpage.xsl page.xsl driver-toc.xsl
	xsltproc --html docpage.xsl $(subst ../lirc.org/html/,,$@) > $@

$(INDEX): programs.html docpage.xsl driver-toc.xsl page.xsl
	xsltproc --html docpage.xsl programs.html > $@
