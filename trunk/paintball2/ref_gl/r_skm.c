/*
Copyright(C) 2002-2003 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_skm.c: skeletal animation model format

#include "gl_local.h"

//static  mesh_t	skm_mesh;

static	vec3_t	skm_mins;
static	vec3_t	skm_maxs;
static	float	skm_radius;
static	vec3_t	default_lightdir = { 0.587785f, 0.425325f, 0.688191f }; // jitskm

/*
==============================================================================

SKM MODELS

==============================================================================
*/

static float	shadelight[3]; // jitskm
static vec3_t	inVertsArray[4096]; // jitskm
static vec3_t	inNormalsArray[4096]; // jitskm

static void SKM_TransformBoneposes (mskmodel_t *model, bonepose_t *boneposes,
									bonepose_t *sourceboneposes, vec3_t angle, vec3_t origin)
{
	register int	j;
	bonepose_t		temppose;
	register int	numbones = (int)model->numbones;
	register int	parent;
	quat_t			q_rot;
	quat_t			q_q2axis, q_temp, q_temp2, q_q2invaxis;
	vec3_t			q2axis = { 90.0f, 0.0f, 90.0f };
	vec3_t			q2invaxis = { -90.0f, -90.0f, 0.0f };

	// todo - optimize
	Quat_FromEulerAngle(q2axis, q_q2axis);
	Quat_FromEulerAngle(q2invaxis, q_q2invaxis);
	Quat_FromEulerAngle(angle, q_temp);
	Quat_Multiply(q_temp, q_q2axis, q_temp2);
	Quat_Multiply(q_q2invaxis, q_temp2, q_rot);

	for (j = 0; j < numbones; j++) 
	{
		parent = model->bones[j].parent;

		if (parent < 0)
		{
			//memcpy(&boneposes[j], &sourceboneposes[j], sizeof(bonepose_t));

			// todo - rotate around origin instead of parent bone
			// vectorsubtract base bone position from origin and rotate vector around axis or whatever.
			memcpy(&temppose, &sourceboneposes[j], sizeof(bonepose_t));
			Quat_ConcatTransforms(q_rot, origin, temppose.quat,
				temppose.origin, boneposes[j].quat, boneposes[j].origin);
		}
		else
		{
			memcpy(&temppose, &sourceboneposes[j], sizeof(bonepose_t));

			Quat_ConcatTransforms(boneposes[parent].quat,
				boneposes[parent].origin, temppose.quat,
				temppose.origin, boneposes[j].quat, boneposes[j].origin);
		}
	}
}

static bonepose_t curboneposescache[SKM_MAX_BONES];
static bonepose_t oldboneposescache[SKM_MAX_BONES];

/*
=================
Mod_LoadSkeletalPose
=================
*/
void Mod_LoadSkeletalPose (char *name, model_t *mod, void *buffer)
{
	int				i, j, k;
	mskmodel_t		*poutmodel;
	dskpheader_t	*pinmodel;
	dskpbone_t		*pinbone;
	mskbone_t		*poutbone;
	dskpframe_t		*pinframe;
	mskframe_t		*poutframe;
	dskpbonepose_t	*pinbonepose;
	bonepose_t		*poutbonepose;
	long			fileid = LittleLong(*(long*)buffer);

	if (fileid != SKMHEADER) // jitskm
		ri.Sys_Error(ERR_DROP, "uknown fileid for %s", name);

	pinmodel = (dskpheader_t *)buffer;
	poutmodel = mod->skmodel;

	if (LittleLong(pinmodel->type) != SKM_MODELTYPE)
		ri.Sys_Error(ERR_DROP, "%s has wrong type number(%i should be %i)",
				 name, LittleLong(pinmodel->type), SKM_MODELTYPE);

	if (LittleLong(pinmodel->filesize) > SKM_MAX_FILESIZE)
		ri.Sys_Error(ERR_DROP, "%s has has wrong filesize(%i should be less than %i)",
				 name, LittleLong(pinmodel->filesize), SKM_MAX_FILESIZE);

	if (LittleLong(pinmodel->num_bones) != poutmodel->numbones)
		ri.Sys_Error(ERR_DROP, "%s has has wrong number of bones(%i should be less than %i)",
				 name, LittleLong(pinmodel->num_bones), poutmodel->numbones);

	poutmodel->numframes = LittleLong(pinmodel->num_frames);

	if (poutmodel->numframes <= 0)
		ri.Sys_Error(ERR_DROP, "%s has no frames", name);
	else if(poutmodel->numframes > SKM_MAX_FRAMES)
		ri.Sys_Error(ERR_DROP, "%s has too many frames", name);

	pinbone = (dskpbone_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_bones));
	poutbone = poutmodel->bones = Mod_Malloc(mod, sizeof(mskbone_t) * poutmodel->numbones);

	for (i = 0; i < poutmodel->numbones; i++, pinbone++, poutbone++)
	{
		Q_strncpyz(poutbone->name, pinbone->name, SKM_MAX_NAME);
		poutbone->flags = LittleLong(pinbone->flags);
		poutbone->parent = LittleLong(pinbone->parent);
	}

	pinframe = (dskpframe_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_frames));
	poutframe = poutmodel->frames = Mod_Malloc(mod, sizeof(mskframe_t) * poutmodel->numframes);

	for (i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++)
	{
		pinbonepose = (dskpbonepose_t *)((byte *)pinmodel + LittleLong(pinframe->ofs_bonepositions));
		poutbonepose = poutframe->boneposes = Mod_Malloc(mod, sizeof(bonepose_t) * poutmodel->numbones);

		for (j = 0; j < poutmodel->numbones; j++, pinbonepose++, poutbonepose++)
		{
			for (k = 0; k < 4; k++)
				poutbonepose->quat[k] = LittleFloat(pinbonepose->quat[k]);

			for (k = 0; k < 3; k++)
				poutbonepose->origin[k] = LittleFloat(pinbonepose->origin[k]);
		}
	}
}

/*
=================
Mod_LoadSkeletalModel
=================
*/
void Mod_LoadSkeletalModel (model_t *mod, model_t *parent, void *buffer)
{
	int				i, j, k, l, m;
	dskmheader_t	*pinmodel;
	mskmodel_t		*poutmodel;
	dskmmesh_t		*pinmesh;
	mskmesh_t		*poutmesh;
	dskmvertex_t	*pinskmvert;
	mskvertex_t		*poutskmvert;
	dskmbonevert_t	*pinbonevert;
	mskbonevert_t	*poutbonevert;
	dskmcoord_t		*pinstcoord;
	vec2_t			*poutstcoord;
	unsigned int	*pintris, *pouttris;
	mskframe_t		*poutframe;
	unsigned int	*pinreferences, *poutreferences;
/*	char			skinname[MAX_QPATH];
	char			*s;*/

	pinmodel = (dskmheader_t *)buffer;

	if (LittleLong(pinmodel->type) != SKM_MODELTYPE)
		ri.Sys_Error(ERR_DROP, "%s has wrong type number(%i should be %i)",
				 mod->name, LittleLong(pinmodel->type), SKM_MODELTYPE);

	if (LittleLong(pinmodel->filesize) > SKM_MAX_FILESIZE)
		ri.Sys_Error(ERR_DROP, "%s has has wrong filesize(%i should be less than %i)",
				 mod->name, LittleLong(pinmodel->filesize), SKM_MAX_FILESIZE);

	poutmodel = mod->skmodel = Mod_Malloc(mod, sizeof(mskmodel_t));
	poutmodel->nummeshes = LittleLong(pinmodel->num_meshes);

	if (poutmodel->nummeshes <= 0)
		ri.Sys_Error(ERR_DROP, "%s has no meshes", mod->name);
	else if(poutmodel->nummeshes > SKM_MAX_MESHES)
		ri.Sys_Error(ERR_DROP, "%s has too many meshes", mod->name);

	poutmodel->numbones = LittleLong(pinmodel->num_bones);

	if (poutmodel->numbones <= 0)
		ri.Sys_Error(ERR_DROP, "%s has no bones", mod->name);
	else if(poutmodel->numbones > SKM_MAX_BONES)
		ri.Sys_Error(ERR_DROP, "%s has too many bones", mod->name);

	pinmesh = (dskmmesh_t *)((byte *)pinmodel + LittleLong(pinmodel->ofs_meshes));
	poutmesh = poutmodel->meshes = Mod_Malloc(mod, sizeof(mskmesh_t) * poutmodel->nummeshes);

	for (i = 0; i < poutmodel->nummeshes; i++, pinmesh++, poutmesh++)
	{
		poutmesh->numverts = LittleLong(pinmesh->num_verts);

		if (poutmesh->numverts <= 0)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has no vertexes", i, mod->name);
		else if(poutmesh->numverts > SKM_MAX_VERTS)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has too many vertexes", i, mod->name);

		poutmesh->numtris = LittleLong(pinmesh->num_tris);

		if (poutmesh->numtris <= 0)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has no indices", i, mod->name);
		else if(poutmesh->numtris > SKM_MAX_TRIS)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has too many indices", i, mod->name);

		poutmesh->numreferences = LittleLong(pinmesh->num_references);

		if (poutmesh->numreferences <= 0)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has no bone references", i, mod->name);
		else if(poutmesh->numreferences > SKM_MAX_BONES)
			ri.Sys_Error(ERR_DROP, "mesh %i in model %s has too many bone references", i, mod->name);

		Q_strncpyz(poutmesh->name, pinmesh->meshname, sizeof(poutmesh->name));
		Mod_StripLODSuffix(poutmesh->name);

		// === jitskm -- get the skin texture
		strcpy(poutmesh->shadername, pinmesh->shadername);
//		poutmesh->skin.shader = R_RegisterSkin(pinmesh->shadername);
/*		strcpy(skinname, mod->name);
		s = strrchr(skinname, '/');

		if (!s)
			s = skinname;
		else
			s++;

		strcpy(s, pinmesh->shadername);
		strcat(s, ".pcx");
		poutmesh->skin.image = R_RegisterSkin(skinname);*/
		// jit ===

		pinskmvert = (dskmvertex_t *)((byte *)pinmodel + LittleLong(pinmesh->ofs_verts));
		poutskmvert = poutmesh->vertexes = Mod_Malloc(mod, sizeof(mskvertex_t) * poutmesh->numverts);

		for (j = 0; j < poutmesh->numverts; j++, poutskmvert++)
		{
			poutskmvert->numbones = LittleLong(pinskmvert->numbones);

			pinbonevert = (dskmbonevert_t *)((byte *)pinskmvert + sizeof(poutskmvert->numbones));
			poutbonevert = poutskmvert->verts = Mod_Malloc(mod, sizeof(mskbonevert_t) * poutskmvert->numbones);

			for (l = 0; l < poutskmvert->numbones; l++, pinbonevert++, poutbonevert++)
			{
				for (k = 0; k < 3; k++)
				{
					poutbonevert->origin[k] = LittleFloat(pinbonevert->origin[k]);
					poutbonevert->normal[k] = LittleFloat(pinbonevert->normal[k]);
				}

				poutbonevert->influence = LittleFloat(pinbonevert->influence);
				poutbonevert->bonenum = LittleLong(pinbonevert->bonenum);
			}

			pinskmvert = (dskmvertex_t *)((byte *)pinbonevert);
		}

		pinstcoord = (dskmcoord_t *)((byte *)pinmodel + LittleLong(pinmesh->ofs_texcoords));
		poutstcoord = poutmesh->stcoords = Mod_Malloc(mod, poutmesh->numverts * sizeof(vec2_t));

		for (j = 0; j < poutmesh->numverts; j++, pinstcoord++)
		{
			poutstcoord[j][0] = LittleFloat(pinstcoord->st[0]);
			poutstcoord[j][1] = LittleFloat(pinstcoord->st[1]);
		}

		pintris = (unsigned *)((byte *)pinmodel + LittleLong(pinmesh->ofs_indices));
		pouttris = poutmesh->indexes = Mod_Malloc(mod, sizeof(unsigned) * poutmesh->numtris * 3);

		for (j = 0; j < poutmesh->numtris; j++, pintris += 3, pouttris += 3)
		{
			pouttris[0] = (unsigned)LittleLong(pintris[0]);
			pouttris[1] = (unsigned)LittleLong(pintris[1]);
			pouttris[2] = (unsigned)LittleLong(pintris[2]);
		}

		pinreferences = (unsigned *)((byte *)pinmodel + LittleLong(pinmesh->ofs_references));
		poutreferences = poutmesh->references = Mod_Malloc(mod, sizeof(unsigned int) * poutmesh->numreferences);

		for (j = 0; j < poutmesh->numreferences; j++, pinreferences++, poutreferences++)
			*poutreferences = LittleLong(*pinreferences);

	//
	// build triangle neighbors
	//
#if SHADOW_VOLUMES
		poutmesh->trneighbors = Mod_Malloc(mod, sizeof(int) * poutmesh->numtris * 3);
		R_BuildTriangleNeighbors(poutmesh->trneighbors, poutmesh->indexes, poutmesh->numtris);
#endif
	}

	// if we have a parent model then we are a LOD file and should use parent model's pose data
	if (parent)
	{
		if (!parent->skmodel)
			ri.Sys_Error(ERR_DROP, "%s is not a LOD model for %s",
					 mod->name, parent->name);

		if (poutmodel->numbones != parent->skmodel->numbones)
			ri.Sys_Error(ERR_DROP, "%s has has wrong number of bones(%i should be less than %i)",
					 mod->name, poutmodel->numbones, parent->skmodel->numbones);

		poutmodel->bones = parent->skmodel->bones;
		poutmodel->frames = parent->skmodel->frames;
		poutmodel->numframes = parent->skmodel->numframes;
	}
	else
	{		// load a config file
		byte *buf;
		char temp[MAX_QPATH];
		char poseName[MAX_QPATH], configName[MAX_QPATH];

		COM_StripExtension(mod->name, temp);
		Q_snprintfz(configName, sizeof(configName), "%s.cfg", temp);

		memset(poseName, 0, sizeof(poseName));

		ri.FS_LoadFile(configName, (void **)&buf);

		if (!buf)
		{
			Q_snprintfz(poseName, sizeof(poseName), "%s.skp", temp);
		}
		else
		{
			char *ptr, *token;

			ptr = (char *)buf;

			while(ptr)
			{
				token = COM_ParseExt(&ptr, true);

				if (!token)
					break;

				if (!Q_strcasecmp(token, "import"))
				{
					token = COM_ParseExt(&ptr, false);
					COM_StripExtension(token, temp);
					Q_snprintfz(poseName, sizeof(poseName), "%s.skp", temp);
					break;
				}
			}

			ri.FS_FreeFile(buf);
		}

		ri.FS_LoadFile(poseName,(void **)&buf);

		if (!buf)
			ri.Sys_Error(ERR_DROP, "Could not find pose file for %s", mod->name);

		Mod_LoadSkeletalPose(poseName, mod, buf);
	}

	// recalculate frame and model bounds according to pose data
	mod->radius = 0;
	ClearBounds(mod->mins, mod->maxs);
	poutframe = poutmodel->frames;

	for (i = 0; i < poutmodel->numframes; i++, poutframe++)
	{
		vec3_t		v, vtemp;
		bonepose_t	*poutbonepose;

		ClearBounds(poutframe->mins, poutframe->maxs);
		poutmesh = poutmodel->meshes;
		SKM_TransformBoneposes(poutmodel, curboneposescache, poutframe->boneposes, vec3_origin, vec3_origin); // jit

		for (m = 0; m < poutmodel->nummeshes; m++, poutmesh++)
		{
			poutskmvert = poutmesh->vertexes;

			for (j = 0; j < poutmesh->numverts; j++, poutskmvert++)
			{
				VectorClear(v);
				poutbonevert = poutskmvert->verts;

				for (l = 0; l < poutskmvert->numbones; l++, poutbonevert++)
				{
					//poutbonepose = poutframe->boneposes + poutbonevert->bonenum;
					poutbonepose = curboneposescache + poutbonevert->bonenum; // jit
					Quat_TransformVector(poutbonepose->quat, poutbonevert->origin, vtemp);
					VectorMA(vtemp, poutbonevert->influence, poutbonepose->origin, vtemp);
					v[0] += vtemp[0]; v[1] += vtemp[1]; v[2] += vtemp[2];
				}

				AddPointToBounds(v, poutframe->mins, poutframe->maxs);
			}
		}

		// enlarge the bounding box a bit so it can be rendered
		// with external boneposes
		/* jit - testing - for (j = 0; j < 3; j++)
		{
			poutframe->mins[j] *= 1.5;
			poutframe->maxs[j] *= 1.5;
		}*/

		poutframe->radius = RadiusFromBounds(poutframe->mins, poutframe->maxs);
		VectorAdd(poutframe->mins, mod->mins, mod->maxs);
		VectorAdd(poutframe->maxs, mod->mins, mod->maxs);
		mod->radius = max(mod->radius, poutframe->radius);
	}

	mod->type = mod_skeletal;
}

/*
================
R_SkeletalGetNumBones
================
*/
int R_SkeletalGetNumBones (model_t *mod, int *numFrames)
{
	if (!mod || mod->type != mod_skeletal)
		return 0;

	if (numFrames)
		*numFrames = mod->skmodel->numframes;

	return mod->skmodel->numbones;
}

/*
================
R_SkeletalGetBoneInfo
================
*/
int R_SkeletalGetBoneInfo (model_t *mod, int bonenum, char *name, int size, int *flags)
{
	mskbone_t *bone;

	if (!mod || mod->type != mod_skeletal)
		return 0;

	if (bonenum < 0 || bonenum >= mod->skmodel->numbones)
		ri.Sys_Error(ERR_DROP, "R_SkeletalGetBone: bad bone number");

	bone = &mod->skmodel->bones[bonenum];

	if (name && size)
		Q_strncpyz(name, bone->name, size);

	if (flags)
		*flags = bone->flags;

	return bone->parent;
}

/*
================
R_SkeletalGetBonePose
================
*/
void R_SkeletalGetBonePose(model_t *mod, int bonenum, int frame, bonepose_t *bonepose)
{
	if (!mod || mod->type != mod_skeletal)
		return;

	if (bonenum < 0 || bonenum >= mod->skmodel->numbones)
		ri.Sys_Error(ERR_DROP, "R_SkeletalGetBonePose: bad bone number");

	if (frame < 0 || frame >= mod->skmodel->numframes)
		ri.Sys_Error(ERR_DROP, "R_SkeletalGetBonePose: bad frame number");

	if (bonepose)
		*bonepose = mod->skmodel->frames[frame].boneposes[bonenum];
}

/*
================
R_SkeletalModelLODForDistance
================
*/
static model_t *R_SkeletalModelLODForDistance (entity_t *e)
{
	return e->model;
#if 0 // jitodo - lod support
	int lod;
	float dist;

	if (!e->model->numlods ||(e->flags & RF_FORCENOLOD))
		return e->model;

	dist = Distance(e->origin, r_origin);
	dist *= tan(r_refdef.fov_x *(M_PI/180) * 0.5f);

	lod = (int)(dist / e->model->radius);

	if (r_lodscale->integer)
		lod /= r_lodscale->integer;

	lod += r_lodbias->integer;

	if (lod < 1)
		return e->model;

	return e->model->lods[min(lod, e->model->numlods)-1];
#endif
}

/*
================
R_SkeletalModelBBox
================
*/
void R_SkeletalModelBBox (entity_t *e, model_t *mod)
{
	int			i;
	mskframe_t	*pframe, *poldframe;
	float		*thismins, *oldmins, *thismaxs, *oldmaxs;
	mskmodel_t	*skmodel = mod->skmodel;

	if ((e->frame >= skmodel->numframes) ||(e->frame < 0))
	{
		ri.Con_Printf(PRINT_DEVELOPER, "R_SkeletalModelBBox %s: no such frame %d\n", mod->name, e->frame);
		e->frame = 0;
	}

	if ((e->oldframe >= skmodel->numframes) ||(e->oldframe < 0))
	{
		ri.Con_Printf(PRINT_DEVELOPER, "R_SkeletalModelBBox %s: no such oldframe %d\n", mod->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = skmodel->frames + e->frame;
	poldframe = skmodel->frames + e->oldframe;

	// compute axially aligned mins and maxs
	if (pframe == poldframe)
	{
		VectorCopy(pframe->mins, skm_mins);
		VectorCopy(pframe->maxs, skm_maxs);
		skm_radius = pframe->radius;
	}
	else
	{
		thismins = pframe->mins;
		thismaxs = pframe->maxs;

		oldmins = poldframe->mins;
		oldmaxs = poldframe->maxs;

		for (i = 0; i < 3; i++)
		{
			skm_mins[i] = min(thismins[i], oldmins[i]);
			skm_maxs[i] = max(thismaxs[i], oldmaxs[i]);
		}

		skm_radius = RadiusFromBounds(thismins, thismaxs);
	}

	if (e->scale && e->scale != 1.0f) // jit
	{
		VectorScale(skm_mins, e->scale, skm_mins);
		VectorScale(skm_maxs, e->scale, skm_maxs);
		skm_radius *= e->scale;
	}
}

/*
================
R_CullSkeletalModel
================
*/

vec3_t axis_origin[3] = { { 1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 } };

qboolean R_CullSkeletalModel (entity_t *e)
{
#if 1
	int i;
	vec3_t tmp, bbox[8], vectors[3];

	if (e->flags & RF_WEAPONMODEL)
		return false;
/*	if (e->flags & RF_VIEWERMODEL)
		return !(r_mirrorview || r_portalview);

	if (r_spherecull->integer) {
		if (R_CullSphere(e->origin, skm_radius, 15))
			return true;
	}
	else*/
	{
		AngleVectors(e->angles, vectors[0], vectors[1], vectors[2]);
		VectorSubtract(vec3_origin, vectors[1], vectors[1]); // Angle vectors returns "right" instead of "left"

		for (i = 0; i < 8; i++)
		{
			tmp[0] = ((i & 1) ? skm_mins[0] : skm_maxs[0]);
			tmp[1] = ((i & 2) ? skm_mins[1] : skm_maxs[1]);
			tmp[2] = ((i & 4) ? skm_mins[2] : skm_maxs[2]);

			bbox[i][0] = vectors[0][0] * tmp[0] + vectors[1][0] * tmp[1] + vectors[2][0] * tmp[2] + e->origin[0];
			bbox[i][1] = vectors[0][1] * tmp[0] + vectors[1][1] * tmp[1] + vectors[2][1] * tmp[2] + e->origin[1];
			bbox[i][2] = vectors[0][2] * tmp[0] + vectors[1][2] * tmp[1] + vectors[2][2] * tmp[2] + e->origin[2];
		}

		{
			int p, f, aggregatemask = ~0;

			for (p = 0; p < 8; p++)
			{
				int mask = 0;

				for (f = 0; f < 4; f++)
				{
					if (DotProduct(frustum[f].normal, bbox[p]) < frustum[f].dist)
						mask |= (1 << f);
				}

				aggregatemask &= mask;
			}

			if (aggregatemask)
				return true;

//#define DRAW_BBOX
#ifdef DRAW_BBOX
			qglDisable(GL_TEXTURE_2D);
			qglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			qglBegin(GL_LINES);
			qglVertex3fv(bbox[0]);
			qglVertex3fv(bbox[1]);
			qglVertex3fv(bbox[0]);
			qglVertex3fv(bbox[2]);
			qglVertex3fv(bbox[2]);
			qglVertex3fv(bbox[3]);
			qglVertex3fv(bbox[3]);
			qglVertex3fv(bbox[1]);
			qglVertex3fv(bbox[0]);
			qglVertex3fv(bbox[4]);
			qglVertex3fv(bbox[4]);
			qglVertex3fv(bbox[5]);
			qglVertex3fv(bbox[4]);
			qglVertex3fv(bbox[6]);
			qglVertex3fv(bbox[5]);
			qglVertex3fv(bbox[7]);
			qglVertex3fv(bbox[6]);
			qglVertex3fv(bbox[7]);
			qglVertex3fv(bbox[7]);
			qglVertex3fv(bbox[3]);
			qglVertex3fv(bbox[6]);
			qglVertex3fv(bbox[2]);
			qglVertex3fv(bbox[5]);
			qglVertex3fv(bbox[1]);

			qglColor3f(1.0f, 0.0f, 0.0f);
			qglVertex3fv(e->origin);
			VectorCopy(vectors[0], tmp);
			VectorScale(tmp, 32, tmp);
			VectorAdd(e->origin, tmp, tmp);
			qglVertex3fv(tmp);

			qglColor3f(0.0f, 1.0f, 0.0f);
			qglVertex3fv(e->origin);
			VectorCopy(vectors[1], tmp);
			VectorScale(tmp, 32, tmp);
			VectorAdd(e->origin, tmp, tmp);
			qglVertex3fv(tmp);

			qglColor3f(0.0f, 0.0f, 1.0f);
			qglVertex3fv(e->origin);
			VectorCopy(vectors[2], tmp);
			VectorScale(tmp, 32, tmp);
			VectorAdd(e->origin, tmp, tmp);
			qglVertex3fv(tmp);
			qglEnd();
			qglEnable(GL_TEXTURE_2D);
#endif
			return false;
		}
	}
/*
	if (R_VisCullSphere(e->origin, skm_radius))
		return true;

	if ((r_mirrorview || r_portalview) && !r_nocull->integer)
		if (PlaneDiff(e->origin, &r_clipplane) < -skm_radius)
			return true;

	return false;*/
#endif
}

/*
================
R_DrawBonesFrameLerp
================
*/

//void R_DrawBonesFrameLerp (const meshbuffer_t *mb, model_t *mod, float backlerp, qboolean shadow)
void R_DrawBonesFrameLerp (entity_t *e, model_t *mod, int meshnum, float backlerp)
{
	int				i/*, meshnum*/;
	int				j, k, l;
//	int				features;
	quat_t			quaternion;
	float			frontlerp = 1.0 - backlerp;
	//vec3_t			move/*, delta*/;
	mskmesh_t		*mesh;
	bonepose_t		*bonepose, *oldbonepose, *bp, *oldbp;
	mskvertex_t		*skmverts;
	mskbonevert_t	*boneverts;
	int				numtris, numverts; // jit
	float			light; // jit
	int				*indexes; // jit
//	entity_t		*e = mb->entity;
	mskmodel_t		*skmodel = mod->skmodel;
	struct { vec3_t axis[3], origin; } skmbonepose[SKM_MAX_BONES], *pose, *out;

//	if (!shadow &&(e->flags & RF_VIEWERMODEL) && !r_mirrorview && !r_portalview)
//		return;

//	meshnum = -(mb->infokey + 1);
	if (meshnum < 0 || meshnum >= skmodel->nummeshes)
		return;

	mesh = skmodel->meshes + meshnum;

#if SHADOW_VOLUMES
	if (shadow && !mesh->trneighbors)
		return;
#endif

	// move should be the delta back to the previous frame * backlerp
/*	VectorSubtract(e->oldorigin, e->origin, delta);
	Matrix_TransformVector(e->axis, delta, move);
	VectorScale(move, e->scale * backlerp, move);*/
	//VectorCopy(e->origin, move); // jitest

	// todo - check if e->frame > max frames

/*	if (e->boneposes)
		bp = e->boneposes;
	else*/
		bp = skmodel->frames[e->frame].boneposes;

	SKM_TransformBoneposes(skmodel, curboneposescache, bp, e->angles, e->origin); // jit

/*	if (e->oldboneposes)
		oldbp = e->oldboneposes;
	else*/
		oldbp = skmodel->frames[e->oldframe].boneposes;

	SKM_TransformBoneposes(skmodel, oldboneposescache, oldbp, e->angles, e->oldorigin); // jit

	for (i = 0; i < mesh->numreferences; i++)
	{
		out = skmbonepose + mesh->references[i];
		//bonepose = bp + mesh->references[i];
		bonepose = curboneposescache + mesh->references[i]; // jit
		//oldbonepose = oldbp + mesh->references[i];
		oldbonepose = oldboneposescache + mesh->references[i]; // jit

		// interpolate quaternions and origins
		Quat_Lerp(oldbonepose->quat, bonepose->quat, frontlerp, quaternion);
		Quat_Matrix(quaternion, out->axis);
/*		out->axis[0][0] *= e->scale; out->axis[0][1] *= e->scale; out->axis[0][2] *= e->scale;
		out->axis[1][0] *= e->scale; out->axis[1][1] *= e->scale; out->axis[1][2] *= e->scale;
		out->axis[2][0] *= e->scale; out->axis[2][1] *= e->scale; out->axis[2][2] *= e->scale;*/
		out->origin[0] = oldbonepose->origin[0] + (bonepose->origin[0] - oldbonepose->origin[0]) * frontlerp;
		out->origin[1] = oldbonepose->origin[1] + (bonepose->origin[1] - oldbonepose->origin[1]) * frontlerp;
		out->origin[2] = oldbonepose->origin[2] + (bonepose->origin[2] - oldbonepose->origin[2]) * frontlerp;
	}

	// --- quake2 md2 style drawing code -- jitskm, just to get it working.
	if (e->flags & (RF_SHELL_HALF_DAM|RF_SHELL_GREEN|RF_SHELL_RED|RF_SHELL_BLUE|RF_SHELL_DOUBLE))
	{
		VectorClear(shadelight);

		if (e->flags & RF_SHELL_HALF_DAM)
		{
				shadelight[0] = 0.56;
				shadelight[1] = 0.59;
				shadelight[2] = 0.45;
		}

		if (e->flags & RF_SHELL_DOUBLE)
		{
			shadelight[0] = 0.9;
			shadelight[1] = 0.7;
		}

		if (e->flags & RF_SHELL_RED)
			shadelight[0] = 1.0;

		if (e->flags & RF_SHELL_GREEN)
			shadelight[1] = 1.0;

		if (e->flags & RF_SHELL_BLUE)
			shadelight[2] = 1.0;
	}
	else if (e->flags & RF_FULLBRIGHT)
	{
		for (i=0; i<3; i++)
			shadelight[i] = 1.0;
	}
	else
	{
		R_LightPoint(e->origin, shadelight);
		
		if (gl_monolightmap->string[0] != '0')
		{
			float s = shadelight[0];

			if (s < shadelight[1])
				s = shadelight[1];

			if (s < shadelight[2])
				s = shadelight[2];

			shadelight[0] = s;
			shadelight[1] = s;
			shadelight[2] = s;
		}
	}

	if (e->flags & RF_MINLIGHT)
	{
		for (i=0; i<3; i++)
			if (shadelight[i] > 0.1)
				break;

		if (i == 3)
		{
			shadelight[0] = 0.1;
			shadelight[1] = 0.1;
			shadelight[2] = 0.1;
		}
	}

	if (e->flags & RF_GLOW)
	{	// bonus items will pulse with time
		float	scale;
		float	min;

		scale = 0.1 * sin(r_newrefdef.time*7);

		for (i=0; i<3; i++)
		{
			min = shadelight[i] * 0.8;
			shadelight[i] += scale;

			if (shadelight[i] < min)
				shadelight[i] = min;
		}
	}

	if (r_newrefdef.rdflags & RDF_IRGOGGLES && e->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}

	numtris = mesh->numtris;
	numverts = mesh->numverts;
	c_alias_polys += numtris;
	boneverts = mesh->vertexes->verts;
	indexes = mesh->indexes;

	// translate vertexes.
	for (j = 0, skmverts = mesh->vertexes; j < numverts; j++, skmverts++)
	{
		//VectorCopy(mesh->vertexes[j].verts->origin, inVertsArray[j]);
		//VectorClear(inVertsArray[j]);
		//VectorAdd(move, inVertsArray[j], inVertsArray[j]);
		
		//VectorCopy(move, inVertsArray[j]);
		VectorClear(inVertsArray[j]);
		VectorClear(inNormalsArray[j]);

		for (l = 0, boneverts = skmverts->verts; l < skmverts->numbones; l++, boneverts++)
		{
			pose = skmbonepose + boneverts->bonenum;

			for (k = 0; k < 3; k++)
			{
				inVertsArray[j][k] +=
					boneverts->origin[0] * pose->axis[k][0] +
					boneverts->origin[1] * pose->axis[k][1] +
					boneverts->origin[2] * pose->axis[k][2] +
					boneverts->influence * pose->origin[k];
				inNormalsArray[j][k] +=
					boneverts->normal[0] * pose->axis[k][0] +
					boneverts->normal[1] * pose->axis[k][1] +
					boneverts->normal[2] * pose->axis[k][2];
			}
		}
	}

#ifdef GLSTATE_DISABLE_BLEND
	GLSTATE_DISABLE_BLEND // for beefquake based renderers
#else
	qglDisable(GL_BLEND);
#endif
	//GL_TexEnv(GL_MODULATE); temp -- put this back for tutorial
	// todo: 
	//qglDisable(GL_TEXTURE_2D);
	GL_TexEnv(GL_COMBINE_EXT); // jitbright
	qglShadeModel(GL_SMOOTH);
	qglColor3fv(shadelight);

	if (e->skin)
	{
		// multiplayer skin
		GL_Bind(e->skin->texnum); // todo -- skin file loading
	}
	else
	{
		register image_t *skin = mesh->skins[e->skinnum];

		if (!skin)
			GL_Bind(r_notexture->texnum);
		else
			GL_Bind(skin->texnum);
	}

	// Good 'ol immediate mode, just to get it working...
	qglBegin(GL_TRIANGLES);

	for (j = 0; j < numtris*3; j++)
	{
		// try to mimick q2's lighting as closely as possible
		light = (DotProduct(inNormalsArray[indexes[j]], default_lightdir) + 1.0f) / 2.0f * 1.4f + 0.6f; // todo - optimize (should only be doing this once per vertex).

		qglColor3f(light * shadelight[0], light * shadelight[1], light * shadelight[2]);
		qglTexCoord2fv(mesh->stcoords[indexes[j]]);
		qglVertex3fv(inVertsArray[indexes[j]]);
		
	}

	qglEnd();
//#define DRAW_NORMALS
#ifdef DRAW_NORMALS
	qglColor3f(1, 0, 0);
	qglBegin(GL_LINES);

	for (j = 0; j < numtris*3; j++)
	{
		vec3_t tempvec1, tempvec2;

		qglVertex3fv(inVertsArray[indexes[j]]);
		VectorScale(inNormalsArray[indexes[j]], 2.5f, tempvec1);
		VectorAdd(inVertsArray[indexes[j]], tempvec1, tempvec2);
		qglVertex3fv(tempvec2);
	}
		
	qglEnd();
#endif

//#define DRAW_BONES
#ifdef DRAW_BONES
	/// testing - draw bones
	qglColor3f(1, 1, 0);
	qglDisable(GL_DEPTH_TEST);
	qglBegin(GL_LINES);
	{
		int numbones = (int)skmodel->numbones;

		//bp = skmodel->frames[e->frame].boneposes;
		
		for (j = 0; j < numbones; j++)
		{
			if (skmodel->bones[j].parent >= 0) 
			{
				qglVertex3f(
					curboneposescache[skmodel->bones[j].parent].origin[0]/* + move[0]*/,
					curboneposescache[skmodel->bones[j].parent].origin[1]/* + move[1]*/,
					curboneposescache[skmodel->bones[j].parent].origin[2]/* + move[2]*/);
				qglVertex3f(
					curboneposescache[j].origin[0]/* + move[0]*/,
					curboneposescache[j].origin[1]/* + move[1]*/,
					curboneposescache[j].origin[2]/* + move[2]*/);
			}
		}
	}
	qglEnd();
	qglEnable(GL_DEPTH_TEST);
	//todo;
#endif
/*	// --- qfusion's render code ---

	features = MF_NONBATCHED | mb->shader->features;

	if (r_shownormals->integer && !shadow)
		features |= MF_NORMALS;

	if (mb->shader->flags & SHADER_AUTOSPRITE)
		features |= MF_NOCULL;

	if (shadow)
		features |= MF_DEFORMVS;

	if ((features & MF_NORMALS) && !shadow)
	{
		for (j = 0, skmverts = mesh->vertexes; j < mesh->numverts; j++, skmverts++)
		{
			VectorCopy(move, inVertsArray[j]);
			VectorClear(inNormalsArray[j]);

			for (l = 0, boneverts = skmverts->verts; l < skmverts->numbones; l++, boneverts++)
			{
				pose = skmbonepose + boneverts->bonenum;

				for (k = 0; k < 3; k++)
				{
					inVertsArray[j][k] +=
						boneverts->origin[0] * pose->axis[k][0] +
						boneverts->origin[1] * pose->axis[k][1] +
						boneverts->origin[2] * pose->axis[k][2] +
						boneverts->influence * pose->origin[k];
					inNormalsArray[j][k] +=
						boneverts->normal[0] * pose->axis[k][0] +
						boneverts->normal[1] * pose->axis[k][1] +
						boneverts->normal[2] * pose->axis[k][2];
				}
			}

			VectorNormalizeFast(inNormalsArray[j]);
		}
	}
	else
	{
		for (j = 0, skmverts = mesh->vertexes; j < mesh->numverts; j++, skmverts++)
		{
			VectorCopy(move, inVertsArray[j]);

			for (l = 0, boneverts = skmverts->verts; l < skmverts->numbones; l++, boneverts++)
			{
				pose = skmbonepose + boneverts->bonenum;

				for (k = 0; k < 3; k++)
				{
					inVertsArray[j][k] += boneverts->origin[0] * pose->axis[k][0] +
						boneverts->origin[1] * pose->axis[k][1] +
						boneverts->origin[2] * pose->axis[k][2] +
						boneverts->influence * pose->origin[k];
				}
			}
		}
	}

	if ((features & MF_STVECTORS) && !shadow)
		R_BuildTangentVectors(mesh->numverts, inVertsArray, mesh->stcoords, mesh->numtris, mesh->indexes, inSVectorsArray, inTVectorsArray);

	skm_mesh.indexes = mesh->indexes;
	skm_mesh.numIndexes = mesh->numtris * 3;
	skm_mesh.numVertexes = mesh->numverts;
	skm_mesh.xyzArray = inVertsArray;
	skm_mesh.stArray = mesh->stcoords;
	skm_mesh.normalsArray = inNormalsArray;
	skm_mesh.sVectorsArray = inSVectorsArray;
	skm_mesh.tVectorsArray = inTVectorsArray;
#if SHADOW_VOLUMES
	skm_mesh.trneighbors = mesh->trneighbors;
	skm_mesh.trnormals = NULL;
#endif

	R_RotateForEntity(e);

	R_PushMesh(&skm_mesh, features);
	R_RenderMeshBuffer(mb, shadow);

	if (shadow)
	{
		if (r_shadows->integer == 1)
		{
			R_Draw_SimpleShadow(e);
		}
		else
		{
#if SHADOW_VOLUMES
			R_SkeletalModelBBox(e, mod);
			R_DrawShadowVolumes(&skm_mesh, e->lightingOrigin, skm_mins, skm_maxs, skm_radius);
#endif
		}
	}
	*/
}

/*
=================
R_DrawSkeletalModel
=================
*/
//void R_DrawSkeletalModel (const meshbuffer_t *mb, qboolean shadow)
void R_DrawSkeletalModel (entity_t *e)
{
//	entity_t *e = mb->entity;
	int i, nummeshes;
	model_t *skm;

	skm = R_SkeletalModelLODForDistance(e); // jit

	nummeshes = e->model->skmodel->nummeshes;

	// hack the depth range to prevent view model from poking into walls
	if (e->flags & RF_WEAPONMODEL)
	{
		qglDepthRange(gldepthmin, gldepthmin + 0.3 *(gldepthmax - gldepthmin));
	}
	else // jit
	{
		R_SkeletalModelBBox(e, skm);

		if (R_CullSkeletalModel(e))
			return;
	}

	// backface culling for left-handed weapons
//	if (e->flags & RF_CULLHACK)
//		qglFrontFace(GL_CW);

	if (!r_lerpmodels->value)
		e->backlerp = 0;

	//R_DrawBonesFrameLerp(mb, R_SkeletalModelLODForDistance(e), e->backlerp, shadow);

	for (i = 0; i < nummeshes; i++) // jit - just to get it functional -- could probably be optimized
		R_DrawBonesFrameLerp(e, skm, i, e->backlerp);

	if (e->flags & RF_WEAPONMODEL)
		qglDepthRange(gldepthmin, gldepthmax);

	//if (e->flags & RF_CULLHACK)
	//	qglFrontFace(GL_CCW);
}

/*
=================
R_AddSkeletalModelToList
=================
*
void R_AddSkeletalModelToList (entity_t *e)
{
	int				i;
	mfog_t			*fog;
	model_t			*mod;
	shader_t		*shader;
	mskmesh_t		*mesh;
	mskmodel_t		*skmodel;

	mod = R_SkeletalModelLODForDistance(e);
	if (!(skmodel = mod->skmodel))
		return;

	R_SkeletalModelBBox(e, mod);
	if (!r_shadows->integer && R_CullSkeletalModel(e))
		return;

	mesh = skmodel->meshes;
	fog = R_FogForSphere(e->origin, skm_radius);

	for (i = 0; i < skmodel->nummeshes; i++, mesh++) {
		if (e->customSkin) {
			shader = R_FindShaderForSkinFile(e->customSkin, mesh->name);
			if (shader)
				R_AddMeshToList(MB_MODEL, fog, shader, -(i+1));
		} else if(e->customShader) {
			R_AddMeshToList(MB_MODEL, fog, e->customShader, -(i+1));
		} else {
			R_AddMeshToList(MB_MODEL, fog, mesh->skin.shader, -(i+1));
		}
	}
}
*/