## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS         = -I m4

CLEANFILES              = man/* html/* man-html/* driver-toc.xsl table.html \
                          api-docs.tar.gz NEWS.txt lirc.hwdb table.html \
                          doxygen*warning* ext-driver-toc.xsl

EXTRA_DIST              = Doxyfile manpage.xsl make-driver-toc.sh \
                          make_rel_symlink.py index.html plugindocs.mk

if DEVEL
website                 = http://lirc.sf.net/lirc.org
else
website                 = http://lirc.org
endif

LIB_SRC                 = $(srcdir)/../lib
API_SRC                 = $(LIB_SRC)/*.c \
                          $(LIB_SRC)/*.h \
                          $(LIB_SRC)/*.dox \
                          $(LIB_SRC)/*.doxhead

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

plugindocsdir           = $(docdir)/plugindocs
dist_plugindocs_DATA    = README $(pluginfiles)

website_htmldir         = $(docdir)/lirc.org/html
nodist_website_html_DATA  = $(generated_html) \
                          $(generated_plugindocs) \
                          NEWS.txt html-source/lirc.css
if HAVE_MAN2HTML
nodist_website_html_DATA  += $(manpage_html)
endif

if HAVE_PYMOD_YAML
nodist_website_html_DATA  += table.html
endif

vardocsdir              = $(localstatedir)/lib/lirc/plugins
vardocs_DATA            = index.html html-source/lirc.css

varimagedir             = $(localstatedir)/lib/lirc/images
varimage_DATA           = $(dist_image_DATA)

website_imagedir        = $(docdir)/lirc.org/images

imagedir                = $(docdir)/images

htmldir                 = $(docdir)/html

dist_html_DATA          = html-source/lirc.css
nodist_html_DATA        =

if HAVE_PYMOD_YAML
nodist_html_DATA        += table.html
endif

if HAVE_PYMOD_YAML
nodist_pkgdata_DATA     = lirc.hwdb
endif

dist_noinst_DATA        = $(man_sources) $(html_sources) $(plugindocs)


all: $(nodist_man_MANS) $(nodist_html_DATA)


install-data-hook:
if HAVE_DOXYGEN
	tar -C $(DESTDIR)$(websitedir) \
	    -xf $(DESTDIR)$(docdir)/api-docs.tar.gz && \
	        rm $(DESTDIR)$(docdir)/api-docs.tar.gz
	-mkdir -p $(DESTDIR)$(website_htmldir)
	ln -sf ../api-docs/html/index.html \
	    $(DESTDIR)$(website_htmldir)/api-docs
else
	ln -sf html/no-api-docs.html $(DESTDIR)$(websitedir)/api-docs
endif
	cd $(DESTDIR)$(websitedir)/api-docs; ln -sf html/index.html .
	$(SED) -i '/class="footer"/,/p>/d' $(DESTDIR)$(plugindocsdir)/page.xsl
	$(PYTHON) $(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)$(vardocsdir) $(DESTDIR)$(plugindocsdir)/var
	$(PYTHON) $(srcdir)/make_rel_symlink.py \
	    $(DESTDIR)$(vardocsdir)/index.html \
	    $(DESTDIR)$(website_htmldir)/plugins-index.html
	cp $(srcdir)/plugindocs.mk $(DESTDIR)$(plugindocsdir)/Makefile

clean-local:
	rm -rf api-docs/*
	rm -f lirc
	rm -f html/programs.html api-docs.tar.gz driver-toc.xsl

uninstall-hook:
	rm -rf $(DESTDIR)$(websitedir) $(DESTDIR)$(htmldir) \
            $(DESTDIR)$(mandir)/man4l $(DESTDIR)$(plugindocsdir)


$(abs_builddir)/driver-toc.xsl: $(plugindocs)
	$(srcdir)/make-driver-toc.sh $(plugindocs)> $@

$(srcdir)/driver-toc.xsl: $(plugindocs)
	$(srcdir)/make-driver-toc.sh $(plugindocs)> $@

$(srcdir)/empty_toc.tmpl: $(generated_html)
	cp html/empty_toc.html $@

elugins.index.html: $(srcdir)/empty_toc.tmpl
	cp -a $? $@

docpage.xsl: $(abs_builddir)/driver-toc.xsl

$(srcdir)/manpage.xsl: $(abs_builddir)/driver-toc.xsl

table.html: docpage.xsl
	test -e lirc || ln -s $(top_srcdir)/tools/lirc-setup lirc || :
	PYTHONPATH=.:$(top_srcdir)/tools \
	$(PYTHON) $(srcdir)/data2table $(top_srcdir)/configs ../configs \
	        | xsltproc --html docpage.xsl - > table.html

lirc.hwdb:
	test -e lirc || ln -s $(top_srcdir)/tools/lirc-setup lirc || :
	PYTHONPATH=.:$(top_srcdir)/tools \
	$(PYTHON) $(srcdir)/data2hwdb $(top_srcdir)/configs ../configs \
	    > lirc.hwdb

NEWS.txt: $(top_srcdir)/NEWS
	cp -a $(top_srcdir)/NEWS $@

$(generated_html): $(html_sources) docpage.xsl
	@mkdir html 2>/dev/null || :
	xsltproc --html docpage.xsl \
	    $$(echo $(srcdir)/$@ | $(SED) 's|html/|html-source/|') > $@
	$(SED) -i -e "s/@version@/$(VERSION)/" $@

$(generated_plugindocs): $(plugindocs) docpage.xsl
	@mkdir html 2>/dev/null || :
	xsltproc --html docpage.xsl \
	    $$(echo $(srcdir)/$@ | $(SED) 's|html/|plugindocs/|') > $@
	$(SED) -i "s/@version@/$(VERSION)/" $@


$(manpage_html): $(nodist_man_MANS) $(srcdir)/manpage.xsl
	@mkdir man-html 2>/dev/null || :
	src=$$(echo $(srcdir)/$@ | \
                   $(SED) -e 's/man-html/man/' -e 's/\.html//'); \
	    man2html -M index.html -r $${src}.[1-8] | \
		$(SED) \
	            -e '1,/^$$/d' \
		    -e '/This document was created/i \
                        <p>'  \
		    -e '/^Time:/a \
                        </p>'  \
		    -e '/HREF/s|".*man[1-9]/\(.*\)[0-9]\.html|"\1html|' | \
			 xsltproc --html $(srcdir)/manpage.xsl - | \
			    $(SED) -e '/href="\.\.\/index.html"/s|\.\./||' > $@

$(nodist_man_MANS): $(man_sources)
	@mkdir man 2>/dev/null || :
	src=$$(echo  $(srcdir)/$@ | $(SED) 's|/man/|/man-source/|'); \
	    $(SED) "s/@version@/$(VERSION)/" <$$src >$@
	@$(SED) -i 's|@prefixdir@|$(prefix)|' $@
	@$(SED) -i 's|@varrundir@|$(localstatedir)/run|' $@
	@$(SED) -i 's|@etcdir@|$(sysconfdir)|' $@
	@$(SED) -i 's|@libpath@|$(libdir)|' $@
	@$(SED) -i 's|@libpath@|$(libdir)|' $@
	@$(SED) -i 's|@website@|$(website)|' $@

api-docs.tar.gz: $(API_SRC)
	cd ../lib;  $(MAKE) dox
	doxygen $(srcdir)/Doxyfile >doxygen-warnings.txt 2>&1
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
                           $(srcdir)/man-source/irtext2udp.1 \
                           $(srcdir)/man-source/irw.1 \
                           $(srcdir)/man-source/lirc-config-tool.1 \
                           $(srcdir)/man-source/lirc-lsplugins.1 \
                           $(srcdir)/man-source/lirc-lsremotes.1 \
                           $(srcdir)/man-source/lirc-setup.1 \
                           $(srcdir)/man-source/lircd-setup.8 \
                           $(srcdir)/man-source/lircd.8 \
                           $(srcdir)/man-source/lircd.conf.5 \
                           $(srcdir)/man-source/lircmd.8 \
                           $(srcdir)/man-source/lircrcd.8 \
                           $(srcdir)/man-source/lircrc.5 \
                           $(srcdir)/man-source/mode2.1 \
                           $(srcdir)/man-source/pronto2lirc.1

if LINUX_KERNEL
man_sources             += $(srcdir)/man-source/irpipe.1 \
                           $(srcdir)/man-source/lirc-make-devinput.1
endif

if HAVE_UINPUT
man_sources             += $(srcdir)/man-source/lircd-uinput.8
endif

plugindocs              =  $(srcdir)/plugindocs/alsa-usb.html \
                           $(srcdir)/plugindocs/atilibusb.html \
                           $(srcdir)/plugindocs/atwf83.html \
                           $(srcdir)/plugindocs/audio-alsa.html \
                           $(srcdir)/plugindocs/audio.html \
                           $(srcdir)/plugindocs/default.html \
                           $(srcdir)/plugindocs/devinput.html \
                           $(srcdir)/plugindocs/file.html \
                           $(srcdir)/plugindocs/ftdi.html \
                           $(srcdir)/plugindocs/ftdix.html \
                           $(srcdir)/plugindocs/girs.html \
                           $(srcdir)/plugindocs/imon-24g.html \
                           $(srcdir)/plugindocs/imon.html \
                           $(srcdir)/plugindocs/irtoy.html \
                           $(srcdir)/plugindocs/srm7500atilibusb.html \
                           $(srcdir)/plugindocs/tira.html \
                           $(srcdir)/plugindocs/udp.html

pluginfiles             =  $(srcdir)/plugpage.xsl \
                           $(srcdir)/driver-toc.xsl \
                           $(srcdir)/empty_index.tmpl \
                           $(srcdir)/index.tmpl \
                           $(srcdir)/make-ext-driver-toc.sh \
                           $(srcdir)/page.xsl \
                           $(srcdir)/html-source/lirc.css

html_sources            =  $(srcdir)/html-source/api-general.html \
                           $(srcdir)/html-source/configuration-guide.html \
                           $(srcdir)/html-source/configure.html \
                           $(srcdir)/html-source/driver-api.html \
                           $(srcdir)/html-source/empty_toc.html \
                           $(srcdir)/html-source/help.html \
                           $(srcdir)/html-source/index.html \
                           $(srcdir)/html-source/install.html \
                           $(srcdir)/html-source/lirc_client.html \
                           $(srcdir)/html-source/no-api-docs.html \
                           $(srcdir)/html-source/programs.html \
                           $(srcdir)/html-source/programs-overview.html \
                           $(srcdir)/html-source/pronto2lirc.html \
                           $(srcdir)/html-source/technical.html

nodist_man_MANS         += man/ircat.1 \
                           man/irdb-get.1 \
                           man/irexec.1 \
                           man/irpty.1 \
                           man/irrecord.1 \
                           man/irsimreceive.1 \
                           man/irsimsend.1 \
                           man/irsend.1 \
                           man/irtext2udp.1 \
                           man/irtestcase.1 \
                           man/irw.1 \
                           man/lirc-config-tool.1 \
                           man/lirc-lsplugins.1 \
                           man/lirc-lsremotes.1 \
                           man/lirc-setup.1 \
                           man/lircd.8 \
                           man/lircd-setup.8 \
                           man/lircd.conf.5 \
                           man/lircmd.8 \
                           man/lircrcd.8 \
                           man/lircrc.5 \
                           man/mode2.1 \
                           man/pronto2lirc.1

if LINUX_KERNEL
nodist_man_MANS         += man/irpipe.1 \
                           man/lirc-make-devinput.1
endif

if HAVE_UINPUT
nodist_man_MANS         += man/lircd-uinput.8
endif


manpage_html            += man-html/ircat.html \
                           man-html/irdb-get.html \
                           man-html/irexec.html \
                           man-html/irpty.html \
                           man-html/irrecord.html \
                           man-html/irsimreceive.html \
                           man-html/irsimsend.html \
                           man-html/irsend.html \
                           man-html/irtestcase.html \
                           man-html/irtext2udp.html \
                           man-html/irw.html \
                           man-html/lirc-config-tool.html \
                           man-html/lirc.html \
                           man-html/lirc-lsplugins.html \
                           man-html/lirc-lsremotes.html \
                           man-html/lirc-setup.html \
                           man-html/lircd-setup.html \
                           man-html/lircd.html \
                           man-html/lircd.conf.html \
                           man-html/lircmd.html \
                           man-html/lircrcd.html \
                           man-html/lircrc.html \
                           man-html/mode2.html \
                           man-html/pronto2lirc.html

if LINUX_KERNEL
manpage_html            += man-html/irpipe.html \
                           man-html/lirc-make-devinput.html
endif

if HAVE_UINPUT
manpage_html            += man-html/lircd-uinput.html
endif

generated_plugindocs    =  html/alsa-usb.html \
                           html/atilibusb.html \
                           html/atwf83.html \
                           html/audio-alsa.html \
                           html/audio.html \
                           html/default.html \
                           html/devinput.html \
                           html/file.html \
                           html/ftdi.html \
                           html/ftdix.html \
                           html/girs.html \
                           html/imon-24g.html \
                           html/imon.html \
                           html/irtoy.html \
                           html/srm7500atilibusb.html \
                           html/tira.html \
                           html/udp.html

generated_html          =  html/api-general.html \
                           html/configuration-guide.html \
                           html/configure.html \
                           html/driver-api.html \
                           html/empty_toc.html \
                           html/help.html \
                           html/index.html \
                           html/install.html \
                           html/lirc_client.html \
                           html/no-api-docs.html \
                           html/programs.html \
                           html/programs-overview.html \
                           html/pronto2lirc.html \
                           html/technical.html

dist_image_DATA         =  lirc.org/images/atwf83.jpg \
                           lirc.org/images/marb18.jpg \
                           lirc.org/images/diode.gif \
                           lirc.org/images/lirc.gif

website_files           =  lirc.org/audio-alsa.html \
                           lirc.org/cir.html \
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
                           lirc.org/PCB.pdf \
                           lirc.org/pictures1.html \
                           lirc.org/pictures2.html \
                           lirc.org/pictures.html \
                           lirc.org/pinout_TRM1038.txt \
                           lirc.org/receivers.html \
                           lirc.org/robots.txt \
                           lirc.org/software.html \
                           lirc.org/transmitters.html \
                           lirc.org/tv_cards.html

dist_website_image_DATA =  lirc.org/favicon.ico \
                           lirc.org/images/atwf83.jpg \
                           lirc.org/images/audio_transmitter.png \
                           lirc.org/images/audio_wave.png \
                           lirc.org/images/banner_en_1.gif \
                           lirc.org/images/better_transmitter.gif \
                           lirc.org/images/d.jpg \
                           lirc.org/images/diode.gif \
                           lirc.org/images/front.jpg \
                           lirc.org/images/improved_transmitter.gif \
                           lirc.org/images/ir-audio.png \
                           lirc.org/images/irrcv_board.gif \
                           lirc.org/images/IR-mouse1.jpg \
                           lirc.org/images/IR-mouse2.jpg \
                           lirc.org/images/lirc.gif \
                           lirc.org/images/lirc1.jpg \
                           lirc.org/images/lirc2.jpg \
                           lirc.org/images/lirc3.jpg \
                           lirc.org/images/lirclogo.gif \
                           lirc.org/images/marb18.jpg \
                           lirc.org/images/schematics.gif \
                           lirc.org/images/screenshot.gif \
                           lirc.org/images/screenshot.jpg \
                           lirc.org/images/screenshot1.gif \
                           lirc.org/images/simple_transmitter.gif \
                           lirc.org/images/top3.jpg \
                           lirc.org/images/tsop173x.png
