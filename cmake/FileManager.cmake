message(STATUS "Building file datasets for the File Manager...")

# Directory settings

set(FM_LOCAL_DIR     "${CMAKE_CURRENT_BINARY_DIR}/local")
set(FM_REMOTE_DIR    "${CMAKE_CURRENT_BINARY_DIR}/remote")

file(REMOVE_RECURSE "${FM_LOCAL_DIR}")
file(REMOVE_RECURSE "${FM_REMOTE_DIR}")

file(MAKE_DIRECTORY "${FM_LOCAL_DIR}")
file(MAKE_DIRECTORY "${FM_REMOTE_DIR}")

target_compile_definitions(Common PRIVATE
    FM_LOCAL_DIR="${FM_LOCAL_DIR}"
    FM_REMOTE_DIR="${FM_REMOTE_DIR}"
)

target_compile_definitions(Server PRIVATE
    FM_LOCAL_DIR="${FM_LOCAL_DIR}"
    FM_REMOTE_DIR="${FM_REMOTE_DIR}"
)

target_compile_definitions(Client PRIVATE
    FM_LOCAL_DIR="${FM_LOCAL_DIR}"
    FM_REMOTE_DIR="${FM_REMOTE_DIR}"
)

# File generation (for client dataset)

## 1kb
execute_process(COMMAND sh -c
    "dd if=/dev/urandom of='${FM_LOCAL_DIR}/dataset1.dat' bs=1K count=1"
)
## 32kb
execute_process(COMMAND sh -c
    "dd if=/dev/urandom of='${FM_LOCAL_DIR}/dataset2.dat' bs=1K count=32"
)
## 512kb
execute_process(COMMAND sh -c
    "dd if=/dev/urandom of='${FM_LOCAL_DIR}/dataset3.dat' bs=1K count=512"
)
## 1Mb
execute_process(COMMAND sh -c
    "dd if=/dev/urandom of='${FM_LOCAL_DIR}/dataset4.dat' bs=1M count=1"
)
## 32Mb
execute_process(COMMAND sh -c
    "dd if=/dev/urandom of='${FM_LOCAL_DIR}/dataset5.dat' bs=1M count=32"
)
## 64 Mb
execute_process(COMMAND sh -c
    "dd if=/dev/urandom of='${FM_LOCAL_DIR}/dataset6.dat' bs=1M count=64"
)

# File generation (for server dataset)
execute_process(
    COMMAND sh -c "tr -dc 'A-Za-z0-9' < /dev/urandom | head -c 1024"
    OUTPUT_FILE "${FM_REMOTE_DIR}/random1.txt"
)
execute_process(
    COMMAND sh -c "tr -dc 'A-Za-z0-9' < /dev/urandom | head -c 1024"
    OUTPUT_FILE "${FM_REMOTE_DIR}/random2.txt"
)

message(STATUS "Files generated.")
