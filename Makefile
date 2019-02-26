#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

ota: all
	./deploy_firmware.sh

EXTRA_COMPONENT_DIRS := $(CURDIR)/../../../esp-idf-lib/components

PROJECT_NAME := mqtt_ssl

include $(IDF_PATH)/make/project.mk
