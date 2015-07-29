## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

DISTCLEANFILES          = man/* html/* man-html/* driver-toc.xsl table.html

EXTRA_DIST              = Doxyfile make-driver-toc.sh \
                          manpage.xsl docpage.xsl docpage.xsl  page.xsl

LIB_SRC                 = $(srcdir)/../lib
API_SRC                 = $(LIB_SRC)/*.c \
                          $(LIB_SRC)/*.h \
                          $(LIB_SRC)/*.dox \
                          $(LIB_SRC)/*.doxhead

install-data-hook:
	ln -s ../images $(DESTDIR)/$(htmldir)/images

dist_noinst_SCRIPTS     = data2table data2hwdb

manpage_html            =
man_sources             =
nodist_man_MANS         =

if BUILD_XTOOLS
man_sources             += $(srcdir)/man-source/xmode2.1 \
                           $(srcdir)/man-source/irxevent.1
nodist_man_MANS         += man/xmode2.1 man/irxevent.1
manpage_html            += man-html/irxevent.html man-html/xmode2.html
endif

docdir                  = @datadir@/doc/@PACKAGE@
dist_doc_DATA           = irxevent.keys
if HAVE_DOXYGEN
dist_doc_DATA           += api-docs.tar.gz
endif

websitedir              = $(docdir)/lirc.org
dist_website_DATA       = $(website_files)

website_htmldir         = $(docdir)/lirc.org/html
dist_website_html_DATA  = $(generated_html) \
                          $(manpage_html) \
                          html-source/lirc.css \
                          lirc.org/parallel.html \
                          lirc.org/receivers.html \
                          lirc.org/transmitters.html

if HAVE_PYMOD_YAML
dist_website_html_DATA  += table.html
endif

website_imagedir        = $(docdir)/lirc.org/images
dist_website_image_DATA = $(dist_image_DATA)

htmldir                 = $(docdir)/html
nodist_html_DATA        = $(generated_html) \
                          lirc.org/parallel.html \
                          lirc.org/receivers.html \
                          lirc.org/transmitters.html
if HAVE_MAN2HTML
nodist_html_DATA        += $(manpage_html)
endif

dist_html_DATA          =  html-source/lirc.css

if HAVE_PYMOD_YAML
dist_html_DATA          += table.html
endif

if HAVE_PYMOD_YAML
dist_pkgdata_DATA       = lirc.hwdb
endif

dist_noinst_DATA        = $(man_sources) $(html_sources)

imagedir                = $(docdir)/images
dist_image_DATA         = images/atwf83.jpg \
                          images/audio_transmitter.png \
                          images/audio_wave.png \
                          images/diode.gif \
                          images/lirc.gif \
                          images/marb18.jpg \
			  images/banner_en_1.gif


all: $(nodist_man_MANS) $(nodist_html_DATA)

clean-local:
	rm -rf $(DISTCLEANFILES)

table.html: driver-toc.xsl
	$(PYTHON) $(srcdir)/data2table  | \
	    xsltproc --html docpage.xsl - > table.html

lirc.hwdb:
	$(PYTHON) $(srcdir)/data2hwdb > lirc.hwdb

driver-toc.xsl: .phony
	$(srcdir)/make-driver-toc.sh > $@

$(generated_html): $(html_sources) $(srcdir)/docpage.xsl driver-toc.xsl
	mkdir html 2>/dev/null || :
	xsltproc --html docpage.xsl \
	    $$(echo $(srcdir)/$@ | $(SED) 's|html/|html-source/|') > $@
	$(SED) -i "s/@version@/$(VERSION)/" $@

$(manpage_html): $(nodist_man_MANS) $(srcdir)/manpage.xsl
	-test -d man-html || mkdir man-html
	src=$$(echo $(srcdir)/$@ | \
                   $(SED) -e 's/man-html/man-source/' -e 's/\.html//'); \
	    man2html -M ../html/index.html $${src}.[1-8] | \
	        sed '1,/^$$/d' | xsltproc --html $(srcdir)/manpage.xsl - > $@

$(nodist_man_MANS): $(man_sources)
	-test -d man || mkdir man
	src=$$(echo  $(srcdir)/$@ | $(SED) 's|/man/|/man-source/|'); \
	    cp $$src $@
	$(SED) -i "s/@version@/$(VERSION)/" $@

api-docs.tar.gz: $(API_SRC)
	cd ../lib;  $(MAKE) dox
	doxygen >doxygen-warnings.txt 2>&1
	find api-docs -name \*.tmp -delete
	tar czf $@ api-docs

.phony:

man_sources             += $(srcdir)/man-source/ircat.1 \
                           $(srcdir)/man-source/irdb-get.1 \
                           $(srcdir)/man-source/irexec.1 \
                           $(srcdir)/man-source/irpty.1 \
                           $(srcdir)/man-source/irrecord.1 \
                           $(srcdir)/man-source/irsimreceive.1 \
                           $(srcdir)/man-source/irsimsend.1 \
                           $(srcdir)/man-source/irsend.1 \
                           $(srcdir)/man-source/irtestcase.1 \
                           $(srcdir)/man-source/irw.1 \
                           $(srcdir)/man-source/lirc-config-tool.1 \
                           $(srcdir)/man-source/lirc.4 \
                           $(srcdir)/man-source/lirc_ioctl.4 \
                           $(srcdir)/man-source/lirc-lsplugins.1 \
                           $(srcdir)/man-source/lirc-lsremotes.1 \
                           $(srcdir)/man-source/lirc-setup.1 \
                           $(srcdir)/man-source/lircd.8 \
                           $(srcdir)/man-source/lircd.conf.5 \
                           $(srcdir)/man-source/lircmd.8 \
                           $(srcdir)/man-source/lircrcd.8 \
                           $(srcdir)/man-source/mode2.1

html_sources            =  $(srcdir)/html-source/alsa-usb_driver.html \
                           $(srcdir)/html-source/api-general.html \
                           $(srcdir)/html-source/atilibusb_driver.html \
                           $(srcdir)/html-source/atwf83_driver.html \
                           $(srcdir)/html-source/audio-alsa_driver.html \
                           $(srcdir)/html-source/audio_driver.html \
                           $(srcdir)/html-source/configuration-guide.html \
                           $(srcdir)/html-source/configure.html \
                           $(srcdir)/html-source/default_driver.html \
                           $(srcdir)/html-source/devinput_driver.html \
                           $(srcdir)/html-source/driver-api.html \
                           $(srcdir)/html-source/file_driver.html \
                           $(srcdir)/html-source/help.html \
                           $(srcdir)/html-source/imon-24g_driver.html \
                           $(srcdir)/html-source/imon_driver.html \
                           $(srcdir)/html-source/index.html \
                           $(srcdir)/html-source/install.html \
                           $(srcdir)/html-source/irtoy_driver.html \
                           $(srcdir)/html-source/lirc_client.html \
                           $(srcdir)/html-source/programs.html \
                           $(srcdir)/html-source/pronto2lirc.html \
                           $(srcdir)/html-source/srm7500atilibusb_driver.html \
                           $(srcdir)/html-source/technical.html \
                           $(srcdir)/html-source/tira_driver.html \
                           $(srcdir)/html-source/udp_driver.html

nodist_man_MANS         += man/ircat.1 \
                           man/irdb-get.1 \
                           man/irexec.1 \
                           man/irpty.1 \
                           man/irrecord.1 \
                           man/irsimreceive.1 \
                           man/irsimsend.1 \
                           man/irsend.1 \
                           man/irtestcase.1 \
                           man/irw.1 \
                           man/lirc-config-tool.1 \
                           man/lirc.4 \
                           man/lirc_ioctl.4 \
                           man/lirc-lsplugins.1 \
                           man/lirc-lsremotes.1 \
                           man/lirc-setup.1 \
                           man/lircd.8 \
                           man/lircd.conf.5 \
                           man/lircmd.8 \
                           man/lircrcd.8 \
                           man/mode2.1

manpage_html            += man-html/ircat.html \
                           man-html/irdb-get.html \
                           man-html/irexec.html \
                           man-html/irpty.html \
                           man-html/irrecord.html \
                           man-html/irsimsend.html \
                           man-html/irsend.html \
                           man-html/irtestcase.html \
                           man-html/irw.html \
                           man-html/lirc-config-tool.html \
                           man-html/lirc.html \
                           man-html/lirc_ioctl.html \
                           man-html/lirc-lsplugins.html \
                           man-html/lirc-lsremotes.html \
                           man-html/lirc-setup.html \
                           man-html/lircd.html \
                           man-html/lircd.conf.html \
                           man-html/lircmd.html \
                           man-html/lircrcd.html \
                           man-html/mode2.html

generated_html          =  html/alsa-usb_driver.html \
                           html/audio-alsa_driver.html \
                           html/atilibusb_driver.html \
                           html/atwf83_driver.html \
                           html/audio_driver.html \
                           html/configuration-guide.html \
                           html/configure.html \
                           html/default_driver.html \
                           html/devinput_driver.html \
                           html/driver-api.html \
                           html/file_driver.html \
                           html/help.html \
                           html/imon-24g_driver.html \
                           html/imon_driver.html \
                           html/index.html \
                           html/install.html \
                           html/irtoy_driver.html \
                           html/lirc_client.html \
                           html/programs.html \
                           html/pronto2lirc.html \
                           html/srm7500atilibusb_driver.html \
                           html/technical.html \
                           html/tira_driver.html \
                           html/udp_driver.html

website_files           =  lirc.org/cir.html \
                           lirc.org/cvs.html \
                           lirc.org/developers.html \
                           lirc.org/faq.html \
                           lirc.org/favicon.ico \
                           lirc.org/git.html \
                           lirc.org/improved_transmitter.html \
                           lirc.org/index.html \
                           lirc.org/ir-audio.html \
                           lirc.org/irda.html \
                           lirc.org/lirc.css \
                           lirc.org/mirrors.html \
                           lirc.org/parallel.html \
                           lirc.org/pictures1.html \
                           lirc.org/pictures2.html \
                           lirc.org/pictures.html \
                           lirc.org/pinout_TRM1038.txt \
                           lirc.org/receivers.html \
                           lirc.org/robots.txt \
                           lirc.org/software.html \
                           lirc.org/transmitters.html \
                           lirc.org/tv_cards.html
