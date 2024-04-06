defmodule Decoder.H265.NativeTest do
  use ExUnit.Case, async: true

  alias Membrane.Nvidia.MMAPI.Decoder.Native
  alias Membrane.Payload

  test "Decode 1 480p frame" do
    in_path = "test/fixtures/h265/input-60-480p.h265"
    ref_path = "test/fixtures/h265/reference-60-480p.raw"

    assert {:ok, file} = File.read(in_path)
    assert {:ok, decoder_ref} = Native.create(:H265, -1, -1)
    assert <<frame::bytes-size(49_947), _rest::binary>> = file
    assert {:ok, _frames, _pts_list} = Native.decode(frame, 0, decoder_ref)
    assert {:ok, [frame], _pts_list} = Native.flush(decoder_ref)
    assert Payload.size(frame) == 460_800
    assert {:ok, ref_file} = File.read(ref_path)
    assert <<ref_frame::bytes-size(460_800), _rest::binary>> = ref_file
    assert Payload.to_binary(frame) == ref_frame
  end
end
