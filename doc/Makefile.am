## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

DISTCLEANFILES          = man/* html/* man-html/*

dist_noinst_SCRIPTS     = data2table data2hwdb

nodist_man_MANS         =
generated_html          =

if BUILD_XTOOLS
nodist_man_MANS         += man/xmode2.1 man/irxevent.1
generated_html          += man-html/xmode2.html man-html/irxevent.html
endif

if BUILD_SMODE2
nodist_man_MANS         += man/smode2.1
generated_html          += man-html/smode2.html
endif

docdir                  = @datadir@/doc/@PACKAGE@
dist_doc_DATA           = lirc.css irxevent.keys

htmldir                 = $(docdir)/html
nodist_html_DATA        = $(generated_html) $(manpage_html)
dist_html_DATA          = $(html_includes) html-source/lirc.css table.html

dist_noinst_DATA        = $(man_sources) $(html_sources)
dist_pkgdata_DATA       = lirc.hwdb


all: $(nodist_man_MANS) $(nodist_html_DATA)

clean-local:
	rm -rf html
	rm -rf man
	rm -rf man-html

table.html:
	src="$(sourcedir)"; $${src:-.}/data2table > table.html

lirc.hwdb:
	src="$(sourcedir)"; $${src:-.}/data2hwdb > lirc.hwdb

$(manpage_html): $(nodist_man_MANS)
	test -d man-html || mkdir man-html
	for f in  $(nodist_man_MANS); do \
	    dest=man-html/$$(basename $$f); \
	    dest=$${dest%.*}.html; \
	    cat $(srcdir)/html-source/head.html > $$dest; \
	    man2html -M ../html/index.html $$f |  \
	        sed -e '1,/<BODY>/d' \
	            -e '/<\/BODY>/,$$d' \
	            -e '/HREF="#index"/d' \
	            -e '/NAME="index"/,/^Time:/d' \
	            -e '/SEE ALSO/,/^<HR>/d' >>$$dest;\
	    cat  $(srcdir)/html-source/foot.html >> $$dest; \
	done

$(generated_html): $(html_sources)
	test -d html || mkdir html
	for f in $(generated_html); do \
	    src=html-source/$$(basename $$f); \
	    test -f $$src || continue; \
	    cat $(srcdir)/html-source/head.html > $$f; \
	    cat $$src >> $$f; \
	    cat  $(srcdir)/html-source/foot.html >> $$f; \
	    sed -i "s/@version@/$(VERSION)/" $$f; \
	done

$(nodist_man_MANS): $(man_sources)
	-test -d man || mkdir man
	cp -a $(man_sources) man
	sed -i "s/@version@/$(VERSION)/" man/*
	cd man; cp mode2.1 smode2.1; cp mode2.1 xmode2.1


imagedir                = $(docdir)/images
dist_image_DATA         =  images/atwf83.jpg \
                           images/audio_transmitter.png \
                           images/audio_wave.png \
                           images/diode.gif \
                           images/lirc.gif \
                           images/marb18.jpg

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

manpage_html            =  man-html/ircat.html \
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

generated_html          += $(srcdir)/html/alsa-usb.html \
                           $(srcdir)/html/audio-alsa.html \
                           $(srcdir)/html/audio.html \
                           $(srcdir)/html/configuration-guide.html \
                           $(srcdir)/html/configure.html \
                           $(srcdir)/html/devinput.html \
                           $(srcdir)/html/driver-api.html \
                           $(srcdir)/html/help.html \
                           $(srcdir)/html/imon-24g.html \
                           $(srcdir)/html/imon.html \
                           $(srcdir)/html/index.html \
                           $(srcdir)/html/install.html \
                           $(srcdir)/html/programs.html \
                           $(srcdir)/html/pronto2lirc.html \
                           $(srcdir)/html/technical.html \
                           $(srcdir)/html/tira.html
