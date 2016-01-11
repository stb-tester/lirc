
# Post-installation generation of plugin docs and programs.html.
# Uses hardcore GNU Make addons not likely to run on any other
# make implementation
#

HERE         = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
THERE        = $(HERE)/var
INDEX        = $(THERE)/index.html
SRC          = $(wildcard $(HERE)/*.html)
DOCS         = $(subst $(HERE), $(THERE), $(SRC))
STYLESHEETS  = $(HERE)/plugpage.xsl $(HERE)/page.xsl \

all: update

update: $(DOCS) $(INDEX)

$(DOCS): $(SRC) $(STYLESHEETS)
	if test -n "$(SRC)"; then \
	    sh $(HERE)/make-ext-driver-toc.sh $(SRC) \
	        > $(THERE)/ext-driver-toc.xsl; \
	    xsltproc --html $(HERE)/plugpage.xsl \
	        $(subst $(THERE), $(HERE), $@) > $@; \
	fi

$(THERE)/ext-driver-toc.xsl: $(SRC)
	sh $(HERE)/make-ext-driver-toc.sh $(SRC) > $@

$(INDEX): $(HERE)/index.tmpl $(STYLESHEETS) $(SRC)
	if [ -n "$(SRC)" ]; then \
	    sh $(HERE)/make-ext-driver-toc.sh $(SRC) \
	        > $(THERE)/ext-driver-toc.xsl; \
	    xsltproc --html $(HERE)/plugpage.xsl $(HERE)/index.tmpl > $@; \
	else \
	    cp empty_index.tmpl $@; \
	fi
