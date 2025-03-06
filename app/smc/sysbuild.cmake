# Copyright (c) 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

if("${SB_CONFIG_BMC_BOARD}" STREQUAL "")
	message(FATAL_ERROR
	"Target ${BOARD}${BOARD_QUALIFIERS} not supported for this sample. "
	"There is no BMC board selected in Kconfig.sysbuild")
endif()

ExternalZephyrProject_Add(
	APPLICATION bmc
	SOURCE_DIR  ${APP_DIR}/../bmc
	BOARD       ${SB_CONFIG_BMC_BOARD}
	BUILD_ONLY 1
)

# Flash the BMFW application first. That way we can hotload the CMFW
# application without writing to SPI.
sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} bmc)

# Build mcuboot for BMC but do not flash it
ExternalZephyrProject_Add(
	APPLICATION mcuboot
	SOURCE_DIR  ${APP_DIR}/../bmc
	BOARD       ${SB_CONFIG_BMC_BOARD}
	BUILD_ONLY 1
)

if(BOARD STREQUAL "tt_blackhole")
  # Map board revision names to folder names for spirom config data
  string(TOUPPER ${BOARD_REVISION} BASE_NAME)
  set(PROD_NAME "${BASE_NAME}-1")
  set(CFG_NAME "${BOARD_REVISION}-bootfs")
elseif(BOARD STREQUAL "native_sim")
  # Use P100 data files to stand in
  set(PROD_NAME "P100-1")
  set(CFG_NAME "p100-bootfs")
else()
  message(FATAL_ERROR "No support for board ${BOARD}")
endif()

set(OUTPUT_BOOTFS ${CMAKE_BINARY_DIR}/tt_boot_fs.bin)
set(OUTPUT_FWBUNDLE ${CMAKE_BINARY_DIR}/update.fwbundle)

# Generate filesystem
add_custom_command(OUTPUT ${OUTPUT_BOOTFS}
  COMMAND ${PYTHON_EXECUTABLE}
  ${APP_DIR}/../../scripts/tt_boot_fs.py mkfs
  ${BOARD_DIRECTORIES}/bootfs/${CFG_NAME}.yaml
  ${OUTPUT_BOOTFS}
  --build-dir ${CMAKE_BINARY_DIR}
  DEPENDS ${DEFAULT_IMAGE} bmc)

# Generate firmware bundle that can be used to flash this build on a board
# using tt-flash
add_custom_command(OUTPUT ${OUTPUT_FWBUNDLE}
  COMMAND ${PYTHON_EXECUTABLE}
  ${APP_DIR}/../../scripts/tt_boot_fs.py fwbundle
  ${OUTPUT_BOOTFS}
  ${PROD_NAME}
  ${OUTPUT_FWBUNDLE}
  DEPENDS ${OUTPUT_BOOTFS})

add_custom_target(gen_bootfs ALL DEPENDS ${OUTPUT_FWBUNDLE})
