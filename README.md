# HID

This package is a NIF wrapper for [hidapi](http://www.signal11.us/oss/hidapi/)
library.

## Installation

[hidapi](http://www.signal11.us/oss/hidapi/) and
[libusb 1.0](http://libusb.info/) are required to be installed on target system.
For debian-based distributions use

```
apt-get install libhidapi-hidraw0 libusb-1.0-0
```

The package can be installed with:

  1. Add `hid` to your list of dependencies in `mix.exs`:

    ```elixir
    def deps do
      [{:hid, "~> 0.1"}]
    end
    ```

  2. Run `mix deps.get`

## Usage

See package [docs](https://hexdocs.pm/hid) for details.

To list all HID devices available in system use `HID.enumerate/2`.

To read or write to device you should open device with `HID.open/3`. This
function will return device handle that you can use for subsequent reading and
writing with `HID.read/2` and `HID.write/2`. When device handle is not needed
any more use `HID.close/1` to free resources.

## TODO

  * [ ] Getting device info by device handle
  * [ ] Reading/writing reports
