target_include_directories(f1rc PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc)

target_sources(f1rc
	PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/mfri/mfri.c
		${CMAKE_CURRENT_LIST_DIR}/nwk/nwk.c
		${CMAKE_CURRENT_LIST_DIR}/simpliciti.c
)