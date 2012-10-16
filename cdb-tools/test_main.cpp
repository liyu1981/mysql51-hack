#include <iostream>
#include "../cdb/cdb.h"

using namespace std;

int main()
{
    if(!init_cdb_shm_mgr("/data/mysql/2"))
    {
        cerr << "init error" << endl;
        exit(1);
    }

    cout << "init success" << endl;

    shutdown_cdb_shm_mgr();
    return 0;
}
