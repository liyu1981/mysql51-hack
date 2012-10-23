#include "cdb.h"
#include "cdb_shm_mgr.h"
#include "tfc_shm_map.h"
#include "tfc_spin_lock.h"
using namespace cdb;

#include <string>
#include <iomanip>
#include <iostream>
using namespace std;

#include "cdb_tool_comm.h"

CDBShmPairConf shm_pair_conf_array[] = {
    //name, shm_name1, shm_name2, conf_index, map_file
    {"cdb_ins_dml", "cdb_ins_dml_1", "cdb_ins_dml_2", 0, "cdb_ins_dml_map.txt"}
};
int shm_pair_conf_size = sizeof(shm_pair_conf_array)/sizeof(shm_pair_conf_array[0]);

void
usage(const char* appname)
{
    cout << "usage: " << appname << " <mysql_data_path> <shm_pair_name>"
         << endl;
}

const char* type_names[] = {
    "NOTNAME",
    "SELECT",
    "INSERT",
    "UPDATE",
    "REPLACE",
    "DELETE"
};
const int dml_type_num = 6;

string
parse_pair_map_file(const char* mysqld_data_path, const char* pair_name)
{
    int found = -1;
    for (int i=0; i<shm_pair_conf_size; ++i) {
        CDBShmPairConf& c = shm_pair_conf_array[i];
        if (strcmp(c._name.c_str(), pair_name) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0)
        return string("");

    CDBShmPairConf& found_c = shm_pair_conf_array[found];
    string pair_map_file_path = string(mysqld_data_path) + "/" + string(found_c._map_file);
    FILE* fp = fopen(pair_map_file_path.c_str(), "r");
    if (fp == NULL)
        return string("");

    int d1, d2, d3, d4;
    char s1[64] = {0};
    char s2[64] = {0};
    fscanf(fp, "last_switch_time: %d %d current: %s %d standby: %s %d", &d1, &d2, &s1, &d3, &s2, &d4);
    string ret = string(s2);

    fclose(fp);
    return ret;
}

bool
check_standby(const CDBShm& s)
{
    char* p = (char*)s._meta_addr;
    p = p + sizeof(struct timeval);
    cdb_errno = *p;
    return *p==0;
}

void
dump_ins_dml(const CDBShm& s)
{
    TfcShmMap<CDBInsDmlOpKey, CDBInsDmlOp> m;
    m._ca = s._ca;

    cout << "shm[" << s._name << "] addr " << hex << s._addr
         << " key " << dec << s._key
         << " size " << dec << s._size << endl;
    cout << "#type result total time_sum time_min time_max >20ms >40ms >60ms >80ms >100ms >500ms >1s >2s >10s" << endl;

    for (TfcShmMap<CDBInsDmlOpKey, CDBInsDmlOp>::iterator it = m.begin(); it != m.end(); it++) {
        CDBInsDmlOp entry;
        if (it.extract(entry) == 0) {
            if( entry._key._type >= 0 && entry._key._type < dml_type_num ) {
                cout << type_names[entry._key._type] << " "
                    << entry._key._result << " "
                    << entry._comm_stat._total << " "
                    << fixed << setprecision(3)
                    << entry._comm_stat._time_sum*1000 << " "
                    << entry._comm_stat._time_min*1000 << " "
                    << entry._comm_stat._time_max*1000 << " ";
                for (int i=0; i<CDB_TIME_BUCKET_SIZE; ++i) {
                    cout << entry._comm_stat._time_bucket[i] << " ";
                }
                cout << endl;
            }
            else {
                cout << entry._key._type << " " << "invalid type" << endl;
            }
        }
    }
}

int
main(int argc, char* argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }

    if (!cdb_attach_shm_mgr(argv[1])) {
        cerr << "attach_cdb_shm_mgr error: " << cdb_errno << "(" << cdb_2nd_errno << ")" << endl;
        exit(cdb_errno);
    }

    CDBShmMgr& sm = CDBShmMgr::getInstance();

    string sm_name = parse_pair_map_file(argv[1], argv[2]);
    if (sm_name.empty()) {
        cerr << "can not find pair map file of " << argv[2] << endl;
        exit(1);
    }
    cout << "standby shm is " << sm_name << endl;

    CDBShm& s = sm.get(sm_name);
    if (s._addr == 0) {
        cerr << "can not found shm " << argv[2] << endl;
        exit(2);
    }

    if (strcmp(argv[2], "cdb_ins_dml") == 0) {
        if(!check_standby(s)) {
            cerr << "check standby error: flag is " << cdb_errno << endl;
        }
        dump_ins_dml(s);
    }

    sm.detach_all();
	cdb_shutdown_shm_mgr();
    return 0;
}

