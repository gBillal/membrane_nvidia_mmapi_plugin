defmodule DecoderTest do
  use ExUnit.Case, async: true
  use Membrane.Pipeline

  import Membrane.Testing.Assertions

  alias Membrane.H264
  alias Membrane.Testing.Pipeline

  defp prepare_paths(filename, tmp_dir) do
    in_path = "test/fixtures/h264/input-#{filename}.h264"
    reference_path = "test/fixtures/h264/reference-#{filename}.raw"
    out_path = Path.join(tmp_dir, "output-decoding-#{filename}.raw")
    {in_path, reference_path, out_path}
  end

  defp make_pipeline(in_path, out_path) do
    Pipeline.start_link_supervised!(
      spec:
        child(:file_src, %Membrane.File.Source{chunk_size: 40_960, location: in_path})
        |> child(:parser, %H264.Parser{
          generate_best_effort_timestamps: %{framerate: {30, 1}}
        })
        |> child(:decoder, Membrane.Nvidia.MMAPI.Decoder)
        |> child(:sink, %Membrane.File.Sink{location: out_path})
    )
  end

  defp assert_files_equal(file_a, file_b) do
    assert {:ok, a} = File.read(file_a)
    assert {:ok, b} = File.read(file_b)
    assert a == b
  end

  defp perform_decoding_test(filename, tmp_dir, timeout) do
    {in_path, ref_path, out_path} = prepare_paths(filename, tmp_dir)

    pid = make_pipeline(in_path, out_path)
    assert_end_of_stream(pid, :sink, :input, timeout)
    assert_files_equal(out_path, ref_path)
    Pipeline.terminate(pid)
  end

  describe "DecodingPipeline should" do
    @describetag :tmp_dir
    test "decode 10 720p frames", ctx do
      perform_decoding_test("10-720p", ctx.tmp_dir, 5000)
    end

    test "decode 100 240p frames", ctx do
      perform_decoding_test("100-240p", ctx.tmp_dir, 5000)
    end

    test "decode 10 720p frames with B frames in main profile", ctx do
      perform_decoding_test("10-720p-main", ctx.tmp_dir, 5000)
    end
  end
end
