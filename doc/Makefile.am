## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

DISTCLEANFILES          = man/* html/* man-html/*

dist_noinst_SCRIPTS     = data2table data2hwdb

nodist_man_MANS         =
generated_html          =
manpage_html            =
nodist_html_DATA        =

if BUILD_XTOOLS
nodist_man_MANS         += man/xmode2.1 man/irxevent.1
manpage_html            += man-html/irxevent.html
nodist_html_DATA        += man-html/xmode2.html
endif

docdir                  = @datadir@/doc/@PACKAGE@
dist_doc_DATA           = lirc.css irxevent.keys

htmldir                 = $(docdir)/html
nodist_html_DATA       += $(generated_html) $(manpage_html)
dist_html_DATA          = $(html_includes) html-source/lirc.css table.html

dist_noinst_DATA        = $(man_sources) $(html_sources)
dist_pkgdata_DATA       = lirc.hwdb

imagedir                = $(docdir)/images
dist_image_DATA         = images/atwf83.jpg \
                          images/audio_transmitter.png \
                          images/audio_wave.png \
                          images/diode.gif \
                          images/lirc.gif \
                          images/marb18.jpg


all: $(nodist_man_MANS) $(nodist_html_DATA)

clean-local:
	rm -rf html
	rm -rf man
	rm -rf man-html

table.html:
	$(PYTHON) $(srcdir)/data2table > table.html

lirc.hwdb:
	$(PYTHON) $(srcdir)/data2hwdb > lirc.hwdb

$(generated_html): $(html_sources)
	test -d html || mkdir html
	cat $(srcdir)/html-source/head.html > $@
	cat $$(echo $(srcdir)/$@ | $(SED) 's|html/|html-source/|') >> $@
	cat $(srcdir)/html-source/foot.html >> $@
	$(SED) -i "s/@version@/$(VERSION)/" $@

$(manpage_html): $(nodist_man_MANS)
	test -d man-html || mkdir man-html
	cat $(srcdir)/html-source/head.html > $@
	src=$$(echo $(srcdir)/$@ | \
                   $(SED) -e 's/man-html/man-source/' -e 's/\.html/.1/'); \
	man2html -M ../html/index.html $${src} | \
	            $(SED) -e '1,/<BODY>/d' \
	                -e '/<\/BODY>/,$$d' \
	                -e '/HREF="#index"/d' \
	                -e '/NAME="index"/,/^Time:/d' \
	                -e '/SEE ALSO/,/^<HR>/d' >> $@

	cat  $(srcdir)/html-source/foot.html >> $@

$(nodist_man_MANS): $(man_sources)
	-test -d man || mkdir man
	cp -a $(man_sources) man
	$(SED) -i "s/@version@/$(VERSION)/" man/*
	cd man;  cp mode2.1 xmode2.1

man-html/xmode2.html: $(manpage_html)
	cp man-html/mode2.html $@


man_sources             =  $(srcdir)/man-source/ircat.1 \
                           $(srcdir)/man-source/irexec.1 \
                           $(srcdir)/man-source/irpty.1 \
                           $(srcdir)/man-source/irrecord.1 \
                           $(srcdir)/man-source/irsend.1 \
                           $(srcdir)/man-source/irw.1 \
                           $(srcdir)/man-source/irxevent.1 \
                           $(srcdir)/man-source/lirc-lsplugins.1 \
                           $(srcdir)/man-source/lirc-setup.1 \
                           $(srcdir)/man-source/lircd.8 \
                           $(srcdir)/man-source/lircd.conf.5 \
                           $(srcdir)/man-source/lircmd.8 \
                           $(srcdir)/man-source/lircrcd.1 \
                           $(srcdir)/man-source/mode2.1

html_sources            =  $(srcdir)/html-source/alsa-usb.html \
                           $(srcdir)/html-source/atwf83.html \
                           $(srcdir)/html-source/audio-alsa.html \
                           $(srcdir)/html-source/audio.html \
                           $(srcdir)/html-source/configuration-guide.html \
                           $(srcdir)/html-source/configure.html \
                           $(srcdir)/html-source/devinput.html \
                           $(srcdir)/html-source/driver-api.html \
                           $(srcdir)/html-source/help.html \
                           $(srcdir)/html-source/imon-24g.html \
                           $(srcdir)/html-source/imon.html \
                           $(srcdir)/html-source/index.html \
                           $(srcdir)/html-source/install.html \
                           $(srcdir)/html-source/programs.html \
                           $(srcdir)/html-source/pronto2lirc.html \
                           $(srcdir)/html-source/technical.html \
                           $(srcdir)/html-source/tira.html

html_includes           =  $(srcdir)/html-source/foot.html \
                           $(srcdir)/html-source/head.html

nodist_man_MANS         += man/ircat.1 \
                           man/irexec.1 \
                           man/irpty.1 \
                           man/irrecord.1 \
                           man/irsend.1 \
                           man/irw.1 \
                           man/lirc-lsplugins.1 \
                           man/lirc-setup.1 \
                           man/lircd.8 \
                           man/lircd.conf.5 \
                           man/lircmd.8 \
                           man/lircrcd.1 \
                           man/mode2.1

manpage_html            += man-html/ircat.html \
                           man-html/irexec.html \
                           man-html/irpty.html \
                           man-html/irrecord.html \
                           man-html/irsend.html \
                           man-html/irw.html \
                           man-html/lirc-setup.html \
                           man-html/lircd.html \
                           man-html/lircd.conf.html \
                           man-html/lircmd.html \
                           man-html/lircrcd.html \
                           man-html/mode2.html

generated_html          += html/alsa-usb.html \
                           html/audio-alsa.html \
                           html/audio.html \
                           html/configuration-guide.html \
                           html/configure.html \
                           html/devinput.html \
                           html/driver-api.html \
                           html/help.html \
                           html/imon-24g.html \
                           html/imon.html \
                           html/index.html \
                           html/install.html \
                           html/programs.html \
                           html/pronto2lirc.html \
                           html/technical.html \
                           html/tira.html
