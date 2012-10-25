#include "../cdb/tfc_cache_hash_map.h"
using namespace tfc::cache;

#include <string>
#include <iostream>
using namespace std;

#include "cdb_tool_comm.h"

#define MB (1024*1024)

void
usage(const char* appname)
{
    cout << "usage: " << appname << " <shm_size_in_MB> <total_node> <bucket_size> <n_chuncks> <chunck_size>"
         << endl;
}

int
main(int argc, char* argv[])
{
    if (argc < 6) {
        usage(argv[0]);
        exit(1);
    }

    long total_node = atoi(argv[2]);
    long bucket_size = atoi(argv[3]);
    long n_chunks = atoi(argv[4]);
    long chunck_size = atoi(argv[5]);

    long need_shm_size = CHashMap::get_total_pool_size(total_node, bucket_size, n_chunks, chunck_size);

    long data_size = n_chunks*chunck_size + total_node*16; // current we use md5 as key, which is 16bytes in tfc_md5.cpp

    long user_shm_size = atoi(argv[1])*MB;

    cout << "input shm size: " << user_shm_size << " bytes" << endl;
    cout << "at least shm size needed: " << need_shm_size << " bytes" << endl;
    cout << "data size calculated: " << data_size << " bytes" << endl;
    cout << "(at least shm size needed)/(input shm size) = : " << ((double)need_shm_size)/user_shm_size << endl;
    cout << "(data size calculated)/(at least shm size needed) = : " << ((double)data_size)/need_shm_size << endl;
    cout << "(data size calculated)/(input shm size) = : " << ((double)data_size)/user_shm_size << endl;

    return 0;
}

