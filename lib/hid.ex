defmodule HID do
  @moduledoc """
  NIF wrapper for [hidapi](http://www.signal11.us/oss/hidapi/) library.
  """

  @on_load :load_nif

  @type handle :: term

  @type enum_result ::
          {:ok, list(HID.DeviceInfo)}
          | {:error, :ehidenum}

  @type open_result ::
          {:ok, handle}
          | {:error, :ehidopen}
          | {:error, :ehidopen, String.t()}

  @type write_result ::
          {:ok, non_neg_integer}
          | {:error, :ehidwrite}
          | {:error, :ehidwrite, String.t()}

  @type read_result ::
          {:ok, binary}
          | {:error, :ehidread}
          | {:error, :ehidread, String.t()}

  @doc false
  def load_nif do
    path = :filename.join(:code.priv_dir(:hid), ~c[hid])
    :ok = :erlang.load_nif(path, 0)
  end

  @doc """
  Enumerates the HID devices.

  See [original docs](http://www.signal11.us/oss/hidapi/hidapi/doxygen/html/group__API.html#ga135931e04d48078a9fb7aebf663676f9)
  for details.

  `vendor_id` and `product_id` are the positive integer masks for enumeration.
  `0x00` means any value. Both arguments have `0x00` as a default value.
  """
  @spec enumerate() :: enum_result
  @spec enumerate(vendor_id :: byte) :: enum_result
  @spec enumerate(vendor_id :: byte, product_id :: byte) :: enum_result
  def enumerate(vendor_id \\ 0, product_id \\ 0) do
    with {:ok, list} <- nif_enumerate(vendor_id, product_id) do
      Enum.reduce(list, [], fn info, acc ->
        case deserialize_device_info(info) do
          nil -> acc
          info -> [info | acc]
        end
      end)
    end
  end

  @doc """
  Opens the HID device.

  See [original docs](http://www.signal11.us/oss/hidapi/hidapi/doxygen/html/group__API.html#gae6910ed9f01c4a99d25539b16800e90c)
  for details.

  You can open device by `path` (`"/dev/hidraw0"` for example)
  or by `vendor_id`, `product_id` and optionally `serial` arguments.

  `path` can be string or char list.

  `vendor_id` and `product_id` are positive integers.

  `serial` can be a string or charlist.

  Function returns device handle that can be used in other device
  reading/writing functions.
  """
  @spec open(path :: String.t() | charlist) :: open_result
  @spec open(vendor_id :: byte, product_id :: byte) :: open_result
  @spec open(
          vendor_id :: byte,
          product_id :: byte,
          serial :: String.t() | charlist
        ) :: open_result
  def open(path) do
    nif_open(path)
  end

  def open(vendor_id, product_id) do
    nif_open(vendor_id, product_id)
  end

  def open(vendor_id, product_id, serial) when is_bitstring(serial) do
    nif_open(vendor_id, product_id, serial |> :binary.bin_to_list())
  end

  def open(vendor_id, product_id, serial) do
    nif_open(vendor_id, product_id, serial)
  end

  @doc """
  Closes the HID device.

  See [original docs](http://www.signal11.us/oss/hidapi/hidapi/doxygen/html/group__API.html#ga9b64828273b8dd052731e79ba9e1a516)
  for details.

  `device` is a device handle, returned by `open`.
  """
  @spec close(device :: handle) :: :ok
  def close(device) do
    nif_close(device)
  end

  @doc """
  Writes an Output report to a HID device.

  See [original docs](http://www.signal11.us/oss/hidapi/hidapi/doxygen/html/group__API.html#gad14ea48e440cf5066df87cc6488493af)
  for details.

  Writes data to HID device. Note that first byte of data must be a HID Report ID.
  If device supports a single reports only this byte must be set to `0x00`.

  Returns a number of actually written bytes.
  """
  @spec write(device :: handle, data :: binary | list(byte)) :: write_result
  def write(device, data) do
    nif_write(device, data)
  end

  @spec write_report(device :: handle, data :: binary | list(byte)) :: write_result
  def write_report(device, data) do
    nif_write_report(device, data)
  end

  @doc """
  Reads an Input report from a HID device.

  See [original docs](http://www.signal11.us/oss/hidapi/hidapi/doxygen/html/group__API.html#ga6b820f3e72097cf7f994e33715dc7af1)
  for details.

  Reads at most `size` bytes from HID device. Note that first byte of data would
  be a HID Report ID, so you should include this byte into `size`.
  """
  @spec read(device :: handle, size :: integer) :: read_result
  def read(device, size) do
    with {:ok, data} <- nif_read(device, size) do
      {:ok, :binary.list_to_bin(data)}
    end
  end

  @doc """
  Reads at most `size` bytes from HID device with a timeout. Note that first byte of data would
  be a HID Report ID, so you should include this byte into `size`. Timeout specified in
  milliseconds, or `:infinity``
  """
  @spec read_timeout(device :: handle, size :: integer, timeout :: timeout()) :: read_result
  def read_timeout(device, size, :infinity), do: read_timeout(device, size, -1)

  def read_timeout(device, size, timeout) do
    with {:ok, data} <- nif_read_timeout(device, size, timeout) do
      {:ok, :binary.list_to_bin(data)}
    end
  end

  @spec read_report(device :: handle, report_id :: integer, size :: integer) :: read_result
  def read_report(device, report_id, size) do
    with {:ok, data} <- nif_read_report(device, report_id, size) do
      {:ok, :binary.list_to_bin(data)}
    end
  end

  defp deserialize_device_info(
         {:hid_device_info, path, vendor_id, product_id, serial_number, release_number,
          manufacturer_string, product_string, usage_page, usage, interface_number}
       ) do
    %HID.DeviceInfo{
      path: path |> :binary.list_to_bin(),
      vendor_id: vendor_id,
      product_id: product_id,
      serial_number: serial_number |> :binary.list_to_bin(),
      release_number: release_number,
      manufacturer_string: manufacturer_string |> :binary.list_to_bin(),
      product_string: product_string |> :binary.list_to_bin(),
      usage_page: usage_page,
      usage: usage,
      interface_number: interface_number
    }
  end

  defp deserialize_device_info(_), do: nil

  @doc false
  def nif_enumerate(_vendor_id, _product_id) do
    raise "NIF enumerate/2 not implemented"
  end

  @doc false
  def nif_open(_path) do
    raise "NIF open/1 not implemented"
  end

  @doc false
  def nif_open(_vendor_id, _product_id) do
    raise "NIF open/2 not implemented"
  end

  @doc false
  def nif_open(_vendor_id, _product_id, _serial) do
    raise "NIF open/3 not implemented"
  end

  @doc false
  def nif_close(_device) do
    raise "NIF close/1 not implemented"
  end

  @doc false
  def nif_write(_device, _data) do
    raise "NIF write/2 not implemented"
  end

  @doc false
  def nif_read(_device, _size) do
    raise "NIF read/2 not implemented"
  end

  @doc false
  def nif_read_timeout(_device, _size, _timeout) do
    raise "NIF read_timeout/2 not implemented"
  end

  @doc false
  def nif_read_report(_device, _report_id, _size) do
    raise "NIF read_report/3 not implemented"
  end

  @doc false
  def nif_write_report(_device, _data) do
    raise "NIF write_report/2 not implemented"
  end
end
