# PA4 - Distributed File System

#### What's all in here?
- A client and a server program that will send files over TCP.
- do `make final` to build server and client executables.  
Default client name is `./executables/dfc`, server is `./executables/dfs`.
- By default, the client requires a config file at `~/dfc.conf`, see `./test-files/dfc.conf` for example

#### Known limitations
- Cannot store files with fewer bytes than the number of servers. (4 bytes in the current configuration)
- Can `put` and `list` files with slash `/` in the file name but cannot list stored files with slash in file name.


Created by David Ran May/2022.  
No license do what ever the hell you want, just don't get me involved, at all. 