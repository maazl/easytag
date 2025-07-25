ACLOCAL_AMFLAGS = -I m4

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--enable-appdata-check \
	--enable-flac \
	--enable-id3v23 \
	--enable-ogg \
	--enable-opus \
	--enable-man \
	--enable-mp3 \
	--enable-mp4 \
	--enable-nautilus-actions \
	--enable-asf \
	--enable-replaygain \
	--enable-speex \
	--enable-tests \
	--enable-wavpack
 
SUBDIRS = help po

bin_PROGRAMS = easytag

easytag_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/tags \
	-I$(top_builddir) \
	-I$(top_builddir)/src \
	-DLOCALEDIR=\"$(localedir)\" \
	$(GLIB_DEPRECATION_FLAGS) \
	$(GTK_DEPRECATION_FLAGS) \
	$(SOUP_DEPRECATION_FLAGS)

easytag_CFLAGS = \
	$(WARN_CFLAGS) \
	$(EASYTAG_CFLAGS)

easytag_CXXFLAGS = \
	$(WARN_CXXFLAGS) \
	$(EASYTAG_CFLAGS)

easytag_SOURCES = \
	src/about.cc \
	src/application.cc \
	src/application_window.cc \
	src/browser.cc \
	src/cddb_dialog.cc \
	src/charset.cc \
	src/crc32.cc \
	src/dlm.c \
	src/easytag.cc \
	src/enums.c \
	src/etflagsaction.cc \
	src/file.cc \
	src/file_area.cc \
	src/file_description.cc \
	src/file_list.cc \
	src/file_name.cc \
	src/file_renderer.cc \
	src/file_tag.cc \
	src/genres.c \
	src/load_files_dialog.cc \
	src/log.cc \
	src/main.cc \
	src/mask.cc \
	src/misc.cc \
	src/picture.cc \
	src/playlist_dialog.cc \
	src/preferences_dialog.cc \
	src/progress_bar.cc \
	src/replaygain.cc \
	src/scan.cc \
	src/scan_dialog.cc \
	src/search_dialog.cc \
	src/setting.cc \
	src/status_bar.cc \
	src/tag_area.cc \
	src/tags/libapetag/apetaglib.cc \
	src/tags/libapetag/is_tag.cc \
	src/tags/libapetag/info_mac.cc \
	src/tags/libapetag/info_mpc.cc \
	src/tags/ape_tag.cc \
	src/tags/asf_tag.cc \
	src/tags/flac_private.c \
	src/tags/flac_tag.cc \
	src/tags/gio_wrapper.cc \
	src/tags/id3_tag.cc \
	src/tags/id3v24_tag.cc \
	src/tags/mp4_tag.cc \
	src/tags/ogg_tag.cc \
	src/tags/opus_tag.cc \
	src/tags/taglib_base.cc \
	src/tags/vcedit.cc \
	src/tags/wavpack_private.c \
	src/tags/wavpack_tag.cc \
	src/win32/win32dep.c \
	src/xptr.cc \
	src/xstring.cc

nodist_easytag_SOURCES = \
	src/resource.c

easytag_headers = \
	src/about.h \
	src/application.h \
	src/application_window.h \
	src/browser.h \
	src/cddb_dialog.h \
	src/charset.h \
	src/crc32.h \
	src/dlm.h \
	src/easytag.h \
	src/etflagsaction.h \
	src/file.h \
	src/file_area.h \
	src/file_description.h \
	src/file_list.h \
	src/file_name.h \
	src/file_renderer.h \
	src/file_tag.h \
	src/genres.h \
	src/load_files_dialog.h \
	src/log.h \
	src/misc.h \
	src/picture.h \
	src/playlist_dialog.h \
	src/preferences_dialog.h \
	src/progress_bar.h \
	src/replaygain.h \
	src/scan.h \
	src/scan_dialog.h \
	src/search_dialog.h \
	src/setting.h \
	src/status_bar.h \
	src/tag_area.h \
	src/tags/libapetag/apetaglib.h \
	src/tags/libapetag/is_tag.h \
	src/tags/libapetag/info_mac.h \
	src/tags/libapetag/info_mpc.h \
	src/tags/ape_tag.h \
	src/tags/asf_tag.h \
	src/tags/flac_private.h \
	src/tags/flac_tag.h \
	src/tags/gio_wrapper.h \
	src/tags/id3_tag.h \
	src/tags/mp4_tag.h \
	src/tags/ogg_tag.h \
	src/tags/opus_tag.h \
	src/tags/taglib_base.h \
	src/tags/vcedit.h \
	src/tags/wavpack_private.h \
	src/tags/wavpack_tag.h \
	src/undo_list.h \
	src/win32/win32dep.h \
	src/xptr.h \
	src/xstring.h

nodist_easytag_headers = \
	src/resource.h

easytag_LDADD = \
	$(EASYTAG_LIBS) \
	$(ID3LIB_LIBS) \
	$(easytag_rc)

easytag_LDFLAGS = \
	$(WARN_LDFLAGS)

noinst_resource_files = \
	$(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir=$(srcdir)/data $(srcdir)/data/org.gnome.EasyTAG.gresource.xml)

src/resource.c src/resource.h: data/org.gnome.EasyTAG.gresource.xml Makefile $(noinst_resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir)/data --generate $<

if WINDOWS_HOST
ICONDIR = $(srcdir)/data/icons/win32
MANIFESTDIR = $(srcdir)/src/win32

easytag_rc.$(OBJEXT): src/win32/easytag.rc src/win32/easytag.manifest data/icons/win32/easytag.ico
	$(AM_V_GEN)$(WINDRES) -DICONDIR='\"$(ICONDIR)\"' \
		-DMANIFESTDIR='\"$(MANIFESTDIR)\"' -i $< -o $@

easytag_rc = easytag_rc.$(OBJEXT)

easytag_LDFLAGS += \
	-Wl,--subsystem,windows

easytag-$(PACKAGE_VERSION)-setup.exe: easytag-win32-installer.nsi easytag.exe Makefile
	$(AM_V_GEN) DESTDIR=`mktemp -d` && \
		make -C $(top_builddir) install DESTDIR=$$DESTDIR >/dev/null && \
		makensis -NOCD -DDESTDIR=$$DESTDIR $< >/dev/null && \
		rm -rf $$DESTDIR
endif

BUILT_SOURCES = \
	$(nodist_easytag_SOURCES) \
	src/enums.h

enum_data = \
	src/enums.c \
	src/enums.h

easytag_enum_headers = \
	$(srcdir)/src/setting.h

src/enums.c: $(easytag_enum_headers) src/enums.h Makefile
	$(AM_V_GEN)$(GLIB_MKENUMS) \
		--fhead "#include \"enums.h\"\n\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--fprod "#include \"@filename@\"\n" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n" \
		--vhead "    static GType type = 0;\n\n" \
		--vhead "    if (!type)\n    {\n" \
		--vhead "        static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "            { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "            { 0, NULL, NULL }\n        };\n\n" \
		--vtail "        type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n    }\n\n" \
		--vtail "  return type;\n}\n\n" \
		$(easytag_enum_headers) > $@

src/enums.h: $(easytag_enum_headers) Makefile
	$(AM_V_GEN)$(GLIB_MKENUMS) \
		--fhead "#ifndef ET_ENUMS_H_\n" \
		--fhead "#define ET_ENUMS_H_\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--fprod "/* enumerations from @filename@ */\n" \
		--vhead "GType @enum_name@_get_type (void);\n" \
		--vhead "#define ET_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* ET_ENUMS_H_ */" \
		$(easytag_enum_headers) > $@

# NSIS installer script.
# TODO: Run script from dist dir.
easytag-win32-installer.nsi: easytag-win32-installer.nsi.in Makefile
	$(AM_V_GEN)$(SED) -e "s|\@PACKAGE_NAME\@|$(PACKAGE_NAME)|" \
		-e "s|\@PACKAGE_TARNAME\@|$(PACKAGE_TARNAME)|" \
		-e "s|\@PACKAGE_URL\@|$(PACKAGE_URL)|" \
		-e "s|\@PACKAGE_VERSION\@|$(PACKAGE_VERSION)|" \
		-e "s|\@PREFIX\@|$(prefix)|" $< > $@

if ENABLE_NAUTILUS_ACTIONS
nautilus_extensiondir = $(NAUTILUS_EXTENSION_DIR)

nautilus_extension_LTLIBRARIES = \
	libnautilus-easytag.la

libnautilus_easytag_la_SOURCES = \
	nautilus-extension/nautilus-easytag.c

libnautilus_easytag_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DLOCALEDIR=\"$(localedir)\" \
	$(NAUTILUS_CFLAGS) \
	$(GLIB_DEPRECATION_FLAGS) \
	$(GTK_DEPRECATION_FLAGS)

libnautilus_easytag_la_CFLAGS = \
	$(WARN_CFLAGS)

libnautilus_easytag_la_LDFLAGS = \
	-module -avoid-version -no-undefined \
	$(WARN_LDFLAGS)

libnautilus_easytag_la_LIBADD = \
	$(NAUTILUS_LIBS)

nautilus_appdata_in_file = data/easytag-nautilus.metainfo.xml.in
endif

# Create a directory stamp in the build dir. Used to workaround the case where
# a pattern rule is used for a target with a path that does not ordinarily
# exist in the build dir.
%/.dstamp:
	$(AM_V_at)$(MKDIR_P) $(@D)
	$(AM_V_at)touch $@

@APPDATA_XML_RULES@
@INTLTOOL_XML_RULE@
appdata_in_files = data/org.gnome.EasyTAG.appdata.xml.in $(nautilus_appdata_in_file)
appdata_XML = $(appdata_in_files:.xml.in=.xml)

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = data/org.gnome.EasyTAG.desktop.in
$(desktop_in_files:.desktop.in=.desktop): share/glib-2.0/schemas/gschemas.compiled
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

gsettings_ENUM_FILES = src/setting.h
gsettings_ENUM_NAMESPACE = org.gnome.EasyTAG
gsettings_SCHEMAS = data/org.gnome.EasyTAG.gschema.xml
@GSETTINGS_RULES@

if ENABLE_MAN
doc/easytag.1: doc/easytag.xml Makefile
	$(AM_V_GEN)$(XSLTPROC) --nonet --param man.charmap.use.subset 0 \
		--path $(builddir)/doc --output $(builddir)/doc/ \
		http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

nodist_man_MANS = doc/easytag.1
endif

images = \
	data/images/all-lowercase.png \
	data/images/all-uppercase.png \
	data/images/artist.png \
	data/images/artist-album.png \
	data/images/first-letter-uppercase.png \
	data/images/first-letter-uppercase-word.png \
	data/images/freedb.png \
	data/images/gnudb.png \
	data/images/invert-selection.png \
	data/images/mask.png \
	data/images/musicbrainz.png \
	data/images/red-lines.png \
	data/images/replaygain.png \
	data/images/sequence-track.png \
	data/images/unselect-all.png

iconthemedir = $(datadir)/icons/hicolor

appicon16dir = $(iconthemedir)/16x16/apps
appicon22dir = $(iconthemedir)/22x22/apps
appicon24dir = $(iconthemedir)/24x24/apps
appicon32dir = $(iconthemedir)/32x32/apps
appicon48dir = $(iconthemedir)/48x48/apps
appicon64dir = $(iconthemedir)/64x64/apps
appicon128dir = $(iconthemedir)/128x128/apps
appicon256dir = $(iconthemedir)/256x256/apps
appiconscalabledir = $(iconthemedir)/scalable/apps
appiconsymbolicdir = $(iconthemedir)/symbolic/apps

dist_appicon16_DATA = \
	data/icons/16x16/org.gnome.EasyTAG.png
dist_appicon22_DATA = \
	data/icons/22x22/org.gnome.EasyTAG.png
dist_appicon24_DATA = \
	data/icons/24x24/org.gnome.EasyTAG.png
dist_appicon32_DATA = \
	data/icons/32x32/org.gnome.EasyTAG.png
dist_appicon48_DATA = \
	data/icons/48x48/org.gnome.EasyTAG.png
dist_appicon64_DATA = \
	data/icons/64x64/org.gnome.EasyTAG.png
dist_appicon128_DATA = \
	data/icons/128x128/org.gnome.EasyTAG.png
dist_appicon256_DATA = \
	data/icons/256x256/org.gnome.EasyTAG.png
dist_appiconscalable_DATA = \
	data/icons/scalable/org.gnome.EasyTAG.svg
dist_appiconsymbolic_DATA = \
	data/icons/symbolic/org.gnome.EasyTAG-symbolic.svg

UPDATE_ICON_CACHE = gtk-update-icon-cache --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(UPDATE_ICON_CACHE) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(UPDATE_ICON_CACHE) "$(iconthemedir)"

dist_noinst_DATA = \
	$(appdata_in_files) \
	$(desktop_in_files) \
	$(gsettings_SCHEMAS) \
	$(easytag_headers) \
	data/org.gnome.EasyTAG.gresource.xml \
	$(noinst_resource_files) \
	src/tags/id3lib/patch_id3lib_3.8.3_UTF16_writing_bug.diff \
	src/tags/libapetag/README.apetag \
	src/tags/libapetag/id3v2_read.c \
	src/tags/libapetag/id3v2_read.h \
	src/tags/mp4_tag.cc \
	src/win32/easytag.manifest \
	src/win32/resource.h \
	data/icons/win32/easytag.ico \
	data/nsis/easytag-header.bmp \
	data/nsis/easytag-sidebar.bmp \
	$(images) \
	doc/easytag.xml \
	COPYING.GPL3 \
	easytag-win32-installer.nsi.in \
	HACKING

dist_noinst_SCRIPTS = \
	autogen.sh

if EASYTAG_ENABLE_TESTS
tests/test-desktop-file-validate.sh: Makefile tests/.dstamp
	$(AM_V_GEN)echo "#!/bin/sh -e" > $@; \
		echo "$(DESKTOP_FILE_VALIDATE) $(top_builddir)/$(desktop_DATA) || exit 1" >> $@; \
		chmod +x $@

# Not automake built-in TESTS_ENVIRONMENT!
TEST_ENVIRONMENT = \
	MALLOC_CHECK_=2 \
	MALLOC_PERTURB_=$$(($${RANDOM:-256} % 256)) \
	G_SLICE=debug-blocks

# test: run all tests.
test: $(check_PROGRAMS)
	$(AM_V_at)$(TEST_ENVIRONMENT) $(GTESTER) --verbose $(check_PROGRAMS)

# test-report: run tests and generate report.
# perf-report: run tests with -m perf and generate report.
# full-report: like test-report: with -m perf and -m slow.
test-report perf-report full-report: $(check_PROGRAMS)
	$(AM_V_at)test -z "$(check_PROGRAMS)" || { \
	  case $@ in \
	  test-report) test_options="-k";; \
	  perf-report) test_options="-k -m=perf";; \
	  full-report) test_options="-k -m=perf -m=slow";; \
	  esac ; \
	  $(TEST_ENVIRONMENT) $(GTESTER) --verbose $$test_options -o test-log.xml $(check_PROGRAMS) ; \
	  if test -d "$(top_srcdir)/.git" ; then \
	    REVISION=`git describe` ; \
	  else \
	    REVISION=$(PACKAGE_VERSION) ; \
	  fi ; \
	  echo '<?xml version="1.0"?>' > $@.xml ; \
	  echo '<report-collection>' >> $@.xml ; \
	  echo '<info>' >> $@.xml ; \
	  echo '  <package>$(PACKAGE_NAME)</package>' >> $@.xml ; \
	  echo '  <version>$(PACKAGE_VERSION)</version>' >> $@.xml ; \
	  echo "  <revision>$$REVISION</revision>" >> $@.xml ; \
	  echo '</info>' >> $@.xml ; \
	  sed '1,1s/^<?xml\b[^>?]*?>//' < test-log.xml >> $@.xml ; \
	  rm test-log.xml ; \
	  echo >> $@.xml ; \
	  echo '</report-collection>' >> $@.xml ; \
	  $(GTESTER_REPORT) --version 2>/dev/null 1>&2 ; test "$$?" != 0 || $(GTESTER_REPORT) $@.xml >$@.html ; \
	}

check_PROGRAMS = \
	tests/test-dlm \
	tests/test-genres \
	tests/test-file_tag \
	tests/test-misc \
	tests/test-picture \
	tests/test-scan \
	tests/test-xstring

common_test_cppflags = \
	-I$(top_srcdir)/src \
	-I$(top_builddir) \
	$(EASYTAG_CFLAGS) \
	$(GLIB_DEPRECATION_FLAGS) \
	$(GTK_DEPRECATION_FLAGS)

common_test_cflags = \
	$(EASYTAG_CFLAGS) \
	$(WARN_CFLAGS)

tests_test_dlm_CPPFLAGS = \
	$(common_test_cppflags)

tests_test_dlm_CFLAGS = \
	$(common_test_cflags)

tests_test_dlm_SOURCES = \
	tests/test-dlm.c \
	src/dlm.c

tests_test_dlm_LDADD = \
	$(EASYTAG_LIBS)

tests_test_file_tag_CPPFLAGS = \
	$(common_test_cppflags) \
	-I$(top_srcdir)/src/tags

tests_test_file_tag_CFLAGS = \
	$(common_test_cflags)

tests_test_file_tag_SOURCES = \
	tests/test-file_tag.cc \
	src/file_tag.cc \
	src/file_name.cc \
	src/charset.cc \
	tests/log-stub.cc \
	src/misc.cc \
	src/xstring.cc \
	src/picture.cc

tests_test_file_tag_LDADD = \
	$(EASYTAG_LIBS)

tests_test_genres_CPPFLAGS = \
	$(common_test_cppflags)

tests_test_genres_CFLAGS = \
	$(common_test_cflags)

tests_test_genres_SOURCES = \
	tests/test-genres.c \
	src/genres.c

tests_test_genres_LDADD = \
	$(EASYTAG_LIBS)

tests_test_misc_CPPFLAGS = \
	$(common_test_cppflags) \
	-I$(top_srcdir)/src/tags

tests_test_misc_CFLAGS = \
	$(common_test_cflags)

tests_test_misc_SOURCES = \
	tests/test-misc.cc \
	src/misc.cc

tests_test_misc_LDADD = \
	$(EASYTAG_LIBS)

tests_test_picture_CPPFLAGS = \
	$(common_test_cppflags) \
	-I$(top_srcdir)/src/tags

tests_test_picture_CFLAGS = \
	$(common_test_cflags)

tests_test_picture_SOURCES = \
	tests/test-picture.cc \
	src/misc.cc \
	src/xstring.cc \
	src/charset.cc \
	tests/log-stub.cc \
	src/picture.cc

tests_test_picture_LDADD = \
	$(EASYTAG_LIBS)

tests_test_scan_CPPFLAGS = \
	$(common_test_cppflags)

tests_test_scan_CFLAGS = \
	$(common_test_cflags)

tests_test_scan_SOURCES = \
	tests/test-scan.cc \
	src/scan.cc

tests_test_scan_LDADD = \
	$(EASYTAG_LIBS)

tests_test_xstring_CPPFLAGS = \
	$(common_test_cppflags)

tests_test_xstring_SOURCES = \
	tests/test-xstring.cc \
	src/charset.cc \
	tests/log-stub.cc \
	src/xstring.cc

tests_test_xstring_LDADD = \
	$(EASYTAG_LIBS)

check_SCRIPTS = \
	tests/test-desktop-file-validate.sh

TESTS = \
	$(check_PROGRAMS) \
	$(check_SCRIPTS)
endif

CLEANFILES = \
	$(appdata_XML) \
	$(check_SCRIPTS) \
	$(desktop_DATA) \
	$(easytag_rc) \
	$(enum_data) \
	$(nodist_man_MANS) \
	*.log \
	easytag-$(PACKAGE_VERSION)-setup.exe \
	easytag-win32-installer.nsi \
	src/resource.c \
	src/resource.h

DISTCLEANFILES = \
	po/.intltool-merge-cache

check-local: test
clean-local: clean-local-dstamp
clean-local-dstamp:
	-rm -f tests/.dstamp

@GENERATE_CHANGELOG_RULES@
dist-hook: dist-ChangeLog

.PHONY: clean-local-dstamp
.PHONY: test test-report perf-report full-report
