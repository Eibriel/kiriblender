
#include <stdlib.h>
#include <stdio.h>
#include "verse_header.h"
#include "v_cmd_gen.h"
#include "v_cmd_buf.h"

#if defined(V_GENERATE_FUNC_MODE)

void v_gen_material_cmd_def(void)
{	
	v_cg_new_cmd(V_NT_MATERIAL,			"m_fragment_create", 68, VCGCT_NORMAL);
	v_cg_add_param(VCGP_NODE_ID,		"node_id");
	v_cg_add_param(VCGP_FRAGMENT_ID,	"frag_id");
	v_cg_add_param(VCGP_END_ADDRESS,	NULL);
	v_cg_add_param(VCGP_ENUM_NAME,		"VNMFragmentType");
	v_cg_add_param(VCGP_ENUM,			"type");
	v_cg_add_param(VCGP_POINTER_TYPE,	"VMatFrag");
	v_cg_add_param(VCGP_POINTER,		"fragment");

	v_cg_add_param(VCGP_PACK_INLINE, 	"\tswitch(type)\n"
	"\t{\n"
	"\tcase VN_M_FT_COLOR :\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->color.red);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->color.green);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->color.blue);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_LIGHT :\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->light.type);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->light.normal_falloff);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], fragment->light.brdf);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->light.brdf_r, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->light.brdf_g, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->light.brdf_b, 16);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_REFLECTION :\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->reflection.normal_falloff);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_TRANSPARENCY :\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->transparency.normal_falloff);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->transparency.refraction_index);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_GEOMETRY :\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->geometry.layer_r, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->geometry.layer_g, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->geometry.layer_b, 16);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_VOLUME :\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->volume.diffusion);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->volume.col_r);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->volume.col_g);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->volume.col_b);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_VIEW :\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_TEXTURE :\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint32(&buf[buffer_pos], fragment->texture.bitmap);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->texture.layer_r, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->texture.layer_g, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->texture.layer_b, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->texture.filtered);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->texture.mapping);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_NOISE :\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->noise.type);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->noise.mapping);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_BLENDER :\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->blender.type);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->blender.data_a);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->blender.data_b);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->blender.control);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_CLAMP :\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->clamp.min);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->clamp.red);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->clamp.green);\n"
	"\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->clamp.blue);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->clamp.data);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_MATRIX :\n"
	"\t\t{\n"
	"\t\t\tunsigned int i;\n"
	"\t\t\tfor(i = 0; i < 16; i++)\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->matrix.matrix[i]);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->matrix.data);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_RAMP :\n"
	"\t\tif(fragment->ramp.point_count == 0)\n"
	"\t\t\treturn;\n"
	"\t\t{\n"
	"\t\t\tunsigned int i, pos;\n"
	"\t\t\tdouble last;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->ramp.type);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], (uint8)fragment->ramp.channel);\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->ramp.mapping);\n"
	"\t\t\tpos = buffer_pos;\n"
	"\t\t\tbuffer_pos += vnp_raw_pack_uint8(&buf[buffer_pos], fragment->ramp.point_count);\n"
	"\t\t\tlast = fragment->ramp.ramp[0].pos - 1;\n"
	"\t\t\tfor(i = 0; i < fragment->ramp.point_count && fragment->ramp.ramp[i].pos > last && i < 48; i++)\n"
	"\t\t\t{\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->ramp.ramp[i].pos);\n"
	"\t\t\t\tlast = fragment->ramp.ramp[i].pos;\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->ramp.ramp[i].red);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->ramp.ramp[i].green);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_pack_real64(&buf[buffer_pos], fragment->ramp.ramp[i].blue);\n"
	"\t\t\t}\n\t\t\tif(i != fragment->ramp.point_count)\n"
	"\t\t\t\tvnp_raw_pack_uint8(&buf[pos], i);\n"
	"\t\t}\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_ANIMATION :\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->animation.label, 16);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_ALTERNATIVE :\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->alternative.alt_a);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->alternative.alt_b);\n"
	"\t\tbreak;\n"
	"\tcase VN_M_FT_OUTPUT :\n"
	"\t\tbuffer_pos += vnp_raw_pack_string(&buf[buffer_pos], fragment->output.label, 16);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->output.front);\n"
	"\t\tbuffer_pos += vnp_raw_pack_uint16(&buf[buffer_pos], fragment->output.back);\n"
	"\t\tbreak;\n"
	"\t}\n");

	v_cg_add_param(VCGP_UNPACK_INLINE, 	"\tif(type <= VN_M_FT_OUTPUT)\n"
	"\t{\n"
	"\t\tVMatFrag frag;\n"
	"\t\tuint8 temp;\n"
	"\t\tswitch(type)\n"
	"\t\t{\n"
	"\t\tcase VN_M_FT_COLOR :\n"
	"\t\t\tif(buffer_pos + 3 * 8 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.color.red);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.color.green);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.color.blue);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_LIGHT :\n"
	"\t\t\tif(buffer_pos + 13 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\tfrag.light.type = (VNMLightType)temp;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.light.normal_falloff);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &frag.light.brdf);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.light.brdf_r, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.light.brdf_g, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.light.brdf_b, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_REFLECTION :\n"
	"\t\t\tif(buffer_pos + 8 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.reflection.normal_falloff);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_TRANSPARENCY :\n"
	"\t\t\tif(buffer_pos + 16 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.transparency.normal_falloff);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.transparency.refraction_index);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_VOLUME :\n"
	"\t\t\tif(buffer_pos + 32 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.volume.diffusion);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.volume.col_r);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.volume.col_g);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.volume.col_b);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_VIEW :\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_GEOMETRY :\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.geometry.layer_r, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.geometry.layer_g, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.geometry.layer_b, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_TEXTURE :\n"
	"\t\t\tif(buffer_pos + 10 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint32(&buf[buffer_pos], &frag.texture.bitmap);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.texture.layer_r, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.texture.layer_g, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.texture.layer_b, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\tfrag.texture.filtered = (VNMNoiseType)temp;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.texture.mapping);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_NOISE :\n"
	"\t\t\tif(buffer_pos + 3 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\tfrag.noise.type = (VNMNoiseType)temp;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.noise.mapping);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_BLENDER :\n"
	"\t\t\tif(buffer_pos + 7 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\tfrag.blender.type = (VNMBlendType)temp;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.blender.data_a);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.blender.data_b);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.blender.control);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_CLAMP :\n"
	"\t\t\tif(buffer_pos + 27 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\tfrag.clamp.min = (VNMBlendType)temp;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.clamp.red);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.clamp.green);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.clamp.blue);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.clamp.data);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_MATRIX :\n"
	"\t\t\tif(buffer_pos + 8 * 16 + 2 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\telse\n"
	"\t\t\t{\n"
	"\t\t\t\tunsigned int i;\n"
	"\t\t\t\tfor(i = 0; i < 16; i++)\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.matrix.matrix[i]);\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.matrix.data);\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_RAMP :\n"
	"\t\t\tif(buffer_pos + 5 + 4 * 8 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\telse\n"
	"\t\t\t{\n"
	"\t\t\t\tunsigned int i, pos;\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\t\tfrag.ramp.type = (VNMRampType)temp;\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &temp);\n"
	"\t\t\t\tfrag.ramp.channel = (VNMRampChannel)temp;\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.ramp.mapping);\n"
	"\t\t\t\tpos = buffer_pos;\n"
	"\t\t\t\tbuffer_pos += vnp_raw_unpack_uint8(&buf[buffer_pos], &frag.ramp.point_count);\n"
	"\t\t\t\tfor(i = 0; i < frag.ramp.point_count && buffer_pos + 8 * 4 <= buffer_length && i < 48; i++)\n"
	"\t\t\t\t{\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.ramp.ramp[i].pos);\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.ramp.ramp[i].red);\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.ramp.ramp[i].green);\n"
	"\t\t\t\t\tbuffer_pos += vnp_raw_unpack_real64(&buf[buffer_pos], &frag.ramp.ramp[i].blue);\n"
	"\t\t\t\t}if(i != frag.ramp.point_count)\n"
	"\t\t\t\t\tfrag.ramp.point_count = i;\n"
	"\t\t\t}\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_ANIMATION :\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.animation.label, 16, buffer_length - buffer_pos);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_ALTERNATIVE :\n"
	"\t\t\tif(buffer_pos + 4 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.alternative.alt_a);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.alternative.alt_b);\n"
	"\t\t\tbreak;\n"
	"\t\tcase VN_M_FT_OUTPUT :\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_string(&buf[buffer_pos], frag.output.label, 16, buffer_length - buffer_pos);\n"
	"\t\t\tif(buffer_pos + 4 > buffer_length)\n"
	"\t\t\t\treturn -1;\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.output.front);\n"
	"\t\t\tbuffer_pos += vnp_raw_unpack_uint16(&buf[buffer_pos], &frag.output.back);\n"
	"\t\t\tbreak;\n"
	"\t\t}\n"
	"\t\tif(func_m_fragment_create != NULL)\n"
	"\t\t\tfunc_m_fragment_create(v_fs_get_user_data(68), node_id, frag_id, type, &frag);\n"
	"\t\treturn buffer_pos;\n"
	"\t}\n");
	v_cg_alias(FALSE, "m_fragment_destroy", "if(type > VN_M_FT_OUTPUT)", 2, NULL);

	v_cg_end_cmd();
}

#endif
