modules = chunk-record.c name-table.c thread-safe-file.c thread-safe-job-stack.c utils.c

directory-structure:
	test -d "./executables" || mkdir "./executables"
	test -d "./executables/dfs1" || mkdir "./executables/dfs1"
	test -d "./executables/dfs2" || mkdir "./executables/dfs2"
	test -d "./executables/dfs3" || mkdir "./executables/dfs3"
	test -d "./executables/dfs4" || mkdir "./executables/dfs4"
	test -d "./executables/ctmp" || mkdir "./executables/ctmp"

tests: directory-structure
	gcc -Wall -g -o ./executables/name-table-test name-table-test.c name-table.c
	gcc -Wall -g -o ./executables/utils-test utils-test.c utils.c chunk-record.c

experiments: directory-structure
	gcc -Wall -o ./executables/hash-modulo-experiment hash-modulo-experiment.c
	gcc -Wall -o ./executables/config-read-scan-experiment config-read-scan-experiment.c

dfs: directory-structure
	gcc -Wextra -pthread -g -o ./executables/dfs server.c $(modules)
	gcc -Wextra -pthread -g -o ./executables/dfc client.c $(modules)

stress-test-dirs: directory-structure
	rm -rf "./stress-test1"
	mkdir "./stress-test1"
	mkdir "./stress-test1/ctmp"
	cp ./test-files/* stress-test1
	rm -rf "./stress-test2"
	mkdir "./stress-test2"
	mkdir "./stress-test2/ctmp"
	cp ./test-files/* stress-test2

stress-test: stress-test-dirs
	gcc -Wextra -pthread -g -o ./executables/dfs server.c $(modules)
	gcc -Wextra -pthread -g -o ./stress-test1/dfc client.c $(modules)
	gcc -Wextra -pthread -g -o ./stress-test2/dfc client.c $(modules)
