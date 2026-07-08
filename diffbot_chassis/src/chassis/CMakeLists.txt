cmake_minimum_required(VERSION 3.8)
project(chassis)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MODBUS REQUIRED libmodbus)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

add_executable(chassis src/chassis.cpp src/modbus4lift.cpp)

target_include_directories(chassis PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${MODBUS_INCLUDE_DIRS})

target_include_directories(chassis PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../bxi_pci_drv/lib
  ) 
  
add_executable(key_control src/control.cpp)
ament_target_dependencies(key_control PUBLIC rclcpp geometry_msgs)

target_link_libraries(chassis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../bxi_pci_drv/lib/libbxi_pci_drv.a /usr/lib/x86_64-linux-gnu/libmodbus.so.5.1.0)
target_link_libraries(chassis PRIVATE ${MODBUS_LIBRARIES})

ament_target_dependencies(chassis PUBLIC rclcpp geometry_msgs)

install(TARGETS
  chassis
  key_control
  DESTINATION lib/${PROJECT_NAME})
  
ament_package()
