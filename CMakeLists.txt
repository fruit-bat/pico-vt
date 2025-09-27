add_library(pico_vtlib INTERFACE)

target_sources(pico_vtlib INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/src/vt_parser.c
	${CMAKE_CURRENT_LIST_DIR}/src/vt_term.c
	${CMAKE_CURRENT_LIST_DIR}/src/vt_tabs.c
	${CMAKE_CURRENT_LIST_DIR}/src/vt_pallet.c
	${CMAKE_CURRENT_LIST_DIR}/src/vt_emu.c
	${CMAKE_CURRENT_LIST_DIR}/src/vt_cell.c
)

target_include_directories(pico_vtlib INTERFACE
    src
)

target_link_libraries(pico_vtlib INTERFACE pico_stdlib)
