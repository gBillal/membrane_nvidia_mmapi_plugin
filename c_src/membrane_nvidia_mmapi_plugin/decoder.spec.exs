module Membrane.Nvidia.MMAPI.Decoder.Native

state_type "State"

interface [NIF]

spec create(format :: atom, width :: int, height :: int) :: {:ok :: label, state} | {:error :: label, reason :: atom}
spec decode(payload, timestamp :: int64, state) :: {:ok :: label, [payload], [int64]} | {:error :: label, reason :: atom}
spec flush(state) :: {:ok :: label, [payload]} | {:error :: label, reason :: atom}

dirty :cpu, decode: 3, flush: 1
