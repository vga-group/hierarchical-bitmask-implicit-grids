#ifndef RAYBASE_GFX_ALIAS_TABLE_GLSL
#define RAYBASE_GFX_ALIAS_TABLE_GLSL

struct alias_table_entry
{
    uint alias_id;
    uint probability;
    float pdf;
    float alias_pdf;
};

#endif
