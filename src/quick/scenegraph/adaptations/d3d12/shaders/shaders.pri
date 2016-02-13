vertexcolor_VSPS = $$PWD/vertexcolor.hlsl
vertexcolor_vshader.input = vertexcolor_VSPS
vertexcolor_vshader.header = hlsl_vs_vertexcolor.h
vertexcolor_vshader.entry = VS_VertexColor
vertexcolor_vshader.type = vs_5_0
vertexcolor_pshader.input = vertexcolor_VSPS
vertexcolor_pshader.header = hlsl_ps_vertexcolor.h
vertexcolor_pshader.entry = PS_VertexColor
vertexcolor_pshader.type = ps_5_0

HLSL_SHADERS = vertexcolor_vshader vertexcolor_pshader
load(hlsl_bytecode_header)
