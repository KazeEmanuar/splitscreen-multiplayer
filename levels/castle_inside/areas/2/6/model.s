inside_castle_seg7_light_0704A1B8: # 0x0704A1B8 - 0x0704A1C0
.byte 0x3D, 0x3D, 0x3F, 0x00, 0x3D, 0x3D, 0x3F, 0x00

inside_castle_seg7_light_0704A1C0: # 0x0704A1C0 - 0x0704A1D0
.byte 0xF5, 0xF5, 0xFF, 0x00, 0xF5, 0xF5, 0xFF, 0x00
.byte 0x28, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00

inside_castle_seg7_vertex_0704A1D0: # 0x0704A1D0 - 0x0704A290
vertex   4332,   1408,   3415,      3415,      1006,  0xff, 0xff, 0xff, 0xff
vertex   4332,   2125,   2647,      2647,      0,  0xff, 0xff, 0xff, 0xff
vertex   4332,   1408,   2647,      2647,      1006,  0xff, 0xff, 0xff, 0xff
vertex   4332,   2125,   3415,      3415,      0,  0xff, 0xff, 0xff, 0xff
vertex   4332,   1408,   1008,      1008,      1006,  0xff, 0xff, 0xff, 0xff
vertex   4332,   2125,    240,      240,      0,  0xff, 0xff, 0xff, 0xff
vertex   4332,   1408,    240,      240,      1006,  0xff, 0xff, 0xff, 0xff
vertex   4332,   2125,   1008,      1008,      0,  0xff, 0xff, 0xff, 0xff
vertex   4332,   1408,   2493,      2493,      1006,  0xff, 0xff, 0xff, 0xff
vertex   4332,   2125,   1162,      1162,      0,  0xff, 0xff, 0xff, 0xff
vertex   4332,   1408,   1162,      1162,      1006, 0xff, 0xff, 0xff, 0xff
vertex   4332,   2125,   2493,      2493,      0,  0xff, 0xff, 0xff, 0xff

inside_castle_seg7_dl_0704A290: # 0x0704A290 - 0x0704A2E0
gsSPLight inside_castle_seg7_light_0704A1C0, 1
gsSPLight inside_castle_seg7_light_0704A1B8, 2
gsSPVertex inside_castle_seg7_vertex_0704A1D0, 12, 0
gsSP2Triangles  0,  1,  2, 0x0,  0,  3,  1, 0x0
gsSP2Triangles  4,  5,  6, 0x0,  4,  7,  5, 0x0
gsSP2Triangles  8,  9, 10, 0x0,  8, 11,  9, 0x0
gsSPEndDisplayList

glabel inside_castle_seg7_dl_0704A2E0 # 0x0704A2E0 - 0x0704A368



gsDPPipeSync
gsDPSetCombineModeLERP1Cycle G_CCMUX_TEXEL0, G_CCMUX_0, G_CCMUX_SHADE, G_CCMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_SHADE
gsSPClearGeometryMode G_LIGHTING
gsDPSetTile G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD
gsSPTexture -1, -1, 0, 0, 1
gsDPTileSync
gsDPSetTile G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 5, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, 5, G_TX_NOLOD
gsDPSetTileSize 0, 0, 0, 124, 124

gsDPSetTextureImage G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, inside_0900A000
gsDPLoadSync
gsDPLoadBlock 7, 0, 0, 0x3FF, 0x100
gsSPVertex inside_castle_seg7_vertex_0704A1D0, 12, 0
gsSP2Triangles  0,  1,  2, 0x0,  0,  3,  1, 0x0
gsSP2Triangles  4,  5,  6, 0x0,  4,  7,  5, 0x0
gsSP2Triangles  8,  9, 10, 0x0,  8, 11,  9, 0x0

gsSPTexture -1, -1, 0, 0, 0
gsDPPipeSync
gsDPSetCombineModeLERP1Cycle G_CCMUX_0, G_CCMUX_0, G_CCMUX_0, G_CCMUX_SHADE, G_ACMUX_0, G_ACMUX_0, G_ACMUX_0, G_ACMUX_SHADE
gsSPSetGeometryMode G_LIGHTING
gsSPEndDisplayList