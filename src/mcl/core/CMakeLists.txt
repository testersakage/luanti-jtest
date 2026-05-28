# src/mcl/core/CMakeLists.txt の内部
set(mcl_core_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/damage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/explosions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/flowlib.cpp
#	${CMAKE_CURRENT_SOURCE_DIR}/liquids.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util_compat.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_environment.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_item.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_misc.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_object.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_queue.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_ringbuffer.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_shape.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_table.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/worlds.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/tga_encoder.cpp
	PARENT_SCOPE
)