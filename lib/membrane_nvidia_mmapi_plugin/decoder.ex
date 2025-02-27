defmodule Membrane.Nvidia.MMAPI.Decoder do
  @moduledoc """
  Membrane element that decodes video in H264 or H265 format using the Jetson hardware decoder based on V4L2 interface.

  The element expects the received buffers to contain an integral access units.
  It also supports scaling the decoded frames using the `VIC` hardware accelerator.
  """

  use Membrane.Filter

  require Membrane.Logger

  alias __MODULE__.Native
  alias Membrane.{Buffer, H264, H265}
  alias Membrane.RawVideo

  def_options width: [
                spec: non_neg_integer(),
                default: nil,
                description: """
                Scale the decoded picture to the provided width.

                If height is not provided, it'll be calculated to keep the aspect ratio.
                """
              ],
              height: [
                spec: non_neg_integer(),
                default: nil,
                description: """
                Scale the decoded picture to the provided height.

                If width is not provided, it'll be calculated to keep the aspect ratio.
                """
              ]

  def_input_pad :input,
    flow_control: :auto,
    accepted_format:
      any_of(
        %H264{alignment: :au, stream_structure: :annexb},
        %H265{alignment: :au, stream_structure: :annexb}
      )

  def_output_pad :output,
    flow_control: :auto,
    accepted_format: %RawVideo{pixel_format: :I420, aligned: true}

  @impl true
  def handle_init(_ctx, opts) do
    state = Map.merge(Map.from_struct(opts), %{decoder_ref: nil})
    {[], state}
  end

  @impl true
  def handle_stream_format(:input, stream_format, ctx, state) do
    old_stream_format = ctx.pads.output.stream_format

    {width, height} = dimensions(stream_format, state)
    framerate = stream_format.framerate || {0, 1}

    if is_nil(old_stream_format) or old_stream_format != stream_format do
      codec =
        case stream_format do
          %H264{} -> :H264
          %H265{} -> :H265
        end

      stream_format = %RawVideo{
        width: width,
        height: height,
        pixel_format: :I420,
        aligned: true,
        framerate: framerate
      }

      {actions, state} =
        if state.decoder_ref,
          do: flush(state),
          else: {[], state}

      {actions ++ [stream_format: {:output, stream_format}],
       %{state | decoder_ref: Native.create!(codec, width, height)}}
    else
      {[], state}
    end
  end

  @impl true
  def handle_buffer(:input, buffer, _ctx, %{decoder_ref: decoder_ref} = state) do
    case Native.decode(buffer.payload, buffer.pts || 0, decoder_ref) do
      {:ok, frames, pts_list} ->
        {wrap_frames(frames, pts_list), state}

      {:error, reason} ->
        raise "Native decoder failed to decode the payload: #{inspect(reason)}"
    end
  end

  @impl true
  def handle_end_of_stream(:input, _ctx, state) do
    {actions, state} = flush(state)
    {actions ++ [end_of_stream: :output], state}
  end

  defp flush(state) do
    case Native.flush(state.decoder_ref) do
      {:ok, frames, pts_list} ->
        {wrap_frames(frames, pts_list), state}

      {:error, reason} ->
        raise "Native decoder failed to flush: #{inspect(reason)}"
    end
  end

  defp wrap_frames([], []), do: []

  defp wrap_frames(frames, pts_list) do
    Enum.zip(frames, pts_list)
    |> Enum.map(fn {frame, pts} ->
      %Buffer{pts: pts, payload: frame}
    end)
    |> then(&[buffer: {:output, &1}])
  end

  defp dimensions(%{width: width, height: height}, %{width: nil, height: nil}),
    do: {width, height}

  defp dimensions(%{width: width, height: height}, %{width: scaled_width, height: nil}) do
    h = div(scaled_width * height, width)
    {scaled_width, h + rem(h, 2)}
  end

  defp dimensions(%{width: width, height: height}, %{width: nil, height: scaled_height}) do
    w = div(scaled_height * width, height)
    {w + rem(w, 2), scaled_height}
  end

  defp dimensions(_stream_format, %{width: scaled_width, height: scaled_height}),
    do: {scaled_width, scaled_height}
end
