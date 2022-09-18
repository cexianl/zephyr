# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_BT_DIS dis.c)

zephyr_sources_ifdef(CONFIG_BT_BAS bas.c)

zephyr_sources_ifdef(CONFIG_BT_HRS hrs.c)

zephyr_sources_ifdef(CONFIG_BT_TPS tps.c)

if(CONFIG_BT_OTS OR CONFIG_BT_OTS_CLIENT)
  add_subdirectory(ots)
endif()

if(CONFIG_BT_IAS OR CONFIG_BT_IAS_CLIENT)
  add_subdirectory(ias)
endif()

if(CONFIG_BT_ANCS_CLIENT)
  add_subdirectory(ancs)
endif()

zephyr_sources_ifdef(CONFIG_BT_GATTP gattp.c)