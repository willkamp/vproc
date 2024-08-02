#include "VProcClass.h"
#include <iostream>

#define LINKAGE extern "C"

unsigned bit_swap(unsigned val){
    unsigned res = 0;
    for (int i = 0; i < sizeof(val)*8; i ++){
        res = (res << 1) | (val & 1);
        val = val >> 1;
    }
    return res;
}

LINKAGE void VUserMain0(void){

    auto proc = new VProc(0);
    unsigned address;
    uint32_t buffer[16];
    const unsigned slave_memory_depth = 0x1000;

    std::cout << "Starting Virtual Processor. Going to read data from the memory, and write it back with bits swapped." << std::endl;

    address = 0;
    std::cout << "Doing some single interleaved reads and writes." << std::endl;
    for (int i = 0; i < sizeof(buffer)/4; i++) {
        proc->read(address, &buffer[i]);
        proc->write(address, bit_swap(buffer[i]));
        address += 4;
    }
    proc->tick(10);

    /* Appears the Vunit Avalon Slave doesn't handle write byte enables */
    // std::cout << "Doing some single byte reads and writes to exercise byte enables." << std::endl;
    // for (int i = 0; i < sizeof(buffer); i++) {
    //     unsigned tmp;
    //     for (int b = 3; b >= 0; b-- ){
    //         proc->readByte(address + b, &buffer[b]);
    //         tmp = (tmp << 8) | buffer[b];
    //         std::cout << "buffer[b] = " << buffer[b] << ", tmp = 0x" << std::hex << tmp << std::endl;
    //     }
    //     tmp = bit_swap(tmp);
    //     std::cout << "tmp = 0x" << std::hex << tmp << std::endl;
    //     for (int b = 3; b >= 0; b-- ){
    //         proc->writeByte(address + b, (tmp >> (b*8)) & 0xFF );
    //     }
    //     address += 4;
    // }  
    // proc->tick(1);

    std::cout << "Doing some burst interleaved reads and writes of different sizes." << std::endl;
    int block_size_bytes = 0;
    while(address < slave_memory_depth){
        block_size_bytes = (block_size_bytes + 4) % sizeof(buffer);
        block_size_bytes = block_size_bytes == 0 ? 4 : block_size_bytes;
        std::cout << "Block Size of " << block_size_bytes << " bytes @ 0x" << std::hex << address << std::endl;
        proc->burstReadBytes(address, buffer, block_size_bytes);
        for (int i = 0; i < block_size_bytes/(sizeof(buffer[0])); i++) {
            buffer[i] = bit_swap(buffer[i]);
        }
        proc->burstWriteBytes(address, buffer, block_size_bytes);
        address += block_size_bytes;

        if (address + block_size_bytes > slave_memory_depth) {
            std::cout << "Almost at the end of the memory." << std::endl;
            break;
        }
    }
    proc->tick(10);

    // Bug in VUnit AvalonMM Master. Making a non-burst read does not set the burst_count back to 1.
    // Work around - make an extra one cycle burst read.
    proc->burstReadBytes(address, buffer, 4);

    std::cout << "Finish with some single interleaved reads and writes." << std::endl;
    while(address < slave_memory_depth){
        unsigned tmp;
        proc->read(address, &tmp);
        proc->write(address, bit_swap(tmp));
        address += 4;
    }

    proc->tick(0x7FFFFFFF); // signal end of code. Wait forever.
}