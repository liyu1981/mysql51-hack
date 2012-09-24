#include "../sql/cdb.h"
#include "../sql/cdb_shm_mgr.h"
using namespace cdb;

#include <string>
#include <iostream>
using namespace std;

void
usage(const char* appname)
{
    cout << "usage: " << appname << " <mysql_data_path> <shm_name>"
         << endl;
}

int
main(int argc, char* argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }

    if (!init_cdb_shm_mgr(argv[1])) {
        cerr << "init_cdb_shm_mgr error: " << cdb_errno << "(" << cdb_2nd_errno << ")" << endl;
        exit(cdb_errno);
    }

    CDBShmMgr& sm = CDBShmMgr::getInstance();
    CDBShm& s = sm.get(argv[2]);

    cout << "shm[" << s._name << "] addr " << hex << s._addr << " size " << s._size << endl;

    sm.detach_all();
    return 0;
}

