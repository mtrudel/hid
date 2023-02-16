defmodule HID.DeviceInfo do
  @moduledoc """
  HID device info structure.

  See [original docs](http://www.signal11.us/oss/hidapi/hidapi/doxygen/html/structhid__device__info.html)
  for details.
  """

  defstruct [:path, :vendor_id, :product_id, :serial_number, :release_number,
             :manufacturer_string, :product_string, :usage_page, :usage,
             :interface_number]
end
