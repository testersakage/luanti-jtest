# src/mcl/core/CMakeLists.txt の内部
set(mcl_core_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/tga_encoder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/util_item.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_misc.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_object.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_queue.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_ringbuffer.cpp  # 
	${CMAKE_CURRENT_SOURCE_DIR}/util_shape.cpp  # 
#	${CMAKE_CURRENT_SOURCE_DIR}/util_table.cpp  # 凍結
	PARENT_SCOPE
)