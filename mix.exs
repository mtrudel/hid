defmodule Mix.Tasks.Compile.Hid do
  @shortdoc "Compiles HID library"

  def run(_) do
    opts = [stderr_to_stdout: true,
            env: [{"HID_PRIV", Path.join(Mix.Project.app_path, "priv")}]]
    {result, _error_code} = System.cmd("make", ["priv/hid.so"], opts)
    Mix.shell.info result
    :ok
  end
end

defmodule HID.Mixfile do
  use Mix.Project

  def project do
    [app: :hid,
     version: "0.1.4",
     elixir: "~> 1.3",
     name: "hid",
     source_url: "http://git.sferosoft.ru/opensource/hid",
     homepage_url: "http://git.sferosoft.ru/opensource/hid",
     compilers: [:hid, :elixir, :app],
     build_embedded: Mix.env == :prod,
     start_permanent: Mix.env == :prod,
     deps: deps(),
     description: description(),
     package: package(),
     docs: docs()]
  end

  def application do
    [applications: [:logger]]
  end

  defp deps do
    [{:ex_doc, "~> 0.14", only: :dev},
     {:credo, "~> 0.4", only: [:dev, :test]}]
  end

  defp description do
    """
    NIF wrapper of hidapi library for reading/writing from/to USB HID devices
    (http://www.signal11.us/oss/hidapi/).
    """
  end

  defp package do
    [name: :hid,
     files: ["lib", "src", "Makefile", "mix.exs", "README*"],
     maintainers: ["Alexey Ovchinnikov <alexiss@sferosoft.ru>"],
     licenses: ["MIT"],
     links: %{"GitLab" => "http://git.sferosoft.ru/opensource/hid",
              "Docs"   => "https://hexdocs.pm/hid"}]
  end

  defp docs do
    [main: "readme",
     #logo: "path/to/logo.png",
     extras: ["README.md"]]
  end
end
