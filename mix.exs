defmodule Membrane.Nvidia.MMAPI.Mixfile do
  use Mix.Project

  @version "0.1.0"
  @github_url "https://github.com/gBillal/membrane_nvidia_mmapi_plugin"

  def project do
    [
      app: :membrane_nvidia_mmapi_plugin,
      version: @version,
      elixir: "~> 1.13",
      compilers: [:unifex, :bundlex] ++ Mix.compilers(),
      elixirc_paths: elixirc_paths(Mix.env()),
      start_permanent: Mix.env() == :prod,
      deps: deps(),
      dialyzer: dialyzer(),

      # hex
      description: "A collection of element that leverage Nvidia Jetson hardware using MMAPI",
      package: package(),

      # docs
      name: "Membrane Nvidia Jetson plugin",
      source_url: @github_url,
      docs: docs()
    ]
  end

  def application do
    [
      extra_applications: []
    ]
  end

  defp elixirc_paths(:test), do: ["lib", "test/support"]
  defp elixirc_paths(_env), do: ["lib"]

  defp deps do
    [
      {:membrane_core, "~> 1.0"},
      {:unifex, "~> 1.2.0"},
      {:membrane_h264_format, "~> 0.6.0"},
      {:membrane_h265_format, "~> 0.2.0"},
      {:membrane_raw_video_format, "~> 0.4.0"},
      {:ex_doc, ">= 0.0.0", only: :dev, runtime: false},
      {:dialyxir, ">= 0.0.0", only: :dev, runtime: false},
      {:credo, ">= 0.0.0", only: :dev, runtime: false},
      {:membrane_file_plugin, "~> 0.17.0", only: :test},
      {:membrane_h26x_plugin, "~> 0.10.0", only: :test}
    ]
  end

  defp dialyzer() do
    opts = [
      flags: [:error_handling]
    ]

    if System.get_env("CI") == "true" do
      # Store PLTs in cacheable directory for CI
      [plt_local_path: "priv/plts", plt_core_path: "priv/plts"] ++ opts
    else
      opts
    end
  end

  defp package do
    [
      maintainers: ["Billal Ghilas"],
      licenses: ["Apache-2.0"],
      links: %{
        "GitHub" => @github_url
      }
    ]
  end

  defp docs do
    [
      main: "readme",
      extras: ["README.md", "LICENSE"],
      formatters: ["html"],
      source_ref: "v#{@version}",
      nest_modules_by_prefix: [Membrane.Nvidia.MMAPI]
    ]
  end
end
