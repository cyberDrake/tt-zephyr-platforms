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
