#include "cdb_tool_comm.h"

char opt_cdb_stat_ins_dml = 0;
char opt_cdb_stat_ins_conn = 0;
char opt_cdb_stat_client_dml = 0;
char opt_cdb_stat_table_dml = 0;

void sql_print_error(const char *format, ...)
{
    // this is fake, in order to pass linking
}

void sql_print_warning(const char *format, ...)
{
    // this is fake, in order to pass linking
}
