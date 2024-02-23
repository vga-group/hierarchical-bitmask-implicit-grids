#ifndef RAYBASE_GFX_CLUSTERING_GLSL
#define RAYBASE_GFX_CLUSTERING_GLSL
#include "math.glsl"

#define CLUSTER_AXIS_COUNT 3

// NOTE: You can get better performance with decals by unrolling where there's
// the commented [[unroll]]! It's not something you should unroll if the content
// of the for-loop traces rays, though.
#define FOR_CLUSTER(world_pos, inv_slice, cluster_offset, cluster_size, cluster_axis_offsets, hierarchy_axis_offsets, cluster_slice_entries, hierarchy_slice_entries, cluster_slices, MASK_SYNC) \
    {\
        const ivec3 slice_index = ivec3( \
            floor(world_pos * (inv_slice) + (cluster_offset)) \
        );\
        if( \
            all(lessThan(slice_index, cluster_size)) && \
            all(greaterThanEqual(slice_index, ivec3(0))) \
        ){ \
            const uint base_entries = (hierarchy_slice_entries == 0 ? cluster_slice_entries : hierarchy_slice_entries); \
            for(uint i = 0; i < base_entries; ++i){ \
                const uvec3 hierarchy_entry_index = (hierarchy_slice_entries == 0 ? cluster_axis_offsets : hierarchy_axis_offsets) + slice_index * base_entries + i; \
                uvec4 hierarchy_mask = \
                    cluster_slices.slices[hierarchy_entry_index.x] & \
                    cluster_slices.slices[hierarchy_entry_index.y] & \
                    cluster_slices.slices[hierarchy_entry_index.z]; \
                MASK_SYNC(hierarchy_mask); \
                for(int j = 0; j < 4; ++j) \
                { \
                    for( \
                        int hoff = findLSB(hierarchy_mask.x); \
                        hoff >= 0; \
                        hoff = findLSB(hierarchy_mask.x) \
                    ){ \
                        hierarchy_mask.x ^= 1 << hoff; \
                        const uint cluster_slice_entry_index = hierarchy_slice_entries == 0 ?  \
                            0 : i * 128 + j * 32 + hoff; \
                        uvec4 cluster_mask = uvec4(0);\
                        if(hierarchy_slice_entries != 0) \
                        { \
                            uvec3 cluster_entry_index = cluster_axis_offsets + slice_index * cluster_slice_entries + cluster_slice_entry_index; \
                            cluster_mask = \
                                cluster_slices.slices[cluster_entry_index.x] & \
                                cluster_slices.slices[cluster_entry_index.y] & \
                                cluster_slices.slices[cluster_entry_index.z]; \
                            MASK_SYNC(cluster_mask); \
                        } \
                        /*[[unroll]]*/ for(int k = 0; k < (hierarchy_slice_entries == 0 ? 1 : 4); ++k) \
                        { \
                            for( \
                                int coff = (hierarchy_slice_entries == 0 ? 0 : findLSB(cluster_mask.x)); \
                                coff >= 0; \
                                coff = (hierarchy_slice_entries == 0 ? coff-1 : findLSB(cluster_mask.x)) \
                            ){ \
                                if(hierarchy_slice_entries != 0) \
                                    cluster_mask.x ^= 1 << coff; \
                                const uint item_index = (hierarchy_slice_entries == 0 ? \
                                    hoff + j * 32 + i * 128 : \
                                    coff + k * 32 + cluster_slice_entry_index * 128);
#define END_FOR_CLUSTER \
                            } \
                            cluster_mask = cluster_mask.yzwx; \
                        } \
                    } \
                    hierarchy_mask = hierarchy_mask.yzwx; \
                } \
            } \
        } \
    }

#define SAMPLE_CLUSTER(world_pos, inv_slice, cluster_offset, cluster_size, cluster_axis_offsets, hierarchy_axis_offsets, cluster_slice_entries, hierarchy_slice_entries, cluster_slices, seed, MASK_SYNC) \
    {\
        ivec3 slice_index = ivec3( \
            floor(world_pos * (inv_slice) + (cluster_offset)) \
        );\
        uvec4 selected_cluster_mask = uvec4(0); \
        int selected_cluster_mask_index = -1; \
        if( \
            all(lessThan(slice_index, cluster_size)) && \
            all(greaterThanEqual(slice_index, ivec3(0))) \
        ){ \
            for(uint i = 0; i < max(hierarchy_slice_entries, 1); ++i){ \
                uvec3 hierarchy_entry_index = hierarchy_slice_entries == 0 ? uvec3(0) : (hierarchy_axis_offsets + slice_index * hierarchy_slice_entries + i); \
                uvec4 hierarchy_mask = uvec4(0); \
                if(hierarchy_slice_entries != 0) \
                { \
                    hierarchy_mask = \
                        cluster_slices.slices[hierarchy_entry_index.x] & \
                        cluster_slices.slices[hierarchy_entry_index.y] & \
                        cluster_slices.slices[hierarchy_entry_index.z]; \
                    MASK_SYNC(hierarchy_mask); \
                } \
                for(int j = 0; j < (hierarchy_slice_entries == 0 ? cluster_slice_entries : 4); ++j) \
                { \
                    for( \
                        int hoff = (hierarchy_slice_entries == 0 ? 0 : findLSB(hierarchy_mask[j])); \
                        hoff >= 0; \
                        hoff = (hierarchy_slice_entries == 0 ? hoff-1 : findLSB(hierarchy_mask[j])) \
                    ){ \
                        if(hierarchy_slice_entries != 0) \
                            hierarchy_mask[j] ^= 1 << hoff; \
                        uint cluster_slice_entry_index = hierarchy_slice_entries == 0 ? j : i * 128 + j * 32 + hoff; \
                        uvec3 cluster_entry_index = cluster_axis_offsets + slice_index * cluster_slice_entries + cluster_slice_entry_index; \
                        uvec4 cluster_mask = \
                            cluster_slices.slices[cluster_entry_index.x] & \
                            cluster_slices.slices[cluster_entry_index.y] & \
                            cluster_slices.slices[cluster_entry_index.z]; \
                        MASK_SYNC(cluster_mask); \
                        float r = generate_uniform_random(seed); \
                        ivec4 count_per_element = bitCount(cluster_mask); \
                        int cur_count = count_per_element.x + count_per_element.y + \
                            count_per_element.z + count_per_element.w; \
                        item_count += cur_count; \
                        if(r * item_count <= cur_count) \
                        { \
                            selected_cluster_mask = cluster_mask; \
                            selected_cluster_mask_index = int(cluster_slice_entry_index); \
                        } \
                    } \
                } \
            } \
        } \
        if(selected_cluster_mask_index >= 0) \
        {\
            float r = generate_uniform_random(seed); \
            ivec4 count_per_element = bitCount(selected_cluster_mask); \
            int count = count_per_element.x + count_per_element.y + \
                count_per_element.z + count_per_element.w; \
            int selected_bit_index = clamp(int(r * count), 0, count-1); \
            selected_index = selected_cluster_mask_index * 128 + int(find_nth_set_bit(selected_cluster_mask, selected_bit_index)); \
        } \
    }

#define GET_CLUSTER_COUNT(world_pos, inv_slice, cluster_offset, cluster_size, cluster_axis_offsets, hierarchy_axis_offsets, cluster_slice_entries, hierarchy_slice_entries, cluster_slices, MASK_SYNC) \
    {\
        ivec3 slice_index = ivec3( \
            floor(world_pos * (inv_slice) + (cluster_offset)) \
        );\
        if( \
            all(lessThan(slice_index, cluster_size)) && \
            all(greaterThanEqual(slice_index, ivec3(0))) \
        ){ \
            for(uint i = 0; i < max(hierarchy_slice_entries, 1); ++i){ \
                uvec3 hierarchy_entry_index = hierarchy_slice_entries == 0 ? uvec3(0) : (hierarchy_axis_offsets + slice_index * hierarchy_slice_entries + i); \
                uvec4 hierarchy_mask = uvec4(0); \
                if(hierarchy_slice_entries != 0) \
                { \
                    hierarchy_mask = \
                        cluster_slices.slices[hierarchy_entry_index.x] & \
                        cluster_slices.slices[hierarchy_entry_index.y] & \
                        cluster_slices.slices[hierarchy_entry_index.z]; \
                    MASK_SYNC(hierarchy_mask); \
                } \
                for(int j = 0; j < (hierarchy_slice_entries == 0 ? cluster_slice_entries : 4); ++j) \
                { \
                    for( \
                        int hoff = (hierarchy_slice_entries == 0 ? 0 : findLSB(hierarchy_mask[j])); \
                        hoff >= 0; \
                        hoff = (hierarchy_slice_entries == 0 ? hoff-1 : findLSB(hierarchy_mask[j])) \
                    ){ \
                        if(hierarchy_slice_entries != 0) \
                            hierarchy_mask[j] ^= 1 << hoff; \
                        uint cluster_slice_entry_index = hierarchy_slice_entries == 0 ? j : i * 128 + j * 32 + hoff; \
                        uvec3 cluster_entry_index = cluster_axis_offsets + slice_index * cluster_slice_entries + cluster_slice_entry_index; \
                        uvec4 cluster_mask = \
                            cluster_slices.slices[cluster_entry_index.x] & \
                            cluster_slices.slices[cluster_entry_index.y] & \
                            cluster_slices.slices[cluster_entry_index.z]; \
                        MASK_SYNC(cluster_mask); \
                        ivec4 count_per_element = bitCount(cluster_mask); \
                        item_count += count_per_element.x + count_per_element.y + \
                            count_per_element.z + count_per_element.w; \
                    } \
                } \
            } \
        } \
    }

#endif
