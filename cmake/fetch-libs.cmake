include(FetchContent)

FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.15
)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        11.2.0
)

FetchContent_MakeAvailable(pugixml)
FetchContent_MakeAvailable(fmt)
