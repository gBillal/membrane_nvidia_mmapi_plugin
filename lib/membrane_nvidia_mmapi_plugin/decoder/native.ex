defmodule Membrane.Nvidia.MMAPI.Decoder.Native do
  @moduledoc false
  use Unifex.Loader

  def create!(codec, width, height) do
    case create(codec, width, height) do
      {:ok, decoder_ref} -> decoder_ref
      {:error, reason} -> raise "could not create decoder due to #{inspect(reason)}"
    end
  end
end
