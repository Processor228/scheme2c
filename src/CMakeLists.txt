add_executable(scheme2c main.cxx block-base.cxx)

target_link_libraries(scheme2c PRIVATE pugixml)
target_link_libraries(scheme2c PRIVATE fmt::fmt)
