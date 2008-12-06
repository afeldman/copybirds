#(c) 2008 by Malte Marwedel. Use under the terms of the GPL Version 2


all:
	make -C lib
	make -C share
	make -C cb_analyze_dep
	make -C cb_remove_from_source
	make -C cb_meta
	make -C cb_meta_merge
	make -C cb_files
	make -C cb_strace_to_xml
	make -C scripts

removebackups:
	make -C share removebackups
	make -C cb_analyze_dep removebackups
	make -C cb_remove_from_source removebackups
	make -C cb_meta removebackups
	make -C cb_meta_merge removebackups
	make -C cb_files removebackups
	make -C cb_strace_to_xml removebackups
	make -C scripts removebackups
	rm -f *~

clean:
	rm -f bin/cb_*
	make -C lib clean
	make -C share clean
	make -C cb_analyze_dep clean
	make -C cb_remove_from_source clean
	make -C cb_meta clean
	make -C cb_meta_merge clean
	make -C cb_files clean
	make -C cb_strace_to_xml clean
	make -C scripts clean

export:
	./export.sh
