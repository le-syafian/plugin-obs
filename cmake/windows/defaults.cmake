# CMake Windows defaults module

include_guard(GLOBAL)

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

include(buildspec)

# Install straight into the OBS program folder (see set_target_properties_plugin)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  file(TO_CMAKE_PATH "$ENV{ProgramFiles}/obs-studio" _obs_studio_dir)
  set(
    CMAKE_INSTALL_PREFIX
    "${_obs_studio_dir}"
    CACHE STRING
    "Default plugin installation directory"
    FORCE
  )
endif()
