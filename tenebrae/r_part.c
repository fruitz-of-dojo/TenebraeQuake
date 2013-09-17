/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include "r_local.h"
#include "te_scripts.h"

#define MAX_PARTICLES			2048	// default max # of particles at one
										//  time
#define MAX_EMITTERS			256		// maximum number of particle emitters
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's
										//  on the command line

typedef struct particleemitter_s {
	ParticleEffect_t *effect; //what to spawn?
	float die;				  //when to die?
	float tick;				  //time between spawns
	float nexttick;			  //time of next tick
	int	count;				  //how many to spawn on tick
	vec3_t	origin;			  //where to spawn
	vec3_t	vel;			  //velocity to base on
	struct particleemitter_s* next;
} ParticleEmitter_t;

int		ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
int		ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
int		ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t	*active_particles, *free_particles;

particle_t	*particles;
int			r_numparticles;

ParticleEmitter_t *emitters, *active_emitters, *free_emitters;
ParticleEffect_t *particleEffects;

vec3_t			r_pright, r_pup, r_ppn;

// <AWE> missing prototypes
extern qboolean SV_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1, vec3_t p2, trace_t *trace);


ParticleEffect_t *ParticleEffectDefinedForName(const char *name);
ParticleEffect_t *ParticleEffectForName(const char *name);

//fill an effect with default values
void DefaultEffect(ParticleEffect_t *eff) {
	
	int i;

	strcpy(eff->name,"noname");
	for (i=0; i<3; i++) {
		eff->emmiterParams1[i] = -16;
		eff->emmiterParams2[i] = 16;
		eff->endcolormax[i] = 1;
		eff->endcolormin[i] = 1;
		eff->startcolormax[i] = 1;
		eff->startcolormin[i] = 1;
		eff->gravity[i] = 0;
		eff->velocitymax[i] = 0;
		eff->velocitymin[i] = 0;
		eff->drag[i] = 1;
	}

	eff->emmiterType = emt_box; //currently only box is supported
	eff->lifemin = 1;
	eff->lifemax = 1;
	eff->rotmin = 0;
	eff->rotmax = 0;
	eff->growmin = 0;
	eff->growmax = 0;
	eff->sizemin = 10;
	eff->sizemax = 10;
	eff->srcblend = GL_ONE;
	eff->dstblend = GL_ONE;
	eff->numbounces = 1;
	eff->texture = 0;
	eff->align = align_view;
	eff->next = 0;
	eff->velscale = 1/64;
	eff->spawn = NULL;
}
/*
=====================
R_InitParticleEffects

  Parse the particle effects out of the script file
=====================
*/
void R_AddEffectsScript(const char *filename) {

//	FILE *fin;
    int token, var, i;
    ParticleEffect_t *effect;
	char *buffer;
	//char	newname[256];
	char* str;

	buffer = (char*) COM_LoadTempFile (filename);

	if (!buffer) {
		Con_Printf("\002Can't load particle effects from: %s\n",filename);
		return;
	}

	SC_Start(buffer,(int)strlen(buffer));

	Con_Printf("Loading particle effects from: %s\n",filename);

    while ( (token = SC_ParseToken()) != TOK_FILE_END) {
    
		if (token == TOK_PARTICLE) {
    		str = SC_ParseIdent();

			//if it already exists just overwrite the old one...
			effect = ParticleEffectDefinedForName(str);
			if (!effect) {
				effect = (ParticleEffect_t *)Hunk_Alloc(sizeof(ParticleEffect_t));			
  				DefaultEffect(effect);
				effect->next = particleEffects;
				particleEffects = effect;
				strcpy(effect->name,str);
				//Con_Printf("effect %s\n",effect->name);
			} else {
				//Con_Printf("redifinition %s\n",effect->name);
			}
			
    		if (SC_ParseToken() != '{') PARSERERROR("'{' expected");
  
  			while ((var = SC_ParseToken()) != '}' && (var != TOK_FILE_END) ) {
  				switch (var) {
					case TOK_EMITTER:
						//parse emmiter shape
						str = SC_ParseIdent();
						
						if (!strcmp(str,"box")) {
							effect->emmiterType = emt_box;
						} else {
							PARSERERROR("Unknown emmiter shape");	
						}

						//parse emmiter values
						for (i=0; i<3; i++)
							effect->emmiterParams1[i] = SC_ParseFloat();
							
						for (i=0; i<3; i++) 
							effect->emmiterParams2[i] = SC_ParseFloat();	
					break;
					case TOK_VELOCITY:
						//parse velocity mins maxs
						for (i=0; i<3; i++)
							effect->velocitymin[i] = SC_ParseFloat();
						for (i=0; i<3; i++) 
							effect->velocitymax[i] = SC_ParseFloat();
					break;
					case TOK_STARTCOLOR:
						//parse color mins maxs
						for (i=0; i<3; i++)
							effect->startcolormin[i] = SC_ParseFloat();
						for (i=0; i<3; i++) 
							effect->startcolormax[i] = SC_ParseFloat();
					break;
					case TOK_ENDCOLOR:
						//parse color mins maxs
						for (i=0; i<3; i++) 
							effect->endcolormin[i] = SC_ParseFloat();
						for (i=0; i<3; i++) 
							effect->endcolormax[i] = SC_ParseFloat();
					break;
					case TOK_LIFETIME:
						//parse lifetime mins maxs
						effect->lifemin = SC_ParseFloat();
						effect->lifemax = SC_ParseFloat();
					break;
					case TOK_FLAGS:
						str = SC_ParseIdent();
					break;
					case TOK_GRAVITY:
						for (i=0; i<3; i++) 
							effect->gravity[i] = SC_ParseFloat();	
					break;
					case TOK_ROTATION:
						effect->rotmin = SC_ParseFloat();
						effect->rotmax = SC_ParseFloat();
					break;
					case TOK_GROW:
						effect->growmin = SC_ParseFloat();
						effect->growmax = SC_ParseFloat();
					break;
					case TOK_SIZE:
						effect->sizemin = SC_ParseFloat();
						effect->sizemax = SC_ParseFloat();
					break;
					case TOK_DRAG:
						for (i=0; i<3; i++) 
							effect->drag[i] = SC_ParseFloat();
					break;
					case TOK_BLENDFUNC:
						effect->srcblend = SC_BlendModeForName(SC_ParseIdent());
						effect->dstblend = SC_BlendModeForName(SC_ParseIdent());
					break;
					case TOK_BOUNCES:
						effect->numbounces = (int)SC_ParseFloat();
					break;
					case TOK_MAP:
						effect->texture = EasyTgaLoad(SC_ParseString());
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
					break;
					case TOK_ORIENTATION:
						str = SC_ParseIdent();
						if (!strcmp(str,"view")) {
							effect->align = align_view;
						} else if (!strcmp(str,"vel")) {
							//Con_Printf("Velocity aligned\n");
							effect->align = align_vel;
							effect->velscale = SC_ParseFloat();
						} else if (!strcmp(str,"surface")) {
							//Con_Printf("Velocity aligned\n");
							effect->align = align_surf;
						} else {
							Con_Printf("\002Script error at line %i: Unknown orientation type %s\n",line_num,str);	
						}
					break;
					case TOK_ONHIT:
						str = SC_ParseIdent();
						effect->spawn = ParticleEffectForName(str);
						if (!effect->spawn)
							printf("\002Script error at line %i: Particle %s not defined yet \n",line_num,str);
					break;
					default:
						Con_Printf("\002Script error at line %i: Unknown field (id%i/%s) for particle definition\n",line_num,var,str);
				}
  			}
		} else if (token == TOK_DECAL) {
            extern int yylex (void);
            
  			while ((var = yylex()) != '}') {
			//do nothing yet...
  			}	
		} else {
			Con_Printf("\002Script error at line %i: Expected definiton (found id%i/%s)\n",line_num,var,str);
		}
    }

	SC_End();
}

void R_InitParticleEffects() {
	//clear list
	particleEffects = NULL;
	//load all scripts
	COM_FindAllExt("particles","particle",R_AddEffectsScript);
}

ParticleEffect_t *ParticleEffectDefinedForName(const char *name) {
	
	ParticleEffect_t *current;

	current = particleEffects;

	while (current) {
		if (!strcmp(current->name,name)) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

ParticleEffect_t *ParticleEffectForName(const char *name) {
	
	ParticleEffect_t *current;

	current = ParticleEffectDefinedForName(name);
	if (!current) Con_Printf("Effect not defined: %s\n",name);
	return current;
}


float RandomMinMax(float min, float max) {
	return min+((rand()%10000)/10000.0)*(max-min);
}

particle_t *InitParticleFromEffect(ParticleEffect_t *effect, vec3_t org) {

	particle_t *p;
	int i;

	if (!effect) return NULL;

	if (effect->align == align_surf) {
		//we can't spawn them here since we need extra information like the surface normal and such...
		return NULL;
	}
	
	//allocate it
	if (!free_particles)
		return NULL;
	p = free_particles;
	free_particles = p->next;
	p->next = active_particles;
	active_particles = p;

	//initialize the fields
	p->lifetime = RandomMinMax(effect->lifemin,effect->lifemax);
	p->die = cl.time + p->lifetime;
	for (i=0; i<3; i++) {
		p->vel[i] = RandomMinMax(effect->velocitymin[i],effect->velocitymax[i]);
		p->startcolor[i] = RandomMinMax(effect->startcolormin[i],effect->startcolormax[i]);
		p->endcolor[i] = RandomMinMax(effect->endcolormin[i],effect->endcolormax[i]);
		p->org[i] = RandomMinMax(effect->emmiterParams1[i],effect->emmiterParams2[i])+org[i];
	}

	p->numbounces = effect->numbounces;
	p->srcblend = effect->srcblend;
	p->dstblend = effect->dstblend;
	p->rspeed = RandomMinMax(effect->rotmin,effect->rotmax);
	p->growspeed = RandomMinMax(effect->growmin,effect->growmax);
	p->size = RandomMinMax(effect->sizemin,effect->sizemax);
	if (effect->align == align_vel) {
		p->velaligned = true;
	} else {
		p->velaligned = false;
	}
	p->velscale = effect->velscale;
	p->texture = effect->texture;
	p->spawn = effect->spawn;
	VectorCopy(effect->gravity,p->gravity);
	VectorCopy(effect->drag,p->drag);
	return p;
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles (void)
{
	int		i;

	i = COM_CheckParm ("-particles");

	if (i)
	{
		r_numparticles = (int)(Q_atoi(com_argv[i+1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t *)
			Hunk_AllocName (r_numparticles * sizeof(particle_t), "particles");

	emitters = (ParticleEmitter_t *)
			Hunk_AllocName (MAX_EMITTERS * sizeof(ParticleEmitter_t), "emitters");
}

/*
===============
R_ParseBasicEmitter

Parse an emitter out of the server message
Basic emitters don't actually spawn an emitter...
===============
*/
void R_ParseBasicEmitter (void)
{
	vec3_t		org;
	int			i, count;
	char		*name;
	ParticleEffect_t *eff;
	particle_t		*p;

	//Con_Printf("Particle effect!!\n");
	//origin to spawn on
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();

	//number of particles to spawn
	count = MSG_ReadByte ();

	//name of effect to spawn
	name = MSG_ReadString();

	eff = ParticleEffectForName(name);
	if (!eff) return;

	for (i=0; i<count; i++) {
		p = InitParticleFromEffect(eff,org);
	}
}

void R_ParseExtendedEmitter (void)
{
	vec3_t		org, vel;
	int			i, count;
	char		*name;
	ParticleEffect_t *eff;
	ParticleEmitter_t *emt;
	float		lifetime, tick;

	//Con_Printf("Particle effect22!!\n");
	//origin to spawn on
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();

	//velocity to spawn on
	for (i=0 ; i<3 ; i++)
		vel[i] = MSG_ReadCoord ();

	//number of particles to spawn
	count = MSG_ReadByte ();

	//duration to live
	lifetime = MSG_ReadLong () / 100.0;

	//animation time
	tick = MSG_ReadLong () / 100.0;

	//name of effect to spawn
	name = MSG_ReadString();

	eff = ParticleEffectForName(name);
	if (!eff) return;

	//allocate it
	if (!free_emitters)
		return;
	emt = free_emitters;
	free_emitters = emt->next;
	emt->next = active_emitters;
	active_emitters = emt;

	emt->effect = eff;
	VectorCopy(org,emt->origin);
	VectorCopy(vel,emt->vel);
	emt->die = cl.time+lifetime;
	emt->tick = tick;
	emt->count = count;
	emt->nexttick = 0;
}








/*
===============
R_EntityParticles
===============
*/

#define NUMVERTEXNORMALS	162
extern	float	r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t	avelocities[NUMVERTEXNORMALS];
float	beamlength = 16;
vec3_t	avelocity = {23, 7, 3};
float	partstep = 0.01;
float	timescale = 0.01;

void R_EntityParticles (entity_t *ent)
{
	int			count;
	int			i;
	particle_t	*p;
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	vec3_t		forward;
	float		dist;
	ParticleEffect_t *eff;
	
	dist = 64;
	count = 50;

if (!avelocities[0][0])
{
for (i=0 ; i<NUMVERTEXNORMALS*3 ; i++)
avelocities[0][i] = (rand()&255) * 0.01;
}


	eff = ParticleEffectForName("pt_entityparticles");
	if (!eff) return;

	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		angle = cl.time * avelocities[i][0];
		sy = sin(angle);
		cy = cos(angle);
		angle = cl.time * avelocities[i][1];
		sp = sin(angle);
		cp = cos(angle);
		angle = cl.time * avelocities[i][2];
		sr = sin(angle);
		cr = cos(angle);
	
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;

		p = InitParticleFromEffect(eff,ent->origin);
		if (!p) return;
		
		p->org[0] = ent->origin[0] + r_avertexnormals[i][0]*dist + forward[0]*beamlength;			
		p->org[1] = ent->origin[1] + r_avertexnormals[i][1]*dist + forward[1]*beamlength;			
		p->org[2] = ent->origin[2] + r_avertexnormals[i][2]*dist + forward[2]*beamlength;			
	}
}


/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles (void)
{
	int		i;
	
	//remove all particles
	free_particles = &particles[0];
	active_particles = NULL;

	for (i=0 ;i<r_numparticles ; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles-1].next = NULL;

	//remove all emitters
	free_emitters = &emitters[0];
	active_emitters = NULL;

	for (i=0 ;i<MAX_EMITTERS ; i++)
		emitters[i].next = &emitters[i+1];
	emitters[MAX_EMITTERS-1].next = NULL;
}


void R_ReadPointFile_f (void)
{
	FILE	*f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t	*p;
	char	name[MAX_OSPATH];
	ParticleEffect_t *eff;

	sprintf (name,"maps/%s.pts", sv.name);

	COM_FOpenFile (name, &f);
	if (!f)
	{
		Con_Printf ("couldn't open %s\n", name);
		return;
	}
	
	eff = ParticleEffectForName("pt_pointfile");
	if (!eff) return;
	
	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for ( ;; )
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;
		
		p = InitParticleFromEffect(eff,org);
		if (!p) return;
		VectorCopy (vec3_origin, p->vel);
	}

	fclose (f);
	Con_Printf ("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect (void)
{
	vec3_t		org, dir;
	int			i, count, msgcount, color;
	
	for (i=0 ; i<3 ; i++)
		org[i] = MSG_ReadCoord ();
	for (i=0 ; i<3 ; i++)
		dir[i] = MSG_ReadChar () * (1.0/16);
	msgcount = MSG_ReadByte ();
	color = MSG_ReadByte ();

if (msgcount == 255)
	count = 1024;
else
	count = msgcount;
	
	R_RunParticleEffect (org, dir, color, count);
}
	
/*
===============
R_ParticleGunHits

PENTA: When a gun hits the wall
===============
*/
void R_ParticleGunHits (vec3_t org, int type)
{
	int			i;//, j;
//	particle_t	*p;
	ParticleEffect_t *eff;
/*
#define	TE_SPIKE			0
#define	TE_SUPERSPIKE		1

#define	TE_EXPLOSION		3
#define	TE_TAREXPLOSION		4
#define	TE_LIGHTNING1		5
#define	TE_LIGHTNING2		6
#define	TE_WIZSPIKE			7
#define	TE_KNIGHTSPIKE		8
#define	TE_LIGHTNING3		9
#define	TE_LAVASPLASH		10
#define	TE_TELEPORT			11
#define TE_EXPLOSION2		12
*/
	switch (type) {

	//Shotgun hitting wall
	case TE_GUNSHOT:

		eff = ParticleEffectForName("pt_gunshot");
		for (i=0; i<2; i++) {
			InitParticleFromEffect(eff,org);
		}

		eff = ParticleEffectForName("pt_gunshotsmoke");
		for (i=0; i<1; i++) {
			InitParticleFromEffect(eff,org);
		}
		break;
	//Nails hitting wall
	case TE_SPIKE:
	case TE_SUPERSPIKE:
		for (i=0 ; i<6 ; i++)
		{

			eff = ParticleEffectForName("pt_spike");
			for (i=0; i<6; i++) {
				InitParticleFromEffect(eff,org);
			}
		}
		break;
	//lightining hitting wall
	case TE_LIGHTNING1:
	case TE_LIGHTNING2:
	case TE_LIGHTNING3:
			eff = ParticleEffectForName("pt_lightning");
			for (i=0; i<6; i++) {
				InitParticleFromEffect(eff,org);
			}
		break;
	default:
		break;
	}
}

/*
===============
R_ParticleHitBlood

PENTA: Changes
===============
*/
void R_ParticleHitBlood (vec3_t org, int color)
{
	int			i;//, j;
//	particle_t	*p;
	ParticleEffect_t *eff;

	//Con_Printf("blood\n");


	eff = ParticleEffectForName("pt_hitblood1");
	for (i=0 ; i<1 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}

	eff = ParticleEffectForName("pt_hitblood2");
	for (i=0 ; i<2 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}
}
/*
===============
R_ParticleExplosion

PENTA: Changes
===============
*/
void R_ParticleExplosion (vec3_t org)
{
	int			i;//, j;
//	particle_t	*p;
	ParticleEffect_t *eff;
	
	eff = ParticleEffectForName("pt_explosion1");
	for (i=0 ; i<128 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}

	eff = ParticleEffectForName("pt_explosion2");
	for (i=0 ; i<128 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}
}

/*
===============
R_ParticleExplosion2

===============
*/
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int			i;//, j;
//	particle_t	*p;
//	int			colorMod = 0;
	ParticleEffect_t *eff;

	eff = ParticleEffectForName("pt_explosion1");
	for (i=0 ; i<64 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}

}

/*
===============
R_BlobExplosion

===============
*/
void R_BlobExplosion (vec3_t org)
{
	int			i;//, j;
//	particle_t	*p;
	ParticleEffect_t *eff;	

	eff = ParticleEffectForName("pt_voreexplosion1");
	for (i=0 ; i<64 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}

	eff = ParticleEffectForName("pt_voreexplosion2");
	for (i=0 ; i<64 ; i++)
	{
		InitParticleFromEffect(eff,org);
	}
}

/*
===============
R_RunParticleEffect

===============
*/
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count)
{
	int			i;//, j;
//	particle_t	*p;
	ParticleEffect_t *eff;

	if ((color == 225) || (color == 73)) {
		R_ParticleHitBlood (org, color);
		return;
	}

	if (count == 1024) {
		R_ParticleExplosion(org);
	}

	eff = ParticleEffectForName("pt_genericsmoke");
	for (i=0; i<count; i++) {
		InitParticleFromEffect(eff,org);
	}
}


/*
===============
R_LavaSplash

===============
*/
void R_LavaSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	ParticleEffect_t *eff;

	eff = ParticleEffectForName("pt_lavasplash");
	for (i=-16 ; i<16 ; i++)
		for (j=-16 ; j<16 ; j++)
			for (k=0 ; k<1 ; k++)
			{
				p = InitParticleFromEffect(eff,org);
				if (!p) return;
		
				dir[0] = j*8 + (rand()&7);
				dir[1] = i*8 + (rand()&7);
				dir[2] = 256;
	
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand()&63);
	
				VectorNormalize (dir);						
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
}

/*
===============
R_TeleportSplash

===============
*/
void R_TeleportSplash (vec3_t org)
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;
	ParticleEffect_t *eff;

	eff = ParticleEffectForName("teleportsplash");
	for (i=-16 ; i<16 ; i+=4)
		for (j=-16 ; j<16 ; j+=4)
			for (k=-24 ; k<32 ; k+=4)
			{
				p = InitParticleFromEffect(eff,org);
				if (!p) return;

				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;
	
				p->org[0] = org[0] + i + (rand()&3);
				p->org[1] = org[1] + j + (rand()&3);
				p->org[2] = org[2] + k + (rand()&3);
	
				VectorNormalize (dir);						
				vel = 50 + (rand()&63);
				VectorScale (dir, vel, p->vel);
			}
}

void R_RocketTrail (vec3_t start, vec3_t end, int type)
{
	vec3_t		vec;
	float		len;
	int			j;
	particle_t	*p;
	int			dec;
//	static int	tracercount;
	ParticleEffect_t *eff;

	VectorSubtract (end, start, vec);
	len = VectorNormalize (vec);
	if (type < 128)

		if ((type == 6) || (type == 5))
			dec = 1;
		else
			dec = 10;
	else
	{
		Con_Printf("ypt\n");
		dec = 1;
		type -= 128;
	}


	switch (type)
	{
		case 0:	// rocket trail
			eff = ParticleEffectForName("pt_rockettrail");
			break;
		case 1:	// smoke smoke
			eff = ParticleEffectForName("pt_smoke");
			break;
		case 2:	// blood
			eff = ParticleEffectForName("pt_bloodtrail");
			break;
		case 4:	// slight blood
			eff = ParticleEffectForName("pt_bloodtrail");
			break;
		case 3:
			eff = ParticleEffectForName("pt_wizzardtrail");
			break;
		case 5:	// tracer
			eff = ParticleEffectForName("pt_hknighttrail");
			break;
		case 6:	// voor trail
			eff = ParticleEffectForName("pt_voretrail");
			break;
		default:
			eff = ParticleEffectForName("pt_genericsmoke");
			break;
		}


	while (len > 0)
	{
		len -= dec;

		p = InitParticleFromEffect(eff,start);
		if (!p) return;

		switch (type)
		{
			case 0:	// rocket trail
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				break;
			case 1:	// smoke smoke
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				break;
			case 2:	// blood
			case 4:	// slight blood
				for (j=0 ; j<3 ; j++)
					p->org[j] = start[j] + ((rand()%6)-3);
				break;
			case 6:	// voor trail
				for (j=0 ; j<3 ; j++) {
					p->org[j] = start[j] + ((rand()%8)-4);
				}
				break;
		}
		
		VectorAdd (start, vec, start);
	}
}


/*
===============
R_DrawParticles
===============

extern	cvar_t	sv_gravity;

void R_DrawParticles (void)
{
	particle_t		*p, *kill;
	float			grav;
	int				i;
	float			time2, time3;
	float			time1;
	float			dvel;
	float			frametime;
	
#ifdef GLQUAKE
	vec3_t			up, right, neworg;
	float			scale, sscale;

	glFogfv(GL_FOG_COLOR, color_black); //Done in actual function now (stops "triangle effect") - Eradicator
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);
	glEnable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(0);

	VectorScale (vup, 1, up);
	VectorScale (vright, 1, right);
	glMatrixMode(GL_TEXTURE);
#else
	D_StartParticles ();

	VectorScale (vright, xscaleshrink, r_pright);
	VectorScale (vup, yscaleshrink, r_pup);
	VectorCopy (vpn, r_ppn);
#endif
	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4*frametime;
	
	for ( ;; ) 
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p=active_particles ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			//XYZ
			if (kill && ((kill->die < cl.time) || (kill->numbounces <= 0)))
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

#ifdef GLQUAKE
		// hack a scale up to keep particles from disapearing
		
		//scale = (p->org[0] - r_origin[0])*vpn[0] + (p->org[1] - r_origin[1])*vpn[1]
		//	+ (p->org[2] - r_origin[2])*vpn[2];
		//if (scale < 20)
		//	scale = 1;
		//else
		//	scale = 1 + scale * 0.004;
		//
		scale = 10;

		if ((p->die - cl.time) < 0.5) {
			byte *c = (byte *)&d_8to24table[(int)p->color];
			float scale = 2*(p->die - cl.time);
			glColor3ub((byte)(c[0]*scale), (byte)(c[1]*scale), (byte)(c[2]*scale));
		} else {
			glColor3ubv ((byte *)&d_8to24table[(int)p->color]);
		}

		GL_Bind(p->texture);

		if ((p->texture ==  particletexture_smoke) ||
		   (p->texture ==  particletexture_blood) ) scale = 50;

		if (p->blendfunc == pb_add) {
			glBlendFunc (GL_ONE, GL_ONE);
		} else {
			glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		}
		//XYZ
		if ((p->texture ==  particletexture_glow) ||
		    (p->texture ==  particletexture_dirblood)){
			float lscale;
			VectorCopy (p->vel, up);
			VectorNormalize(up);
			CrossProduct(vpn,up,right);
			lscale = (Length(p->vel)/64);
			VectorScale(up,lscale,up);
		} else {
			VectorCopy (vup, up);
			VectorCopy (vright, right);
		}

		glLoadIdentity();
		glTranslatef(0.5,0.5,0);
		glRotatef(p->rot,0,0,1);
		glTranslatef(-0.5,-0.5,0);

		glBegin(GL_TRIANGLES);
		glTexCoord2f (0,0);
		
		sscale = -scale/4;
		VectorMA(p->org,sscale,up,neworg);
		VectorMA(neworg,sscale,right,neworg);

		glVertex3fv (neworg);
		glTexCoord2f (2,0);
		glVertex3f (neworg[0] + up[0]*scale, neworg[1] + up[1]*scale, neworg[2] + up[2]*scale);
		glTexCoord2f (0,2);
		glVertex3f (neworg[0] + right[0]*scale, neworg[1] + right[1]*scale, neworg[2] + right[2]*scale);
		glEnd();
#else
		D_DrawParticle (p);
#endif

		neworg[0] = p->org[0]+p->vel[0]*frametime;
		neworg[1] = p->org[1]+p->vel[1]*frametime;
		neworg[2] = p->org[2]+p->vel[2]*frametime;
		p->rot = p->rot+p->rspeed*frametime;

		{
			trace_t	trace;
			float	d;

			memset (&trace, 0, sizeof(trace));
			trace.fraction = 1;
			SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, p->org, neworg, &trace);

			if (trace.fraction < 1) {
				vec3_t tangent;
				//calc reflection vector
				d = DotProduct (p->vel, trace.plane.normal);
				VectorMA (p->vel, -2*d, trace.plane.normal, p->vel);
				VectorScale(p->vel,0.33,p->vel);
				VectorCopy(trace.endpos,p->org);
				//XYZ
				p->numbounces--;

				CrossProduct(trace.plane.normal,p->vel,tangent);
				R_SpawnDecal(trace.endpos, trace.plane.normal, tangent, dt_blood);
			} else {
				VectorCopy(neworg,p->org);
			}
		}


		
		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6)
				p->die = -1;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;

		case pt_explode:
			p->ramp += time2;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp1[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >=8)
				p->die = -1;
			else
				p->color = ramp2[(int)p->ramp];
			for (i=0 ; i<3 ; i++)
				p->vel[i] -= p->vel[i]*frametime;
			p->vel[2] -= grav;
			break;

		case pt_blob:
			for (i=0 ; i<3 ; i++)
				p->vel[i] += p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_blob2:
			for (i=0 ; i<2 ; i++)
				p->vel[i] -= p->vel[i]*dvel;
			p->vel[2] -= grav;
			break;

		case pt_grav:
#ifdef QUAKE2
			p->vel[2] -= grav * 20;
			break;
#endif
		case pt_slowgrav:
			p->vel[2] -= grav*6;
			break;
		}
	}

#ifdef GLQUAKE
	glDepthMask(1);
	glDisable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//XYZ
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glFogfv(GL_FOG_COLOR, fog_color); //Done in actual function now (stops "triangle effect") - Eradicator
#else
	D_EndParticles ();
#endif
}

*/
extern	cvar_t	sv_gravity;

void R_DrawParticles (void)
{
	particle_t		*p, *kill;
	float			grav;
	int				i;
	float			time2, time3;
	float			time1;
	float			dvel, blend, blend1;
	float			frametime;
	vec3_t			up, right, neworg;
	float			scale, sscale;
	ParticleEmitter_t *ekill, *emt;

	if (gl_wireframe.value)
		return;

	glFogfv(GL_FOG_COLOR, color_black); //Done in actual function now (stops "triangle effect") - Eradicator
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.01);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(0);

	VectorScale (vup, 1, up);
	VectorScale (vright, 1, right);
	glMatrixMode(GL_TEXTURE);

	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * sv_gravity.value * 0.05;
	dvel = 4*frametime;

	//remove expired emitters
	for ( ;; ) 
	{
		ekill = active_emitters;
		if (ekill && ekill->die < cl.time)
		{
			active_emitters = ekill->next;
			ekill->next = free_emitters;
			free_emitters = ekill;
			continue;
		}
		break;
	}

	//Do the particle logic/drawing
	for (emt=active_emitters ; emt ; emt=emt->next)
	{
		for ( ;; )
		{
			ekill = emt->next;
			//XYZ
			if (ekill && (ekill->die < cl.time))
			{
				emt->next = ekill->next;
				ekill->next = free_emitters;
				free_emitters = ekill;
				continue;
			}
			break;
		}

		if (emt->nexttick < cl.time) {
			vec3_t length;
			VectorSubtract(emt->origin, r_refdef.vieworg, length);
			//dont emit if we are to far away to see it
			if (Length(length) < 600.0f) {
				for (i=0; i<emt->count; i++) {
					InitParticleFromEffect(emt->effect,emt->origin);
				}
			}
			emt->nexttick = cl.time + emt->tick;
		}
	}

	//remove expired particles
	for ( ;; ) 
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	//Do the particle logic/drawing
	for (p=active_particles ; p ; p=p->next)
	{
		for ( ;; )
		{
			kill = p->next;
			//XYZ
			if (kill && ((kill->die < cl.time) || (kill->numbounces <= 0)))
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

		scale = p->size;
		p->size += p->growspeed*frametime;

		//calculate color based on life ...
		blend = (p->die-cl.time)/p->lifetime;
		blend1 = 1-blend;
		for (i=0; i<3; i++) {
			p->color[i] = p->startcolor[i] * blend + p->endcolor[i] * blend1;
		}

		if ((p->die - cl.time) < 0.5) {
			float fade = 2*(p->die - cl.time);
			glColor4f(p->color[0]*fade, p->color[1]*fade, p->color[2]*fade, fade);
		} else {
			glColor3fv(&p->color[0]);
		}

		GL_Bind(p->texture);
		glBlendFunc (p->srcblend, p->dstblend);

		//Align with velocity
		if (p->velaligned){
			float lscale;
			VectorCopy (p->vel, up);
			VectorNormalize(up);
			CrossProduct(vpn,up,right);
			VectorNormalize(right);
			lscale = (Length(p->vel)*p->velscale);
			VectorScale(up,lscale,up);
		} else {
			VectorCopy (vup, up);
			VectorCopy (vright, right);
		}

		glLoadIdentity();
		glTranslatef(0.5,0.5,0);
		glRotatef(p->rot,0,0,1);
		glTranslatef(-0.5,-0.5,0);

		sscale = -scale/4;
		VectorMA(p->org,sscale,up,neworg);
		VectorMA(neworg,sscale,right,neworg);

                // draw the particle as two triangles
                scale /= 2;
		glBegin(GL_TRIANGLE_FAN);
		glTexCoord2f (0,0);
		glVertex3fv (neworg);
		glTexCoord2f (0,1);
		glVertex3f (neworg[0] + up[0]*scale, neworg[1] + up[1]*scale,
                            neworg[2] + up[2]*scale);
		glTexCoord2f (1,1);
		glVertex3f (neworg[0] + up[0]*scale + right[0]*scale, neworg[1] + up[1]*scale + right[1]*scale,
                            neworg[2] + up[2]*scale + right[2]*scale);
		glTexCoord2f (1,0);
		glVertex3f (neworg[0] + right[0]*scale, neworg[1] + right[1]*scale,
                            neworg[2] + right[2]*scale);
		glEnd();
                scale *= 2;

		//calculate new position/rotation
		neworg[0] = p->org[0]+p->vel[0]*frametime;
		neworg[1] = p->org[1]+p->vel[1]*frametime;
		neworg[2] = p->org[2]+p->vel[2]*frametime;
		p->rot = p->rot+p->rspeed*frametime;

		//do collision detection
		{
			trace_t	trace;
			float	d;

			memset (&trace, 0, sizeof(trace));
			trace.fraction = 1;
			SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, p->org, neworg, &trace);

			if (trace.fraction < 1) {
				vec3_t tangent;
				//calc reflection vector
				d = DotProduct (p->vel, trace.plane.normal);
				VectorMA (p->vel, -2*d, trace.plane.normal, p->vel);
				VectorScale(p->vel,0.33,p->vel);
				VectorCopy(trace.endpos,p->org);
				//XYZ
				p->numbounces--;

				if (p->spawn) {
					CrossProduct(trace.plane.normal,p->vel,tangent);
					if (p->spawn->align == align_surf) {
						R_SpawnDecal(p->org, trace.plane.normal, tangent, p->spawn);
					} else {
						InitParticleFromEffect(p->spawn, p->org);
					}
				}
			} else {
				VectorCopy(neworg,p->org);
			}
		}

		for (i=0; i<3; i++) {
			p->vel[i] += p->gravity[i]*frametime;
		}
		
		for (i=0; i<3; i++) {
			p->vel[i] *= p->drag[i];
		}
	}

	glDepthMask(1);
	glDisable (GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER,0.666);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//XYZ
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glFogfv(GL_FOG_COLOR, fog_color); //Done in actual function now (stops "triangle effect") - Eradicator
}
