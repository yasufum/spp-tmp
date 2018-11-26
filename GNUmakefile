# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

VERSION = 18.05.1

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

DIRS-y += src

include $(RTE_SDK)/mk/rte.extsubdir.mk

DOC_ROOT = docs/guides

# Compile RST documents
.PHONY: doc
doc: doc-pdf doc-html

.PHONY: doc-html
doc-html:
	make -C $(DOC_ROOT) html

.PHONY: doc-pdf
doc-pdf:
	python $(DOC_ROOT)/gen_pdf_imgs.py
	make -C $(DOC_ROOT) latexpdf
	@echo "Succeeded to generate '$(DOC_ROOT)/_build/latex/SoftPatchPanel.pdf'"

.PHONY: doc-clean
doc-clean:
	find $(DOC_ROOT)/images/ -type f -name "*.pdf" -delete
	make -C $(DOC_ROOT) clean
