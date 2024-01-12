#include "disk.h"
#include "diskmanager.h"
#include "partitionmanager.h"
#include <iostream>
using namespace std;


PartitionManager::PartitionManager(DiskManager *dm, char partitionname, int partitionsize)
{
  myDM = dm;
  myPartitionName = partitionname;
  myPartitionSize = myDM->getPartitionSize(myPartitionName);
  char buffer[64];

  /* If needed, initialize bit vector to keep track of free and allocted
     blocks in this partition */
  //Need to make sure block 1 of the partition is not free

  myDM->readDiskBlock(partitionname, 0, buffer);
  bitblock = new BitVector(partitionsize);
  if(buffer[0] == '#'){
    bitblock->setBit(0);
    bitblock->getBitVector((unsigned int*) buffer);
    myDM->writeDiskBlock(partitionname, 0, buffer);
  } else {
    bitblock->setBitVector((unsigned int*) buffer);
  }
}

PartitionManager::~PartitionManager()
{
}

/*
 * return blocknum, -1 otherwise
 */
int PartitionManager::getFreeDiskBlock()
{
  for (int i = 0; i < myPartitionSize; i++) {
    if (bitblock->testBit(i) == OFF) { //Tests to see if the block is free
      bitblock->setBit(i); //Where i is the block number
      char buffer[64];
      bitblock->getBitVector((unsigned int *) buffer); //Need to get value of actual BitVector
      myDM->writeDiskBlock(myPartitionName, 0, buffer); //Write data to block 0 as specified in project-info.
      return i;
    }
  }
  /* write the code for allocating a partition block */
  return -1; //place holder so there are no compiler warnings
}

/*
 * return 0 for sucess, -1 otherwise
 */
int PartitionManager::returnDiskBlock(int blknum)
{
  if (bitblock->testBit(blknum) == ON) { //Test to see if bit is allocated
    bitblock->resetBit(blknum); //Deallocate bit
    char buffer[64];
    bitblock->getBitVector((unsigned int *) buffer);
    myDM->writeDiskBlock(myPartitionName, 0, buffer); //Write updated bitvector to diskblock
    
    for (int i = 0; i < 64; i++) {
      buffer[i] = '#'; //Need to write blanks to the returned block number
    }
    myDM->writeDiskBlock(myPartitionName, blknum, buffer); //Write updated bitvector to diskblock
    return 0;
  }
  return -1; //place holder so there are no compiler warnings
}


int PartitionManager::readDiskBlock(int blknum, char *blkdata)
{
  return myDM->readDiskBlock(myPartitionName, blknum, blkdata);
}

int PartitionManager::writeDiskBlock(int blknum, char *blkdata)
{
  return myDM->writeDiskBlock(myPartitionName, blknum, blkdata);
}

int PartitionManager::getBlockSize() 
{
  return myDM->getBlockSize();
}
