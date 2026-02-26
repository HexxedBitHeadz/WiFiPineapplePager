# ─────────────────────────────────────────────────────────────────────
# Makefile — Build hardware test utilities for WiFi Pineapple Pager
#
# Usage:
#   make                    # Build all tests (native)
#   make CROSS_COMPILE=mipsel-linux-muslsf-   # Cross-compile for MIPS (soft-float)
#   make deploy             # Build + SCP binaries to Pager
#   make clean              # Remove build artifacts
# ─────────────────────────────────────────────────────────────────────

# ── Toolchain ───────────────────────────────────────────────────────
CROSS_COMPILE ?=
CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip
CFLAGS  = -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE -I ../games/engine
LDFLAGS = -static -lm

# Add MIPS-specific flags when cross-compiling
ifneq ($(CROSS_COMPILE),)
  CFLAGS += -march=24kc -msoft-float
endif

# ── Pager Connection ───────────────────────────────────────────────
PAGER_HOST ?= root@172.16.52.1
PAGER_DIR  ?= /root/tests
SSH_OPTS   ?= $(SSH_KEY) -o StrictHostKeyChecking=no

# ── Payload Integration ───────────────────────────────────────────
# Deploy payloads to the "general" category so they appear in the Pager UI.
PAYLOAD_DIR ?= /mmc/root/payloads/user/general

# ── Test Programs ──────────────────────────────────────────────────
TESTS = test_hw

# ── Targets ────────────────────────────────────────────────────────

.PHONY: all clean deploy deploy-payloads help

all: $(TESTS)
	@echo ""
	@echo "=== All test utilities built successfully ==="
	@echo "Run 'make deploy' to install on Pager"

test_hw: test_hw.c ../games/engine/pager_engine.h ../games/engine/pager_gfx.h ../games/engine/pager_input.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@
	@echo "[OK] test_hw"

# ── Deploy to Pager ────────────────────────────────────────────────

deploy: all
	@echo "=== Deploying to $(PAGER_HOST):$(PAGER_DIR) ==="
	ssh $(SSH_OPTS) $(PAGER_HOST) "mkdir -p $(PAGER_DIR)"
	scp $(SSH_OPTS) test_hw $(PAGER_HOST):$(PAGER_DIR)/
	@echo "=== Deployed! Run $(PAGER_DIR)/test_hw on the Pager ==="

# ── Deploy Payloads (appear in Pager UI under general) ─────────────

deploy-payloads: deploy
	@echo "=== Installing payloads to $(PAGER_HOST):$(PAYLOAD_DIR) ==="
	ssh $(SSH_OPTS) $(PAGER_HOST) "mkdir -p $(PAYLOAD_DIR)/test_hw"
	scp $(SSH_OPTS) payloads/test_hw/payload.sh $(PAGER_HOST):$(PAYLOAD_DIR)/test_hw/
	ssh $(SSH_OPTS) $(PAGER_HOST) "chmod +x $(PAYLOAD_DIR)/*/payload.sh"
	@echo "=== Payloads installed! They should now appear in the Pager UI under Payloads > general ==="

# ── Clean ──────────────────────────────────────────────────────────

clean:
	rm -f $(TESTS)
	@echo "Cleaned."

# ── Help ───────────────────────────────────────────────────────────

help:
	@echo "WiFi Pineapple Pager — Test Utilities"
	@echo ""
	@echo "Targets:"
	@echo "  all              Build all test utilities (default)"
	@echo "  deploy           Build + copy binaries to Pager via SCP"
	@echo "  deploy-payloads  Deploy + install payloads (appear in Pager UI > general)"
	@echo "  clean            Remove built binaries"
	@echo ""
	@echo "Variables:"
	@echo "  CROSS_COMPILE  Cross-compiler prefix (e.g., mipsel-linux-muslsf-)"
	@echo "  PAGER_HOST     SSH target (default: root@172.16.52.1)"
	@echo "  PAGER_DIR      Install dir on Pager (default: /root/tests)"
