defmodule Membrane.Nvidia.MMAPI.BundlexProject do
  @moduledoc false

  use Bundlex.Project

  def project() do
    [
      natives: natives(Bundlex.get_target())
    ]
  end

  defp natives(_platform) do
    [
      decoder: [
        interface: :nif,
        language: :cpp,
        sources: [
          "decoder.cpp",
          "decoder_nif.cpp",
          "common/NvApplicationProfiler.cpp",
          "common/NvBuffer.cpp",
          "common/NvBufSurface.cpp",
          "common/NvElement.cpp",
          "common/NvElementProfiler.cpp",
          "common/NvLogging.cpp",
          "common/NvV4l2Element.cpp",
          "common/NvV4l2ElementPlane.cpp",
          "common/NvVideoDecoder.cpp"
        ],
        includes: ["/usr/src/jetson_multimedia_api/include/"],
        lib_dirs: ["/usr/lib/aarch64-linux-gnu", "/usr/lib/aarch64-linux-gnu/tegra"],
        libs: ["pthread", "nvv4l2", "nvbufsurface", "nvbufsurftransform"],
        compiler_flags: ["-std=c++17"],
        preprocessor: Unifex
      ]
    ]
  end
end
