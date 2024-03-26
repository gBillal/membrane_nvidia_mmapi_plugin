# Membrane Nvidia MMAPI Plugin

[![Hex.pm](https://img.shields.io/hexpm/v/membrane_template_plugin.svg)](https://hex.pm/packages/membrane_template_plugin)
[![API Docs](https://img.shields.io/badge/api-docs-yellow.svg?style=flat)](https://hexdocs.pm/membrane_nvidia_mmapi_plugin)

A collection of elements that leverage the Nvidia Jetson hardware using [Multimedia API](https://docs.nvidia.com/jetson/l4t-multimedia/mmapi_group.html)

## Elements
The plugin will contain the following elements:
| Element | Codecs | Description | Status |
|---------|--------|-------------|--------|
| Decoder | H264,H265 | Hardware video decoder | Implemented |
| Encoder | H264,H265 | Hardware video encoder | Planned | 
| Transcoder | H264,H265 | Hardware video transcoder | Planned | 
| Jpeg Encoder | I420 | Hardware jpeg encoder | Planned | 
| Jpeg Decoder | I420 | Hardware jpeg decoder | Planned | 

## Installation

Add the following line to your list of dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:membrane_template_plugin, "~> 0.1.0"}
  ]
end
```

## Usage

### Video Encoder
