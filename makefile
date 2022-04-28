directory-structure:
	test -d "./executables" || mkdir "./executables"
	test -d "./executables/dfs1" || mkdir "./executables/dfs1"
	test -d "./executables/dfs2" || mkdir "./executables/dfs2"
	test -d "./executables/dfs3" || mkdir "./executables/dfs3"
	test -d "./executables/dfs4" || mkdir "./executables/dfs4"

tests: directory-structure
	gcc -Wall -o ./executables/name-table-test name-table-test.c name-table.c

experiments: directory-structure
	gcc -Wall -o ./executables/hash-modulo-experiment hash-modulo-experiment.c