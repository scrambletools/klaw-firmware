VIA_ENABLE = yes
VIAL_ENABLE = yes
ENCODER_MAP_ENABLE = yes
VIALRGB_ENABLE = yes
LTO_ENABLE = yes

# A bare Pro Micro is the ATmega32U4. Vial extras and mouse keys are off to
# fit its flash. A converter target (CONVERT_TO=kb2040 for the Adafruit KB2040)
# has room for all of them, so they stay at their Vial defaults.
ifeq ($(strip $(CONVERT_TO)),)
MOUSEKEY_ENABLE = no
QMK_SETTINGS = no
TAP_DANCE_ENABLE = no
COMBO_ENABLE = no
KEY_OVERRIDE_ENABLE = no

# vial-qmk's LUFA glue trips gcc 16's unused-but-set warning under -Werror
EXTRAFLAGS += -Wno-error=unused-but-set-variable
endif
