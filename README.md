Unix style File System created by Zakk Loveall and Chet Russell.

List of functions supported:
- createFile(char *filename, int fname_len)
- createDirectory(char *dirname, int dnameLen)
- lockFile(char *filename, int fname_len)
- unlockFile(char *filename, int fname_len, int lock_id)
- deleteFile(char *filename, int fname_len)
- deleteDirectory(char *dirname, int dnameLen)
- openFile(char *filename, int fname_len, char mode, int lock_id)
- closeFile(int filedesc)
- readFile(int filedesc, char *data, int length)
- writeFile(int filedesc, char *data, int length)
- appendFile(int filedesc, char *data, int length)
- seekFile(int filedesc, int offset, int flag)
- truncFile(int filedesc, int offset, int flag)
- renameFile(char *fname1, int fname1_len, char *fname2, int fname2_len)
- renameDirectory(char *dname1, int dname1_len, char *dname2, int dname2_len)
- setAttributes(char *filename, int fnameLen, int user, char attribute)
- getAttributes(char *filename, int fnameLen, char *returnbuffer)

Small Note:
- There's a small problem with truncFile() but it's mostly working.
