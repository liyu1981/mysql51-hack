#yuli: cdb sources and objects start
cdb_HEADERS=$(cdb_prefix)tfc_binlog.h $(cdb_prefix)tfc_cache_access.h $(cdb_prefix)tfc_cache_chunk_alloc.h \
			$(cdb_prefix)tfc_cache_hash_map.h $(cdb_prefix)tfc_md5.h $(cdb_prefix)cdb_shm_mgr.h $(cdb_prefix)cdb.h

cdb_SOURCES=$(cdb_prefix)tfc_binlog.cpp $(cdb_prefix)tfc_cache_access.cpp $(cdb_prefix)tfc_cache_chunk_alloc.cpp \
			$(cdb_prefix)tfc_cache_hash_map.cpp $(cdb_prefix)tfc_md5.cpp $(cdb_prefix)cdb_shm_mgr.cpp $(cdb_prefix)cdb.cpp

cdb_OBJECTS=$(cdb_prefix)tfc_binlog.$(OBJEXT) $(cdb_prefix)tfc_cache_access.$(OBJEXT) \
			$(cdb_prefix)tfc_cache_chunk_alloc.$(OBJEXT) $(cdb_prefix)tfc_cache_hash_map.$(OBJEXT) \
            $(cdb_prefix)tfc_md5.$(OBJEXT) $(cdb_prefix)cdb_shm_mgr.$(OBJEXT) $(cdb_prefix)cdb.$(OBJEXT)

cdb_CXXFLAGS=-fimplicit-templates
#yuli: cdb sources and objects end


