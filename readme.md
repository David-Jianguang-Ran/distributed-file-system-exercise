# Programming Assignment 4 - Distributed File System

#### What is this?
This is a distributed file system consisting of a client and server program.  
This is built using C socket and pthread library.  
This program is purely an academic exercise. 

#### What does it do?
The server programs take arguments: `<storage-directory> <port#>`  
The client supports following actions:  
- `put <file-names> ... ` will attempt to store named files on the servers.  
- `get <file-names> ... ` will attempt to retrieve named files
- `list` will print out files stored on the servers.  

#### What's all in here?
- A client and a server program that will send files over TCP.
- do `make final` to build server and client executables.  
Default client name is `./executables/dfc`, server is `./executables/dfs`.
- By default, the client requires a config file at `~/dfc.conf`, see `./test-files/dfc.conf` for example

#### Known limitations
- Cannot store files with fewer bytes than the number of servers. (4 bytes in the current configuration)
- Can `put` and `list` files with slash `/` in the file name but cannot list stored files with slash in file name.


Created by David Ran May/2022.  
