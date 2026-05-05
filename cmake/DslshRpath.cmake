include_guard(GLOBAL)

function(dslsh_set_install_rpath target)
  if(APPLE)
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path/../lib")
  elseif(UNIX)
    set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
  endif()
endfunction()
