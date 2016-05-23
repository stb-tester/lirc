#!/usr/bin/env bash
#
# Create a list of automake module setups.

echo '## Created by make-pluginlist.sh'

skipped=$(sed -n  '/_la_SOURCES/s/.*=//p' < Makefile.am | \
     tr -d '\\' | \
     while read first garbage; do echo $first; done)

for file in *.c; do
    [[ "$file" == *common.c ]] && continue
    basename=${file/.c/}
    [[ "$skipped" = *${file}* ]] && continue
    header=${file/.c/.h}
    sources="$file"
    test -e $header && sources="$file $header"
    echo "plugin_LTLIBRARIES              += ${basename}.la"
    printf "%-32s = %s\n" "${basename}_la_SOURCES " "$sources"
    echo
done | sed  '$ d'
