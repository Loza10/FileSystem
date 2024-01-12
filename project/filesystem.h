#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <vector>

struct fdesc {
  int unique_id = 0;
  char unique_mode = 'x';
  int rw = 0;
};

struct fileInfo {
  char *filepath = nullptr;
  int lockId = -1;
  int fLen = 0;
  std::vector<fdesc> file_in;
  int opened = 0;
};

class FileSystem {
  DiskManager *myDM;
  PartitionManager *myPM;
  char myfileSystemName;
  int myfileSystemSize;
  int totalFiles = 1;
  int instance_id = 1;

  /* declare other private members here */
  fileInfo fileTable[100];

  public:
    FileSystem(DiskManager *dm, char fileSystemName);
    int createFile(char *filename, int fnameLen);
    int createDirectory(char *dirname, int dnameLen);
    int lockFile(char *filename, int fnameLen);
    int unlockFile(char *filename, int fnameLen, int lockId);
    int deleteFile(char *filename, int fnameLen);
    int deleteDirectory(char *dirname, int dnameLen);
    int openFile(char *filename, int fnameLen, char mode, int lockId);
    int closeFile(int fileDesc);
    int readFile(int fileDesc, char *data, int len);
    int writeFile(int fileDesc, char *data, int len);
    int appendFile(int fileDesc, char *data, int len);
    int seekFile(int fileDesc, int offset, int flag);
    int truncFile(int fileDesc, int offset, int flag);
    int renameFile(char *filename1, int fnameLen1, char *filename2, int fnameLen2);
    int renameDirectory(char *dirname1, int dnameLen1, char *dirname2, int dnameLen2);
    int getAttributes(char *filename, int fnameLen, char *returnbuffer/* ... and other parameters as needed */);
    int setAttributes(char *filename, int fnameLen, int user, char attributes /* ... and other parameters as needed */);

    /* declare other public members here */

    void modifyDir(char name, int dirblknum, int blknum, char type);
    bool checkValidity(char *filename, int flen);
    int findFile(char *fname, int flen);
    bool checkDirEmpty(int blk);
    int traverseDir(int dir, char name, char type);
    void removeFromRoot(int blk, char name, char type);
    void setDirectFileBlock(int blknum);
    int findFirstEmpty(int blknum);
    void setIndirectFileBlock(int blknum);
    int findFirstEmptyIndirect(int blknum);
    int containsID(int id);
    void setFileSize(int blknum, int size);
    int getFileSize(int blknum);
    void trimDir(char *path, int plen);
    void trimDirRec(int prevdirblk, int dirblk);
    bool checkLastAddressEmpty(int dirblk);
    std::vector<int> getTruncBlks(int start_block, int fileblk);

    bool addFileToDirectory(char filename, int last_block, int main_block, char type);
    int traverseDirectory(char *filename, int len);
};
#endif
