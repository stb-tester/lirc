## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

EXTRA_DIST              = release-html.sh release-man.sh table.sh
DISTCLEANFILES          = man/* html/* man-html/* lirc.hwdb
noinst_PROGRAMS         = man2html

nodist_man_MANS         =
generated_html          =

nodist_man_MANS         += man/xmode2.1 man/irxevent.1
generated_html          += html/xmode2.html html/irxevent.html

nodist_man_MANS         += man/smode2.1
generated_html          += html/smode2.html

docdir                  = @datadir@/doc/@PACKAGE@
dist_doc_DATA           = lirc.css irxevent.keys

htmldir                 = $(docdir)/html
nodist_html_DATA        = $(generated_html)
dist_html_DATA          = $(html_includes) html-source/lirc.css

dist_noinst_DATA        = $(man_sources) $(html_sources)

clean-local:
	rm -rf html
	rm -rf man

all: $(nodist_man_MANS) $(nodist_html_DATA) lirc.hwdb

$(generated_html): $(nodist_man_MANS)

$(nodist_man_MANS): $(man_sources) man2html
	test -d man || mkdir man
	top_srcdir="$(top_srcdir)" srcdir="$(srcdir)" \
	        builddir="$(builddir)"  top_builddir="$(top_builddir)" \
	        $(srcdir)/release-man.sh $(nodist_man_MANS)

$(nodist_html_DATA) : $(html_sources) $(nodist_man_MANS)
	test -d html || mkdir html
	top_srcdir="$(top_srcdir)" srcdir="$(srcdir)" \
	    builddir="$(builddir)" top_builddir="$(top_builddir)" \
	    $(srcdir)/release-html.sh
	cp $(srcdir)/html-source/lirc.css html

lirc.hwdb: html/table.html
html/table.html:
	test -d html || mkdir html
	top_srcdir="$(top_srcdir)" srcdir="$(srcdir)" \
	        builddir="$(builddir)" top_builddir="$(top_builddir)" \
	        $(srcdir)/table.sh lirc.hwdb > html/table.html


imagedir                = $(docdir)/images
dist_image_DATA         =  images/atwf83.jpg \
                           images/audio_transmitter.png \
                           images/audio_wave.png \
                           images/diode.gif \
                           images/lirc.gif \
                           images/marb18.jpg

man_sources             =  man-source/daemons.inc \
                           man-source/irexec.inc \
                           man-source/irw.inc \
                           man-source/lircrcd.inc \
                           man-source/irpty.inc \
                           man-source/irxevent.inc \
                           man-source/mode2-common.inc \
                           man-source/help2man.inc \
                           man-source/irrecord.inc \
                           man-source/lircd.inc \
                           man-source/mode2.inc \
                           man-source/smode2.inc \
                           man-source/ircat.inc \
                           man-source/irsend.inc \
                           man-source/lircmd.inc \
                           man-source/xmode2.inc

html_sources            =  html-source/alsa-usb.html \
                           html-source/atwf83.html \
                           html-source/audio-alsa.html \
                           html-source/audio.html \
                           html-source/configuration-guide.html \
                           html-source/configure.html \
                           html-source/devinput.html \
                           html-source/help.html \
                           html-source/imon-24g.html \
                           html-source/imon.html \
                           html-source/index.html \
                           html-source/install.html \
                           html-source/programs.html \
                           html-source/pronto2lirc.html \
                           html-source/technical.html \
                           html-source/tira.html

html_includes           =  html-source/foot.html html-source/head.html

nodist_man_MANS         += man/irexec.1 \
                           man/ircat.1 \
                           man/irpty.1 \
                           man/irrecord.1 \
                           man/irw.1 \
                           man/lircd.8 \
                           man/lircmd.8 \
                           man/lircrcd.1 \
                           man/mode2.1 \
                           man/irsend.1

generated_html          += html/alsa-usb.html \
                           html/audio-alsa.html \
                           html/audio.html \
                           html/configuration-guide.html \
                           html/configure.html \
                           html/devinput.html \
                           html/help.html \
                           html/imon-24g.html \
                           html/imon.html \
                           html/index.html \
                           html/install.html \
                           html/ircat.html \
                           html/irexec.html \
                           html/irpty.html \
                           html/irrecord.html \
                           html/irsend.html \
                           html/irw.html \
                           html/lircd.html \
                           html/lircmd.html \
                           html/lircrcd.html \
                           html/mode2.html \
                           html/programs.html \
                           html/pronto2lirc.html \
                           html/table.html \
                           html/technical.html \
                           html/tira.html
