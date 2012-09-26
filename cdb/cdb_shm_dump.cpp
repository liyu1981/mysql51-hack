#include "../sql/cdb.h"
#include "../sql/cdb_shm_mgr.h"
#include "../sql/tfc_shm_map.h"
#include "../sql/tfc_spin_lock.h"
using namespace cdb;

#include <string>
#include <iomanip>
#include <iostream>
using namespace std;

void
usage(const char* appname)
{
    cout << "usage: " << appname << " <mysql_data_path> <shm_name>"
         << endl;
}

const char* type_names[] = {
    "NOTNAME",
    "SELECT"
};

void
dump_ins_dml(const CDBShm& s)
{
    TfcShmMap<CDBInsDmlOpKey, CDBInsDmlOp> m;
    m._ca = s._ca;

    cout << "shm[" << s._name << "] addr " << hex << s._addr
         << " key " << s._key
         << " size " << dec << s._size << endl;
    cout << "#type result total time_sum time_min time_max" << endl;

    spin_lock(s._lock);
    for (TfcShmMap<CDBInsDmlOpKey, CDBInsDmlOp>::iterator it = m.begin();
         it != m.end();
         it++) {
         CDBInsDmlOp entry;
         if (it.extract(entry) == 0) {
            cout << type_names[entry._key._type] << " "
                 << entry._key._result << " "
                 << entry._total << " "
                 << fixed << setprecision(3)
                 << entry._time_sum*1000 << " "
                 << entry._time_min*1000 << " "
                 << entry._time_max*1000 << " "
                 << endl;
         }
    }
    spin_unlock(s._lock);
}

int
main(int argc, char* argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }

    if (!attach_cdb_shm_mgr(argv[1])) {
        cerr << "init_cdb_shm_mgr error: " << cdb_errno << "(" << cdb_2nd_errno << ")" << endl;
        exit(cdb_errno);
    }

    CDBShmMgr& sm = CDBShmMgr::getInstance();
    CDBShm& s = sm.get(argv[2]);
    if (s._addr == 0) {
        cerr << "can not found shm " << argv[2] << endl;
        exit(2);
    }

    if (strcmp(argv[2], "cdb_ins_dml") == 0)
        dump_ins_dml(s);

    sm.detach_all();
    return 0;
}

