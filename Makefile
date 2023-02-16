MIX = mix
HID_PRIV ?= priv

CFLAGS = -g -O3 -fPIC -Wall -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format -Wno-int-conversion

ERLANG_PATH = $(shell erl -eval 'io:format("~s", [lists:concat([code:root_dir(), "/erts-", erlang:system_info(version), "/include"])])' -s init stop -noshell)
CFLAGS += -I$(ERLANG_PATH)

HIDAPI_PATH = $(shell pkg-config hidapi-hidraw --variable=libdir)
LIBUSB_PATH = $(shell pkg-config libusb-1.0 --variable=libdir)

CFLAGS  += $(shell pkg-config hidapi-hidraw --cflags)
LDFLAGS += $(shell pkg-config hidapi-hidraw --libs) -L$(shell pkg-config hidapi-hidraw --variable=libdir)

.PHONY: all mix clean

all: mix

mix:
	$(MIX) compile

priv:
	mkdir -p $(HID_PRIV)

priv/hid.so: priv
	$(CC) $(CFLAGS) -shared src/hid.c $(LDFLAGS) -o $(HID_PRIV)/hid.so

clean:
	$(MIX) clean
	$(RM) $(HID_PRIV)/hid.so
