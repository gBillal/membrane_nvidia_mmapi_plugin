Logger.configure(level: :info)

Mix.install([
  :membrane_nvidia_mmapi_plugin,
  :membrane_h26x_plugin,
  :membrane_file_plugin,
  :req
])

h264 = Req.get!("https://raw.githubusercontent.com/membraneframework/static/gh-pages/samples/ffmpeg-testsrc.h264").body
File.write!("input.h264", h264)

defmodule Decoding.Pipeline do
  use Membrane.Pipeline

  @impl true
  def handle_init(_ctx, _opts) do
    structure =
      child(%Membrane.File.Source{chunk_size: 40_960, location: "input.h264"})
      |> child(Membrane.H264.Parser)
      |> child(Membrane.Nvidia.MMAPI.Decoder)
      |> child(:sink, %Membrane.File.Sink{location: "output.raw"})

    {[spec: structure], %{}}
  end

  @impl true
  def handle_element_end_of_stream(:sink, _pad, _ctx, state) do
    {[terminate: :normal], state}
  end

  @impl true
  def handle_element_end_of_stream(_element, _pad, _ctx, state) do
    {[], state}
  end
end

{:ok, _pipeline, pid} = Membrane.Pipeline.start_link(Decoding.Pipeline)
ref = Process.monitor(pid)

receive do
  {:DOWN, ^ref, :process, _pid, _reason} -> :ok
end
