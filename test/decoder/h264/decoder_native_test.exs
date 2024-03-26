defmodule Decoder.NativeTest do
  use ExUnit.Case, async: true

  alias Membrane.Nvidia.MMAPI.Decoder.Native
  alias Membrane.Payload

  test "Decode 1 240p frame" do
    in_path = "test/fixtures/h264/input-100-240p.h264"
    ref_path = "test/fixtures/h264/reference-100-240p.raw"

    assert {:ok, file} = File.read(in_path)
    assert {:ok, decoder_ref} = Native.create(:H264, -1, -1)
    assert <<frame::bytes-size(7469), _rest::binary>> = file
    assert {:ok, _frames, _pts_list} = Native.decode(frame, 0, decoder_ref)
    assert {:ok, [frame], _pts_list} = Native.flush(decoder_ref)
    assert Payload.size(frame) == 115_200
    assert {:ok, ref_file} = File.read(ref_path)
    assert <<ref_frame::bytes-size(115_200), _rest::binary>> = ref_file
    assert Payload.to_binary(frame) == ref_frame
  end

  test "Decode and scale 1 240p frame" do
    in_path = "test/fixtures/h264/input-100-240p.h264"
    ref_path = "image2.raw"

    assert {:ok, file} = File.read(in_path)
    assert {:ok, decoder_ref} = Native.create(:H264, 160, 120)
    assert <<frame::bytes-size(7469), _rest::binary>> = file
    assert {:ok, _frames, _pts_list} = Native.decode(frame, 0, decoder_ref)
    assert {:ok, [frame], _pts_list} = Native.flush(decoder_ref)
    assert Payload.size(frame) == 28800
  end
end
