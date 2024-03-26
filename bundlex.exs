defmodule Membrane.Nvidia.MMAPI.BundlexProject do
  @moduledoc false

  use Bundlex.Project

  def project() do
    [
      natives: natives(Bundlex.platform())
    ]
  end

  defp natives(_platform) do
    [
    ]
  end
end
