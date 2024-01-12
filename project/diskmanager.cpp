#include "disk.h"
#include "diskmanager.h"
#include <iostream>
using namespace std;

DiskManager::DiskManager(Disk *d, int partcount, DiskPartition *dp)
{
  myDisk = d;
  partCount = partcount;
  int r = myDisk->initDisk();
  char buffer[64];
  
  for(int i = 0; i < 64; i++)
  {
    buffer[i] = '#';
  }
  diskP = new DiskPartition[partCount];
  /* If needed, initialize the disk to keep partition information */

  diskP = dp;
  
  if(r == 1) {
    intToChar(buffer, partCount, 0);
    for(int i = 0; i < partCount; i++) {
      buffer[4 + (i * 5)] = diskP[i].partitionName;
      intToChar(buffer, dp[i].partitionSize, 4 + ((i * 5) + 1));
    }
    myDisk->writeDiskBlock(0, buffer); // this writes the superblock
  }
  else if (r == 0) { 
    myDisk->readDiskBlock(0, buffer);
    partCount = charToInt(buffer, 0);

    for(int i = 0; i < partCount; i++)
    {
      diskP[i].partitionName = buffer[4 + (i * 5)];
      diskP[i].partitionSize = charToInt(buffer, 4 + ((i * 5) + 1));
    }
  }
}
/*
 *   returns: 
 *   0, if the block is successfully read;
 *  -1, if disk can't be opened; (same as disk)
 *  -2, if blknum is out of bounds; (same as disk)
 *  -3 if partition doesn't exist
 */
int DiskManager::readDiskBlock(char partitionname, int blknum, char *blkdata)
{
  /* write the code for reading a disk block from a partition */

  int length = 1; // starting at 1 to account for the superblock

  for(int i = 0; i < 3; i++) { // less than 3 because the maximum number of partitions is 3
    if(diskP[i].partitionName == partitionname) { // can use == here because we are comparing chars
      return myDisk->readDiskBlock(length + blknum, blkdata); // accounting for length of previous partitions and superblock
    }
    else {
      length += diskP[i].partitionSize;
    }
  }
  
  return -3;
}


/*
 *   returns: 
 *   0, if the block is successfully written;
 *  -1, if disk can't be opened; (same as disk)
 *  -2, if blknum is out of bounds;  (same as disk)
 *  -3 if partition doesn't exist
 */
int DiskManager::writeDiskBlock(char partitionname, int blknum, char *blkdata)
{
  /* write the code for writing a disk block to a partition */

  int length = 1; // starting at 1 to account for the superblock

  for(int i = 0; i < 3; i++) { // less than 3 because the maximum number of partitions is 3
    if(diskP[i].partitionName == partitionname) { // can use == here because we are comparing chars
      return myDisk->writeDiskBlock(length + blknum, blkdata); // accounting for length of previous partitions and superblock
    }
    else {
      length += diskP[i].partitionSize;
    }
  }

  return -3;
}

/*
 * return size of partition
 * -1 if partition doesn't exist.
 */
int DiskManager::getPartitionSize(char partitionname)
{
  /* write the code for returning partition size */

  for(int i = 0; i < 3; i++) { // less than 3 because the maximum number of partitions is 3
    if(diskP[i].partitionName == partitionname) { // can use == here because we are comparing chars
      return diskP[i].partitionSize; // return the partition size of the given partition
    }
  }

  return -1; //place holder so there is no warnings when compiling. 
}

int DiskManager::charToInt(char *buffer, int pos) {
    int charint = 0;
    for (int i = 0; i < 4; i++) {
        charint *= 10;
        charint += (buffer[pos + i] - '0');
    }
    return charint;
}

void DiskManager::intToChar(char *buffer, int num, int pos) {
    for (int i = 3; i >= 0; i--) {
        int new_pos = pos + i;
        buffer[new_pos] = '0' + num % 10;
        num /= 10;
    }
}
