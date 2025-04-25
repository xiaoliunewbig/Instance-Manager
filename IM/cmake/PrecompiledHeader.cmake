function(add_precompiled_header _target _header)
  if(CMAKE_COMPILER_IS_GNUCXX)
    get_filename_component(_name ${_header} NAME)
    set(_pch_header "${CMAKE_CURRENT_BINARY_DIR}/${_name}")
    set(_pch_binary "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gch")
    
    # 生成预编译命令
    add_custom_command(
      OUTPUT "${_pch_binary}"
      COMMAND ${CMAKE_CXX_COMPILER} 
              ${CMAKE_CXX_FLAGS} 
              -x c++-header 
              -o "${_pch_binary}" 
              "${_header}"
      DEPENDS "${_header}"
      COMMENT "Building Precompiled Header ${_name}")
    
    # 设置编译选项
    set_target_properties(${_target} PROPERTIES
      COMPILE_FLAGS "-include ${_name} -Winvalid-pch")
      
    # 添加依赖
    add_custom_target(pch-${_target} DEPENDS "${_pch_binary}")
    add_dependencies(${_target} pch-${_target})
  endif()
endfunction()
