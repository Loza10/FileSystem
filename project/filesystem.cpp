#include "disk.h"
#include "diskmanager.h"
#include "partitionmanager.h"
#include "filesystem.h"
#include <time.h>
#include <cstdlib>
#include <iostream>
using namespace std;

FileSystem::FileSystem(DiskManager *dm, char fileSystemName)
{
  myDM = dm;
  myfileSystemName = fileSystemName;
  myfileSystemSize = myDM->getPartitionSize(myfileSystemName);
  myPM = new PartitionManager(myDM, myfileSystemName, myfileSystemSize);

  // root dir creation
  int root = myPM->getFreeDiskBlock();
  char buffer[64];
  for (int i = 0; i < 64; i++) {
    buffer[i] = '#';
  }
  myPM->writeDiskBlock(root, buffer);
}

int charToInt(char *buffer, int pos) {
    int charint = 0;
    for (int i = 0; i < 4; i++) {
        charint *= 10;
        charint += (buffer[pos + i] - '0');
    }
    return charint;
}

void intToChar(char *buffer, int num, int pos) {
    for (int i = 3; i >= 0; i--) {
        int new_pos = pos + i;
        buffer[new_pos] = '0' + num % 10;
        num /= 10;
    }
}

int validInput(char *name, int nameLength) {
  if (nameLength % 2 == 0) {
    for (int i = 0; i < nameLength; i++) {
      if ((i % 2 == 0) && (name[i] != '/')) {
        return -1;
      } else if ((i % 2 != 0) && !((name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= 'a' && name[i] <= 'z'))) {
        return -1;
      }
    }
  } else {
    return -1;
  }
  return 0;
}

bool FileSystem::checkValidity(char *filename, int flen) {
  int dir_blk;
  if (flen == 4) {
    dir_blk = traverseDir(1, filename[1], 'd');
    if (dir_blk == -1) {
      return false;
    }
    dir_blk = traverseDir(dir_blk, filename[3], 'f');
    if (dir_blk == -1) {
      return false;
    }
    return true;
  }
  else if (flen > 4) {
    dir_blk = traverseDir(1, filename[1], 'd');
    for (int i = 3; i < flen; i += 2) {
      if (i < flen-1) {
        dir_blk = traverseDir(dir_blk, filename[i], 'd');
      }
      else {
        dir_blk = traverseDir(dir_blk, filename[i], 'f');
      }
      if (dir_blk == -1) {
        return false;
      }
    }
    return true;
  }
  return false;
}

void FileSystem::modifyDir(char name, int dirblknum, int blknum, char type) {
  char buffer[64];
  myPM->readDiskBlock(dirblknum, buffer);
  for (int i = 0; i < 60; i+=6) {
    if (buffer[i] == '#') {
      buffer[i] = name;
      intToChar(buffer, blknum, i+1);
      buffer[i+5] = type;
      myPM->writeDiskBlock(dirblknum, buffer);
      return;
    }
  }
  int tail = charToInt(buffer, 60);
  if (tail > 0) {
    modifyDir(name, tail, blknum, type);
  } else if (buffer[59] != '#') {
    int extend = myPM->getFreeDiskBlock();
    if (extend != -1) {
      intToChar(buffer, extend, 60);
      myPM->writeDiskBlock(dirblknum, buffer);
      modifyDir(name, extend, blknum, type);
    } 
  }
}



int FileSystem::containsID(int id) {
  for (int i = 0; i <= totalFiles; i++) {
    for (int x = 0; x < fileTable[i].file_in.size(); x++) {
      if (fileTable[i].file_in[x].unique_id == id) {
        return i;
      }
    }
  }
  return -1;
}

int FileSystem::findFile(char *fname, int flen) {
  //for(int i = 0; i < myfileSystemSize; i++) {
  //  char buffer[64];
  //  myPM->readDiskBlock(i,buffer);
  //  if (fname == buffer[0] && buffer[1] == 'f') {
  //    return i;
  //  }
  //}

  char buffer[64];

  if (flen == 2) {
    // returns -1 if it cannot find it in root
    return traverseDir(1, fname[1], 'f');
  }
  else if (flen == 4) {
    int dirblk = traverseDir(1, fname[1], 'd');
    return traverseDir(dirblk, fname[3], 'f');
  }
  else if (flen > 4) {
    //traversal of root
    int dirblk = traverseDir(1, fname[1], 'd');
    for (int i = 3; i < flen-2; i+=2) {
      dirblk = traverseDir(dirblk, fname[i], 'd');
    }
    return traverseDir(dirblk, fname[flen-1], 'f');
  }

  return -1;
}

void FileSystem::removeFromRoot(int blk, char name, char type) {
  char buffer[64];
  myPM->readDiskBlock(blk, buffer);
  for (int i = 0; i < 60; i+=6) {
    if (buffer[i] == name && buffer[i+5] == type) {
      for (int x = i; x < i+6; x++) {
        buffer[x] = '#';
      }
      myPM->writeDiskBlock(blk, buffer);
      return;
    }
  }
  if (charToInt(buffer, 60) > 0) {
    removeFromRoot(charToInt(buffer, 60), name, type);
  }
}

int FileSystem::traverseDir(int dir, char name, char type) {
  char buffer[64];
  myPM->readDiskBlock(dir, buffer);
  for (int i = 0; i <= 60; i+=6) {
    if (buffer[i] == name && buffer[i+5] == type) {
      return charToInt(buffer, i+1);
    }
  }
  int nextdir = charToInt(buffer, 60);
  if (nextdir > 0) {
    return traverseDir(nextdir, name, type);
  }
  return -1;
}

void FileSystem::setDirectFileBlock(int blknum) {
  char buffer[64];
  myPM->readDiskBlock(blknum,buffer);
  if (charToInt(buffer, 18) != 0) {
    this->setIndirectFileBlock(blknum);
  }
  for (int i = 6; i <= 18; i+=4) {
    if ((charToInt(buffer, i) != 0) && (i == 18)) {
      this->setIndirectFileBlock(blknum);
      break;
    }
    if (charToInt(buffer, i) == 0) {
      int main_block = myPM->getFreeDiskBlock();
      if (main_block == -1) {
        break;
      } else {
        intToChar(buffer, main_block, i);
        //cout << "BUFFER INFO" << endl;
        //cout << buffer << endl;
        myPM->writeDiskBlock(blknum, buffer);
        char empty[64];
        if (i < 18) {
          //For direct address
          for (i = 0; i < 64; i++) {
            empty[i] = '#';
          }
        } else {
          //For indirect inode
          for (i = 0; i < 64; i++) {
            empty[i] = '0';
          }
        }
        myPM->writeDiskBlock(main_block, empty);
        if (i >= 18) {
          this->setIndirectFileBlock(blknum);
        }
        break;
      }
    }
  }
}
int FileSystem::findFirstEmptyIndirect(int blknum) {
  char buffer[64];
  myPM->readDiskBlock(blknum,buffer);
  int indirect_address = charToInt(buffer, 18);
  myPM->readDiskBlock(indirect_address,buffer);
  for (int i = 0; i < 63; i += 4) {
    int direct_block = charToInt(buffer, i);
    if (direct_block != 0) {
      char buffer[64];
      myPM->readDiskBlock(direct_block,buffer);
      if (buffer[63] == '#') {
        return direct_block;
      }
    }
  }
  return -1;
}
int FileSystem::findFirstEmpty(int blknum){
  char buffer[64];
  myPM->readDiskBlock(blknum, buffer);
  for (int i = 6; i <= 18; i += 4) {
    int x = charToInt(buffer, i);
    if (i == 18) {
      return this->findFirstEmptyIndirect(blknum);
    }
    else {
      if (x != 0) {
        char buffer[64];
        myPM->readDiskBlock(x, buffer);
        if (buffer[63] == '#') {
          return x;
        }
      }
    }
  }
  return -1;
}

void FileSystem::setIndirectFileBlock(int blknum) {
  char buffer[64];
  myPM->readDiskBlock(blknum,buffer);
  int indirect_block = charToInt(buffer, 18);
  myPM->readDiskBlock(indirect_block,buffer);
  for (int i = 0; i < 64; i += 4) {
    int block = charToInt(buffer, i);
    if (block == 0) {
      int main_block = myPM->getFreeDiskBlock();
      if (main_block == -1) {
        break;
      } else {
        char empty[64];
        for (int i = 0; i < 64; i++) {
          empty[i] = '#';
        }
        myPM->writeDiskBlock(main_block, empty);
        intToChar(buffer, main_block, i);
        myPM->writeDiskBlock(indirect_block, buffer);
        break;
      }
    }
  }
}

void FileSystem::setFileSize(int blknum, int size){
  char buffer[64];
  myPM->readDiskBlock(blknum,buffer);
  intToChar(buffer, size, 2);
  myPM->writeDiskBlock(blknum, buffer);
}

int FileSystem::getFileSize(int blknum){
  char buffer[64];
  myPM->readDiskBlock(blknum,buffer);
  return charToInt(buffer, 2);
}

vector<int> FileSystem::getTruncBlks(int start_block, int fileblk) {
  vector<int> block_locs;
  int curr_address = 1;
  int buffer_log = 6 + start_block * 4;
  char main_file[64];
  char inode_buf[64];
  char buffer[64];

  myPM->readDiskBlock(fileblk, main_file);
  while (curr_address > 0) {
    buffer_log = 6 + start_block * 4;
    if (buffer_log >= 18) {
      buffer_log = (start_block - 3) * 4;
      if (buffer_log < 64) {
        myPM->readDiskBlock(charToInt(main_file, 18), inode_buf);
        curr_address = charToInt(inode_buf, buffer_log);
        if (curr_address > 0) {
          block_locs.push_back(curr_address);
        }
      } else {
        break;
      }
    } else {
      curr_address = charToInt(main_file, buffer_log);
      if (curr_address > 0) {
        block_locs.push_back(curr_address);
      }
    }
    start_block++;
  }
  return block_locs;
}

bool FileSystem::addFileToDirectory(char filename, int last_block, int main_block, char type) {
  char buffer[64];
  myPM->readDiskBlock(last_block, buffer);

  if (traverseDir(last_block, filename, type) != -1) {
    return false; // File already exists
  }

  buffer[0] = filename;
  buffer[1] = type;

  myPM->writeDiskBlock(main_block, buffer);
  modifyDir(filename, last_block, main_block, type);

  return true;
}

int FileSystem::traverseDirectory(char *filename, int len) {
  int last_block = 0;

  for (int i = 1; i <= len; i += 2) {
    if (i == 1) {
      last_block = traverseDir(1, filename[i], 'd');
    } else {
      last_block = traverseDir(last_block, filename[i], 'd');
    }

    if (last_block == -1) {
      return -1;
    }

  }

  return last_block;
}

int FileSystem::createFile(char *filename, int fnameLen) {
  if (fnameLen < 2) {
    return -3;
  }

  if (validInput(filename, fnameLen) == -1) {
    return -3;
  }
  if (findFile(filename, fnameLen) != -1) {
    return -1; 
  }

  char buffer[64];
  int main_block = myPM->getFreeDiskBlock();
  int last_block = 0;

  if (main_block == -1) {
    return -2;
  }

  if (fnameLen == 2) {
    if (!addFileToDirectory(filename[1], 0, main_block, 'f')) {
      myPM->returnDiskBlock(main_block);
      return -4;
    }
    last_block = 1;
  } else if (fnameLen > 2) {
    last_block = traverseDirectory(filename, fnameLen - 2);

    if (last_block == -1) {
      myPM->returnDiskBlock(main_block);
      return -4;
    }

    if (!addFileToDirectory(filename[fnameLen - 1], last_block, main_block, 'f')) {
      myPM->returnDiskBlock(main_block);
      return -4;
    }
  } else {
    myPM->returnDiskBlock(main_block);
    return -3;
  }

  fileTable[totalFiles].filepath = filename;
  fileTable[totalFiles].fLen = fnameLen;
  totalFiles++;

  for (int i = 0; i < 64; i++) {
    buffer[i] = '0';
  }

  buffer[0] = filename[fnameLen - 1];
  buffer[1] = 'f';

  myPM->writeDiskBlock(main_block, buffer);
  setDirectFileBlock(main_block);
  if (fnameLen == 2) {
    modifyDir(filename[fnameLen-1], 1, main_block, 'f');
  }
  return 0;
}


int FileSystem::createDirectory(char *dirname, int dnameLen) {
  int main_block = myPM->getFreeDiskBlock();
  char buffer[64];

  if (main_block == -1) {
    return -2;
  }

  if (validInput(dirname, dnameLen) == -1) {
    myPM->returnDiskBlock(main_block);
    return -3;
  }

  if (dnameLen == 2) {
    int blk = traverseDir(1, dirname[1], 'd');
    if (blk != -1) {
      myPM->returnDiskBlock(main_block);
      return -1;
    }

    for (int i = 0; i < 64; i++) {
      buffer[i] = '#';
    }

    myPM->writeDiskBlock(main_block, buffer);
    modifyDir(dirname[1], 1, main_block, 'd');
    return 0;
  } else if (dnameLen == 4) {
    int blk = traverseDir(1, dirname[1], 'd');
    if (blk == -1) {
      myPM->returnDiskBlock(main_block);
      return -4;
    }

    for (int i = 0; i < 64; i++) {
      buffer[i] = '#';
    }

    myPM->writeDiskBlock(main_block, buffer);
    modifyDir(dirname[3], blk, main_block, 'd');
    return 0;
  } else if (dnameLen > 4) {
    int blknum = traverseDir(1, dirname[1], 'd');
    int last_block = 0;

    if (blknum == -1) {
      myPM->returnDiskBlock(main_block);
      return -4;
    }

    for (int i = 3; i < dnameLen; i += 2) {
      if (i < dnameLen - 1) {
        if (last_block == 0) {
          last_block = traverseDir(blknum, dirname[i], 'd');
        } else {
          last_block = traverseDir(last_block, dirname[i], 'd');
        }
        if (last_block == -1) {
          myPM->returnDiskBlock(main_block);
          return -4;
        }
      } else {
        blknum = traverseDir(last_block, dirname[i], 'd');
        if (blknum != -1) {
          myPM->returnDiskBlock(main_block);
          return -1;
        }
      }
    }

    for (int i = 0; i < 64; i++) {
      buffer[i] = '#';
    }

    myPM->writeDiskBlock(main_block, buffer);
    modifyDir(dirname[dnameLen - 1], last_block, main_block, 'd');
    return 0;
  }
  myPM->returnDiskBlock(main_block);
  return -4;
}


int FileSystem::lockFile(char *filename, int fnameLen)
{
  bool flag = false;
  if (findFile(filename, fnameLen) == -1 || validInput(filename, fnameLen) == -1) {
    return -2;
  }
  if (fnameLen > 2) {
    if (checkValidity(filename, fnameLen) == false) {
      return -4;
    }
  }
   for (int i = 0; i < totalFiles; i++) {
     //Found file, no need to put it in the table
     if (fileTable[i].filepath == filename) {
       flag = true;
       break;
     }
   }
   //Couldn't find file in table, add it
   if (flag == false) {
     fileTable[totalFiles].filepath = filename;
     fileTable[totalFiles].fLen = fnameLen;
     totalFiles++;
   }
  for (int i = 0; i < totalFiles; i++) {
    if (fileTable[i].filepath == filename) { // iterate to the nearest empty spot
      if (fileTable[i].opened > 0) {
        return -3;
      }
      if (fileTable[i].lockId == i) {
        return -1;
      }
      //Needed unique lock id, so just made it the index
      fileTable[i].lockId = i;
      return i;
    }
  }
  return -4;
}

int FileSystem::unlockFile(char *filename, int fnameLen, int lockId)
{
  if (findFile(filename, fnameLen) == -1 || validInput(filename, fnameLen) == -1) {
    return -2;
  }
  if (fnameLen > 2) {
    if (checkValidity(filename, fnameLen) == false) {
      return -4;
    }
  }
  for (int i = 0; i < totalFiles; i++) {
    if (fileTable[i].filepath == filename) { // iterate to the nearest empty spot
      if (lockId != i) {
        return -1;
      }
      if (fileTable[i].lockId == lockId) {
        fileTable[i].lockId = -1;
        return 0;
      }
    }
  }
  return -2;
}

int FileSystem::deleteFile(char *filename, int fnameLen) {
  if (validInput(filename, fnameLen) == -1) {
    return -3;
  } 
  bool flag = false;
  char buffer[64];

  char dirbuff[64];
  int index = 0;
  int blknum;
  int dir_block;

  if (fnameLen == 2) {
    blknum = findFile(filename, fnameLen);
  }
  else if (fnameLen == 4) {
    dir_block = traverseDir(1, filename[1], 'd');
    if (dir_block == -1) {
      return -3;
    }
    int main_block = traverseDir(main_block, filename[3], 'f');
    if (main_block == -1) {
      return -1;
    }

    myPM->readDiskBlock(dir_block, dirbuff);
    blknum = findFile(filename, fnameLen);
  }
  else if (fnameLen > 4) {
    int last_blk = traverseDir(1, filename[1], 'd');
    dir_block = 0;
    // if directory does not exist
    if (last_blk == -1) {
      return -1;
    }

    for (int i = 3; i < fnameLen; i+=2) {
      if (i < fnameLen - 1) {
        if (dir_block == 0) {
          dir_block = traverseDir(last_blk, filename[i], 'd');
        } 
        else {
          dir_block = traverseDir(dir_block, filename[i], 'd');
        }
        if (dir_block == -1) {
            return -1;
        }
      }
      else {
        last_blk = traverseDir(dir_block, filename[i], 'f');
        if (last_blk == -1) {
          return -1;
        }
      }
    }
    myPM->readDiskBlock(dir_block, dirbuff);
    blknum = findFile(filename, fnameLen);
  }
  if (blknum == -1) {
    return -1;
  }
  for (int i = 0; i < totalFiles; i++) {
   //Found file, no need to put it in the table
   if (fileTable[i].filepath == filename) {
     flag = true;
     break;
   }
 }
 //Couldn't find file in table, add it
 if (flag == false) {
   fileTable[totalFiles].filepath = filename;
   fileTable[totalFiles].fLen = fnameLen;
   totalFiles++;
 }
 int cor_file;
 for (int i = 0; i < totalFiles; i++) {
   if (fileTable[i].filepath == filename) {
     if (fileTable[i].opened > 0 || fileTable[i].lockId != -1) {
       return -2;
     }
     cor_file = i;
     myPM->readDiskBlock(blknum, buffer);
     for (int k = 6; k <= 18; k+=4) {
       if (k != 18) {
         int blk_loc = charToInt(buffer, k);
         if (blk_loc != 0) {
           myPM->readDiskBlock(blk_loc, buffer);
           for (int x = 0; x < 64; x++) {
             buffer[x] = '#';
           }
           myPM->returnDiskBlock(blk_loc);
           myPM->writeDiskBlock(blk_loc, buffer);
         }
       } else {
         int indirect_loc = charToInt(buffer, k);
         if (indirect_loc != 0) {
           myPM->readDiskBlock(indirect_loc, buffer);
           for (int x = 0; x < 64; x+=4) {
             int direct_inode = charToInt(buffer, x);
             if (direct_inode != 0) {
               myPM->readDiskBlock(direct_inode, buffer);
               for (int j = 0; j < 64; j++) {
                 buffer[j] = '#';
               }
               myPM->returnDiskBlock(direct_inode);
               myPM->writeDiskBlock(direct_inode, buffer);
             }
           }
           myPM->returnDiskBlock(indirect_loc);
         }
       }
     }
     
     removeFromRoot(1, fileTable[cor_file].filepath[fileTable[cor_file].fLen-1], 'f');
     if (fnameLen > 2) {
       removeFromRoot(dir_block, fileTable[cor_file].filepath[fileTable[cor_file].fLen-1], 'f');
       trimDir(filename, fnameLen-2);
     }
     myPM->readDiskBlock(blknum, buffer);
     for (int i = 0; i < 64; i++) {
       buffer[i] = '#';
     }
     myPM->writeDiskBlock(blknum, buffer);
     myPM->returnDiskBlock(blknum);
     fileTable[cor_file].filepath = nullptr;
     fileTable[cor_file].file_in.clear();
     fileTable[cor_file].opened = 0;
     fileTable[cor_file].lockId = -1;
     fileTable[cor_file].fLen = 0;

    return 0;
   }
 }
 return -3; //place holder so there is no warnings when compiling.
}

bool FileSystem::checkDirEmpty(int blk) {
  char buffer[64];
  myPM->readDiskBlock(blk, buffer);
  for (int i = 0; i < 60; i++) {
    if (buffer[i] != '#') {
      return false;
    }
  }
  return true;
}

int FileSystem::deleteDirectory(char *dirname, int dnameLen)
{
  int buffer[64];
  int blk;
  int empty;

  if (dnameLen == 2) {
    blk = traverseDir(1, dirname[1], 'd');
    // if directory does not exist
    if (blk == -1) {
      return -1;
    }
    trimDir(dirname, dnameLen);
    empty = checkDirEmpty(blk);

    if (empty == true) {
      removeFromRoot(1, dirname[1], 'd');
      myPM->returnDiskBlock(blk);
      return 0;
    }
    else {
      return -2;
    }
  }
  else if (dnameLen == 4) {
    int dir_block = 0;
    blk = traverseDir(1, dirname[1], 'd');
    // if directory does not exist
    
    if (blk == -1) {
      return -1;
    }

    dir_block = traverseDir(blk, dirname[3], 'd');

    if (dir_block != -1) {
      trimDir(dirname, dnameLen);
      empty = checkDirEmpty(dir_block);

      if (empty == true) {
        removeFromRoot(blk, dirname[3], 'd');
        myPM->returnDiskBlock(dir_block);
        return 0;
      }
      else {
        return -2;
      }
    }
    else {
      return -1;
    }

  }
  else if (dnameLen > 4) {
    blk = traverseDir(1, dirname[1], 'd');
    int last_blk = 0;
  // if directory does not exist
    if (blk == -1) {
      return -1;
    }

    for (int i = 3; i < dnameLen; i+=2) {
      if (i < dnameLen - 1) {
        if (last_blk == 0) {
          last_blk = traverseDir(blk, dirname[i], 'd');
        } 
        else {
          last_blk = traverseDir(last_blk, dirname[i], 'd');
        }
        if (last_blk == -1) {
          return -1;
        }
      } else {
        blk = traverseDir(last_blk, dirname[i], 'd');
        if (blk == -1) {
          return -1;
        }
        
      }
    }
    if (blk != -1) {
      trimDir(dirname, dnameLen);
      empty = checkDirEmpty(blk);
      char temp[64];
      if (empty == true) {
        removeFromRoot(last_blk, dirname[dnameLen-1], 'd');
        myPM->returnDiskBlock(blk);
        return 0;
      } else {
        return -2;
      }
    } else {
      return -1;
    }
  }
  return -3; //place holder so there is no warnings when compiling.
}
int FileSystem::openFile(char *filename, int fnameLen, char mode, int lockId)
{
  int flag = 0;
  int correct_loc = 0;
  if (validInput(filename, fnameLen) == -1) {
    return -4;
  }

  if (fnameLen > 2) {
    if (checkValidity(filename, fnameLen) == false) {
      return -4;
    }
  }

  if (findFile(filename, fnameLen) == -1) {
    return -1;
  }
  for (int i = 0; i < totalFiles; i++) {
    if (fileTable[i].filepath == filename) {
      flag = 1;
      correct_loc = i;
      break;
    }
  }
  if (flag != 1) {
    fileTable[totalFiles].filepath = filename;
    fileTable[totalFiles].fLen = fnameLen;
    correct_loc = totalFiles;
    totalFiles++;
  }
  if (mode == 'r' || mode == 'w' || mode == 'm') {
    if ((fileTable[correct_loc].lockId != lockId)) {
      return -3;
    } else if ((fileTable[correct_loc].lockId == -1) && (lockId != -1)) {
      return -3;
    }
    fdesc desc;
    desc.unique_id = instance_id;
    desc.unique_mode = mode;
    desc.rw = 0; // assign the rw pointer to the beginning of the file
    fileTable[correct_loc].opened++;      
    fileTable[correct_loc].file_in.push_back(desc);
    instance_id++;
    return desc.unique_id; // nearest empty spot
  }
  else {
    return -2; // mode is invalid
  }
 return -4; //place holder so there is no warnings when compiling.
}

int FileSystem::closeFile(int fileDesc)
{
  int correct_loc = containsID(fileDesc);
  if (correct_loc == -1) {
    return -1;
  }
  if (fileTable[correct_loc].opened == 0) {
    return -1;
  }

  if (findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen) != -1) {
    //fileTable[fileDesc].lockId = -1;
    for (auto it = fileTable[correct_loc].file_in.begin(); it != fileTable[correct_loc].file_in.end(); it++) {
      if (it->unique_id == fileDesc) {
        fileTable[correct_loc].file_in.erase(it);
        break;
      }
    }
    fileTable[correct_loc].opened -= 1;
    return 0;
  } else {
    return -1;
  }

  return -2; // return an error if it does not work
}

int FileSystem::readFile(int fileDesc, char *data, int len)
{
  fdesc correct_in;
  int correct_rw_in;
  int correct_loc = containsID(fileDesc);
  if (correct_loc == -1) {
    return -1;
  }
  for (int i = 0; i <= fileTable[correct_loc].file_in.size(); i++) {
    if (fileTable[correct_loc].file_in[i].unique_id == fileDesc) {
      correct_in = fileTable[correct_loc].file_in[i];
      correct_rw_in = i;
      correct_in = fileTable[correct_loc].file_in[i];
      break;
    }
  }
  int blknum = findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen);
  if (blknum == -1) {
    return -1;
  }
  if (correct_in.unique_mode == 'w' || correct_in.unique_mode == 'x') {
    return -3;
  }
  if (len < 0) {
    return -2;
  }
  if (fileTable[correct_loc].opened > 0) {
    int sb = 0;
    int blk_loc = 0;
    int data_block = fileTable[correct_loc].file_in[correct_rw_in].rw / 64;
    int blk_offset = 0;
    int buffer_log = 6 + data_block * 4;
    char buffer[64];
    char mainfile_buffer[64];
    if (fileTable[correct_loc].file_in[correct_rw_in].rw % 64 == 0) {
      blk_offset = (fileTable[correct_loc].file_in[correct_rw_in].rw % 64);
    } else {
      blk_offset = (fileTable[correct_loc].file_in[correct_rw_in].rw % 64) + 1;
    }
    myPM->readDiskBlock(blknum, mainfile_buffer);

    if (data_block < 3) {
      blk_loc = charToInt(mainfile_buffer, buffer_log);
    } else {
      blk_loc = charToInt(mainfile_buffer, 18);
      myPM->readDiskBlock(blk_loc, buffer);
      buffer_log = (data_block - 3) * 4;
      blk_loc = charToInt(buffer, buffer_log);
      char buffer[64];
      myPM->readDiskBlock(blk_loc, buffer);
    }
    myPM->readDiskBlock(blk_loc, buffer);
    int pos_in_buffer = blk_offset;
    for (int i = blk_offset; i < blk_offset + len; i++) {
      if (pos_in_buffer + (data_block * 64) > this->getFileSize(blknum)) {
        break;
      }
      if (sb >= getFileSize(blknum)) {
        break;
      }
      if (pos_in_buffer >= 64) {
        data_block++;
        buffer_log = 6 + data_block * 4;
        if (data_block > 2) {
          int indirect_block = charToInt(mainfile_buffer, 18);
          myPM->readDiskBlock(indirect_block, buffer);
          blk_loc = charToInt(buffer, (data_block - 3) * 4);
          if (blk_loc == 0) {
            break;
          }
          myPM->readDiskBlock(blk_loc, buffer);
          pos_in_buffer = 0;
        } else {
          blk_loc = charToInt(mainfile_buffer, buffer_log);
          if (blk_loc == 0) {
            break;
          }
          myPM->readDiskBlock(blk_loc, buffer);
          pos_in_buffer = 0;
        }
      }
      data[sb] = buffer[pos_in_buffer];
      sb++;
      pos_in_buffer++;
    }
    fileTable[correct_loc].file_in[correct_rw_in].rw += sb;
    return sb;
  } else {
    return -3;
  }
  return 0; //place holder so there is no warnings when compiling.
}

int FileSystem::writeFile(int fileDesc, char *data, int len)
{
  fdesc correct_in;
  int correct_rw_in;
  int correct_loc = containsID(fileDesc);
  if (correct_loc == -1) {
    return -1;
  }
  for (int i = 0; i < fileTable[correct_loc].file_in.size(); i++) {
    if (fileTable[correct_loc].file_in[i].unique_id == fileDesc) {
      correct_in = fileTable[correct_loc].file_in[i];
      correct_rw_in = i;
      break;
    }
  }
  //int blknum = findFile(fileTable[correct_loc].filename);
  int blknum = findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen);
  if (blknum == -1) {
    return -1;
  }
  if (correct_in.unique_mode == 'r' || correct_in.unique_mode == 'x') {
    return -3;
  }
  if (len < 0) {
    return -2;
  }
  if (fileTable[correct_loc].opened > 0) {
    int sb = 0;
    int blk_loc = 0;
    int rw_update = 0;
    int blk_offset = 0;
    //Find the block we need to be at
    int data_block = fileTable[correct_loc].file_in[correct_rw_in].rw / 64;
    //Find the starting location of the block to write data to
    // if (fileTable[correct_loc].file_in[correct_rw_in].rw % 64 == 0) {
    //   blk_offset = (fileTable[correct_loc].file_in[correct_rw_in].rw % 64);
    // } else {
    //   blk_offset = (fileTable[correct_loc].file_in[correct_rw_in].rw % 64) + 1;
    // }
    blk_offset = fileTable[correct_loc].file_in[correct_rw_in].rw % 64;
    //Convert the block to the buffer location in file
    int buffer_log = 6 + data_block * 4;
    char buffer[64];
    char mainfile_buffer[64];
    myPM->readDiskBlock(blknum, mainfile_buffer);
    if (data_block < 3) {
      blk_loc = charToInt(mainfile_buffer, buffer_log);
    } else {
      blk_loc = charToInt(mainfile_buffer, 18);
      myPM->readDiskBlock(blk_loc, buffer);
      buffer_log = (data_block - 3) * 4;
      blk_loc = charToInt(buffer, buffer_log);
    }
    myPM->readDiskBlock(blk_loc, buffer);
    int pos_in_buffer = blk_offset;
    for (int i = blk_offset; i < blk_offset + len; i++) {
      if (pos_in_buffer >= 64) {
        myPM->writeDiskBlock(blk_loc, buffer);
        data_block++;
        buffer_log = 6 + data_block * 4;
        if (data_block > 2) {
          blk_loc = charToInt(mainfile_buffer, buffer_log);
          if (blk_loc == 0) {
            this->setDirectFileBlock(blknum);
          }
          myPM->readDiskBlock(blknum, mainfile_buffer);
          blk_loc = charToInt(mainfile_buffer, 18);
          myPM->readDiskBlock(blk_loc, buffer);
          //cout << "BUFFER: " << buffer << endl;
          buffer_log = (data_block - 3) * 4;
          myPM->readDiskBlock(blk_loc, buffer);
          blk_loc = charToInt(buffer, buffer_log);
          myPM->readDiskBlock(blk_loc, buffer);
          //cout << "BLOCK LOC: " << blk_loc << endl;
          pos_in_buffer = 0;
        } else {
          blk_loc = charToInt(mainfile_buffer, buffer_log);
          if (blk_loc == 0) {
            this->setDirectFileBlock(blknum);
          }
          myPM->readDiskBlock(blknum, mainfile_buffer);
          blk_loc = charToInt(mainfile_buffer, buffer_log);
          myPM->readDiskBlock(blk_loc, buffer);
          //cout << "BUFFER: " << buffer << endl;
          //cout << "BLOCK LOC: " << blk_loc << endl;
          pos_in_buffer = 0;
        }
      }
      if (buffer[pos_in_buffer] == '#') {
        rw_update+=1;
      }
      buffer[pos_in_buffer] = data[sb];
      sb++;
      pos_in_buffer++;
    }
    myPM->writeDiskBlock(blk_loc, buffer);
    fileTable[correct_loc].file_in[correct_rw_in].rw += sb;
    this->setFileSize(blknum, this->getFileSize(blknum) + rw_update);
    return sb;
  }
  else {
    return -3;
  }
}

int FileSystem::appendFile(int fileDesc, char *data, int len)
{
  fdesc correct_in;
  int correct_rw_in;
  int correct_loc = containsID(fileDesc);
  if (correct_loc == -1) {
    return -1;
  }
  for (int i = 0; i <= fileTable[correct_loc].file_in.size(); i++) {
    if (fileTable[correct_loc].file_in[i].unique_id == fileDesc) {
      correct_in = fileTable[correct_loc].file_in[i];
      correct_rw_in = i;
      break;
    }
  }
  if (correct_in.unique_mode == 'r' || correct_in.unique_mode == 'x') {
    return -3;
  }
  int blknum = findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen);
  if (blknum == -1) {
    return -1;
  }
  if(getFileSize(blknum)+len>1216){
    return -3;
  }
  if (len < 0) {
    return -2;
  }
  if (fileTable[correct_loc].opened > 0) {
    int sb = 0;
    int pos_in_buffer = 0;
    char buffer[64];
    int freeBlock = this->findFirstEmpty(blknum);
    if (freeBlock == -1) {
      return -3;
    }
    myPM->readDiskBlock(freeBlock, buffer);
    for (int i = 0; i < 64; i++) {
      if(getFileSize(blknum)+sb>1216){
        break;
      }
      if (buffer[i] == '#') {
        buffer[i] = data[pos_in_buffer];
        sb++;
        pos_in_buffer++;
      }
      if (sb >= len) {
        if (i < 64 && buffer[i] == '#') {
          buffer[i] = data[pos_in_buffer];
        }
        break;
      }
      if (i == 63 && sb < len) {
        myPM->writeDiskBlock(freeBlock, buffer);
        freeBlock = this->findFirstEmpty(blknum);
        if (freeBlock == -1) {
          break;
        }
        myPM->readDiskBlock(freeBlock, buffer);
        i = 0;
        pos_in_buffer = 0;
      }
    }
    myPM->writeDiskBlock(freeBlock, buffer);
    fileTable[correct_loc].file_in[correct_rw_in].rw += sb;
    this->setFileSize(blknum, this->getFileSize(blknum) + sb);
    return sb;
  }
 return -1; //place holder so there is no warnings when compiling.
}

int FileSystem::truncFile(int fileDesc, int offset, int flag) {
  int correct_loc = containsID(fileDesc);
  int correct_rw_in;
  int bytesToDelete = 0;
  if (correct_loc == -1) {
    return -1;
  }

  for (int i = 0; i <= fileTable[correct_loc].file_in.size(); i++) {
    if (fileTable[correct_loc].file_in[i].unique_id == fileDesc) {
      correct_rw_in = i;
      break;
    }
  }
  fdesc& fileDescriptor = fileTable[correct_loc].file_in[correct_rw_in]; 
  int blknum = findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen);
  if (blknum == -1) {
    return -1;
  }
  if (flag < 0 && offset < 0) {
    return -1;
  }
  if (fileDescriptor.unique_mode == 'r' || fileDescriptor.unique_mode == 'x') {
    return -3;
  }

  if ((flag == 0 && offset < 0) || flag == 0 && offset >= 0) {
    fileTable[correct_loc].file_in[correct_rw_in].rw += offset;
    //GREATER THAN OR EQUAL TO BECAUSE CANT BE THE END OF THE FILE
    if (fileTable[correct_loc].file_in[correct_rw_in].rw > getFileSize(blknum)) {
      fileTable[correct_loc].file_in[correct_rw_in].rw = 0;
    } else if (fileTable[correct_loc].file_in[correct_rw_in].rw < 0) {
      fileTable[correct_loc].file_in[correct_rw_in].rw = 0;
    }
  } else if (flag > 0 && offset >= 0) {
    //GREATER THAN OR EQUAL TO BECAUSE CANT BE THE END OF THE FILE
    if (offset > getFileSize(blknum)) {
      return -2;
    } else if (offset < 0) {
      return -2;
    } else {
      fileTable[correct_loc].file_in[correct_rw_in].rw = offset;
    }
  } else if (flag > 0 && offset < 0) {
    return -1;
  }

  if (fileTable[correct_loc].file_in[correct_rw_in].rw < 0 || fileTable[correct_loc].file_in[correct_rw_in].rw > getFileSize(blknum)) {
    return -2;
  }

  int blockIndex = fileTable[correct_loc].file_in[correct_rw_in].rw / 64;
  int offsetInBlock = fileTable[correct_loc].file_in[correct_rw_in].rw % 64;
  vector<int> rm_blocks = getTruncBlks(blockIndex, blknum);
  for (int i = 0; i < rm_blocks.size(); i++) {
    char buffer[64];
    myPM->readDiskBlock(rm_blocks[i], buffer);
    if (i == 0) {
      for (int x = offsetInBlock; x < 64; x++) {
        if (buffer[x] != '#') {
          buffer[x] = '#';
          bytesToDelete++;
        }
      }
      myPM->writeDiskBlock(rm_blocks[i], buffer);
    } else {
      for (int x = 0; x < 64; x++) {
        if (buffer[x] != '#') {
          buffer[x] = '#';
          bytesToDelete++;
        }
      }
      myPM->writeDiskBlock(rm_blocks[i], buffer);
    }
  }
  char main_buf[64];
  for (int i = rm_blocks.size(); i > 0; i--) {
    myPM->readDiskBlock(blknum, main_buf);
    bool flag = true;
    for (int x = 6; x <= 18; x+=4) {
      if ((charToInt(main_buf, x) != 0) && (charToInt(main_buf, x) == rm_blocks[i])) {
        myPM->returnDiskBlock(rm_blocks[i]);
        intToChar(main_buf, 0, x);
        myPM->writeDiskBlock(blknum, main_buf);
        flag = false;
      }
    }
    if (flag == true) {
      char inode_buf[64];
      myPM->readDiskBlock(charToInt(main_buf, 18), inode_buf);
      for (int x = 0; x < 64; x+=4) {
        if ((charToInt(inode_buf, x) != 0) && (charToInt(inode_buf, x) == rm_blocks[i])) {
          intToChar(inode_buf, 0, x);
          myPM->writeDiskBlock(charToInt(main_buf, 18), inode_buf);
          myPM->returnDiskBlock(rm_blocks[i]);
          if (x == 0) {
            myPM->returnDiskBlock(charToInt(main_buf, 18));
            intToChar(main_buf, 0, 18);
            myPM->writeDiskBlock(blknum, main_buf);
          }
        }
      }
    }
  }
  setFileSize(blknum, getFileSize(blknum) - bytesToDelete);
  return bytesToDelete;
}
int FileSystem::seekFile(int fileDesc, int offset, int flag)
{
  int correct_in;
  int correct_loc = containsID(fileDesc);
  if (correct_loc == -1) {
    return -1;
  }
  for (int i = 0; i <= fileTable[correct_loc].file_in.size(); i++) {
    if (fileTable[correct_loc].file_in[i].unique_id == fileDesc) {
      correct_in = i;
      break;
    }
  }
 if (findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen) == -1) {
   return -1;
 }
 int blknum = findFile(fileTable[correct_loc].filepath, fileTable[correct_loc].fLen);
 if (flag == 0) {
    if ((fileTable[correct_loc].file_in[correct_in].rw + offset) >= this->getFileSize(blknum)) {
      fileTable[correct_loc].file_in[correct_in].rw = 0;
    } else if ((fileTable[correct_loc].file_in[correct_in].rw + offset) < 0) {
      return -2;
    } else {
      fileTable[correct_loc].file_in[correct_in].rw += offset;
    }
   return 0;
 } else if (flag >= 0) {
   fileTable[correct_loc].file_in[correct_in].rw = offset;
   return 0;
 } else if (flag < 0 && offset == 0) {
   fileTable[correct_loc].file_in[correct_in].rw = 0;
   return 0;
 } else if (flag < 0 && offset > 0) {
   if ((fileTable[correct_loc].file_in[correct_in].rw + offset) < this->getFileSize(blknum)) {
    fileTable[correct_loc].file_in[correct_in].rw = offset;
   }
   return 0;
 }
 return -1; //place holder so there is no warnings when compiling.
}

int FileSystem::renameFile(char *filename1, int fnameLen1, char *filename2, int fnameLen2)
{
 int blknum = findFile(filename1, fnameLen1);
 char buffer[64];
 int index = 0;
 int dir_index = 0;
 bool flag = false;
 int dir_block = 0;
 char dirbuff[64];
 //-1 is invalid file name
 if (validInput(filename1, fnameLen1) == -1 || validInput(filename2, fnameLen2) == -1) {
   return -1;
 }
 //-2 file no exist
 if (blknum == -1) {
   return -2;
 }
 //-3 for new file name already exist
 if (findFile(filename2, fnameLen2) != -1) {
   return -3;
 }
 if (fnameLen1 == 2) {
    int update = 0;
    myPM->readDiskBlock(1, dirbuff);
    while (dir_block == 0) {
      for (int i = 0; i < 60; i++) {
        if (dirbuff[i] == filename1[1]) {
          if (update == 0) {
            dir_block = 1;
          } else {
            dir_block = update;
          }
          dir_index = i;
          break;
        }
      }
      if (dir_block == 0) {
        update = charToInt(dirbuff, 60);
        myPM->readDiskBlock(update, dirbuff);
      } else {
        break;
      }
    }
  } else if (fnameLen1 == 4) {
    dir_block = traverseDir(1, filename1[1], 'd');
    if (dir_block == -1) {
      return -3;
    }
    blknum = traverseDir(dir_block, filename1[3], 'f');
    if (blknum == -1) {
      return -1;
    }
    myPM->readDiskBlock(dir_block, dirbuff);
    for (int i = 0; i < 60; i += 6) {
      if (dirbuff[i] == filename1[3] && dirbuff[i+5] == 'f') {
        dir_index = i;
      }
   }
 } else if (fnameLen1 > 4) {
    blknum = traverseDir(1, filename1[1], 'd');
    dir_block = 0;
    // if directory does not exist
    if (blknum == -1) {
      return -1;
    }

    for (int i = 3; i < fnameLen1; i+=2) {
      if (i < fnameLen1 - 1) {
        if (dir_block == 0) {
          dir_block = traverseDir(blknum, filename1[i], 'd');
        } 
        else {
          dir_block = traverseDir(dir_block, filename1[i], 'd');
        }
        if (dir_block == -1) {
          return -1;
        }
      }
      else {
        blknum = traverseDir(dir_block, filename1[i], 'f');
        if (blknum == -1) {
          return -1;
        }
      }
  }
 myPM->readDiskBlock(dir_block, dirbuff);
 for (int i = 0; i < 60; i += 6) {
   if (dirbuff[i] == filename1[fnameLen1-1] && dirbuff[i+5] == 'f') {
     dir_index = i;
     break;
   }
  }
 }
 for (int i = 0; i < totalFiles; i++) {
   //Found file, no need to put it in the table
   if (fileTable[i].filepath == filename1) {
     flag = true;
     break;
   }
 }
 //Couldn't find file in table, add it
 if (flag == false) {
   fileTable[totalFiles].filepath = filename1;
   fileTable[totalFiles].fLen = fnameLen1;
   totalFiles++;
 }
 for (int i = 0; i < totalFiles; i++) {
   if (fileTable[i].filepath == filename1) {
     index = i;
     if (fileTable[i].opened != 0 || fileTable[i].lockId != -1) {
       //-4 for opened/locked file
       return -4;
     }
     myPM->readDiskBlock(blknum, buffer);
     buffer[0] = filename2[fnameLen2-1];
     myPM->writeDiskBlock(blknum, buffer);
     fileTable[index].filepath = filename2;
     fileTable[index].fLen = fnameLen2;
     dirbuff[dir_index] = filename2[fnameLen2-1];
     myPM->writeDiskBlock(dir_block, dirbuff);
     return 0;
   }
 }
 return -5; //place holder so there is no warnings when compiling.
}

int FileSystem::renameDirectory(char *dirname1, int dnameLen1, char *dirname2, int dnameLen2)
{
 int blk = 0;
 int last_blk = 0;
 char buffer[64];
 if (validInput(dirname1, dnameLen1) == -1 || validInput(dirname2, dnameLen2) == -1) {
   return -1;
 }
 if (dnameLen2 == 2) {
   last_blk = traverseDir(1, dirname2[dnameLen2-1], 'd');
   if (last_blk != -1) {
      return -3;
   }
 }
 if (dnameLen1 == 2) {
   int x = 0;
   last_blk = traverseDir(1, dirname1[1], 'd');
   if (last_blk == -1) {
     return -2;
   }
   myPM->readDiskBlock(1, buffer);
   bool flag = false;
   while (flag == false) {
     for (int i = 0; i < 64; i++) {
       if (buffer[i] == dirname1[1] && buffer[i+5] == 'd') {
         buffer[i] = dirname2[dnameLen2-1];
         flag = true;
         break;
       }
     }
     if (flag == false) {
       x = charToInt(buffer, 60);
       if (x != 0) {
        myPM->readDiskBlock(x, buffer);
       } else {
         flag = true;
         break;
       }
     }
   }
   myPM->writeDiskBlock(x, buffer);
   return 0;
 }
 if (dnameLen1 == 4) {
   last_blk = 0;
   blk = traverseDir(1, dirname1[1], 'd');
   if (blk == -1) {
     return -2;
   }
   last_blk = traverseDir(blk, dirname1[3], 'd');
   if (last_blk == -1) {
      return -2;
    }
    myPM->readDiskBlock(blk, buffer);
    for (int i = 0; i < 64; i++) {
      if (buffer[i] == dirname1[3]) {
        buffer[i] = dirname2[dnameLen2-1];
      }
    }
    myPM->writeDiskBlock(blk, buffer);
    return 0;
 }
 if (dnameLen1 > 4) {
  blk = traverseDir(1, dirname1[1], 'd');
  last_blk = 0;
  // if directory does not exist
  if (blk == -1) {
    return -2;
  }

  for (int i = 3; i < dnameLen1; i+=2) {
    if (i < dnameLen1 - 1) {
      if (last_blk == 0) {
        last_blk = traverseDir(blk, dirname1[i], 'd');
      } else {
        last_blk = traverseDir(last_blk, dirname1[i], 'd');
      }
      if (last_blk == -1) {
        return -2;
      }
    } else {
      blk = traverseDir(last_blk, dirname1[i], 'd');
      if (blk == -1) {
        return -2;
      }
    }
  }
  myPM->readDiskBlock(last_blk, buffer);
  for (int i = 0; i < 64; i++) {
    if (buffer[i] == dirname1[dnameLen1-1]) {
      buffer[i] = dirname2[dnameLen2-1];
    }
  }
  myPM->writeDiskBlock(last_blk, buffer);
  return 0;
 }
 return -4; //place holder so there is no warnings when compiling.
}

int FileSystem::getAttributes(char *filename, int fnameLen, char *returnbuffer/* ... and other parameters as needed */)
{
  if (validInput(filename, fnameLen) == -1) {
    return -1;
  }
  int fileblk = findFile(filename, fnameLen);
  // cannot get attributes on a non-existent file
  if (fileblk == -1) {
    return -2;
  }

  char buffer[64];
  myPM->readDiskBlock(fileblk, buffer);
  for (int i = 0; i < 5; i++) {
    returnbuffer[i] = buffer[22+i];
  }
  return 0;
}

int FileSystem::setAttributes(char *filename, int fnameLen, int user, char attribute/* ... and other parameters as needed */)
{
  if (validInput(filename, fnameLen) == -1 || user < 1) {
    return -1;
  }
  int fileblk = findFile(filename, fnameLen);
  // cannot set attributes on a non-existent file
  if (fileblk == -1) {
    return -2;
  }

  char buffer[64];
  myPM->readDiskBlock(fileblk, buffer);
  intToChar(buffer, user, 22);
  buffer[26] = attribute;
  myPM->writeDiskBlock(fileblk, buffer);
  return 0; //place holder so there is no warnings when compiling.
}

bool FileSystem::checkLastAddressEmpty(int dirblk) {
  char buffer[64];
  myPM->readDiskBlock(dirblk, buffer);
  for (int i = 60; i < 64; i++) {
    if (buffer[i] != '#') {
      return false;
    }
  }
  return true;
}

void FileSystem::trimDirRec(int prevdirblk, int dirblk) {
  char buffer[64];

  if (checkDirEmpty(dirblk) && checkLastAddressEmpty(dirblk)) {
    myPM->readDiskBlock(prevdirblk, buffer);
    int x = charToInt(buffer, 60);
    myPM->returnDiskBlock(x);
    for (int i = 60; i < 64; i++) {
      buffer[i] = '#';
    }
    myPM->writeDiskBlock(prevdirblk, buffer);
  } else if (checkLastAddressEmpty(dirblk) == false) {
    myPM->readDiskBlock(dirblk, buffer);
    int nxtblk = charToInt(buffer, 60);
    trimDirRec(dirblk, nxtblk);
  }
}

void FileSystem::trimDir(char *path, int plen) {
  int dirblk = traverseDirectory(path, plen);
  trimDirRec(dirblk, dirblk);
}
