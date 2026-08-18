#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>



int main() {
    Disk disk_run;

    unsigned char blockBuffer[BLOCK_SIZE];

    int blockNumber = 0;

    int status = Disk::readBlock(blockBuffer, blockNumber);

    if (status != SUCCESS) {
        std::cout << "Failed to read block "
                  << blockNumber
                  << ". Error code: "
                  << status
                  << std::endl;

        return 1;
    }

    for(int i=0; i < BLOCK_SIZE; ++i) {
        std::cout << static_cast<int>(blockBuffer[i]) << " ";
    }
    std::cout << std::endl;

    return 0;
}