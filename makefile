modules = chunk-record.c name-table.c thread-safe-file.c thread-safe-job-stack.c utils.c

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
	gcc -Wall -o ./executables/config-read-scan-experiment config-read-scan-experiment.c

dfs: directory-structure
	gcc -Wextra -pthread -g -o ./executables/dfs server.c $(modules)
	gcc -Wextra -pthread -g -o ./executables/dfc client.c $(modules)