constexpr uint8_t material_lut[32*32*4] = {
    19, 236, 255, 255,
    54, 201, 87, 255,
    85, 170, 3, 255,
    112, 143, 0, 255,
    135, 120, 0, 255,
    156, 99, 0, 255,
    173, 82, 0, 255,
    188, 67, 0, 255,
    201, 54, 0, 255,
    211, 44, 0, 255,
    220, 35, 0, 255,
    227, 28, 0, 255,
    234, 21, 0, 255,
    239, 16, 0, 255,
    243, 12, 0, 255,
    246, 9, 0, 255,
    248, 7, 0, 255,
    250, 5, 0, 255,
    252, 3, 0, 255,
    253, 2, 0, 255,
    253, 2, 0, 255,
    254, 1, 0, 255,
    254, 1, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    20, 232, 255, 252,
    55, 200, 255, 255,
    85, 169, 196, 255,
    112, 143, 79, 255,
    136, 119, 30, 255,
    156, 99, 11, 255,
    173, 82, 3, 255,
    188, 67, 1, 255,
    201, 54, 0, 255,
    211, 44, 0, 255,
    220, 35, 0, 255,
    227, 28, 0, 255,
    234, 21, 0, 255,
    239, 16, 0, 255,
    243, 12, 0, 255,
    246, 9, 0, 255,
    248, 7, 0, 255,
    250, 5, 0, 255,
    252, 3, 0, 255,
    253, 2, 0, 255,
    253, 2, 0, 255,
    254, 1, 0, 255,
    254, 1, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    25, 212, 255, 238,
    56, 196, 255, 252,
    86, 168, 255, 254,
    113, 142, 171, 255,
    136, 119, 101, 255,
    156, 99, 58, 255,
    173, 82, 33, 255,
    188, 67, 18, 255,
    201, 54, 10, 255,
    211, 44, 5, 255,
    220, 35, 2, 255,
    227, 28, 1, 255,
    234, 21, 1, 255,
    238, 16, 0, 255,
    242, 12, 0, 255,
    246, 9, 0, 255,
    248, 7, 0, 255,
    250, 5, 0, 255,
    252, 3, 0, 255,
    253, 2, 0, 255,
    253, 2, 0, 255,
    254, 1, 0, 255,
    254, 1, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    37, 191, 255, 228,
    60, 185, 255, 246,
    88, 163, 255, 251,
    114, 139, 211, 253,
    137, 117, 145, 254,
    156, 98, 100, 254,
    173, 81, 68, 254,
    188, 67, 46, 255,
    201, 54, 30, 255,
    211, 44, 20, 255,
    220, 35, 13, 255,
    227, 27, 8, 255,
    233, 21, 5, 255,
    238, 17, 3, 255,
    242, 13, 2, 255,
    246, 9, 1, 255,
    248, 7, 1, 255,
    250, 5, 0, 255,
    251, 3, 0, 255,
    253, 2, 0, 255,
    253, 2, 0, 255,
    254, 1, 0, 255,
    254, 1, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    54, 175, 255, 229,
    67, 169, 255, 236,
    92, 154, 255, 246,
    116, 134, 223, 250,
    138, 114, 167, 252,
    157, 96, 125, 253,
    174, 80, 94, 254,
    188, 66, 70, 254,
    201, 54, 51, 254,
    211, 43, 38, 254,
    220, 35, 27, 254,
    227, 27, 19, 255,
    233, 21, 14, 255,
    238, 17, 10, 255,
    242, 13, 7, 255,
    245, 9, 4, 255,
    248, 7, 3, 255,
    250, 5, 2, 255,
    251, 3, 1, 255,
    252, 2, 1, 255,
    253, 2, 0, 255,
    254, 1, 0, 255,
    254, 1, 0, 255,
    254, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    73, 161, 255, 233,
    78, 151, 255, 229,
    97, 141, 255, 238,
    119, 126, 223, 245,
    139, 109, 176, 248,
    158, 92, 139, 250,
    174, 78, 110, 252,
    188, 64, 87, 252,
    200, 53, 68, 253,
    211, 43, 53, 253,
    219, 34, 41, 254,
    227, 27, 32, 254,
    233, 21, 24, 254,
    238, 17, 18, 254,
    242, 13, 14, 254,
    245, 9, 10, 254,
    247, 7, 7, 254,
    249, 5, 5, 254,
    251, 4, 4, 255,
    252, 2, 3, 255,
    253, 2, 2, 255,
    254, 1, 1, 255,
    254, 1, 1, 255,
    254, 0, 0, 255,
    254, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    255, 0, 0, 255,
    92, 146, 255, 238,
    92, 135, 255, 227,
    105, 126, 255, 232,
    123, 115, 218, 238,
    142, 101, 178, 243,
    159, 87, 146, 246,
    174, 74, 120, 248,
    188, 62, 99, 250,
    200, 51, 81, 251,
    210, 42, 66, 252,
    219, 34, 53, 252,
    226, 27, 43, 253,
    232, 21, 35, 253,
    237, 16, 28, 253,
    241, 13, 22, 254,
    244, 10, 17, 254,
    247, 7, 13, 254,
    249, 5, 10, 254,
    250, 4, 8, 254,
    252, 3, 6, 254,
    252, 2, 4, 254,
    253, 1, 3, 254,
    254, 1, 2, 254,
    254, 0, 2, 254,
    254, 0, 1, 254,
    254, 0, 1, 254,
    254, 0, 0, 254,
    254, 0, 0, 254,
    254, 0, 0, 254,
    254, 0, 0, 254,
    255, 0, 0, 255,
    255, 0, 0, 255,
    110, 131, 255, 241,
    107, 121, 255, 228,
    115, 112, 253, 228,
    129, 103, 211, 232,
    145, 92, 177, 237,
    160, 81, 149, 241,
    175, 69, 126, 244,
    188, 59, 106, 246,
    199, 49, 89, 248,
    209, 40, 75, 249,
    217, 33, 63, 250,
    225, 26, 53, 251,
    231, 21, 44, 251,
    236, 16, 36, 252,
    240, 13, 30, 252,
    243, 10, 24, 253,
    246, 7, 20, 253,
    248, 5, 16, 253,
    249, 4, 13, 253,
    251, 3, 10, 253,
    252, 2, 8, 253,
    252, 1, 6, 254,
    253, 1, 5, 254,
    253, 0, 3, 254,
    254, 0, 2, 254,
    254, 0, 2, 254,
    254, 0, 1, 254,
    254, 0, 1, 254,
    254, 0, 0, 254,
    254, 0, 0, 254,
    254, 0, 0, 254,
    254, 0, 0, 254,
    126, 117, 255, 243,
    122, 108, 255, 230,
    127, 100, 239, 226,
    137, 91, 203, 228,
    149, 82, 174, 232,
    163, 73, 150, 236,
    175, 64, 129, 239,
    187, 54, 111, 242,
    198, 46, 96, 244,
    208, 38, 82, 246,
    216, 31, 70, 247,
    223, 25, 60, 248,
    229, 20, 51, 249,
    234, 16, 44, 250,
    238, 12, 37, 250,
    241, 10, 31, 251,
    244, 7, 26, 251,
    246, 5, 22, 252,
    248, 4, 18, 252,
    249, 3, 15, 252,
    251, 2, 12, 252,
    251, 1, 9, 253,
    252, 1, 7, 253,
    252, 1, 6, 253,
    253, 0, 4, 253,
    253, 0, 3, 253,
    253, 0, 2, 253,
    253, 0, 2, 253,
    253, 0, 1, 253,
    253, 0, 1, 253,
    253, 0, 0, 254,
    254, 0, 0, 254,
    141, 104, 255, 245,
    136, 96, 255, 232,
    138, 88, 226, 226,
    145, 81, 196, 226,
    155, 73, 170, 228,
    165, 65, 149, 231,
    177, 57, 130, 234,
    187, 50, 114, 237,
    197, 42, 100, 239,
    206, 36, 87, 241,
    214, 30, 76, 243,
    221, 24, 66, 245,
    226, 20, 58, 246,
    231, 16, 50, 247,
    236, 12, 43, 248,
    239, 9, 37, 249,
    242, 7, 32, 249,
    244, 5, 27, 250,
    246, 4, 23, 250,
    248, 3, 19, 251,
    249, 2, 16, 251,
    250, 1, 13, 251,
    250, 1, 11, 251,
    251, 1, 9, 252,
    251, 0, 7, 252,
    252, 0, 5, 252,
    252, 0, 4, 252,
    252, 0, 3, 252,
    252, 0, 2, 252,
    252, 0, 1, 252,
    253, 0, 1, 253,
    253, 0, 0, 253,
    154, 92, 255, 246,
    149, 85, 249, 234,
    150, 78, 215, 227,
    154, 71, 189, 225,
    161, 64, 166, 225,
    169, 58, 147, 227,
    178, 51, 130, 229,
    187, 45, 116, 232,
    196, 38, 103, 234,
    204, 33, 91, 237,
    211, 27, 81, 239,
    218, 23, 71, 240,
    224, 18, 63, 242,
    228, 15, 55, 243,
    233, 12, 49, 244,
    236, 9, 42, 245,
    239, 7, 37, 246,
    242, 5, 32, 247,
    244, 4, 28, 248,
    245, 3, 24, 248,
    247, 2, 20, 249,
    248, 1, 17, 249,
    249, 1, 14, 249,
    249, 1, 12, 250,
    250, 0, 10, 250,
    250, 0, 8, 250,
    250, 0, 6, 251,
    251, 0, 5, 251,
    251, 0, 4, 251,
    251, 0, 3, 251,
    251, 0, 2, 251,
    251, 0, 1, 251,
    166, 82, 255, 247,
    161, 75, 235, 236,
    160, 69, 206, 229,
    162, 62, 182, 225,
    167, 57, 162, 224,
    173, 51, 145, 224,
    180, 45, 130, 226,
    188, 40, 116, 227,
    195, 34, 104, 230,
    202, 29, 94, 232,
    209, 25, 84, 234,
    215, 21, 75, 236,
    220, 17, 67, 237,
    225, 14, 60, 239,
    229, 11, 53, 240,
    233, 9, 47, 242,
    236, 7, 42, 243,
    238, 5, 37, 244,
    240, 4, 32, 244,
    242, 3, 28, 245,
    244, 2, 24, 246,
    245, 2, 21, 246,
    246, 1, 18, 247,
    247, 1, 15, 247,
    247, 0, 13, 248,
    248, 0, 11, 248,
    248, 0, 9, 248,
    249, 0, 7, 249,
    249, 0, 5, 249,
    249, 0, 4, 249,
    250, 0, 3, 250,
    250, 0, 2, 250,
    176, 72, 255, 248,
    171, 66, 223, 237,
    170, 60, 197, 230,
    170, 55, 176, 225,
    173, 50, 158, 223,
    178, 45, 142, 222,
    183, 40, 129, 223,
    189, 35, 116, 224,
    195, 31, 105, 225,
    201, 26, 95, 227,
    206, 23, 86, 229,
    212, 19, 78, 231,
    217, 16, 70, 233,
    221, 13, 63, 234,
    225, 11, 57, 236,
    229, 8, 51, 237,
    232, 7, 46, 238,
    234, 5, 41, 239,
    237, 4, 36, 240,
    238, 3, 32, 241,
    240, 2, 28, 242,
    241, 2, 25, 243,
    243, 1, 21, 244,
    243, 1, 18, 244,
    244, 1, 16, 245,
    245, 0, 13, 245,
    246, 0, 11, 246,
    246, 0, 9, 246,
    246, 0, 7, 247,
    247, 0, 6, 247,
    247, 0, 4, 247,
    247, 0, 3, 247,
    185, 64, 245, 248,
    180, 58, 213, 238,
    178, 53, 190, 231,
    178, 48, 170, 226,
    179, 43, 154, 222,
    182, 39, 140, 221,
    186, 35, 127, 220,
    190, 31, 116, 221,
    195, 27, 106, 222,
    199, 23, 97, 223,
    204, 20, 88, 224,
    209, 17, 80, 226,
    213, 14, 73, 227,
    217, 12, 66, 229,
    221, 10, 60, 231,
    224, 8, 54, 232,
    227, 6, 49, 233,
    230, 5, 44, 235,
    232, 4, 40, 236,
    234, 3, 36, 237,
    236, 2, 32, 238,
    237, 2, 28, 239,
    238, 1, 25, 240,
    239, 1, 22, 240,
    240, 1, 19, 241,
    241, 0, 16, 242,
    242, 0, 14, 242,
    243, 0, 12, 243,
    243, 0, 10, 243,
    244, 0, 8, 244,
    244, 0, 6, 244,
    244, 0, 4, 244,
    192, 56, 233, 248,
    187, 51, 204, 239,
    185, 46, 183, 231,
    184, 42, 165, 226,
    184, 38, 151, 222,
    186, 34, 138, 220,
    188, 30, 126, 218,
    191, 27, 116, 218,
    195, 24, 106, 218,
    198, 21, 97, 219,
    202, 18, 89, 220,
    206, 15, 82, 221,
    210, 13, 75, 222,
    213, 11, 69, 224,
    216, 9, 63, 225,
    219, 7, 57, 227,
    222, 6, 52, 228,
    225, 5, 47, 229,
    227, 4, 43, 230,
    229, 3, 39, 232,
    231, 2, 35, 233,
    232, 2, 31, 234,
    233, 1, 28, 235,
    235, 1, 25, 236,
    236, 1, 22, 236,
    237, 0, 19, 237,
    238, 0, 16, 238,
    238, 0, 14, 238,
    239, 0, 12, 239,
    240, 0, 10, 240,
    240, 0, 8, 240,
    241, 0, 6, 241,
    199, 50, 222, 248,
    194, 45, 196, 239,
    191, 41, 177, 231,
    189, 37, 161, 226,
    189, 33, 147, 222,
    189, 30, 135, 219,
    190, 27, 124, 217,
    192, 23, 115, 216,
    195, 21, 106, 215,
    197, 18, 98, 215,
    200, 16, 90, 216,
    203, 13, 83, 217,
    206, 11, 77, 217,
    209, 10, 71, 219,
    212, 8, 65, 220,
    214, 7, 60, 221,
    217, 5, 55, 222,
    219, 4, 50, 223,
    221, 3, 46, 225,
    223, 3, 42, 226,
    225, 2, 38, 227,
    226, 2, 34, 228,
    228, 1, 31, 229,
    229, 1, 28, 230,
    230, 1, 25, 231,
    231, 0, 22, 232,
    232, 0, 19, 233,
    233, 0, 17, 233,
    234, 0, 14, 234,
    235, 0, 12, 235,
    235, 0, 10, 235,
    236, 0, 8, 236,
    204, 44, 213, 248,
    199, 40, 189, 239,
    195, 36, 171, 231,
    193, 32, 157, 226,
    192, 29, 144, 221,
    192, 26, 133, 218,
    192, 23, 123, 215,
    193, 21, 114, 214,
    194, 18, 106, 213,
    196, 16, 98, 212,
    198, 14, 91, 212,
    200, 12, 84, 212,
    203, 10, 78, 213,
    205, 9, 73, 213,
    207, 7, 67, 214,
    209, 6, 62, 215,
    211, 5, 57, 216,
    213, 4, 53, 217,
    215, 3, 48, 218,
    217, 3, 44, 219,
    219, 2, 41, 221,
    220, 1, 37, 222,
    221, 1, 34, 223,
    223, 1, 30, 224,
    224, 1, 27, 225,
    225, 0, 24, 226,
    226, 0, 22, 226,
    227, 0, 19, 227,
    228, 0, 17, 228,
    229, 0, 14, 229,
    230, 0, 12, 230,
    230, 0, 10, 230,
    209, 39, 206, 248,
    203, 35, 183, 238,
    199, 32, 167, 231,
    197, 28, 153, 225,
    195, 25, 141, 220,
    194, 23, 131, 217,
    193, 20, 122, 214,
    194, 18, 113, 211,
    194, 16, 105, 210,
    195, 14, 98, 209,
    196, 12, 91, 208,
    198, 10, 85, 208,
    199, 9, 79, 208,
    201, 8, 74, 208,
    202, 6, 69, 209,
    204, 5, 64, 209,
    206, 4, 59, 210,
    207, 4, 55, 211,
    209, 3, 51, 212,
    210, 2, 47, 213,
    212, 2, 43, 214,
    213, 1, 40, 215,
    215, 1, 36, 216,
    216, 1, 33, 217,
    217, 1, 30, 218,
    218, 0, 27, 219,
    219, 0, 24, 219,
    220, 0, 21, 220,
    221, 0, 19, 221,
    222, 0, 16, 222,
    223, 0, 14, 223,
    224, 0, 12, 224,
    213, 35, 199, 248,
    207, 31, 178, 238,
    202, 28, 162, 230,
    199, 25, 149, 224,
    197, 22, 139, 219,
    195, 20, 129, 215,
    194, 18, 120, 212,
    193, 16, 112, 209,
    193, 14, 105, 207,
    193, 12, 98, 206,
    194, 11, 92, 204,
    195, 9, 86, 204,
    195, 8, 80, 203,
    196, 7, 75, 203,
    198, 6, 70, 203,
    199, 5, 65, 203,
    200, 4, 61, 204,
    201, 3, 57, 204,
    202, 3, 53, 205,
    204, 2, 49, 206,
    205, 2, 45, 207,
    206, 1, 42, 207,
    207, 1, 38, 208,
    208, 1, 35, 209,
    209, 1, 32, 210,
    210, 0, 29, 211,
    211, 0, 26, 212,
    212, 0, 24, 213,
    213, 0, 21, 213,
    214, 0, 19, 214,
    215, 0, 16, 215,
    216, 0, 14, 216,
    216, 31, 192, 247,
    209, 28, 173, 237,
    205, 25, 158, 229,
    201, 22, 146, 223,
    198, 20, 136, 218,
    196, 17, 127, 213,
    194, 15, 119, 210,
    193, 14, 111, 207,
    192, 12, 104, 204,
    192, 11, 98, 202,
    191, 9, 92, 201,
    192, 8, 86, 199,
    192, 7, 81, 199,
    192, 6, 76, 198,
    193, 5, 71, 198,
    193, 4, 67, 198,
    194, 4, 63, 198,
    195, 3, 58, 198,
    196, 2, 55, 198,
    197, 2, 51, 199,
    198, 2, 47, 199,
    198, 1, 44, 200,
    199, 1, 41, 200,
    200, 1, 37, 201,
    201, 1, 34, 202,
    202, 0, 32, 202,
    203, 0, 29, 203,
    204, 0, 26, 204,
    205, 0, 23, 205,
    206, 0, 21, 206,
    206, 0, 18, 206,
    207, 0, 16, 207,
    219, 28, 187, 247,
    212, 25, 168, 236,
    206, 22, 155, 228,
    202, 19, 144, 221,
    198, 17, 134, 216,
    196, 15, 125, 211,
    194, 14, 117, 207,
    192, 12, 110, 204,
    190, 11, 104, 201,
    189, 9, 98, 199,
    189, 8, 92, 197,
    188, 7, 87, 195,
    188, 6, 82, 194,
    188, 5, 77, 193,
    188, 4, 72, 192,
    188, 4, 68, 192,
    188, 3, 64, 191,
    189, 3, 60, 191,
    189, 2, 56, 191,
    190, 2, 53, 191,
    190, 1, 49, 191,
    191, 1, 46, 192,
    191, 1, 43, 192,
    192, 1, 39, 193,
    193, 0, 36, 193,
    193, 0, 34, 194,
    194, 0, 31, 194,
    195, 0, 28, 195,
    195, 0, 26, 196,
    196, 0, 23, 196,
    197, 0, 21, 197,
    198, 0, 18, 198,
    221, 25, 182, 246,
    213, 22, 165, 235,
    207, 19, 152, 227,
    202, 17, 141, 220,
    198, 15, 132, 214,
    195, 14, 124, 209,
    192, 12, 116, 204,
    190, 10, 109, 201,
    188, 9, 103, 198,
    187, 8, 97, 195,
    186, 7, 92, 193,
    185, 6, 87, 191,
    184, 5, 82, 189,
    183, 5, 78, 188,
    183, 4, 73, 186,
    182, 3, 69, 186,
    182, 3, 65, 185,
    182, 2, 61, 184,
    182, 2, 58, 184,
    182, 2, 54, 184,
    182, 1, 51, 184,
    183, 1, 48, 184,
    183, 1, 44, 184,
    183, 1, 41, 184,
    184, 0, 38, 184,
    184, 0, 36, 185,
    185, 0, 33, 185,
    185, 0, 30, 185,
    186, 0, 28, 186,
    186, 0, 25, 186,
    187, 0, 23, 187,
    187, 0, 20, 187,
    223, 23, 178, 246,
    214, 20, 161, 234,
    208, 17, 149, 225,
    202, 15, 139, 218,
    198, 14, 130, 211,
    194, 12, 122, 206,
    191, 10, 115, 201,
    188, 9, 109, 197,
    186, 8, 103, 194,
    184, 7, 97, 191,
    182, 6, 92, 188,
    181, 5, 87, 186,
    179, 5, 82, 184,
    178, 4, 78, 182,
    177, 3, 74, 181,
    177, 3, 70, 179,
    176, 2, 66, 178,
    175, 2, 62, 177,
    175, 2, 59, 177,
    175, 1, 56, 176,
    175, 1, 52, 176,
    175, 1, 49, 175,
    175, 1, 46, 175,
    175, 1, 43, 175,
    175, 0, 40, 175,
    175, 0, 37, 175,
    175, 0, 35, 175,
    175, 0, 32, 175,
    176, 0, 29, 176,
    176, 0, 27, 176,
    176, 0, 25, 176,
    177, 0, 22, 177,
    225, 20, 174, 245,
    215, 18, 158, 233,
    208, 16, 146, 223,
    202, 14, 136, 215,
    197, 12, 128, 209,
    193, 11, 121, 203,
    189, 9, 114, 198,
    186, 8, 108, 194,
    183, 7, 102, 190,
    180, 6, 97, 187,
    178, 5, 92, 184,
    176, 5, 87, 181,
    175, 4, 83, 179,
    173, 3, 79, 177,
    172, 3, 75, 175,
    171, 3, 71, 173,
    170, 2, 67, 172,
    169, 2, 64, 170,
    168, 1, 60, 169,
    167, 1, 57, 169,
    167, 1, 54, 168,
    166, 1, 51, 167,
    166, 1, 48, 167,
    166, 1, 45, 166,
    165, 0, 42, 166,
    165, 0, 39, 166,
    165, 0, 36, 165,
    165, 0, 34, 165,
    165, 0, 31, 165,
    165, 0, 29, 165,
    165, 0, 26, 165,
    165, 0, 24, 165,
    226, 19, 170, 244,
    215, 16, 155, 231,
    207, 14, 144, 221,
    201, 12, 134, 213,
    195, 11, 126, 206,
    190, 9, 119, 200,
    186, 8, 113, 195,
    183, 7, 107, 190,
    180, 6, 102, 186,
    177, 5, 97, 182,
    174, 5, 92, 179,
    172, 4, 87, 176,
    170, 4, 83, 173,
    168, 3, 79, 171,
    166, 3, 75, 169,
    164, 2, 71, 167,
    163, 2, 68, 165,
    162, 2, 64, 163,
    161, 1, 61, 162,
    160, 1, 58, 161,
    159, 1, 55, 160,
    158, 1, 52, 159,
    157, 1, 49, 158,
    157, 0, 46, 157,
    156, 0, 43, 156,
    156, 0, 41, 156,
    155, 0, 38, 155,
    155, 0, 36, 155,
    155, 0, 33, 155,
    154, 0, 31, 155,
    154, 0, 28, 154,
    154, 0, 26, 154,
    227, 17, 167, 244,
    215, 15, 152, 230,
    206, 13, 142, 219,
    199, 11, 133, 210,
    193, 10, 125, 203,
    188, 8, 118, 196,
    184, 7, 112, 191,
    179, 6, 106, 186,
    176, 6, 101, 181,
    173, 5, 96, 177,
    170, 4, 92, 174,
    167, 4, 87, 170,
    164, 3, 83, 168,
    162, 3, 79, 165,
    160, 2, 76, 162,
    158, 2, 72, 160,
    156, 2, 69, 158,
    155, 1, 65, 156,
    153, 1, 62, 155,
    152, 1, 59, 153,
    151, 1, 56, 152,
    150, 1, 53, 150,
    149, 1, 50, 149,
    148, 0, 48, 148,
    147, 0, 45, 147,
    146, 0, 42, 146,
    145, 0, 40, 146,
    145, 0, 37, 145,
    144, 0, 35, 144,
    144, 0, 32, 144,
    143, 0, 30, 143,
    143, 0, 28, 143,
    227, 16, 164, 243,
    215, 13, 150, 228,
    205, 11, 140, 217,
    198, 10, 131, 207,
    191, 9, 124, 200,
    185, 7, 117, 193,
    180, 6, 111, 187,
    176, 6, 106, 182,
    172, 5, 101, 177,
    168, 4, 96, 172,
    165, 4, 92, 169,
    162, 3, 87, 165,
    159, 3, 83, 162,
    156, 2, 80, 159,
    154, 2, 76, 156,
    152, 2, 73, 153,
    150, 1, 69, 151,
    148, 1, 66, 149,
    146, 1, 63, 147,
    144, 1, 60, 145,
    143, 1, 57, 144,
    141, 1, 54, 142,
    140, 0, 51, 141,
    139, 0, 49, 139,
    138, 0, 46, 138,
    137, 0, 44, 137,
    136, 0, 41, 136,
    135, 0, 39, 135,
    134, 0, 36, 134,
    133, 0, 34, 133,
    132, 0, 32, 132,
    132, 0, 29, 132,
    228, 14, 161, 242,
    214, 12, 148, 226,
    204, 10, 138, 214,
    196, 9, 129, 205,
    189, 8, 122, 196,
    182, 7, 116, 189,
    177, 6, 110, 183,
    172, 5, 105, 177,
    168, 4, 100, 172,
    164, 4, 96, 167,
    160, 3, 91, 163,
    157, 3, 87, 159,
    153, 2, 84, 156,
    150, 2, 80, 153,
    148, 2, 76, 150,
    145, 1, 73, 147,
    143, 1, 70, 144,
    141, 1, 67, 142,
    139, 1, 64, 140,
    137, 1, 61, 137,
    135, 1, 58, 136,
    133, 0, 55, 134,
    132, 0, 53, 132,
    130, 0, 50, 130,
    129, 0, 47, 129,
    127, 0, 45, 128,
    126, 0, 42, 126,
    125, 0, 40, 125,
    124, 0, 38, 124,
    123, 0, 35, 123,
    122, 0, 33, 122,
    121, 0, 31, 121,
    228, 13, 159, 241,
    213, 11, 146, 225,
    203, 9, 136, 212,
    194, 8, 128, 202,
    186, 7, 121, 193,
    179, 6, 115, 185,
    173, 5, 110, 178,
    168, 4, 104, 172,
    163, 4, 100, 167,
    159, 3, 95, 162,
    155, 3, 91, 158,
    151, 2, 87, 154,
    148, 2, 84, 150,
    144, 2, 80, 146,
    141, 2, 77, 143,
    139, 1, 73, 140,
    136, 1, 70, 137,
    134, 1, 67, 135,
    131, 1, 64, 132,
    129, 1, 62, 130,
    127, 1, 59, 128,
    125, 0, 56, 126,
    123, 0, 54, 124,
    122, 0, 51, 122,
    120, 0, 49, 120,
    118, 0, 46, 119,
    117, 0, 44, 117,
    116, 0, 41, 116,
    114, 0, 39, 114,
    113, 0, 37, 113,
    112, 0, 35, 112,
    111, 0, 32, 111,
    228, 12, 157, 241,
    213, 10, 144, 223,
    201, 9, 135, 209,
    191, 7, 127, 198,
    183, 6, 120, 189,
    176, 5, 114, 181,
    170, 5, 109, 174,
    164, 4, 104, 168,
    159, 3, 99, 162,
    154, 3, 95, 157,
    150, 3, 91, 152,
    146, 2, 87, 148,
    142, 2, 84, 144,
    138, 2, 80, 140,
    135, 1, 77, 136,
    132, 1, 74, 133,
    129, 1, 71, 130,
    127, 1, 68, 127,
    124, 1, 65, 125,
    122, 1, 62, 122,
    119, 0, 60, 120,
    117, 0, 57, 118,
    115, 0, 55, 116,
    113, 0, 52, 114,
    111, 0, 50, 112,
    110, 0, 47, 110,
    108, 0, 45, 108,
    107, 0, 43, 107,
    105, 0, 40, 105,
    104, 0, 38, 104,
    102, 0, 36, 102,
    101, 0, 34, 101,
    228, 11, 155, 240,
    211, 9, 142, 221,
    199, 8, 133, 207,
    189, 7, 126, 195,
    180, 6, 119, 186,
    172, 5, 113, 177,
    166, 4, 108, 170,
    159, 4, 103, 163,
    154, 3, 99, 157,
    149, 3, 95, 151,
    144, 2, 91, 146,
    140, 2, 87, 142,
    136, 2, 84, 138,
    132, 1, 80, 134,
    129, 1, 77, 130,
    126, 1, 74, 127,
    122, 1, 71, 123,
    120, 1, 68, 120,
    117, 1, 66, 118,
    114, 0, 63, 115,
    112, 0, 60, 112,
    110, 0, 58, 110,
    107, 0, 55, 108,
    105, 0, 53, 106,
    103, 0, 51, 103,
    101, 0, 48, 102,
    100, 0, 46, 100,
    98, 0, 44, 98,
    96, 0, 42, 96,
    95, 0, 39, 95,
    93, 0, 37, 93,
    92, 0, 35, 92,
    228, 11, 153, 239,
    210, 9, 141, 219,
    197, 7, 132, 204,
    186, 6, 124, 192,
    177, 5, 118, 182,
    169, 4, 113, 173,
    161, 4, 107, 165,
    155, 3, 103, 158,
    149, 3, 99, 152,
    144, 2, 95, 146,
    139, 2, 91, 141,
    134, 2, 87, 136,
    130, 1, 84, 132,
    126, 1, 81, 127,
    122, 1, 78, 124,
    119, 1, 75, 120,
    116, 1, 72, 117,
    113, 1, 69, 113,
    110, 1, 66, 110,
    107, 0, 64, 108,
    105, 0, 61, 105,
    102, 0, 59, 102,
    100, 0, 56, 100,
    98, 0, 54, 98,
    95, 0, 52, 96,
    93, 0, 49, 94,
    92, 0, 47, 92,
    90, 0, 45, 90,
    88, 0, 43, 88,
    86, 0, 41, 86,
    85, 0, 39, 85,
    83, 0, 36, 83,
};

