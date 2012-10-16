# Copyright (C) 2012 Tencent, Inc.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
INCLUDES =		-I$(srcdir)
pkglib_LIBRARIES =	libcdb.a

noinst_HEADERS = cdb_error.h cdb_shm_mgr.h tfc_cache_access.h tfc_cache_hash_map.h tfc_shm_map.h\
			cdb.h tfc_binlog.h tfc_cache_chunk_alloc.h tfc_md5.h tfc_spin_lock.h

libcdb_a_SOURCES =    cdb.cpp cdb_shm_mgr.cpp tfc_binlog.cpp tfc_cache_access.cpp tfc_cache_chunk_alloc.cpp \
			tfc_cache_hash_map.cpp tfc_md5.cpp


# Don't update the files from bitkeeper
%::SCCS/s.%
