# ─────────────────────────────────────────────────────────────────────
# Makefile — Build all Pineapple Pager games
# Target: WiFi Pineapple Pager
#
# Usage:
#   make                    # Build all games (native)
#   make CROSS_COMPILE=mipsel-linux-muslsf-   # Cross-compile for MIPS (soft-float)
#   make deploy             # Build + SCP binaries to Pager
#   make deploy-payloads    # Deploy + install payloads in Pager UI
#   make find-payload-dir   # Find payload path on Pager (run first!)
#   make clean              # Remove build artifacts
# ─────────────────────────────────────────────────────────────────────

# ── Toolchain ───────────────────────────────────────────────────────
CROSS_COMPILE ?=
CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip
CFLAGS  = -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE -I engine
LDFLAGS = -static -lm

# Add MIPS-specific flags when cross-compiling
ifneq ($(CROSS_COMPILE),)
  CFLAGS += -march=24kc -msoft-float
endif

# ── Pager Connection ───────────────────────────────────────────────
PAGER_HOST ?= root@172.16.52.1
PAGER_DIR  ?= /root/games
SSH_OPTS ?= $(SSH_KEY) -o StrictHostKeyChecking=no

# ── Payload Integration ───────────────────────────────────────────
# This is where the Pager UI looks for user payloads.
# Confirmed path: /mmc/root/payloads/user/games
# (Remember /root → /mmc/root on the Pager's eMMC)
PAYLOAD_DIR ?= /mmc/root/payloads/user/games

# ── Games ───────────────────────────────────────────────────────────
GAMES = \
	snake/snake \
	pong/pong \
	packet_catcher/packet_catcher

# ── Targets ─────────────────────────────────────────────────────────

.PHONY: all clean deploy deploy-payloads find-payload-dir run-snake run-pong run-packet-catcher help

all: $(GAMES)
	@echo ""
	@echo "=== All games built successfully ==="
	@echo "Run 'make deploy' to install on Pager"

snake/snake: snake/snake.c engine/pager_engine.h engine/pager_gfx.h engine/pager_input.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@
	@echo "[OK] snake"

pong/pong: pong/pong.c engine/pager_engine.h engine/pager_gfx.h engine/pager_input.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@
	@echo "[OK] pong"

packet_catcher/packet_catcher: packet_catcher/packet_catcher.c engine/pager_engine.h engine/pager_gfx.h engine/pager_input.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@
	@echo "[OK] packet_catcher"

# ── Deploy to Pager ────────────────────────────────────────────────

deploy: all
	@echo "=== Deploying to $(PAGER_HOST):$(PAGER_DIR) ==="
	ssh $(SSH_OPTS) $(PAGER_HOST) "mkdir -p $(PAGER_DIR)/snake $(PAGER_DIR)/pong $(PAGER_DIR)/packet_catcher"
	scp $(SSH_OPTS) snake/snake                         $(PAGER_HOST):$(PAGER_DIR)/snake/
	scp $(SSH_OPTS) pong/pong                           $(PAGER_HOST):$(PAGER_DIR)/pong/
	scp $(SSH_OPTS) packet_catcher/packet_catcher       $(PAGER_HOST):$(PAGER_DIR)/packet_catcher/
	@echo "=== Deployed! Launch games from Payloads > games in the Pager UI ==="

# ── Deploy Payloads (appear in Pager UI) ───────────────────────────

deploy-payloads: deploy
	@echo "=== Installing payloads to $(PAGER_HOST):$(PAYLOAD_DIR) ==="
	ssh $(SSH_OPTS) $(PAGER_HOST) "mkdir -p $(PAYLOAD_DIR)/snake $(PAYLOAD_DIR)/pong $(PAYLOAD_DIR)/packet_catcher"
	scp $(SSH_OPTS) payloads/snake/payload.sh              $(PAGER_HOST):$(PAYLOAD_DIR)/snake/
	scp $(SSH_OPTS) payloads/pong/payload.sh               $(PAGER_HOST):$(PAYLOAD_DIR)/pong/
	scp $(SSH_OPTS) payloads/packet_catcher/payload.sh     $(PAGER_HOST):$(PAYLOAD_DIR)/packet_catcher/
	ssh $(SSH_OPTS) $(PAGER_HOST) "chmod +x $(PAYLOAD_DIR)/*/payload.sh"
	@echo "=== Payloads installed! They should now appear in the Pager UI under Payloads > games ==="

# ── Find Payload Directory (run this first if unsure of path) ──────

find-payload-dir:
	@echo "=== Searching for payload directories on Pager ==="
	ssh $(SSH_OPTS) $(PAGER_HOST) "find / -name 'payloads' -type d 2>/dev/null; echo '---'; ls -la /root/payloads/ 2>/dev/null || echo '/root/payloads/ not found'; echo '---'; ls -la /pineapple/payloads/ 2>/dev/null || echo '/pineapple/payloads/ not found'"

# ── Run Games ──────────────────────────────────────────────────────

run-snake: deploy
	ssh -t $(SSH_OPTS) $(PAGER_HOST) "$(PAGER_DIR)/snake/snake"

run-pong: deploy
	ssh -t $(SSH_OPTS) $(PAGER_HOST) "$(PAGER_DIR)/pong/pong"

run-packet-catcher: deploy
	ssh -t $(SSH_OPTS) $(PAGER_HOST) "$(PAGER_DIR)/packet_catcher/packet_catcher"

# ── Clean ──────────────────────────────────────────────────────────

clean:
	rm -f $(GAMES)
	@echo "Cleaned."

# ── Help ───────────────────────────────────────────────────────────

help:
	@echo "WiFi Pineapple Pager — Game Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all                Build all games (default)"
	@echo "  deploy             Build + copy binaries to Pager via SCP"
	@echo "  deploy-payloads    Deploy + install payloads (appear in Pager UI)"
	@echo "  find-payload-dir   Find the payload directory on your Pager"
	@echo "  run-snake          Deploy + run Snake directly via SSH"
	@echo "  run-pong           Deploy + run Pong directly via SSH"
	@echo "  run-packet-catcher Deploy + run Packet Catcher directly via SSH"
	@echo "  clean              Remove built binaries"
	@echo ""
	@echo "Variables:"
	@echo "  CROSS_COMPILE      Cross-compiler prefix (e.g., mipsel-linux-muslsf-)"
	@echo "  PAGER_HOST         SSH target (default: root@172.16.52.1)"
	@echo "  PAGER_DIR          Game binary install dir (default: /root/games)"
	@echo "  PAYLOAD_DIR        Payload install dir (default: /root/payloads/games)"
