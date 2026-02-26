# WiFi Pineapple Pager — Hardware Test Utilities

Diagnostic tools for verifying Pager hardware (buttons, display, input mapping) before developing games or payloads.

## Programs

| Program | Description | Quit |
|---------|-------------|------|
| **test_hw** | Visual button + display test using the game engine — shows D-pad state, held buttons, and press counts on the display | Hold A+B |

## Quick Start

```bash
# Set up cross-compiler (if not already on PATH)
export PATH="/opt/mipsel-linux-muslsf-cross/bin:$PATH"

# Build
cd tests
make clean && make CROSS_COMPILE=mipsel-linux-muslsf-

# Deploy binaries to Pager
make deploy CROSS_COMPILE=mipsel-linux-muslsf-

# Deploy payloads so they appear in Pager UI under Payloads > general
make deploy-payloads CROSS_COMPILE=mipsel-linux-muslsf-
```

Once deployed with `deploy-payloads`, both test tools appear in the Pager UI under **Payloads > general** and can be launched directly from the device — no SSH required.

## Running on the Pager

```bash
# SSH into the Pager
ssh root@172.16.52.1

# Run the visual button/display test
/root/tests/test_hw
```

### test_hw

Renders a live button test screen on the Pager display:
- Current held buttons highlighted in green (B in red)
- Raw bitmask values for debugging
- Last event log and total press counter
- Visual D-pad + A/B button diagram
