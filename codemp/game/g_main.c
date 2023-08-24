/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/


#include "g_local.h"
#include "g_ICARUScb.h"
#include "g_nav.h"
#include "bg_saga.h"
#include "b_local.h"
#include "qcommon/q_version.h"

NORETURN_PTR void (*Com_Error)( int level, const char *error, ... );
void (*Com_Printf)( const char *msg, ... );

level_locals_t	level;

int		eventClearTime = 0;
static int navCalcPathTime = 0;
extern int fatalErrors;

int killPlayerTimer = 0;

gentity_t		g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];

qboolean gDuelExit = qfalse;

void G_InitGame					( int levelTime, int randomSeed, int restart );
void G_RunFrame					( int levelTime );
void G_ShutdownGame				( int restart );
void CheckExitRules				( void );
void G_ROFF_NotetrackCallback	( gentity_t *cent, const char *notetrack);

extern stringID_table_t setTable[];

qboolean G_ParseSpawnVars( qboolean inSubBSP );
void G_SpawnGEntityFromSpawnVars( qboolean inSubBSP );


qboolean NAV_ClearPathToPoint( gentity_t *self, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask, int okToHitEntNum );
qboolean NPC_ClearLOS2( gentity_t *ent, const vec3_t end );
int NAVNEW_ClearPathBetweenPoints(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int ignore, int clipmask);
qboolean NAV_CheckNodeFailedForEnt( gentity_t *ent, int nodeNum );
qboolean G_EntIsUnlockedDoor( int entityNum );
qboolean G_EntIsDoor( int entityNum );
qboolean G_EntIsBreakable( int entityNum );
qboolean G_EntIsRemovableUsable( int entNum );
void CP_FindCombatPointWaypoints( void );

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=MAX_CLIENTS, e=g_entities+i ; i < level.num_entities ; i++,e++ ) {
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		if (e->r.contents==CONTENTS_TRIGGER)
			continue;//triggers NEVER link up in teams!
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

//	trap->Print ("%i teams with %i entities\n", c, c2);
}

sharedBuffer_t gSharedBuffer;

void WP_SaberLoadParms( void );
void BG_VehicleLoadParms( void );

void G_CacheGametype( void )
{
	// check some things
	if ( g_gametype.string[0] && isalpha( g_gametype.string[0] ) )
	{
		int gt = BG_GetGametypeForString( g_gametype.string );
		if ( gt == -1 )
		{
			trap->Print( "Gametype '%s' unrecognised, defaulting to FFA/Deathmatch\n", g_gametype.string );
			level.gametype = GT_FFA;
		}
		else
			level.gametype = gt;
	}
	else if ( g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE )
	{
		trap->Print( "g_gametype %i is out of range, defaulting to 0 (FFA/Deathmatch)\n", g_gametype.integer );
		level.gametype = GT_FFA;
	}
	else
		level.gametype = atoi( g_gametype.string );

	trap->Cvar_Set( "g_gametype", va( "%i", level.gametype ) );
	trap->Cvar_Update( &g_gametype );
}

void G_CacheMapname( const vmCvar_t *mapname )
{
	Com_sprintf( level.mapname, sizeof( level.mapname ), "maps/%s.bsp", mapname->string );
	Com_sprintf( level.rawmapname, sizeof( level.rawmapname ), "maps/%s", mapname->string );
}

// zyk: this function spawns an info_player_deathmatch entity in the map
extern void zyk_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_spawn_entity(gentity_t *ent);
extern void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_main_spawn_entity(gentity_t *ent);
void zyk_create_info_player_deathmatch(int x, int y, int z, int yaw)
{
	gentity_t *spawn_ent = NULL;

	spawn_ent = G_Spawn();
	if (spawn_ent)
	{
		int i = 0;
		gentity_t *this_ent;
		gentity_t *spawn_point_ent = NULL;

		for (i = 0; i < level.num_entities; i++)
		{
			this_ent = &g_entities[i];
			if (Q_stricmp( this_ent->classname, "info_player_deathmatch") == 0)
			{ // zyk: found the original SP map spawn point
				spawn_point_ent = this_ent;
				break;
			}
		}

		zyk_set_entity_field(spawn_ent,"classname","info_player_deathmatch");
		zyk_set_entity_field(spawn_ent,"origin",va("%d %d %d",x,y,z));
		zyk_set_entity_field(spawn_ent,"angles",va("0 %d 0",yaw));
		if (spawn_point_ent && spawn_point_ent->target)
		{ // zyk: setting the target for SP map spawn points so they will work properly
			zyk_set_entity_field(spawn_ent,"target",spawn_point_ent->target);
		}

		zyk_spawn_entity(spawn_ent);
	}
}

// zyk: creates a ctf flag spawn point
void zyk_create_ctf_flag_spawn(int x, int y, int z, qboolean redteam)
{
	gentity_t *spawn_ent = NULL;

	spawn_ent = G_Spawn();
	if (spawn_ent)
	{
		if (redteam == qtrue)
			zyk_set_entity_field(spawn_ent,"classname","team_CTF_redflag");
		else
			zyk_set_entity_field(spawn_ent,"classname","team_CTF_blueflag");

		zyk_set_entity_field(spawn_ent,"origin",va("%d %d %d",x,y,z));
		zyk_spawn_entity(spawn_ent);
	}
}

// zyk: creates a ctf player spawn point
void zyk_create_ctf_player_spawn(int x, int y, int z, int yaw, qboolean redteam, qboolean team_begin_spawn_point)
{
	gentity_t *spawn_ent = NULL;

	spawn_ent = G_Spawn();
	if (spawn_ent)
	{
		if (redteam == qtrue)
		{
			if (team_begin_spawn_point == qtrue)
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_redplayer");
			else
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_redspawn");
		}
		else
		{
			if (team_begin_spawn_point == qtrue)
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_blueplayer");
			else
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_bluespawn");
		}

		zyk_set_entity_field(spawn_ent,"origin",va("%d %d %d",x,y,z));
		zyk_set_entity_field(spawn_ent,"angles",va("0 %d 0",yaw));

		zyk_spawn_entity(spawn_ent);
	}
}

// zyk: used to fix func_door entities in SP maps that wont work and must be removed without causing the door glitch
void fix_sp_func_door(gentity_t *ent)
{
	ent->spawnflags = 0;
	ent->flags = 0;
	GlobalUse(ent,ent,ent);
	G_FreeEntity( ent );
}

// zyk: sets the music to play in this quest map. Music will start after some miliseconds set in music_timer
void zyk_set_map_music(zyk_map_music_t music_to_set, int music_timer)
{
	level.current_map_music = music_to_set;
	level.map_music_reset_timer = level.time + music_timer;
}

extern gentity_t *NPC_Spawn_Do( gentity_t *ent );

// zyk: spawn an npc at a given x, y and z coordinates
gentity_t *Zyk_NPC_SpawnType( char *npc_type, int x, int y, int z, int yaw )
{
	gentity_t		*NPCspawner;
	vec3_t			forward, end, viewangles;
	trace_t			trace;
	vec3_t origin;

	origin[0] = x;
	origin[1] = y;
	origin[2] = z;

	viewangles[0] = 0;
	viewangles[1] = yaw;
	viewangles[2] = 0;

	NPCspawner = G_Spawn();

	if(!NPCspawner)
	{
		Com_Printf( S_COLOR_RED"NPC_Spawn Error: Out of entities!\n" );
		return NULL;
	}

	NPCspawner->think = G_FreeEntity;
	NPCspawner->nextthink = level.time + FRAMETIME;

	//rwwFIXMEFIXME: Care about who is issuing this command/other clients besides 0?
	//Spawn it at spot of first player
	//FIXME: will gib them!
	AngleVectors(viewangles, forward, NULL, NULL);
	VectorNormalize(forward);
	VectorMA(origin, 0, forward, end);
	trap->Trace(&trace, origin, NULL, NULL, end, 0, MASK_SOLID, qfalse, 0, 0);
	VectorCopy(trace.endpos, end);
	end[2] -= 24;
	trap->Trace(&trace, trace.endpos, NULL, NULL, end, 0, MASK_SOLID, qfalse, 0, 0);
	VectorCopy(trace.endpos, end);
	end[2] += 24;
	G_SetOrigin(NPCspawner, end);
	VectorCopy(NPCspawner->r.currentOrigin, NPCspawner->s.origin);
	//set the yaw so that they face away from player
	NPCspawner->s.angles[1] = viewangles[1];

	trap->LinkEntity((sharedEntity_t *)NPCspawner);

	NPCspawner->NPC_type = G_NewString( npc_type );

	NPCspawner->count = 1;

	NPCspawner->delay = 0;

	NPCspawner = NPC_Spawn_Do( NPCspawner );

	if ( NPCspawner != NULL )
		return NPCspawner;

	G_FreeEntity( NPCspawner );

	return NULL;
}

void zyk_set_quest_npc_events(gentity_t* npc_ent)
{
	if (npc_ent && npc_ent->client)
	{
		int i = 0;

		npc_ent->client->pers.quest_npc_timer = 0;
		npc_ent->client->pers.quest_npc_current_event = 0;

		if (level.quest_map == QUESTMAP_DESERT_CITY)
		{ // zyk: main city
			if (npc_ent->client->pers.quest_npc == 1)
			{
				for (i = 0; i < MAX_QUEST_NPC_EVENTS; i++)
				{
					npc_ent->client->pers.quest_npc_anims[i] = BOTH_WALK1TALKCOMM1 + i;
					npc_ent->client->pers.quest_npc_anim_duration[i] = 2000;
					npc_ent->client->pers.quest_npc_interval_timer[i] = 3000;
				}
			}
		}
	}
}

void zyk_set_quest_npc_magic(gentity_t* npc_ent, int magic_powers_levels[MAX_MAGIC_POWERS])
{
	int i = 0;

	// zyk: sets magic power levels to this npc
	if (npc_ent && npc_ent->client)
	{
		for (i = 0; i < MAX_MAGIC_POWERS; i++)
		{
			if ((SKILL_MAGIC_MAGIC_SENSE + i) < NUMBER_OF_SKILLS)
			{
				npc_ent->client->pers.skill_levels[SKILL_MAGIC_MAGIC_SENSE + i] = magic_powers_levels[i];
			}
		}
	}
}

// zyk: spawns a quest npc and sets additional stuff, like levels, etc
gentity_t* zyk_spawn_quest_npc(char* npc_type, int x, int y, int z, int yaw, int level, int quest_npc_number)
{
	gentity_t* npc_ent = Zyk_NPC_SpawnType(npc_type, x, y, z, yaw);

	if (npc_ent && npc_ent->client)
	{
		npc_ent->client->pers.quest_npc = quest_npc_number;
		npc_ent->client->pers.level = level;
		npc_ent->client->pers.magic_power = level * 5;
		npc_ent->client->ps.stats[STAT_MAX_HEALTH] += 10 * level;
		npc_ent->health = npc_ent->client->ps.stats[STAT_MAX_HEALTH];

		// zyk: every quest stuff will have this spawnflag
		npc_ent->spawnflags |= 131072;

		return npc_ent;
	}

	return NULL;
}

void zyk_quest_item_use(gentity_t *self, gentity_t* other, gentity_t* activator)
{
	if (level.quest_map == QUESTMAP_HERO_HOUSE && self->count == 1)
	{ // zyk: Book in the table
		// zyk: starts tutorial
		if (other && other->client && other->client->sess.amrpgmode == 2)
		{
			other->client->pers.tutorial_step = 0;
			other->client->pers.tutorial_timer = level.time + 1000;
			other->client->pers.player_statuses |= (1 << 25);
		}
	}
}

// zyk: spawns quest entities
// entity_type can be:
// 0: model
// 1: effect

void zyk_load_quest_model(char *origin, char *angles, char *model_path, int spawnflags, char *mins, char *maxs, int model_scale, int count, int entity_type)
{
	gentity_t* new_ent = G_Spawn();

	if (entity_type == 1)
	{
		zyk_main_set_entity_field(new_ent, "classname", "fx_runner");
		zyk_main_set_entity_field(new_ent, "spawnflags", va("%d", spawnflags));
		zyk_main_set_entity_field(new_ent, "origin", G_NewString(origin));
		zyk_main_set_entity_field(new_ent, "angles", G_NewString(angles));
		zyk_main_set_entity_field(new_ent, "fxFile", G_NewString(model_path));
	}
	else
	{
		zyk_main_set_entity_field(new_ent, "classname", "misc_model_breakable");
		zyk_main_set_entity_field(new_ent, "spawnflags", va("%d", spawnflags));
		zyk_main_set_entity_field(new_ent, "zykmodelscale", va("%d", model_scale));
		zyk_main_set_entity_field(new_ent, "origin", G_NewString(origin));
		zyk_main_set_entity_field(new_ent, "angles", G_NewString(angles));
		zyk_main_set_entity_field(new_ent, "model", G_NewString(model_path));
		zyk_main_set_entity_field(new_ent, "mins", G_NewString(mins));
		zyk_main_set_entity_field(new_ent, "maxs", G_NewString(maxs));
	}

	zyk_main_spawn_entity(new_ent);

	new_ent->count = count;

	// zyk: every quest stuff will have this spawnflag
	new_ent->spawnflags |= 131072;

	if (level.quest_map == QUESTMAP_HERO_HOUSE && count == 1)
	{ // zyk: Book in the table
		new_ent->use = zyk_quest_item_use;
		new_ent->r.svFlags |= SVF_PLAYER_USABLE;
	}
}

// zyk: spawn quest stuff (like models and npcs)
// If load_quest_player_stuff is qfalse, load the default quest stuff. Oherwise, load the specific stuff of the current mission for the quest players
void zyk_spawn_quest_stuff(qboolean load_quest_player_stuff)
{
	if (zyk_allow_quests.integer > 0)
	{
		if (level.quest_map == QUESTMAP_HERO_HOUSE)
		{
			if (load_quest_player_stuff == qfalse)
			{
				zyk_load_quest_model("295 -350 93", "0 0 0", "models/map_objects/nar_shaddar/book.md3", 1, "-8 -8 -8", "8 8 8", 100, 1, 0);
			}
		}
		else if (level.quest_map == QUESTMAP_DESERT_CITY)
		{
			// zyk: spawning the citizens
			if (load_quest_player_stuff == qfalse)
			{
				gentity_t* npc_ent = zyk_spawn_quest_npc("quest_citizen", 12253, -454, -486, 45, 1, 0);
				zyk_set_quest_npc_events(npc_ent);

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 11893, -1262, -486, -179, 1, 1);

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 10183, -3482, -486, 15, 1, 2);

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 10354, -3528, -486, 90, 1, 3);

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 9895, -2367, -486, 60, 1, 4);

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 8748, 298, -230, 172, 1, 5);

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", 8918, -2460, 536, 45, 5, 6); // zyk: guard in one of the towers in the first area of the map

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", 8056, 452, 536, 15, 5, 7); // zyk: the other guard at the other tower in the first area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 9189, 2528, -359, -179, 1, 8);

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 9755, 991, -486, -90, 5, 9); // zyk: npc near the stairs. Will unlock next Main Quest mission

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", 2355, -306, 376, -45, 5, 10); // zyk: guard in the second area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 2453, -2353, -486, 179, 1, 11); // zyk: npc inside a building in second area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 2442, 1269, -486, 0, 1, 12); // zyk: npc inside another building (with turrets) in second area

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", 3005, 530, -486, 90, 5, 13); // zyk: guard inside building with turrets in second area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 2417, 1122, -486, 90, 1, 14); // zyk: npc in turrets building in second area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 2417, 1223, -486, -90, 1, 15); // zyk: npc in turrets building in second area talking to the one above

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -2020, -1286, -7, 90, 1, 16); // zyk: npc in arena

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -2020, -312, -7, -90, 1, 17); // zyk: npc in arena

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -2612, 254, 284, -60, 1, 18); // zyk: npc in arena sits

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -3144, -1281, 268, 19, 1, 19); // zyk: npc in arena sits

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -2591, -798, -7, 0, 1, 20); // zyk: npc in arena sits. The announcer

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -6059, -2286, -487, -179, 1, 21); // zyk: npc locked in small room in third area. Will unlock a side quest

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -7610, -1664, -369, 179, 1, 22); // zyk: npc inside building in third area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -9131, 662, -67, 97, 1, 23); // zyk: npc inside high building in third area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -6555, 1240, -390, -179, 1, 24); // zyk: npc inside building in third area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -7468, -1735, -487, 90, 1, 25); // zyk: npc talking to guard

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", -7468, -1599, -487, -90, 5, 26); // zyk: guard talking to npc

				npc_ent = zyk_spawn_quest_npc("quest_citizen", -8917, -2755, -487, 0, 1, 27); // zyk: npc in third area

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", -11007, -1045, 8, 0, 5, 28); // zyk: guard outside tower in third area

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", -11014, -118, 8, 0, 5, 29); // zyk: guard outside tower in third area

				npc_ent = zyk_spawn_quest_npc("quest_city_guard_officer", -10852, -638, 8, 0, 5, 30); // zyk: officer inside tower in third area

				npc_ent = zyk_spawn_quest_npc("quest_citizen", 10162, -3299, -487, -5, 1, 31); // zyk: seller. Will unlock the seller for the player

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", 12900, -909, -487, 158, 5, 32); // zyk: guard near entrance in first area

				npc_ent = zyk_spawn_quest_npc("quest_city_guard", 12105, 617, -487, -97, 5, 33); // zyk: guard near entrance in first area
			}
		}
	}
}

// zyk: remove all entities used in quests
void zyk_clear_quest_stuff()
{
	int i = 0;

	for (i = MAX_CLIENTS + BODY_QUEUE_SIZE; i < level.num_entities; i++)
	{
		gentity_t* this_ent = &g_entities[i];

		if (this_ent && this_ent->spawnflags & (131072))
		{ // zyk: remove quest entity
			G_FreeEntity(this_ent);
		}
	}
}

/*
============
G_InitGame

============
*/
extern void RemoveAllWP(void);
extern void BG_ClearVehicleParseParms(void);
gentity_t *SelectRandomDeathmatchSpawnPoint( qboolean isbot );
void SP_info_jedimaster_start( gentity_t *ent );
extern void zyk_create_dir(char *file_path);
extern void load_custom_quest_mission();
void G_InitGame( int levelTime, int randomSeed, int restart ) {
	int					i;
	vmCvar_t	mapname;
	vmCvar_t	ckSum;
	char serverinfo[MAX_INFO_STRING] = {0};
	// zyk: variable used in the SP buged maps fix
	char zyk_mapname[128] = {0};
	FILE *zyk_entities_file = NULL;
	FILE *zyk_remap_file = NULL;
	FILE *zyk_duel_arena_file = NULL;
	FILE *zyk_melee_arena_file = NULL;

	//Init RMG to 0, it will be autoset to 1 if there is terrain on the level.
	trap->Cvar_Set("RMG", "0");
	RMG.integer = 0;

	//Clean up any client-server ghoul2 instance attachments that may still exist exe-side
	trap->G2API_CleanEntAttachments();

	BG_InitAnimsets(); //clear it out

	B_InitAlloc(); //make sure everything is clean

	trap->SV_RegisterSharedMemory( gSharedBuffer.raw );

	//Load external vehicle data
	BG_VehicleLoadParms();

	trap->Print ("------- Game Initialization -------\n");
	trap->Print ("gamename: %s\n", GAMEVERSION);
	trap->Print ("gamedate: %s\n", SOURCE_DATE);

	srand( randomSeed );

	G_RegisterCvars();

	G_ProcessIPBans();

	G_InitMemory();

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.startTime = levelTime;

	level.follow1 = level.follow2 = -1;

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	level.snd_hack = G_SoundIndex("sound/player/hacking.wav");
	level.snd_medHealed = G_SoundIndex("sound/player/supp_healed.wav");
	level.snd_medSupplied = G_SoundIndex("sound/player/supp_supplied.wav");

	//trap->SP_RegisterServer("mp_svgame");

	if ( g_log.string[0] )
	{
		trap->FS_Open( g_log.string, &level.logFile, g_logSync.integer ? FS_APPEND_SYNC : FS_APPEND );
		if ( level.logFile )
			trap->Print( "Logging to %s\n", g_log.string );
		else
			trap->Print( "WARNING: Couldn't open logfile: %s\n", g_log.string );
	}
	else
		trap->Print( "Not logging game events to disk.\n" );

	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	G_LogPrintf( "------------------------------------------------------------\n" );
	G_LogPrintf( "InitGame: %s\n", serverinfo );

	if ( g_securityLog.integer )
	{
		if ( g_securityLog.integer == 1 )
			trap->FS_Open( SECURITY_LOG, &level.security.log, FS_APPEND );
		else if ( g_securityLog.integer == 2 )
			trap->FS_Open( SECURITY_LOG, &level.security.log, FS_APPEND_SYNC );

		if ( level.security.log )
			trap->Print( "Logging to "SECURITY_LOG"\n" );
		else
			trap->Print( "WARNING: Couldn't open logfile: "SECURITY_LOG"\n" );
	}
	else
		trap->Print( "Not logging security events to disk.\n" );


	G_LogWeaponInit();

	G_CacheGametype();

	G_InitWorldSession();

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	level.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = sv_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]) );
	level.clients = g_clients;

	// set client fields on player ents
	for ( i=0 ; i<level.maxclients ; i++ ) {
		g_entities[i].client = level.clients + i;
	}

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	for ( i=0 ; i<MAX_CLIENTS ; i++ ) {
		g_entities[i].classname = "clientslot";
	}

	// let the server system know where the entites are
	trap->LocateGameData( (sharedEntity_t *)level.gentities, level.num_entities, sizeof( gentity_t ),
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	//Load sabers.cfg data
	WP_SaberLoadParms();

	NPC_InitGame();

	TIMER_Clear();
	//
	//ICARUS INIT START

//	Com_Printf("------ ICARUS Initialization ------\n");

	trap->ICARUS_Init();

//	Com_Printf ("-----------------------------------\n");

	//ICARUS INIT END
	//

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	//make sure saber data is loaded before this! (so we can precache the appropriate hilts)
	InitSiegeMode();

	trap->Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
	G_CacheMapname( &mapname );
	trap->Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

	// navCalculatePaths	= ( trap->Nav_Load( mapname.string, ckSum.integer ) == qfalse );
	// zyk: commented line above. Was taking a lot of time to load some maps, example mp/duel7 and mp/siege_desert in FFA Mode
	// zyk: now it will always force calculating paths
	navCalculatePaths = qtrue;

	// zyk: getting mapname
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));
	strcpy(level.zykmapname, zyk_mapname);

	level.is_vjun3_map = qfalse;
	if (Q_stricmp(zyk_mapname, "vjun3") == 0)
	{ // zyk: fixing vjun3 map. It will not load protocol_imp npc to prevent exceeding the npc model limit (16) and crashing clients
		level.is_vjun3_map = qtrue;
	}

	if (Q_stricmp(zyk_mapname, "yavin1") == 0 || Q_stricmp(zyk_mapname, "yavin1b") == 0 || Q_stricmp(zyk_mapname, "yavin2") == 0 || 
		Q_stricmp(zyk_mapname, "t1_danger") == 0 || Q_stricmp(zyk_mapname, "t1_fatal") == 0 || Q_stricmp(zyk_mapname, "t1_inter") == 0 ||
		Q_stricmp(zyk_mapname, "t1_rail") == 0 || Q_stricmp(zyk_mapname, "t1_sour") == 0 || Q_stricmp(zyk_mapname, "t1_surprise") == 0 ||
		Q_stricmp(zyk_mapname, "hoth2") == 0 || Q_stricmp(zyk_mapname, "hoth3") == 0 || Q_stricmp(zyk_mapname, "t2_dpred") == 0 ||
		Q_stricmp(zyk_mapname, "t2_rancor") == 0 || Q_stricmp(zyk_mapname, "t2_rogue") == 0 || Q_stricmp(zyk_mapname, "t2_trip") == 0 ||
		Q_stricmp(zyk_mapname, "t2_wedge") == 0 || Q_stricmp(zyk_mapname, "vjun1") == 0 || Q_stricmp(zyk_mapname, "vjun2") == 0 ||
		Q_stricmp(zyk_mapname, "vjun3") == 0 || Q_stricmp(zyk_mapname, "t3_bounty") == 0 || Q_stricmp(zyk_mapname, "t3_byss") == 0 ||
		Q_stricmp(zyk_mapname, "t3_hevil") == 0 || Q_stricmp(zyk_mapname, "t3_rift") == 0 || Q_stricmp(zyk_mapname, "t3_stamp") == 0 ||
		Q_stricmp(zyk_mapname, "taspir1") == 0 || Q_stricmp(zyk_mapname, "taspir2") == 0 || Q_stricmp(zyk_mapname, "kor1") == 0 ||
		Q_stricmp(zyk_mapname, "kor2") == 0)
	{
		level.sp_map = qtrue;
	}

	level.quest_map = QUESTMAP_NONE;
	level.quest_debounce_timer = 0;
	level.quest_event_counter = 0;

	// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
	if (zyk_allow_quests.integer > 0)
	{
		if (Q_strncmp(zyk_mapname, "t1_inter", 9) == 0)
		{
			level.quest_map = QUESTMAP_HERO_HOUSE;
		}
		else if (Q_strncmp(zyk_mapname, "mp/siege_desert", 16) == 0 && g_gametype.integer == GT_FFA)
		{
			level.quest_map = QUESTMAP_DESERT_CITY;
		}
	}

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString(qfalse);

	if (level.gametype == GT_CTF)
	{ // zyk: maps that will now have support to CTF gametype (like some SP maps) must have the CTF flags placed before the G_CheckTeamItems function call
		if (Q_stricmp(zyk_mapname, "t1_fatal") == 0)
		{
			zyk_create_ctf_flag_spawn(-2366,-2561,4536,qtrue);
			zyk_create_ctf_flag_spawn(2484,1732,4656,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t1_rail") == 0)
		{
			zyk_create_ctf_flag_spawn(-2607,-4,24,qtrue);
			zyk_create_ctf_flag_spawn(23146,-3,216,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t1_surprise") == 0)
		{
			zyk_create_ctf_flag_spawn(1337,-6492,224,qtrue);
			zyk_create_ctf_flag_spawn(2098,4966,800,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t2_dpred") == 0)
		{
			zyk_create_ctf_flag_spawn(3,-3974,664,qtrue);
			zyk_create_ctf_flag_spawn(-701,126,24,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t2_trip") == 0)
		{
			zyk_create_ctf_flag_spawn(-20421,18244,1704,qtrue);
			zyk_create_ctf_flag_spawn(19903,-2638,1672,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t3_bounty") == 0)
		{
			zyk_create_ctf_flag_spawn(-7538,-545,-327,qtrue);
			zyk_create_ctf_flag_spawn(614,-509,344,qfalse);
		}
	}

	// general initialization
	G_FindTeams();

	// make sure we have flags for CTF, etc
	if( level.gametype >= GT_TEAM ) {
		G_CheckTeamItems();
	}
	else if ( level.gametype == GT_JEDIMASTER )
	{
		trap->SetConfigstring ( CS_CLIENT_JEDIMASTER, "-1" );
	}

	if (level.gametype == GT_POWERDUEL)
	{
		trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1|-1") );
	}
	else
	{
		trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1") );
	}
// nmckenzie: DUEL_HEALTH: Default.
	trap->SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("-1|-1|!") );
	trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("-1") );

	if (1)
	{ // zyk: registering all items because of entity system
		int item_it = 0;

		for (item_it = 0; item_it < bg_numItems; item_it++)
		{
			gitem_t *this_item = &bg_itemlist[item_it];
			if (this_item)
			{
				RegisterItem(this_item);
			}
		}
	}

	SaveRegisteredItems();

	//trap->Print ("-----------------------------------\n");

	if( level.gametype == GT_SINGLE_PLAYER || trap->Cvar_VariableIntegerValue( "com_buildScript" ) ) {
		G_ModelIndex( SP_PODIUM_MODEL );
		G_SoundIndex( "sound/player/gurp1.wav" );
		G_SoundIndex( "sound/player/gurp2.wav" );
	}

	if ( trap->Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( );
	} else {
		G_LoadArenas();
	}

	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
	{
		G_LogPrintf("Duel Tournament Begun: kill limit %d, win limit: %d\n", fraglimit.integer, duel_fraglimit.integer );
	}

	if ( navCalculatePaths )
	{//not loaded - need to calc paths
		navCalcPathTime = level.time + START_TIME_NAV_CALC;//make sure all ents are in and linked
	}
	else
	{//loaded
		//FIXME: if this is from a loadgame, it needs to be sure to write this
		//out whenever you do a savegame since the edges and routes are dynamic...
		//OR: always do a navigator.CheckBlockedEdges() on map startup after nav-load/calc-paths
		//navigator.pathsCalculated = qtrue;//just to be safe?  Does this get saved out?  No... assumed
		trap->Nav_SetPathsCalculated(qtrue);
		//need to do this, because combatpoint waypoints aren't saved out...?
		CP_FindCombatPointWaypoints();
		navCalcPathTime = 0;

		/*
		if ( g_eSavedGameJustLoaded == eNO )
		{//clear all the failed edges unless we just loaded the game (which would include failed edges)
			trap->Nav_ClearAllFailedEdges();
		}
		*/
		//No loading games in MP.
	}

	if (level.gametype == GT_SIEGE)
	{ //just get these configstrings registered now...
		while (i < MAX_CUSTOM_SIEGE_SOUNDS)
		{
			if (!bg_customSiegeSoundNames[i])
			{
				break;
			}
			G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
			i++;
		}
	}

	if ( level.gametype == GT_JEDIMASTER ) {
		gentity_t *ent = NULL;
		int i=0;
		for ( i=0, ent=g_entities; i<level.num_entities; i++, ent++ ) {
			if ( ent->isSaberEntity )
				break;
		}

		if ( i == level.num_entities ) {
			// no JM saber found. drop one at one of the player spawnpoints
			gentity_t *spawnpoint = SelectRandomDeathmatchSpawnPoint( qfalse );

			if( !spawnpoint ) {
				trap->Error( ERR_DROP, "Couldn't find an FFA spawnpoint to drop the jedimaster saber at!\n" );
				return;
			}

			ent = G_Spawn();
			G_SetOrigin( ent, spawnpoint->s.origin );
			SP_info_jedimaster_start( ent );
		}
	}

	// zyk: initializing race mode
	level.race_mode = 0;

	// zyk: initializing custom quest values
	level.custom_quest_map = -1;
	level.zyk_custom_quest_effect_id = -1;

	// zyk: initializing bounty_quest_target_id value
	level.bounty_quest_target_id = 0;
	level.bounty_quest_choose_target = qtrue;

	level.map_music_reset_timer = 0;

	level.voting_player = -1;

	level.server_empty_change_map_timer = 0;
	level.num_fully_connected_clients = 0;

	// zyk: initializing Duel Tournament variables
	level.duel_tournament_mode = 0;
	level.duel_tournament_paused = qfalse;
	level.duelists_quantity = 0;
	level.duel_matches_quantity = 0;
	level.duel_matches_done = 0;
	level.duel_tournament_rounds = 0;
	level.duel_tournament_timer = 0;
	level.duelist_1_id = -1;
	level.duelist_2_id = -1;
	level.duelist_1_ally_id = -1;
	level.duelist_2_ally_id = -1;
	level.duel_tournament_model_id = -1;
	level.duel_arena_loaded = qfalse;
	level.duel_leaderboard_step = 0;

	// zyk: initializing Sniper Battle variables
	level.sniper_mode = 0;
	level.sniper_mode_quantity = 0;

	// zyk: initializing Melee Battle variables
	level.melee_mode = 0;
	level.melee_model_id = -1;
	level.melee_mode_timer = 0;
	level.melee_mode_quantity = 0;
	level.melee_arena_loaded = qfalse;

	// zyk: initializing RPG LMS variables
	level.rpg_lms_mode = 0;
	level.rpg_lms_quantity = 0;

	level.last_spawned_entity = NULL;

	level.ent_origin_set = qfalse;

	level.spawned_quest_stuff = qfalse;

	level.load_entities_timer = 0;
	strcpy(level.load_entities_file,"");

	if (1)
	{
		FILE *quest_file = NULL;
		char content[8192];
		int zyk_iterator = 0;

		strcpy(content, "");

		for (zyk_iterator = 0; zyk_iterator < MAX_CLIENTS; zyk_iterator++)
		{ // zyk: initializing duelist scores
			level.duel_players[zyk_iterator] = -1;
			level.sniper_players[zyk_iterator] = -1;
			level.melee_players[zyk_iterator] = -1;
			level.rpg_lms_players[zyk_iterator] = -1;

			// zyk: initializing ally table
			level.duel_allies[zyk_iterator] = -1;
		}

		for (zyk_iterator = 0; zyk_iterator < MAX_DUEL_MATCHES; zyk_iterator++)
		{ // zyk: initializing duel matches
			level.duel_matches[zyk_iterator][0] = -1;
			level.duel_matches[zyk_iterator][1] = -1;
			level.duel_matches[zyk_iterator][2] = 0;
			level.duel_matches[zyk_iterator][3] = 0;
		}

		for (zyk_iterator = 0; zyk_iterator < MAX_RACERS; zyk_iterator++)
		{ // zyk: initializing race vehicle ids
			level.race_mode_vehicle[zyk_iterator] = -1;
		}

		for (zyk_iterator = 0; zyk_iterator < ENTITYNUM_MAX_NORMAL; zyk_iterator++)
		{ // zyk: initializing special power variables
			level.special_power_effects[zyk_iterator] = -1;
			level.special_power_effects_timer[zyk_iterator] = 0;
		}

		for (zyk_iterator = 0; zyk_iterator < MAX_CLIENTS; zyk_iterator++)
		{
			level.read_screen_message[zyk_iterator] = qfalse;
			level.screen_message_timer[zyk_iterator] = 0;
			level.ignored_players[zyk_iterator][0] = 0;
			level.ignored_players[zyk_iterator][1] = 0;
		}

		for (zyk_iterator = 0; zyk_iterator < MAX_CUSTOM_QUESTS; zyk_iterator++)
		{ // zyk: initializing custom quest values
			level.zyk_custom_quest_mission_count[zyk_iterator] = -1;

			quest_file = fopen(va("zykmod/customquests/%d.txt", zyk_iterator), "r");
			if (quest_file)
			{
				// zyk: initializes amount of quest missions
				level.zyk_custom_quest_mission_count[zyk_iterator] = 0;

				// zyk: reading the first line, which contains the main quest fields
				if (fgets(content, sizeof(content), quest_file) != NULL)
				{
					int j = 0;
					int k = 0; // zyk: current spawn string position
					char field[256];

					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					while (content[k] != '\0')
					{
						int l = 0;

						// zyk: getting the field
						while (content[k] != ';')
						{
							field[l] = content[k];

							l++;
							k++;
						}
						field[l] = '\0';
						k++;

						level.zyk_custom_quest_main_fields[zyk_iterator][j] = G_NewString(field);

						j++;
					}
				}

				while (fgets(content, sizeof(content), quest_file) != NULL)
				{
					int j = 0; // zyk: the current key/value being used
					int k = 0; // zyk: current spawn string position

					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					while (content[k] != '\0')
					{
						int l = 0;
						char zyk_key[256];
						char zyk_value[256];

						// zyk: getting the key
						while (content[k] != ';')
						{
							zyk_key[l] = content[k];

							l++;
							k++;
						}
						zyk_key[l] = '\0';
						k++;

						// zyk: getting the value
						l = 0;
						while (content[k] != ';')
						{
							zyk_value[l] = content[k];

							l++;
							k++;
						}
						zyk_value[l] = '\0';
						k++;

						// zyk: copying the key and value to the fields array
						level.zyk_custom_quest_missions[zyk_iterator][level.zyk_custom_quest_mission_count[zyk_iterator]][j] = G_NewString(zyk_key);
						level.zyk_custom_quest_missions[zyk_iterator][level.zyk_custom_quest_mission_count[zyk_iterator]][j + 1] = G_NewString(zyk_value);

						j += 2;
					}

					level.zyk_custom_quest_mission_values_count[zyk_iterator][level.zyk_custom_quest_mission_count[zyk_iterator]] = j;
					level.zyk_custom_quest_mission_count[zyk_iterator]++;
				}

				fclose(quest_file);
			}
		}
	}

	// zyk: added this fix for SP maps
	if (Q_stricmp(zyk_mapname, "academy1") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy2") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy3") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy4") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy5") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy6") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);

		// zyk: hangar spawn points
		zyk_create_info_player_deathmatch(-23,458,-486,0);
		zyk_create_info_player_deathmatch(2053,3401,-486,-90);
		zyk_create_info_player_deathmatch(4870,455,-486,-179);
	}
	else if (Q_stricmp(zyk_mapname, "yavin1") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(472,-4833,437,74);
		zyk_create_info_player_deathmatch(-167,-4046,480,0);
	}
	else if (Q_stricmp(zyk_mapname, "yavin1b") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "door1") == 0)
			{
				fix_sp_func_door(ent);
			}
			else if (Q_stricmp( ent->classname, "trigger_hurt") == 0 && Q_stricmp( ent->targetname, "tree_hurt_trigger") != 0)
			{ // zyk: trigger_hurt entity of the bridge area
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(472,-4833,437,74);
		zyk_create_info_player_deathmatch(-167,-4046,480,0);
	}
	else if (Q_stricmp(zyk_mapname, "yavin2") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "t530") == 0 || Q_stricmp( ent->targetname, "Putz_door") == 0 || Q_stricmp( ent->targetname, "afterdroid_door") == 0 || Q_stricmp( ent->targetname, "pit_door") == 0 || Q_stricmp( ent->targetname, "door1") == 0)
			{
				fix_sp_func_door(ent);
			}
			else if (Q_stricmp( ent->classname, "trigger_hurt") == 0 && ent->spawnflags == 62)
			{ // zyk: removes the trigger hurt entity of the second bridge
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(2516,-5593,89,-179);
		zyk_create_info_player_deathmatch(2516,-5443,89,-179);
	}
	else if (Q_stricmp(zyk_mapname, "hoth2") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-2114,10195,1027,-14);
		zyk_create_info_player_deathmatch(-1808,9640,982,-17);
	}
	else if (Q_stricmp(zyk_mapname, "hoth3") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
			if (i == 232 || i == 233)
			{ // zyk: fixing the final door
				ent->targetname = NULL;
				zyk_main_set_entity_field(ent, "targetname", "zykremovekey");

				zyk_main_spawn_entity(ent);
			}
		}

		zyk_create_info_player_deathmatch(-1908,562,992,-90);
		zyk_create_info_player_deathmatch(-1907,356,801,-90);
	}
	else if (Q_stricmp(zyk_mapname, "t1_danger") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->classname, "NPC_Monster_Sand_Creature") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-3705,-3362,1121,90);
		zyk_create_info_player_deathmatch(-3705,-2993,1121,90);
	}
	else if (Q_stricmp(zyk_mapname, "t1_fatal") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];

			if (Q_stricmp(ent->targetname, "door_trap") == 0)
			{ // zyk: fixing this door so it will not lock
				fix_sp_func_door(ent);
			}
				
			if (Q_stricmp(ent->targetname, "lobbydoor1") == 0 || Q_stricmp(ent->targetname, "lobbydoor2") == 0 || 
				Q_stricmp(ent->targetname, "t7708018") == 0 || Q_stricmp(ent->targetname, "t7708017") == 0)
			{ // zyk: fixing these doors so they will not lock
				GlobalUse(ent, ent, ent);
			}

			if (i == 443)
			{ // zyk: trigger_hurt at the spawn area
				G_FreeEntity( ent );
			}

		}
		zyk_create_info_player_deathmatch(-1563,-4241,4569,-157);
		zyk_create_info_player_deathmatch(-1135,-4303,4569,179);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-3083,-2683,4696,-90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-2371,-3325,4536,90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-1726,-2957,4536,90,qtrue,qtrue);

			zyk_create_ctf_player_spawn(1277,2947,4540,-45,qfalse,qtrue);
			zyk_create_ctf_player_spawn(3740,482,4536,135,qfalse,qtrue);
			zyk_create_ctf_player_spawn(2489,1451,4536,135,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-3083,-2683,4696,-90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-2371,-3325,4536,90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-1726,-2957,4536,90,qtrue,qfalse);

			zyk_create_ctf_player_spawn(1277,2947,4540,-45,qfalse,qfalse);
			zyk_create_ctf_player_spawn(3740,482,4536,135,qfalse,qfalse);
			zyk_create_ctf_player_spawn(2489,1451,4536,135,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t1_inter") == 0)
	{
		zyk_create_info_player_deathmatch(-65,-686,89,90);
		zyk_create_info_player_deathmatch(56,-686,89,90);
		zyk_create_info_player_deathmatch(-329, -686, 89, 90);
		zyk_create_info_player_deathmatch(202, -686, 89, 90);
		zyk_create_info_player_deathmatch(-411, -491, 89, 0);
		zyk_create_info_player_deathmatch(280, -491, 89, 179);
		zyk_create_info_player_deathmatch(-411, -252, 89, 0);
		zyk_create_info_player_deathmatch(280, -252, 89, 179);
	}
	else if (Q_stricmp(zyk_mapname, "t1_rail") == 0)
	{
		zyk_create_info_player_deathmatch(-3135,1,33,0);
		zyk_create_info_player_deathmatch(-3135,197,25,0);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-2569,-2,25,179,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-1632,257,136,-90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-1743,0,500,0,qtrue,qtrue);

			zyk_create_ctf_player_spawn(22760,-128,152,90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(22866,0,440,-179,qfalse,qtrue);
			zyk_create_ctf_player_spawn(21102,2,464,179,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-2569,-2,25,179,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-1632,257,136,-90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-1743,0,500,0,qtrue,qfalse);

			zyk_create_ctf_player_spawn(22760,-128,152,90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(22866,0,440,-179,qfalse,qfalse);
			zyk_create_ctf_player_spawn(21102,2,464,179,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t1_sour") == 0)
	{
		zyk_create_info_player_deathmatch(9828,-5521,153,90);
		zyk_create_info_player_deathmatch(9845,-5262,153,153);
	}
	else if (Q_stricmp(zyk_mapname, "t1_surprise") == 0)
	{
		int i = 0;
		gentity_t *ent;
		qboolean found_bugged_switch = qfalse;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];

			if (Q_stricmp( ent->targetname, "fire_hurt") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "droid_door") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->targetname, "tube_door") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (found_bugged_switch == qfalse && Q_stricmp( ent->classname, "misc_model_breakable") == 0 && Q_stricmp( ent->model, "models/map_objects/desert/switch3.md3") == 0)
			{
				G_FreeEntity(ent);
				found_bugged_switch = qtrue;
			}
			if (Q_stricmp( ent->classname, "func_static") == 0 && (int)ent->s.origin[0] == 3064 && (int)ent->s.origin[1] == 5040 && (int)ent->s.origin[2] == 892)
			{ // zyk: elevator inside sand crawler near the wall fire
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->classname, "func_door") == 0 && i > 200 && Q_stricmp( ent->model, "*63") == 0)
			{ // zyk: tube door in which the droid goes in SP
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(1913,-6151,222,153);
		zyk_create_info_player_deathmatch(1921,-5812,222,-179);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(1948,-6020,222,138,qtrue,qtrue);
			zyk_create_ctf_player_spawn(1994,-4597,908,19,qtrue,qtrue);
			zyk_create_ctf_player_spawn(404,-4521,249,-21,qtrue,qtrue);

			zyk_create_ctf_player_spawn(2341,4599,1056,83,qfalse,qtrue);
			zyk_create_ctf_player_spawn(1901,5425,916,-177,qfalse,qtrue);
			zyk_create_ctf_player_spawn(918,3856,944,0,qfalse,qtrue);

			zyk_create_ctf_player_spawn(1948,-6020,222,138,qtrue,qfalse);
			zyk_create_ctf_player_spawn(1994,-4597,908,19,qtrue,qfalse);
			zyk_create_ctf_player_spawn(404,-4521,249,-21,qtrue,qfalse);

			zyk_create_ctf_player_spawn(2341,4599,1056,83,qfalse,qfalse);
			zyk_create_ctf_player_spawn(1901,5425,916,-177,qfalse,qfalse);
			zyk_create_ctf_player_spawn(918,3856,944,0,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t2_rancor") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			
			if (Q_stricmp( ent->targetname, "t857") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->targetname, "Kill_Brush_Canyon") == 0)
			{ // zyk: trigger_hurt at the spawn area
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(-898,1178,1718,90);
		zyk_create_info_player_deathmatch(-898,1032,1718,90);
	}
	else if (Q_stricmp(zyk_mapname, "t2_rogue") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "t475") == 0)
			{ // zyk: remove the invisible wall at the end of the bridge at start
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "field_counter1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "field_counter2") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "field_counter3") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
			if (Q_stricmp(ent->targetname, "ractoroomdoor") == 0)
			{ // zyk: remove office door
				G_FreeEntity(ent);
			}
			if (i == 142)
			{ // zyk: remove the elevator
				G_FreeEntity(ent);
			}
			if (i == 166)
			{ // zyk: remove the elevator button
				G_FreeEntity(ent);
			}
		}

		// zyk: adding new elevator and buttons that work properly
		ent = G_Spawn();

		zyk_main_set_entity_field(ent, "classname", "func_plat");
		zyk_main_set_entity_field(ent, "spawnflags", "4096");
		zyk_main_set_entity_field(ent, "targetname", "zyk_lift_1");
		zyk_main_set_entity_field(ent, "lip", "8");
		zyk_main_set_entity_field(ent, "height", "1280");
		zyk_main_set_entity_field(ent, "speed", "200");
		zyk_main_set_entity_field(ent, "model", "*38");
		zyk_main_set_entity_field(ent, "origin", "2848 2144 700");
		zyk_main_set_entity_field(ent, "soundSet", "platform");

		zyk_main_spawn_entity(ent);

		ent = G_Spawn();

		zyk_main_set_entity_field(ent, "classname", "trigger_multiple");
		zyk_main_set_entity_field(ent, "spawnflags", "4");
		zyk_main_set_entity_field(ent, "target", "zyk_lift_1");
		zyk_main_set_entity_field(ent, "origin", "2664 2000 728");
		zyk_main_set_entity_field(ent, "mins", "-32 -32 -32");
		zyk_main_set_entity_field(ent, "maxs", "32 32 32");
		zyk_main_set_entity_field(ent, "wait", "1");
		zyk_main_set_entity_field(ent, "delay", "2");

		zyk_main_spawn_entity(ent);

		ent = G_Spawn();

		zyk_main_set_entity_field(ent, "classname", "trigger_multiple");
		zyk_main_set_entity_field(ent, "spawnflags", "4");
		zyk_main_set_entity_field(ent, "target", "zyk_lift_1");
		zyk_main_set_entity_field(ent, "origin", "2577 2023 -551");
		zyk_main_set_entity_field(ent, "mins", "-32 -32 -32");
		zyk_main_set_entity_field(ent, "maxs", "32 32 32");
		zyk_main_set_entity_field(ent, "wait", "1");
		zyk_main_set_entity_field(ent, "delay", "2");

		zyk_main_spawn_entity(ent);

		zyk_create_info_player_deathmatch(1974,-1983,-550,90);
		zyk_create_info_player_deathmatch(1779,-1983,-550,90);
	}
	else if (Q_stricmp(zyk_mapname, "t2_trip") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "t546") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "end_level") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "cin_door") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "endJaden") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "endJaden2") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "endswoop") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->classname, "func_door") == 0 && i > 200)
			{ // zyk: door after the teleports of the race mode
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "t547") == 0)
			{ // zyk: removes swoop at end of map. Must be removed to prevent bug in racemode
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(-5698,-22304,1705,90);
		zyk_create_info_player_deathmatch(-5433,-22328,1705,90);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-20705,18794,1704,0,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-20729,17692,1704,0,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-20204,18736,1503,0,qtrue,qtrue);

			zyk_create_ctf_player_spawn(20494,-2922,1672,90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(19321,-2910,1672,90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(19428,-2404,1470,90,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-20705,18794,1704,0,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-20729,17692,1704,0,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-20204,18736,1503,0,qtrue,qfalse);

			zyk_create_ctf_player_spawn(20494,-2922,1672,90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(19321,-2910,1672,90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(19428,-2404,1470,90,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t2_wedge") == 0)
	{
		zyk_create_info_player_deathmatch(6328,539,-110,-178);
		zyk_create_info_player_deathmatch(6332,743,-110,-178);
	}
	else if (Q_stricmp(zyk_mapname, "t2_dpred") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "prisonshield1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "t556") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->target, "field_counter1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "t62241") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "t62243") == 0)
			{
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-2152,-3885,-134,90);
		zyk_create_info_player_deathmatch(-2152,-3944,-134,90);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(0,-4640,664,90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(485,-3721,632,-179,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-212,-3325,656,-179,qtrue,qtrue);

			zyk_create_ctf_player_spawn(0,125,24,-90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(-1242,128,24,0,qfalse,qtrue);
			zyk_create_ctf_player_spawn(369,67,296,-179,qfalse,qtrue);

			zyk_create_ctf_player_spawn(0,-4640,664,90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(485,-3721,632,-179,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-212,-3325,656,-179,qtrue,qfalse);

			zyk_create_ctf_player_spawn(0,125,24,-90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(-1242,128,24,0,qfalse,qfalse);
			zyk_create_ctf_player_spawn(369,67,296,-179,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "vjun1") == 0)
	{
		int i = 0;
		gentity_t *ent;
		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (i == 123 || i == 124)
			{ // zyk: removing tie fighter misc_model_breakable entities to prevent client crashes
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(-6897,7035,857,-90);
		zyk_create_info_player_deathmatch(-7271,7034,857,-90);
	}
	else if (Q_stricmp(zyk_mapname, "vjun2") == 0)
	{
		zyk_create_info_player_deathmatch(-831,166,217,90);
		zyk_create_info_player_deathmatch(-700,166,217,90);
	}
	else if (Q_stricmp(zyk_mapname, "vjun3") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-8272,-391,1433,179);
		zyk_create_info_player_deathmatch(-8375,-722,1433,179);
	}
	else if (Q_stricmp(zyk_mapname, "t3_hevil") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (i == 42)
			{
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(512,-2747,-742,90);
		zyk_create_info_player_deathmatch(872,-2445,-742,108);
	}
	else if (Q_stricmp(zyk_mapname, "t3_bounty") == 0)
	{
		zyk_create_info_player_deathmatch(-3721,-726,73,75);
		zyk_create_info_player_deathmatch(-3198,-706,73,90);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-7740,-543,-263,0,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-8470,-210,24,90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-7999,-709,-7,132,qtrue,qtrue);

			zyk_create_ctf_player_spawn(616,-978,344,0,qfalse,qtrue);
			zyk_create_ctf_player_spawn(595,482,360,-90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(1242,255,36,-179,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-7740,-543,-263,0,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-8470,-210,24,90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-7999,-709,-7,132,qtrue,qfalse);

			zyk_create_ctf_player_spawn(616,-978,344,0,qfalse,qfalse);
			zyk_create_ctf_player_spawn(595,482,360,-90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(1242,255,36,-179,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t3_byss") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			
			if (Q_stricmp( ent->targetname, "wall_door1") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->target, "field_counter1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave1_tie1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave1_tie2") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave1_tie3") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave2_tie1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave2_tie2") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave2_tie3") == 0)
			{
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(968,111,25,-90);
		zyk_create_info_player_deathmatch(624,563,25,-90);
	}
	else if (Q_stricmp(zyk_mapname, "t3_rift") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "fakewall1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "t778") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "t779") == 0)
			{
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(2195,7611,4380,-90);
		zyk_create_info_player_deathmatch(2305,7640,4380,-90);
	}
	else if (Q_stricmp(zyk_mapname, "t3_stamp") == 0)
	{
		zyk_create_info_player_deathmatch(1208,445,89,179);
		zyk_create_info_player_deathmatch(1208,510,89,179);
	}
	else if (Q_stricmp(zyk_mapname, "taspir1") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp(ent->targetname, "t278") == 0)
			{
				G_FreeEntity(ent);
			}
			if (Q_stricmp(ent->targetname, "bldg2_ext_door") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp(ent->targetname, "end_level") == 0)
			{
				G_FreeEntity(ent);
			}
		}
		zyk_create_info_player_deathmatch(-1609, -1792, 649, 112);
		zyk_create_info_player_deathmatch(-1791, -1838, 649, 90);
	}
	else if (Q_stricmp(zyk_mapname, "taspir2") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp(ent->targetname, "force_field") == 0)
			{
				G_FreeEntity(ent);
			}
			if (Q_stricmp(ent->targetname, "kill_toggle") == 0)
			{
				G_FreeEntity(ent);
			}
		}

		zyk_create_info_player_deathmatch(286, -2859, 345, 92);
		zyk_create_info_player_deathmatch(190, -2834, 345, 90);
	}
	else if (Q_stricmp(zyk_mapname, "kor1") == 0)
	{
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (i >= 418 && i <= 422)
			{ // zyk: remove part of the door on the floor on the first puzzle
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(190,632,-1006,-89);
		zyk_create_info_player_deathmatch(-249,952,-934,-89);
	}
	else if (Q_stricmp(zyk_mapname, "kor2") == 0)
	{
		zyk_create_info_player_deathmatch(2977,3137,-2526,0);
		zyk_create_info_player_deathmatch(3072,2992,-2526,0);
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_korriban") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove some entities
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "cyrstalsinplace") == 0)
			{
				G_FreeEntity( ent );
			}
			if (i >= 236 && i <= 238)
			{ // zyk: removing the trigger_hurt from the lava in Guardian of Universe arena
				G_FreeEntity( ent );
			}
		}
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_desert") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove the shield in the final part
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];

			if (Q_stricmp(ent->targetname, "rebel_obj_2_doors") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp(ent->targetname, "shield") == 0)
			{
				G_FreeEntity(ent);
			}
			if (i == 362 || i == 358 || i == 359)
			{
				G_FreeEntity(ent);
			}
			if (Q_stricmp(ent->targetname, "gatedestroy_doors") == 0)
			{
				G_FreeEntity(ent);
			}
			if (i >= 153 && i <= 160)
			{
				G_FreeEntity(ent);
			}
			if (Q_stricmp(ent->classname, "info_player_deathmatch") == 0)
			{
				G_FreeEntity(ent);
			}
		}

		if (level.quest_map == QUESTMAP_DESERT_CITY)
		{ // zyk: loading quest stuff
			zyk_create_info_player_deathmatch(12500, 32, -486, 179);
			zyk_create_info_player_deathmatch(12500, -93, -486, 179);
			zyk_create_info_player_deathmatch(12500, -235, -486, 179);
			zyk_create_info_player_deathmatch(12500, -419, -486, 179);
			zyk_create_info_player_deathmatch(12500, -599, -486, 179);
			zyk_create_info_player_deathmatch(12500, -748, -486, 179);
			zyk_create_info_player_deathmatch(12500, -911, -486, 179);
			zyk_create_info_player_deathmatch(12500, -1091, -486, 179);
		}
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_destroyer") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove the shield at the destroyer
		int i = 0;
		gentity_t *ent;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "ubershield") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->classname, "info_player_deathmatch") == 0)
			{
				G_FreeEntity( ent );
			}
		}
		// zyk: rebel area spawnpoints
		zyk_create_info_player_deathmatch(31729,-32219,33305,90);
		zyk_create_info_player_deathmatch(31229,-32219,33305,90);
		zyk_create_info_player_deathmatch(30729,-32219,33305,90);
		zyk_create_info_player_deathmatch(32229,-32219,33305,90);
		zyk_create_info_player_deathmatch(32729,-32219,33305,90);
		zyk_create_info_player_deathmatch(31729,-32019,33305,90);

		// zyk: imperial area spawnpoints
		zyk_create_info_player_deathmatch(2545,8987,1817,-90);
		zyk_create_info_player_deathmatch(2345,8987,1817,-90);
		zyk_create_info_player_deathmatch(2745,8987,1817,-90);

		zyk_create_info_player_deathmatch(2597,7403,1817,90);
		zyk_create_info_player_deathmatch(2397,7403,1817,90);
		zyk_create_info_player_deathmatch(2797,7403,1817,90);
	}

	level.sp_map = qfalse;

	if (Q_stricmp(level.default_map_music, "") == 0)
	{ // zyk: if the default map music is empty (the map has no music) then set a default music
		strcpy(level.default_map_music, "music/yavin2/yavtemp2_explore.mp3");
	}

	// zyk: loading quest map stuff, like npcs, models, etc
	zyk_spawn_quest_stuff(qfalse);

	zyk_create_dir(va("entities/%s", zyk_mapname));

	// zyk: loading entities set as default (Entity System)
	zyk_entities_file = fopen(va("zykmod/entities/%s/default.txt",zyk_mapname),"r");

	if (zyk_entities_file != NULL)
	{ // zyk: default file exists. Load entities from it
		fclose(zyk_entities_file);

		// zyk: cleaning entities. Only the ones from the file will be in the map. Do not remove CTF flags
		for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
		{
			gentity_t *target_ent = &g_entities[i];

			if (target_ent && Q_stricmp(target_ent->classname, "team_CTF_redflag") != 0 && Q_stricmp(target_ent->classname, "team_CTF_blueflag") != 0)
				G_FreeEntity( target_ent );
		}

		strcpy(level.load_entities_file, va("zykmod/entities/%s/default.txt",zyk_mapname));

		level.load_entities_timer = level.time + 1050;
	}

	// zyk: loading default remaps
	zyk_remap_file = fopen(va("zykmod/remaps/%s/default.txt",zyk_mapname),"r");

	if (zyk_remap_file != NULL)
	{
		char old_shader[128];
		char new_shader[128];
		char time_offset[128];

		strcpy(old_shader,"");
		strcpy(new_shader,"");
		strcpy(time_offset,"");

		while(fscanf(zyk_remap_file,"%s",old_shader) != EOF)
		{
			fscanf(zyk_remap_file,"%s",new_shader);
			fscanf(zyk_remap_file,"%s",time_offset);

			AddRemap(G_NewString(old_shader), G_NewString(new_shader), atof(time_offset));
		}
		
		fclose(zyk_remap_file);

		trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
	}

	// zyk: loading duel arena, if this map has one
	zyk_duel_arena_file = fopen(va("zykmod/duelarena/%s/origin.txt", zyk_mapname), "r");
	if (zyk_duel_arena_file != NULL)
	{
		char duel_arena_content[16];

		strcpy(duel_arena_content, "");

		fscanf(zyk_duel_arena_file, "%s", duel_arena_content);
		level.duel_tournament_origin[0] = atoi(duel_arena_content);

		fscanf(zyk_duel_arena_file, "%s", duel_arena_content);
		level.duel_tournament_origin[1] = atoi(duel_arena_content);

		fscanf(zyk_duel_arena_file, "%s", duel_arena_content);
		level.duel_tournament_origin[2] = atoi(duel_arena_content);

		fclose(zyk_duel_arena_file);

		level.duel_arena_loaded = qtrue;
	}

	// zyk: loading melee arena, if this map has one
	zyk_melee_arena_file = fopen(va("zykmod/meleearena/%s/origin.txt", zyk_mapname), "r");
	if (zyk_melee_arena_file != NULL)
	{
		char melee_arena_content[16];

		strcpy(melee_arena_content, "");

		fscanf(zyk_melee_arena_file, "%s", melee_arena_content);
		level.melee_mode_origin[0] = atoi(melee_arena_content);

		fscanf(zyk_melee_arena_file, "%s", melee_arena_content);
		level.melee_mode_origin[1] = atoi(melee_arena_content);

		fscanf(zyk_melee_arena_file, "%s", melee_arena_content);
		level.melee_mode_origin[2] = atoi(melee_arena_content);

		fclose(zyk_melee_arena_file);

		level.melee_arena_loaded = qtrue;
	}

	// zyk: setting map as a custom quest map if it has a mission
	load_custom_quest_mission();
}



/*
=================
G_ShutdownGame
=================
*/
void G_ShutdownGame( int restart ) {
	int i = 0;
	gentity_t *ent;

//	trap->Print ("==== ShutdownGame ====\n");

	G_CleanAllFakeClients(); //get rid of dynamically allocated fake client structs.

	BG_ClearAnimsets(); //free all dynamic allocations made through the engine

//	Com_Printf("... Gameside GHOUL2 Cleanup\n");
	while (i < MAX_GENTITIES)
	{ //clean up all the ghoul2 instances
		ent = &g_entities[i];

		if (ent->ghoul2 && trap->G2API_HaveWeGhoul2Models(ent->ghoul2))
		{
			trap->G2API_CleanGhoul2Models(&ent->ghoul2);
			ent->ghoul2 = NULL;
		}
		if (ent->client)
		{
			int j = 0;

			while (j < MAX_SABERS)
			{
				if (ent->client->weaponGhoul2[j] && trap->G2API_HaveWeGhoul2Models(ent->client->weaponGhoul2[j]))
				{
					trap->G2API_CleanGhoul2Models(&ent->client->weaponGhoul2[j]);
				}
				j++;
			}
		}
		i++;
	}
	if (g2SaberInstance && trap->G2API_HaveWeGhoul2Models(g2SaberInstance))
	{
		trap->G2API_CleanGhoul2Models(&g2SaberInstance);
		g2SaberInstance = NULL;
	}
	if (precachedKyle && trap->G2API_HaveWeGhoul2Models(precachedKyle))
	{
		trap->G2API_CleanGhoul2Models(&precachedKyle);
		precachedKyle = NULL;
	}

//	Com_Printf ("... ICARUS_Shutdown\n");
	trap->ICARUS_Shutdown ();	//Shut ICARUS down

//	Com_Printf ("... Reference Tags Cleared\n");
	TAG_Init();	//Clear the reference tags

	G_LogWeaponOutput();

	if ( level.logFile ) {
		G_LogPrintf( "ShutdownGame:\n------------------------------------------------------------\n" );
		trap->FS_Close( level.logFile );
		level.logFile = 0;
	}

	if ( level.security.log )
	{
		G_SecurityLogPrintf( "ShutdownGame\n\n" );
		trap->FS_Close( level.security.log );
		level.security.log = 0;
	}

	// write all the client session data so we can get it back
	G_WriteSessionData();

	trap->ROFF_Clean();

	if ( trap->Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}

	B_CleanupAlloc(); //clean up all allocations made with B_Alloc
}

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 2 ) {
		return;
	}

	// never change during intermission
//	if ( level.intermissiontime ) {
//		return;
//	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if (!g_allowHighPingDuelist.integer && client->ps.ping >= 999)
		{ //don't add people who are lagging out if cvar is not set to allow it.
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ||
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorNum > nextInLine->sess.spectatorNum )
			nextInLine = client;
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );
}

/*
=======================
AddTournamentQueue

Add client to end of tournament queue
=======================
*/

void AddTournamentQueue( gclient_t *client )
{
	int index;
	gclient_t *curclient;

	for( index = 0; index < level.maxclients; index++ )
	{
		curclient = &level.clients[index];

		if ( curclient->pers.connected != CON_DISCONNECTED )
		{
			if ( curclient == client )
				curclient->sess.spectatorNum = 0;
			else if ( curclient->sess.sessionTeam == TEAM_SPECTATOR )
				curclient->sess.spectatorNum++;
		}
	}
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[1];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

void G_PowerDuelCount(int *loners, int *doubles, qboolean countSpec)
{
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS)
	{
		cl = g_entities[i].client;

		if (g_entities[i].inuse && cl && (countSpec || cl->sess.sessionTeam != TEAM_SPECTATOR))
		{
			if (cl->sess.duelTeam == DUELTEAM_LONE)
			{
				(*loners)++;
			}
			else if (cl->sess.duelTeam == DUELTEAM_DOUBLE)
			{
				(*doubles)++;
			}
		}
		i++;
	}
}

qboolean g_duelAssigning = qfalse;
void AddPowerDuelPlayers( void )
{
	int			i;
	int			loners = 0;
	int			doubles = 0;
	int			nonspecLoners = 0;
	int			nonspecDoubles = 0;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 3 )
	{
		return;
	}

	nextInLine = NULL;

	G_PowerDuelCount(&nonspecLoners, &nonspecDoubles, qfalse);
	if (nonspecLoners >= 1 && nonspecDoubles >= 2)
	{ //we have enough people, stop
		return;
	}

	//Could be written faster, but it's not enough to care I suppose.
	G_PowerDuelCount(&loners, &doubles, qtrue);

	if (loners < 1 || doubles < 2)
	{ //don't bother trying to spawn anyone yet if the balance is not even set up between spectators
		return;
	}

	//Count again, with only in-game clients in mind.
	loners = nonspecLoners;
	doubles = nonspecDoubles;
//	G_PowerDuelCount(&loners, &doubles, qfalse);

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_FREE)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
		{
			continue;
		}

		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ||
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorNum > nextInLine->sess.spectatorNum )
			nextInLine = client;
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );

	//Call recursively until everyone is in
	AddPowerDuelPlayers();
}

qboolean g_dontFrickinCheck = qfalse;

void RemovePowerDuelLosers(void)
{
	int remClients[3];
	int remNum = 0;
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS && remNum < 3)
	{
		//cl = &level.clients[level.sortedClients[i]];
		cl = &level.clients[i];

		if (cl->pers.connected == CON_CONNECTED)
		{
			if ((cl->ps.stats[STAT_HEALTH] <= 0 || cl->iAmALoser) &&
				(cl->sess.sessionTeam != TEAM_SPECTATOR || cl->iAmALoser))
			{ //he was dead or he was spectating as a loser
                remClients[remNum] = i;
				remNum++;
			}
		}

		i++;
	}

	if (!remNum)
	{ //Time ran out or something? Oh well, just remove the main guy.
		remClients[remNum] = level.sortedClients[0];
		remNum++;
	}

	i = 0;
	while (i < remNum)
	{ //set them all to spectator
		SetTeam( &g_entities[ remClients[i] ], "s" );
		i++;
	}

	g_dontFrickinCheck = qfalse;

	//recalculate stuff now that we have reset teams.
	CalculateRanks();
}

void RemoveDuelDrawLoser(void)
{
	int clFirst = 0;
	int clSec = 0;
	int clFailure = 0;

	if ( level.clients[ level.sortedClients[0] ].pers.connected != CON_CONNECTED )
	{
		return;
	}
	if ( level.clients[ level.sortedClients[1] ].pers.connected != CON_CONNECTED )
	{
		return;
	}

	clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
	clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];

	if (clFirst > clSec)
	{
		clFailure = 1;
	}
	else if (clSec > clFirst)
	{
		clFailure = 0;
	}
	else
	{
		clFailure = 2;
	}

	if (clFailure != 2)
	{
		SetTeam( &g_entities[ level.sortedClients[clFailure] ], "s" );
	}
	else
	{ //we could be more elegant about this, but oh well.
		SetTeam( &g_entities[ level.sortedClients[1] ], "s" );
	}
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[0];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			clientNum;

	if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
		level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
		level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
		level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
	{
		int clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
		int clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];
		int clFailure = 0;
		int clSuccess = 0;

		if (clFirst > clSec)
		{
			clFailure = 1;
			clSuccess = 0;
		}
		else if (clSec > clFirst)
		{
			clFailure = 0;
			clSuccess = 1;
		}
		else
		{
			clFailure = 2;
			clSuccess = 2;
		}

		if (clFailure != 2)
		{
			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
		else
		{
			clSuccess = 0;
			clFailure = 1;

			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
	else
	{
		clientNum = level.sortedClients[0];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );

			trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );
		}

		clientNum = level.sortedClients[1];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b ) {
	gclient_t	*ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	if (level.gametype == GT_POWERDUEL)
	{
		//sort single duelists first
		if (ca->sess.duelTeam == DUELTEAM_LONE && ca->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return -1;
		}
		if (cb->sess.duelTeam == DUELTEAM_LONE && cb->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return 1;
		}

		//others will be auto-sorted below but above spectators.
	}

	// sort special clients last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0  ) {
		return -1;
	}

	// then connecting clients
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}


	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorNum > cb->sess.spectatorNum ) {
			return -1;
		}
		if ( ca->sess.spectatorNum < cb->sess.spectatorNum ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}

qboolean gQueueScoreMessage = qfalse;
int gQueueScoreMessageTime = 0;

//A new duel started so respawn everyone and make sure their stats are reset
qboolean G_CanResetDuelists(void)
{
	int i;
	gentity_t *ent;

	i = 0;
	while (i < 3)
	{ //precheck to make sure they are all respawnable
		ent = &g_entities[level.sortedClients[i]];

		if (!ent->inuse || !ent->client || ent->health <= 0 ||
			ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
			ent->client->sess.duelTeam <= DUELTEAM_FREE)
		{
			return qfalse;
		}
		i++;
	}

	return qtrue;
}

qboolean g_noPDuelCheck = qfalse;
void G_ResetDuelists(void)
{
	int i;
	gentity_t *ent = NULL;

	i = 0;
	while (i < 3)
	{
		ent = &g_entities[level.sortedClients[i]];

		g_noPDuelCheck = qtrue;
		player_die(ent, ent, ent, 999, MOD_SUICIDE);
		g_noPDuelCheck = qfalse;
		trap->UnlinkEntity ((sharedEntity_t *)ent);
		ClientSpawn(ent);
		i++;
	}
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
//	int		preNumSpec = 0;
	//int		nonSpecIndex = -1;
	gclient_t	*cl;

//	preNumSpec = level.numNonSpectatorClients;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedClients = 0;
	level.num_fully_connected_clients = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients = 0;
	level.numVotingClients = 0;		// don't count bots

	for ( i = 0; i < ARRAY_LEN(level.numteamVotingClients); i++ ) {
		level.numteamVotingClients[i] = 0;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if (level.clients[i].pers.connected == CON_CONNECTED)
			{
				level.num_fully_connected_clients++;
			}

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
			{
				if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR)
				{
					level.numNonSpectatorClients++;
					//nonSpecIndex = i;
				}

				// decide if this should be auto-followed
				if ( level.clients[i].pers.connected == CON_CONNECTED )
				{
					if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || level.clients[i].iAmALoser)
					{
						level.numPlayingClients++;
					}
					if ( !(g_entities[i].r.svFlags & SVF_BOT) )
					{
						level.numVotingClients++;
						if ( level.clients[i].sess.sessionTeam == TEAM_RED )
							level.numteamVotingClients[0]++;
						else if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							level.numteamVotingClients[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	if ( !g_warmup.integer || level.gametype == GT_SIEGE )
		level.warmupTime = 0;

	/*
	if (level.numNonSpectatorClients == 2 && preNumSpec < 2 && nonSpecIndex != -1 && level.gametype == GT_DUEL && !level.warmupTime)
	{
		gentity_t *currentWinner = G_GetDuelWinner(&level.clients[nonSpecIndex]);

		if (currentWinner && currentWinner->client)
		{
			trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s %s\n\"",
			currentWinner->client->pers.netname, G_GetStringEdString("MP_SVGAME", "VERSUS"), level.clients[nonSpecIndex].pers.netname));
		}
	}
	*/
	//NOTE: for now not doing this either. May use later if appropriate.

	qsort( level.sortedClients, level.numConnectedClients,
		sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ( level.gametype >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {
		rank = -1;
		score = 0;
		for ( i = 0;  i < level.numPlayingClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next client is checked
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank;
			} else if(i != 0 ){
				// we are tied with the previous client
				level.clients[ level.sortedClients[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( level.gametype == GT_SINGLE_PLAYER && level.numPlayingClients == 1 ) {
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( level.gametype >= GT_TEAM ) {
		trap->SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
		trap->SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
		if ( level.numConnectedClients == 0 ) {
			trap->SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap->SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedClients == 1 ) {
			trap->SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap->SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap->SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap->SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
		}

		if (level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL)
		{ //when not in duel, use this configstring to pass the index of the player currently in first place
			if ( level.numConnectedClients >= 1 )
			{
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", level.sortedClients[0] ) );
			}
			else
			{
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission or in multi-frag Duel game mode, send the new info to everyone
	if ( level.intermissiontime || level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		gQueueScoreMessage = qtrue;
		gQueueScoreMessageTime = level.time + 500;
		//SendScoreboardMessageToAllClients();
		//rww - Made this operate on a "queue" system because it was causing large overflows
	}
}


/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[ i ].pers.connected == CON_CONNECTED ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
extern void G_LeaveVehicle( gentity_t *ent, qboolean ConCheck );
void MoveClientToIntermission( gentity_t *ent ) {
	// take out of follow mode if needed
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent );
	}

	FindIntermissionPoint();
	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->client->ps.origin );
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups) );

	G_LeaveVehicle( ent, qfalse );

	ent->client->ps.rocketLockIndex = ENTITYNUM_NONE;
	ent->client->ps.rocketLockTime = 0;

	ent->client->ps.eFlags = 0;
	ent->s.eFlags = 0;
	ent->client->ps.eFlags2 = 0;
	ent->s.eFlags2 = 0;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;
	ent->s.event = 0;
	ent->r.contents = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
extern qboolean	gSiegeRoundBegun;
extern qboolean	gSiegeRoundEnded;
extern int	gSiegeRoundWinningTeam;
void FindIntermissionPoint( void ) {
	gentity_t	*ent = NULL;
	gentity_t	*target;
	vec3_t		dir;

	// find the intermission spot
	if ( level.gametype == GT_SIEGE
		&& level.intermissiontime
		&& level.intermissiontime <= level.time
		&& gSiegeRoundEnded )
	{
	   	if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM1)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_red");
			if ( ent && ent->target2 )
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	   	else if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM2)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_blue");
			if ( ent && ent->target2 )
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	}
	if ( !ent )
	{
		ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	}
	if ( !ent ) {	// the map creator forgot to put in an intermission point...
		SelectSpawnPoint ( vec3_origin, level.intermission_origin, level.intermission_angle, TEAM_SPECTATOR, qfalse );
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}
}

qboolean DuelLimitHit(void);

/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void ) {
	int			i;
	gentity_t	*client;

	if ( level.intermissiontime ) {
		return;		// already active
	}

	// if in tournament mode, change the wins / losses
	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );

		if (level.gametype != GT_POWERDUEL)
		{
			AdjustTournamentScores();
		}
		if (DuelLimitHit())
		{
			gDuelExit = qtrue;
		}
		else
		{
			gDuelExit = qfalse;
		}
	}

	level.intermissiontime = level.time;

	// move all clients to the intermission point
	for (i=0 ; i< level.maxclients ; i++) {
		client = g_entities + i;
		if (!client->inuse)
			continue;
		// respawn if dead
		if (client->health <= 0) {
			if (level.gametype != GT_POWERDUEL ||
				!client->client ||
				client->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ //don't respawn spectators in powerduel or it will mess the line order all up
				ClientRespawn(client);
			}
		}
		MoveClientToIntermission( client );
	}

	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();
}

qboolean DuelLimitHit(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		if ( duel_fraglimit.integer && cl->sess.wins >= duel_fraglimit.integer )
		{
			return qtrue;
		}
	}

	return qfalse;
}

void DuelResetWinsLosses(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		cl->sess.wins = 0;
		cl->sess.losses = 0;
	}
}

/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar

=============
*/
extern void SiegeDoTeamAssign(void); //g_saga.c
extern siegePers_t g_siegePersistant; //g_saga.c
void ExitLevel (void) {
	int		i;
	gclient_t *cl;

	// if we are running a tournament map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		if (!DuelLimitHit())
		{
			if ( !level.restarted ) {
				trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
				level.restarted = qtrue;
				level.changemap = NULL;
				level.intermissiontime = 0;
			}
			return;
		}

		DuelResetWinsLosses();
	}


	if (level.gametype == GT_SIEGE &&
		g_siegeTeamSwitch.integer &&
		g_siegePersistant.beatingTime)
	{ //restart same map...
		trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
	}
	else
	{
		trap->SendConsoleCommand( EXEC_APPEND, "vstr nextmap\n" );
	}
	level.changemap = NULL;
	level.intermissiontime = 0;

	if (level.gametype == GT_SIEGE &&
		g_siegeTeamSwitch.integer)
	{ //switch out now
		SiegeDoTeamAssign();
	}

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before chaning to CON_CONNECTING
	G_WriteSessionData();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i=0 ; i< sv_maxclients.integer ; i++) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}

}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024] = {0};
	int			mins, seconds, msec, l;

	msec = level.time - level.startTime;

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds %= 60;
//	msec %= 1000;

	Com_sprintf( string, sizeof( string ), "%i:%02i ", mins, seconds );

	l = strlen( string );

	va_start( argptr, fmt );
	Q_vsnprintf( string + l, sizeof( string ) - l, fmt, argptr );
	va_end( argptr );

	if ( dedicated.integer )
		trap->Print( "%s", string + l );

	if ( !level.logFile )
		return;

	trap->FS_Write( string, strlen( string ), level.logFile );
}
/*
=================
G_SecurityLogPrintf

Print to the security logfile with a time stamp if it is open
=================
*/
void QDECL G_SecurityLogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024] = {0};
	time_t		rawtime;
	int			timeLen=0;

	time( &rawtime );
	localtime( &rawtime );
	strftime( string, sizeof( string ), "[%Y-%m-%d] [%H:%M:%S] ", gmtime( &rawtime ) );
	timeLen = strlen( string );

	va_start( argptr, fmt );
	Q_vsnprintf( string+timeLen, sizeof( string ) - timeLen, fmt, argptr );
	va_end( argptr );

	if ( dedicated.integer )
		trap->Print( "%s", string + timeLen );

	if ( !level.security.log )
		return;

	trap->FS_Write( string, strlen( string ), level.security.log );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;
//	qboolean		won = qtrue;
	G_LogPrintf( "Exit: %s\n", string );

	level.intermissionQueued = level.time;

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap->SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( level.gametype >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		if (level.gametype >= GT_TEAM) {
			G_LogPrintf( "(%s) score: %i  ping: %i  client: [%s] %i \"%s^7\"\n", TeamName(cl->ps.persistant[PERS_TEAM]), cl->ps.persistant[PERS_SCORE], ping, cl->pers.guid, level.sortedClients[i], cl->pers.netname );
		} else {
			G_LogPrintf( "score: %i  ping: %i  client: [%s] %i \"%s^7\"\n", cl->ps.persistant[PERS_SCORE], ping, cl->pers.guid, level.sortedClients[i], cl->pers.netname );
		}
//		if (g_singlePlayer.integer && (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)) {
//			if (g_entities[cl - level.clients].r.svFlags & SVF_BOT && cl->ps.persistant[PERS_RANK] == 0) {
//				won = qfalse;
//			}
//		}
	}

	//yeah.. how about not.
	/*
	if (g_singlePlayer.integer) {
		if (level.gametype >= GT_CTF) {
			won = level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE];
		}
		trap->SendConsoleCommand( EXEC_APPEND, (won) ? "spWin\n" : "spLose\n" );
	}
	*/
}

qboolean gDidDuelStuff = qfalse; //gets reset on game reinit

/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady;
	int			i;
	gclient_t	*cl;
	int			readyMask;

	// see which players are ready
	ready = 0;
	notReady = 0;
	readyMask = 0;
	for (i=0 ; i< sv_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			continue;
		}

		if ( cl->readyToExit ) {
			ready++;
			if ( i < 16 ) {
				readyMask |= 1 << i;
			}
		} else {
			notReady++;
		}
	}

	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && !gDidDuelStuff &&
		(level.time > level.intermissiontime + 2000) )
	{
		gDidDuelStuff = qtrue;

		if ( g_austrian.integer && level.gametype != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Results:\n");
			//G_LogPrintf("Duel Time: %d\n", level.time );
			G_LogPrintf("winner: %s, score: %d, wins/losses: %d/%d\n",
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
			G_LogPrintf("loser: %s, score: %d, wins/losses: %d/%d\n",
				level.clients[level.sortedClients[1]].pers.netname,
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[1]].sess.wins,
				level.clients[level.sortedClients[1]].sess.losses );
		}
		// if we are running a tournament map, kick the loser to spectator status,
		// which will automatically grab the next spectator and restart
		if (!DuelLimitHit())
		{
			if (level.gametype == GT_POWERDUEL)
			{
				RemovePowerDuelLosers();
				AddPowerDuelPlayers();
			}
			else
			{
				if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
					level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
					level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
					level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
				{
					RemoveDuelDrawLoser();
				}
				else
				{
					RemoveTournamentLoser();
				}
				AddTournamentPlayer();
			}

			if ( g_austrian.integer )
			{
				if (level.gametype == GT_POWERDUEL)
				{
					G_LogPrintf("Power Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n",
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						level.clients[level.sortedClients[2]].pers.netname,
						level.clients[level.sortedClients[2]].sess.wins,
						level.clients[level.sortedClients[2]].sess.losses,
						fraglimit.integer );
				}
				else
				{
					G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d, kill limit: %d\n",
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						fraglimit.integer );
				}
			}

			if (level.gametype == GT_POWERDUEL)
			{
				if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
				{
					trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
					trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}
			}
			else
			{
				if (level.numPlayingClients >= 2)
				{
					trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
					trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}
			}

			return;
		}

		if ( g_austrian.integer && level.gametype != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Tournament Winner: %s wins/losses: %d/%d\n",
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
		}

		if (level.gametype == GT_POWERDUEL)
		{
			RemovePowerDuelLosers();
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{
				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
		else
		{
			//this means we hit the duel limit so reset the wins/losses
			//but still push the loser to the back of the line, and retain the order for
			//the map change
			if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
				level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
				level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
			{
				RemoveDuelDrawLoser();
			}
			else
			{
				RemoveTournamentLoser();
			}

			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	if ((level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && !gDuelExit)
	{ //in duel, we have different behaviour for between-round intermissions
		if ( level.time > level.intermissiontime + 4000 )
		{ //automatically go to next after 4 seconds
			ExitLevel();
			return;
		}

		for (i=0 ; i< sv_maxclients.integer ; i++)
		{ //being in a "ready" state is not necessary here, so clear it for everyone
		  //yes, I also thinking holding this in a ps value uniquely for each player
		  //is bad and wrong, but it wasn't my idea.
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED )
			{
				continue;
			}
			cl->ps.stats[STAT_CLIENTS_READY] = 0;
		}
		return;
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
	for (i=0 ; i< sv_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	}

	// never exit in less than five seconds
	if ( level.time < level.intermissiontime + 5000 ) {
		return;
	}

	if (d_noIntermissionWait.integer)
	{ //don't care who wants to go, just go.
		ExitLevel();
		return;
	}

	// if nobody wants to go, clear timer
	if ( !ready ) {
		level.readyToExit = qfalse;
		return;
	}

	// if everyone wants to go, go now
	if ( !notReady ) {
		ExitLevel();
		return;
	}

	// the first person to ready starts the ten second timeout
	if ( !level.readyToExit ) {
		level.readyToExit = qtrue;
		level.exitTime = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if ( level.time < level.exitTime + 10000 ) {
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void ) {
	int		a, b;

	if ( level.numPlayingClients < 2 ) {
		return qfalse;
	}

	if ( level.gametype >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
	b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

	return a == b;
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
qboolean g_endPDuel = qfalse;
void CheckExitRules( void ) {
 	int			i;
	gclient_t	*cl;
	char *sKillLimit;
	qboolean printLimit = qtrue;
	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit ();
		return;
	}

	if (gDoSlowMoDuel)
	{ //don't go to intermission while in slow motion
		return;
	}

	if (gEscaping)
	{
		int numLiveClients = 0;

		for ( i=0; i < MAX_CLIENTS; i++ )
		{
			if (g_entities[i].inuse && g_entities[i].client && g_entities[i].health > 0)
			{
				if (g_entities[i].client->sess.sessionTeam != TEAM_SPECTATOR &&
					!(g_entities[i].client->ps.pm_flags & PMF_FOLLOW))
				{
					numLiveClients++;
				}
			}
		}
		if (gEscapeTime < level.time)
		{
			gEscaping = qfalse;
			LogExit( "Escape time ended." );
			return;
		}
		if (!numLiveClients)
		{
			gEscaping = qfalse;
			LogExit( "Everyone failed to escape." );
			return;
		}
	}

	if ( level.intermissionQueued ) {
		//int time = (g_singlePlayer.integer) ? SP_INTERMISSION_DELAY_TIME : INTERMISSION_DELAY_TIME;
		int time = INTERMISSION_DELAY_TIME;
		if ( level.time - level.intermissionQueued >= time ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
		return;
	}

	/*
	if (level.gametype == GT_POWERDUEL)
	{
		if (level.numPlayingClients < 3)
		{
			if (!level.intermissiontime)
			{
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Duel forfeit (1)\n");
				}
				LogExit("Duel forfeit.");
				return;
			}
		}
	}
	*/

	// check for sudden death
	if (level.gametype != GT_SIEGE)
	{
		if ( ScoreIsTied() ) {
			// always wait for sudden death
			if ((level.gametype != GT_DUEL) || !timelimit.value)
			{
				if (level.gametype != GT_POWERDUEL)
				{
					return;
				}
			}
		}
	}

	if (level.gametype != GT_SIEGE)
	{
		if ( timelimit.value > 0.0f && !level.warmupTime ) {
			if ( level.time - level.startTime >= timelimit.value*60000 ) {
//				trap->SendServerCommand( -1, "print \"Timelimit hit.\n\"");
				trap->SendServerCommand( -1, va("print \"%s.\n\"",G_GetStringEdString("MP_SVGAME", "TIMELIMIT_HIT")));
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Timelimit hit (1)\n");
				}
				LogExit( "Timelimit hit." );
				return;
			}
		}

		if (zyk_server_empty_change_map_time.integer > 0)
		{
			if (level.num_fully_connected_clients == 0)
			{ // zyk: changes map if server has no one for some time
				if (level.server_empty_change_map_timer == 0)
					level.server_empty_change_map_timer = level.time;

				if ((level.time - level.server_empty_change_map_timer) > zyk_server_empty_change_map_time.integer)
					ExitLevel();
			}
			else
			{ // zyk: if someone connects, reset the counter
				level.server_empty_change_map_timer = 0;
			}
		}
	}

	if (level.gametype == GT_POWERDUEL && level.numPlayingClients >= 3)
	{
		if (g_endPDuel)
		{
			g_endPDuel = qfalse;
			LogExit("Powerduel ended.");
		}

		//yeah, this stuff was completely insane.
		/*
		int duelists[3];
		duelists[0] = level.sortedClients[0];
		duelists[1] = level.sortedClients[1];
		duelists[2] = level.sortedClients[2];

		if (duelists[0] != -1 &&
			duelists[1] != -1 &&
			duelists[2] != -1)
		{
			if (!g_entities[duelists[0]].inuse ||
				!g_entities[duelists[0]].client ||
				g_entities[duelists[0]].client->ps.stats[STAT_HEALTH] <= 0 ||
				g_entities[duelists[0]].client->sess.sessionTeam != TEAM_FREE)
			{ //The lone duelist lost, give the other two wins (if applicable) and him a loss
				if (g_entities[duelists[0]].inuse &&
					g_entities[duelists[0]].client)
				{
					g_entities[duelists[0]].client->sess.losses++;
					ClientUserinfoChanged(duelists[0]);
				}
				if (g_entities[duelists[1]].inuse &&
					g_entities[duelists[1]].client)
				{
					if (g_entities[duelists[1]].client->ps.stats[STAT_HEALTH] > 0 &&
						g_entities[duelists[1]].client->sess.sessionTeam == TEAM_FREE)
					{
						g_entities[duelists[1]].client->sess.wins++;
					}
					else
					{
						g_entities[duelists[1]].client->sess.losses++;
					}
					ClientUserinfoChanged(duelists[1]);
				}
				if (g_entities[duelists[2]].inuse &&
					g_entities[duelists[2]].client)
				{
					if (g_entities[duelists[2]].client->ps.stats[STAT_HEALTH] > 0 &&
						g_entities[duelists[2]].client->sess.sessionTeam == TEAM_FREE)
					{
						g_entities[duelists[2]].client->sess.wins++;
					}
					else
					{
						g_entities[duelists[2]].client->sess.losses++;
					}
					ClientUserinfoChanged(duelists[2]);
				}

				//Will want to parse indecies for two out at some point probably
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", duelists[1] ) );

				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Coupled duelists won (1)\n");
				}
				LogExit( "Coupled duelists won." );
				gDuelExit = qfalse;
			}
			else if ((!g_entities[duelists[1]].inuse ||
				!g_entities[duelists[1]].client ||
				g_entities[duelists[1]].client->sess.sessionTeam != TEAM_FREE ||
				g_entities[duelists[1]].client->ps.stats[STAT_HEALTH] <= 0) &&
				(!g_entities[duelists[2]].inuse ||
				!g_entities[duelists[2]].client ||
				g_entities[duelists[2]].client->sess.sessionTeam != TEAM_FREE ||
				g_entities[duelists[2]].client->ps.stats[STAT_HEALTH] <= 0))
			{ //the coupled duelists lost, give the lone duelist a win (if applicable) and the couple both losses
				if (g_entities[duelists[1]].inuse &&
					g_entities[duelists[1]].client)
				{
					g_entities[duelists[1]].client->sess.losses++;
					ClientUserinfoChanged(duelists[1]);
				}
				if (g_entities[duelists[2]].inuse &&
					g_entities[duelists[2]].client)
				{
					g_entities[duelists[2]].client->sess.losses++;
					ClientUserinfoChanged(duelists[2]);
				}

				if (g_entities[duelists[0]].inuse &&
					g_entities[duelists[0]].client &&
					g_entities[duelists[0]].client->ps.stats[STAT_HEALTH] > 0 &&
					g_entities[duelists[0]].client->sess.sessionTeam == TEAM_FREE)
				{
					g_entities[duelists[0]].client->sess.wins++;
					ClientUserinfoChanged(duelists[0]);
				}

				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", duelists[0] ) );

				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Lone duelist won (1)\n");
				}
				LogExit( "Lone duelist won." );
				gDuelExit = qfalse;
			}
		}
		*/
		return;
	}

	if ( level.numPlayingClients < 2 ) {
		return;
	}

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{
		if (fraglimit.integer > 1)
		{
			sKillLimit = "Kill limit hit.";
		}
		else
		{
			sKillLimit = "";
			printLimit = qfalse;
		}
	}
	else
	{
		sKillLimit = "Kill limit hit.";
	}
	if ( level.gametype < GT_SIEGE && fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= fraglimit.integer ) {
			trap->SendServerCommand( -1, va("print \"Red %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (1)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= fraglimit.integer ) {
			trap->SendServerCommand( -1, va("print \"Blue %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (2)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

			if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && duel_fraglimit.integer && cl->sess.wins >= duel_fraglimit.integer )
			{
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Duel limit hit (1)\n");
				}
				LogExit( "Duel limit hit." );
				gDuelExit = qtrue;
				trap->SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the win limit.\n\"",
					cl->pers.netname ) );
				return;
			}

			if ( cl->ps.persistant[PERS_SCORE] >= fraglimit.integer ) {
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Kill limit (3)\n");
				}
				LogExit( sKillLimit );
				gDuelExit = qfalse;
				if (printLimit)
				{
					trap->SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s.\n\"",
													cl->pers.netname,
													G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")
													)
											);
				}
				return;
			}
		}
	}

	if ( level.gametype >= GT_CTF && capturelimit.integer ) {

		if ( level.teamScores[TEAM_RED] >= capturelimit.integer )
		{
			trap->SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")));
			trap->SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= capturelimit.integer ) {
			trap->SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")));
			trap->SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}
	}
}



/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

void G_RemoveDuelist(int team)
{
	int i = 0;
	gentity_t *ent;
	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client && ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
			ent->client->sess.duelTeam == team)
		{
			SetTeam(ent, "s");
		}
        i++;
	}
}

/*
=============
CheckTournament

Once a frame, check for changes in tournament player state
=============
*/
int g_duelPrintTimer = 0;
void CheckTournament( void ) {
	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
//	if ( level.numPlayingClients == 0 && (level.gametype != GT_POWERDUEL) ) {
//		return;
//	}

	if (level.gametype == GT_POWERDUEL)
	{
		if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
		{
			trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
		}
	}
	else
	{
		if (level.numPlayingClients >= 2)
		{
			trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
		}
	}

	if ( level.gametype == GT_DUEL )
	{
		// pull in a spectator if needed
		if ( level.numPlayingClients < 2 && !level.intermissiontime && !level.intermissionQueued ) {
			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
			}
		}

		if (level.numPlayingClients >= 2)
		{
// nmckenzie: DUEL_HEALTH
			if ( g_showDuelHealths.integer >= 1 )
			{
				playerState_t *ps1, *ps2;
				ps1 = &level.clients[level.sortedClients[0]].ps;
				ps2 = &level.clients[level.sortedClients[1]].ps;
				trap->SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("%i|%i|!",
					ps1->stats[STAT_HEALTH], ps2->stats[STAT_HEALTH]));
			}
		}

		//rww - It seems we have decided there will be no warmup in duel.
		//if (!g_warmup.integer)
		{ //don't care about any of this stuff then, just add people and leave me alone
			level.warmupTime = 0;
			return;
		}
#if 0
		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingClients != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingClients == 2 ) {
				// fudge by -1 to account for extra delays
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;

				if (level.warmupTime < (level.time + 3000))
				{ //rww - this is an unpleasent hack to keep the level from resetting completely on the client (this happens when two map_restarts are issued rapidly)
					level.warmupTime = level.time + 3000;
				}
				trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap->Cvar_Set( "g_restarted", "1" );
			trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
#endif
	}
	else if (level.gametype == GT_POWERDUEL)
	{
		if (level.numPlayingClients < 2)
		{ //hmm, ok, pull more in.
			g_dontFrickinCheck = qfalse;
		}

		if (level.numPlayingClients > 3)
		{ //umm..yes..lets take care of that then.
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone > 1)
			{
				G_RemoveDuelist(DUELTEAM_LONE);
			}
			else if (dbl > 2)
			{
				G_RemoveDuelist(DUELTEAM_DOUBLE);
			}
		}
		else if (level.numPlayingClients < 3)
		{ //hmm, someone disconnected or something and we need em
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
			else if (dbl < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
		}

		// pull in a spectator if needed
		if (level.numPlayingClients < 3 && !g_dontFrickinCheck)
		{
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 &&
				G_CanResetDuelists())
			{
				gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
				te->r.svFlags |= SVF_BROADCAST;
				//this is really pretty nasty, but..
				te->s.otherEntityNum = level.sortedClients[0];
				te->s.otherEntityNum2 = level.sortedClients[1];
				te->s.groundEntityNum = level.sortedClients[2];

				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				G_ResetDuelists();

				g_dontFrickinCheck = qtrue;
			}
			else if (level.numPlayingClients > 0 ||
				level.numConnectedClients > 0)
			{
				if (g_duelPrintTimer < level.time)
				{ //print once every 10 seconds
					int lone = 0, dbl = 0;

					G_PowerDuelCount(&lone, &dbl, qtrue);
					if (lone < 1)
					{
						trap->SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMORESINGLE")) );
					}
					else
					{
						trap->SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMOREPAIRED")) );
					}
					g_duelPrintTimer = level.time + 10000;
				}
			}

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{ //pulled in a needed person
				if (G_CanResetDuelists())
				{
					gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
					te->r.svFlags |= SVF_BROADCAST;
					//this is really pretty nasty, but..
					te->s.otherEntityNum = level.sortedClients[0];
					te->s.otherEntityNum2 = level.sortedClients[1];
					te->s.groundEntityNum = level.sortedClients[2];

					trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );

					if ( g_austrian.integer )
					{
						G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n",
							level.clients[level.sortedClients[0]].pers.netname,
							level.clients[level.sortedClients[0]].sess.wins,
							level.clients[level.sortedClients[0]].sess.losses,
							level.clients[level.sortedClients[1]].pers.netname,
							level.clients[level.sortedClients[1]].sess.wins,
							level.clients[level.sortedClients[1]].sess.losses,
							level.clients[level.sortedClients[2]].pers.netname,
							level.clients[level.sortedClients[2]].sess.wins,
							level.clients[level.sortedClients[2]].sess.losses,
							fraglimit.integer );
					}
					//trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
					//FIXME: This seems to cause problems. But we'd like to reset things whenever a new opponent is set.
				}
			}
		}
		else
		{ //if you have proper num of players then don't try to add again
			g_dontFrickinCheck = qtrue;
		}

		level.warmupTime = 0;
		return;
	}
	else if ( level.warmupTime != 0 ) {
		int		counts[TEAM_NUM_TEAMS];
		qboolean	notEnough = qfalse;

		if ( level.gametype > GT_TEAM ) {
			counts[TEAM_BLUE] = TeamCount( -1, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( -1, TEAM_RED );

			if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1) {
				notEnough = qtrue;
			}
		} else if ( level.numPlayingClients < 2 ) {
			notEnough = qtrue;
		}

		if ( notEnough ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return; // still waiting for team members
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		/*
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}
		*/

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			// fudge by -1 to account for extra delays
			if ( g_warmup.integer > 1 ) {
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
			} else {
				level.warmupTime = 0;
			}
			trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap->Cvar_Set( "g_restarted", "1" );
			trap->Cvar_Update( &g_restarted );
			trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
	}
}

void G_KickAllBots(void)
{
	int i;
	gclient_t	*cl;

	for ( i=0 ; i< sv_maxclients.integer ; i++ )
	{
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED )
		{
			continue;
		}
		if ( !(g_entities[i].r.svFlags & SVF_BOT) )
		{
			continue;
		}
		trap->SendConsoleCommand( EXEC_INSERT, va("clientkick %d\n", i) );
	}
}

/*
==================
CheckVote
==================
*/
void CheckVote( void ) {
	if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;
		trap->SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );

		if (level.votingGametype)
		{
			if (level.gametype != level.votingGametypeTo)
			{ //If we're voting to a different game type, be sure to refresh all the map stuff
				const char *nextMap = G_RefreshNextMap(level.votingGametypeTo, qtrue);

				if (level.votingGametypeTo == GT_SIEGE)
				{ //ok, kick all the bots, cause the aren't supported!
                    G_KickAllBots();
					//just in case, set this to 0 too... I guess...maybe?
					//trap->Cvar_Set("bot_minplayers", "0");
				}

				if (nextMap && nextMap[0] && zyk_change_map_gametype_vote.integer)
				{
					trap->SendConsoleCommand( EXEC_APPEND, va("map %s\n", nextMap ) );
				}
				else
				{ // zyk: if zyk_change_map_gametype_vote is 0, just restart the current map
					trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n"  );
				}
			}
			else
			{ //otherwise, just leave the map until a restart
				G_RefreshNextMap(level.votingGametypeTo, qfalse);
			}

			if (g_fraglimitVoteCorrection.integer)
			{ //This means to auto-correct fraglimit when voting to and from duel.
				const int currentGT = level.gametype;
				const int currentFL = fraglimit.integer;
				const int currentTL = timelimit.integer;

				if ((level.votingGametypeTo == GT_DUEL || level.votingGametypeTo == GT_POWERDUEL) && currentGT != GT_DUEL && currentGT != GT_POWERDUEL)
				{
					if (currentFL > 1 || !currentFL)
					{ //if voting to duel, and fraglimit is more than 1 (or unlimited), then set it down to 1
						trap->SendConsoleCommand(EXEC_APPEND, "fraglimit 1\n"); // zyk: changed from 3 to 1
					}
					if (currentTL)
					{ //if voting to duel, and timelimit is set, make it unlimited
						trap->SendConsoleCommand(EXEC_APPEND, "timelimit 0\n");
					}
				}
				else if ((level.votingGametypeTo != GT_DUEL && level.votingGametypeTo != GT_POWERDUEL) &&
					(currentGT == GT_DUEL || currentGT == GT_POWERDUEL))
				{
					if (currentFL != 0)
					{ //if voting from duel, an fraglimit is different than 0, then set it up to 0
						trap->SendConsoleCommand(EXEC_APPEND, "fraglimit 0\n"); // zyk: changed from 20 to 0
					}
				}
			}

			level.votingGametype = qfalse;
			level.votingGametypeTo = 0;
		}
	}
	if ( !level.voteTime ) {
		return;
	}
	if ( level.time-level.voteTime >= VOTE_TIME) // || level.voteYes + level.voteNo == 0 ) zyk: no longer does this
	{
		if (level.voteYes > level.voteNo)
		{ // zyk: now vote pass if number of Yes is greater than number of No
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEPASSED"), level.voteStringClean) );
			level.voteExecuteTime = level.time + level.voteExecuteDelay;
		}
		else
		{
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEFAILED"), level.voteStringClean) );
		}

		// zyk: set the timer for the next vote of this player
		if (zyk_vote_timer.integer > 0 && level.voting_player > -1)
			g_entities[level.voting_player].client->sess.vote_timer = zyk_vote_timer.integer;
	}
	else 
	{
		if ( level.voteYes > level.numVotingClients/2 ) {
			// execute the command, then remove the vote
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEPASSED"), level.voteStringClean) );
			level.voteExecuteTime = level.time + level.voteExecuteDelay;
			// zyk: set the timer for the next vote of this player
			if (zyk_vote_timer.integer > 0 && level.voting_player > -1)
				g_entities[level.voting_player].client->sess.vote_timer = zyk_vote_timer.integer;
		}
		// same behavior as a timeout
		else if ( level.voteNo >= (level.numVotingClients+1)/2 )
		{
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEFAILED"), level.voteStringClean) );
			// zyk: set the timer for the next vote of this player
			if (zyk_vote_timer.integer > 0 && level.voting_player > -1)
				g_entities[level.voting_player].client->sess.vote_timer = zyk_vote_timer.integer;
		}
		else // still waiting for a majority
			return;
	}
	level.voteTime = 0;
	trap->SetConfigstring( CS_VOTE_TIME, "" );
}

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char *message) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		trap->SendServerCommand( i, message );
	}
}

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int client) {
	int i;

	if ( level.clients[client].pers.connected == CON_DISCONNECTED ) {
		PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname) );
		return;
	}
	if (level.clients[client].sess.sessionTeam != team) {
		PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname) );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader) {
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged(i);
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );
	PrintTeam(team, va("print \"%s %s\n\"", level.clients[client].pers.netname, G_GetStringEdString("MP_SVGAME", "NEWTEAMLEADER")) );
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( int team ) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader)
			break;
	}
	if (i >= level.maxclients) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
		if ( i >= level.maxclients ) {
			for ( i = 0 ; i < level.maxclients ; i++ ) {
				if ( level.clients[i].sess.sessionTeam != team )
					continue;
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
	}
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( int team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( level.teamVoteExecuteTime[cs_offset] && level.teamVoteExecuteTime[cs_offset] < level.time ) {
		level.teamVoteExecuteTime[cs_offset] = 0;
		if ( !Q_strncmp( "leader", level.teamVoteString[cs_offset], 6) ) {
			//set the team leader
			SetLeader(team, atoi(level.teamVoteString[cs_offset] + 7));
		}
		else {
			trap->SendConsoleCommand( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ) );
		}
	}

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}

	if ( level.time-level.teamVoteTime[cs_offset] >= VOTE_TIME || level.teamVoteYes[cs_offset] + level.teamVoteNo[cs_offset] == 0 ) {
		trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED"), level.teamVoteStringClean[cs_offset]) );
	}
	else {
		if ( level.teamVoteYes[cs_offset] > level.numteamVotingClients[cs_offset]/2 ) {
			// execute the command, then remove the vote
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEPASSED"), level.teamVoteStringClean[cs_offset]) );
			level.voteExecuteTime = level.time + 3000;
		}

		// same behavior as a timeout
		else if ( level.teamVoteNo[cs_offset] >= (level.numteamVotingClients[cs_offset]+1)/2 )
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED"), level.teamVoteStringClean[cs_offset]) );

		else // still waiting for a majority
			return;
	}
	level.teamVoteTime[cs_offset] = 0;
	trap->SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );
}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;

	if ( g_password.modificationCount != lastMod ) {
		char password[MAX_INFO_STRING];
		char *c = password;
		lastMod = g_password.modificationCount;

		strcpy( password, g_password.string );
		while( *c )
		{
			if ( *c == '%' )
			{
				*c = '.';
			}
			c++;
		}
		trap->Cvar_Set("g_password", password );

		if ( *g_password.string && Q_stricmp( g_password.string, "none" ) ) {
			trap->Cvar_Set( "g_needpass", "1" );
		} else {
			trap->Cvar_Set( "g_needpass", "0" );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink (gentity_t *ent) {
	float	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		goto runicarus;
	}
	if (thinktime > level.time) {
		goto runicarus;
	}

	ent->nextthink = 0;
	if (!ent->think) {
		//trap->Error( ERR_DROP, "NULL ent->think");
		goto runicarus;
	}
	ent->think (ent);

runicarus:
	if ( ent->inuse )
	{
		SaveNPCGlobals();
		if(NPCS.NPCInfo == NULL && ent->NPC != NULL)
		{
			SetNPCGlobals( ent );
		}
		trap->ICARUS_MaintainTaskManager(ent->s.number);
		RestoreNPCGlobals();
	}
}

int g_LastFrameTime = 0;
int g_TimeSinceLastFrame = 0;

qboolean gDoSlowMoDuel = qfalse;
int gSlowMoDuelTime = 0;

//#define _G_FRAME_PERFANAL

void NAV_CheckCalcPaths( void )
{
	if ( navCalcPathTime && navCalcPathTime < level.time )
	{//first time we've ever loaded this map...
		vmCvar_t	mapname;
		vmCvar_t	ckSum;

		trap->Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		trap->Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

		//clear all the failed edges
		trap->Nav_ClearAllFailedEdges();

		//Calculate all paths
		NAV_CalculatePaths( mapname.string, ckSum.integer );

		trap->Nav_CalculatePaths(qfalse);

#ifndef FINAL_BUILD
		if ( fatalErrors )
		{
			Com_Printf( S_COLOR_RED"Not saving .nav file due to fatal nav errors\n" );
		}
		else
#endif
		if ( trap->Nav_Save( mapname.string, ckSum.integer ) == qfalse )
		{
			Com_Printf("Unable to save navigations data for map \"%s\" (checksum:%d)\n", mapname.string, ckSum.integer );
		}
		navCalcPathTime = 0;
	}
}

//so shared code can get the local time depending on the side it's executed on
int BG_GetTime(void)
{
	return level.time;
}

// zyk: similar to TeleportPlayer(), but this one doesnt spit the player out at the destination
void zyk_TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	gentity_t	*tent;
	qboolean	isNPC = qfalse;

	if (player->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		tent = G_TempEntity( player->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity( origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = player->s.clientNum;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap->UnlinkEntity ((sharedEntity_t *)player);

	VectorCopy ( origin, player->client->ps.origin );
	player->client->ps.origin[2] += 1;

	// set angles
	SetClientViewAngle( player, angles );

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// kill anything at the destination
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		G_KillBox (player);
	}

	// save results of pmove
	BG_PlayerStateToEntityState( &player->client->ps, &player->s, qtrue );
	if (isNPC)
	{
		player->s.eType = ET_NPC;
	}

	// use the precise origin for linking
	VectorCopy( player->client->ps.origin, player->r.currentOrigin );

	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		trap->LinkEntity ((sharedEntity_t *)player);
	}
}

// zyk: function to kill npcs with the name as parameter
void zyk_NPC_Kill_f( char *name )
{
	int	n = 0;
	gentity_t *player = NULL;

	for ( n = level.maxclients; n < level.num_entities; n++) 
	{
		player = &g_entities[n];
		if ( player && player->NPC && player->client )
		{
			if(Q_stricmp( name, player->NPC_type ) == 0 || Q_stricmp( name, "all" ) == 0)
			{
				player->health = 0;
				player->client->ps.stats[STAT_HEALTH] = 0;
				if (player->die)
				{
					player->die(player, player, player, 100, MOD_UNKNOWN);
				}
			}
		}
	}
}

// zyk: tests if ent has other as ally
qboolean zyk_is_ally(gentity_t *ent, gentity_t *other)
{
	if (ent && other && !ent->NPC && !other->NPC && ent != other && ent->client && other->client && other->client->pers.connected == CON_CONNECTED)
	{
		if (other->s.number > 15 && (ent->client->sess.ally2 & (1 << (other->s.number-16))))
		{
			return qtrue;
		}
		else if (ent->client->sess.ally1 & (1 << other->s.number))
		{
			return qtrue;
		}
	}

	return qfalse;
}

// zyk: counts how many allies this player has
int zyk_number_of_allies(gentity_t *ent, qboolean in_rpg_mode)
{
	int i = 0;
	int number_of_allies = 0;

	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *allied_player = &g_entities[i];

		if (zyk_is_ally(ent,allied_player) == qtrue && (in_rpg_mode == qfalse || (allied_player->client->sess.amrpgmode == 2 && allied_player->client->sess.sessionTeam != TEAM_SPECTATOR)))
			number_of_allies++;
	}

	return number_of_allies;
}

// zyk: starts the boss battle music
void zyk_start_boss_battle_music(gentity_t *ent)
{
	if (!(ent->client->pers.player_settings & (1 << SETTINGS_BOSS_MUSIC)))
	{
		trap->SetConfigstring(CS_MUSIC, "music/kor_lite/korrib_action.mp3");
	}
}

// zyk: tests if this player is one of the Duel Tournament duelists
qboolean duel_tournament_is_duelist(gentity_t *ent)
{
	if ((ent->s.number == level.duelist_1_id || ent->s.number == level.duelist_2_id || ent->s.number == level.duelist_1_ally_id || ent->s.number == level.duelist_2_ally_id))
	{
		return qtrue;
	}

	return qfalse;
}

/*
// zyk: shows a text message from the file based on the language set by the player. Can receive additional arguments to concat in the final string
void zyk_text_message(gentity_t* ent, char* filename, qboolean show_in_chat, qboolean broadcast_message, ...)
{
	va_list argptr;
	char content[MAX_STRING_CHARS];
	const char* file_content;
	static char string[MAX_STRING_CHARS];
	char language[128];
	char console_cmd[64];
	int client_id = -1;
	FILE* text_file = NULL;

	strcpy(content, "");
	strcpy(string, "");
	strcpy(console_cmd, "print");

	if (broadcast_message == qfalse)
		client_id = ent->s.number;

	if (show_in_chat == qtrue)
		strcpy(console_cmd, "chat");

	strcpy(language, "english");

	text_file = fopen(va("zykmod/textfiles/%s/%s.txt", language, filename), "r");
	if (text_file)
	{
		fgets(content, sizeof(content), text_file);
		if (content[strlen(content) - 1] == '\n')
			content[strlen(content) - 1] = '\0';

		fclose(text_file);
	}
	else
	{
		strcpy(content, "^1File could not be open!");
	}

	file_content = va("%s", content);

	va_start(argptr, broadcast_message);
	Q_vsnprintf(string, sizeof(string), file_content, argptr);
	va_end(argptr);

	trap->SendServerCommand(client_id, va("%s \"%s\n\"", console_cmd, string));
}
*/

void zyk_quest_effect_spawn(gentity_t *ent, gentity_t *target_ent, char *targetname, char *spawnflags, char *effect_path, int start_time, int damage, int radius, int duration)
{
	gentity_t *new_ent = G_Spawn();

	if (!strstr(effect_path, ".md3"))
	{// zyk: effect power
		zyk_set_entity_field(new_ent, "classname", "fx_runner");
		zyk_set_entity_field(new_ent, "spawnflags", spawnflags);
		zyk_set_entity_field(new_ent, "targetname", targetname);

		if (Q_stricmp(targetname, "zyk_effect_scream") == 0)
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)target_ent->r.currentOrigin[0], (int)target_ent->r.currentOrigin[1], (int)target_ent->r.currentOrigin[2] + 50));
		else
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)target_ent->r.currentOrigin[0], (int)target_ent->r.currentOrigin[1], (int)target_ent->r.currentOrigin[2]));

		new_ent->s.modelindex = G_EffectIndex(effect_path);

		zyk_spawn_entity(new_ent);

		if (damage > 0)
			new_ent->splashDamage = damage;

		if (radius > 0)
			new_ent->splashRadius = radius;

		if (start_time > 0)
			new_ent->nextthink = level.time + start_time;

		level.special_power_effects[new_ent->s.number] = ent->s.number;
		level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
	}
	else
	{ // zyk: model power
		zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
		zyk_set_entity_field(new_ent, "spawnflags", spawnflags);

		if (Q_stricmp(targetname, "zyk_tree_of_life") == 0)
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)target_ent->r.currentOrigin[0], (int)target_ent->r.currentOrigin[1], (int)target_ent->r.currentOrigin[2] + 350));
		else
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)target_ent->r.currentOrigin[0], (int)target_ent->r.currentOrigin[1], (int)target_ent->r.currentOrigin[2]));

		zyk_set_entity_field(new_ent, "model", effect_path);

		zyk_set_entity_field(new_ent, "targetname", targetname);

		zyk_spawn_entity(new_ent);

		level.special_power_effects[new_ent->s.number] = ent->s.number;
		level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
	}
}

// zyk: tests if the target player can be hit by the attacker gun/saber damage, force power or special power
qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target)
{
	if (attacker && attacker->client && target && target->client && !attacker->NPC && !target->NPC)
	{
		if (level.duel_tournament_mode == 4 && duel_tournament_is_duelist(attacker) != duel_tournament_is_duelist(target))
		{ // zyk: cannot hit duelists in Duel Tournament
			return qfalse;
		}

		if (level.sniper_mode > 1 && ((level.sniper_players[attacker->s.number] != -1 && level.sniper_players[target->s.number] == -1) || 
			(level.sniper_players[attacker->s.number] == -1 && level.sniper_players[target->s.number] != -1)))
		{ // zyk: players outside sniper battle cannot hit ones in it and vice-versa
			return qfalse;
		}

		if (level.melee_mode > 1 && ((level.melee_players[attacker->s.number] != -1 && level.melee_players[target->s.number] == -1) ||
			(level.melee_players[attacker->s.number] == -1 && level.melee_players[target->s.number] != -1)))
		{ // zyk: players outside melee battle cannot hit ones in it and vice-versa
			return qfalse;
		}

		if (level.rpg_lms_mode > 1 && ((level.rpg_lms_players[attacker->s.number] != -1 && level.rpg_lms_players[target->s.number] == -1) ||
			(level.rpg_lms_players[attacker->s.number] == -1 && level.rpg_lms_players[target->s.number] != -1)))
		{ // zyk: players outside rpg lms cannot hit ones in it and vice-versa
			return qfalse;
		}

		if (attacker->client->pers.player_statuses & (1 << 26) && attacker != target)
		{ // zyk: used nofight command, cannot hit anyone
			return qfalse;
		}

		if (target->client->pers.player_statuses & (1 << 26) && attacker != target)
		{ // zyk: used nofight command, cannot be hit by anyone
			return qfalse;
		}

		if (attacker->client->noclip == qtrue || target->client->noclip == qtrue)
		{ // zyk: noclip does not allow hitting
			return qfalse;
		}

		if (level.duel_tournament_mode > 0 && level.duel_players[attacker->s.number] != -1 && level.duel_players[target->s.number] != -1 && 
			level.duel_allies[attacker->s.number] == target->s.number && level.duel_allies[target->s.number] == attacker->s.number)
		{ // zyk: Duel Tournament allies. Cannot hit each other
			return qfalse;
		}
	}

	return qtrue;
}

qboolean npcs_on_same_team(gentity_t *attacker, gentity_t *target)
{
	if (attacker->NPC && target->NPC && attacker->client->playerTeam == target->client->playerTeam)
	{
		return qtrue;
	}

	return qfalse;
}

// zyk: tests if the target entity can be hit by the attacker special power
qboolean zyk_special_power_can_hit_target(gentity_t *attacker, gentity_t *target, int i, int min_distance, int max_distance, qboolean hit_breakable, int *targets_hit)
{
	if ((*targets_hit) >= zyk_max_special_power_targets.integer)
		return qfalse;

	if (attacker->s.number != i && target && target->client && target->health > 0 && zyk_can_hit_target(attacker, target) == qtrue && 
		(i > MAX_CLIENTS || (target->client->pers.connected == CON_CONNECTED && target->client->sess.sessionTeam != TEAM_SPECTATOR && 
		 target->client->ps.duelInProgress == qfalse)))
	{ // zyk: target is a player or npc that can be hit by the attacker
		int player_distance = (int)Distance(attacker->client->ps.origin,target->client->ps.origin);

		if (player_distance > min_distance && player_distance < max_distance)
		{
			int is_ally = 0;

			if (i < level.maxclients && !attacker->NPC && 
				zyk_is_ally(attacker,target) == qtrue)
			{ // zyk: allies will not be hit by this power
				is_ally = 1;
			}

			if (OnSameTeam(attacker, target) == qtrue || npcs_on_same_team(attacker, target) == qtrue)
			{ // zyk: if one of them is npc, also check for allies
				is_ally = 1;
			}

			if (is_ally == 0)
			{ // zyk: players in bosses can only hit bosses and their helper npcs. Players not in boss battles
			  // can only hit normal enemy npcs and npcs spawned by bosses but not the bosses themselves. Magic-using npcs can hit everyone that are not their allies
				(*targets_hit)++;

				return qtrue;
			}
		}
	}
	else if (i >= MAX_CLIENTS && hit_breakable == qtrue && target && !target->client && target->health > 0 && target->takedamage == qtrue)
	{
		int entity_distance = (int)Distance(attacker->client->ps.origin,target->r.currentOrigin);

		if (entity_distance > min_distance && entity_distance < max_distance)
		{
			(*targets_hit)++;

			return qtrue;
		}
	}

	return qfalse;
}

// zyk: similar to the function above, but for powers with which the effect/model itself must be tested
qboolean zyk_magic_effect_can_hit_target(gentity_t* attacker, gentity_t* target, vec3_t effect_origin, int i, int min_distance, int max_distance, qboolean hit_breakable)
{
	if (attacker->s.number != i && target && target->client && target->health > 0 && zyk_can_hit_target(attacker, target) == qtrue &&
		(i > MAX_CLIENTS || (target->client->pers.connected == CON_CONNECTED && target->client->sess.sessionTeam != TEAM_SPECTATOR &&
			target->client->ps.duelInProgress == qfalse)))
	{ // zyk: target is a player or npc that can be hit by the attacker
		int player_distance = (int)Distance(effect_origin, target->client->ps.origin);

		if (player_distance > min_distance && player_distance < max_distance)
		{
			int is_ally = 0;

			if (i < level.maxclients && !attacker->NPC &&
				zyk_is_ally(attacker, target) == qtrue)
			{ // zyk: allies will not be hit by this power
				is_ally = 1;
			}

			if (OnSameTeam(attacker, target) == qtrue || npcs_on_same_team(attacker, target) == qtrue)
			{ // zyk: if one of them is npc, also check for allies
				is_ally = 1;
			}

			if (is_ally == 0)
			{ // zyk: players in bosses can only hit bosses and their helper npcs. Players not in boss battles
			  // can only hit normal enemy npcs and npcs spawned by bosses but not the bosses themselves. Magic-using npcs can hit everyone that are not their allies
				return qtrue;
			}
		}
	}
	else if (i >= MAX_CLIENTS && hit_breakable == qtrue && target && !target->client && target->health > 0 && target->takedamage == qtrue)
	{
		int entity_distance = (int)Distance(effect_origin, target->r.currentOrigin);

		if (entity_distance > min_distance && entity_distance < max_distance)
		{
			return qtrue;
		}
	}

	return qfalse;
}

extern int zyk_get_magic_index(int skill_index);

zyk_magic_element_t zyk_get_magic_element(int magic_number)
{
	int i = 0;
	zyk_magic_element_t magic_power_elements[MAX_MAGIC_POWERS] = {
		MAGICELEMENT_NONE,
		MAGICELEMENT_NONE,
		MAGICELEMENT_NONE,
		MAGICELEMENT_NONE,
		MAGICELEMENT_WATER,
		MAGICELEMENT_EARTH,
		MAGICELEMENT_FIRE,
		MAGICELEMENT_AIR,
		MAGICELEMENT_DARK,
		MAGICELEMENT_LIGHT,
		MAGICELEMENT_FIRE,
		MAGICELEMENT_AIR,
		MAGICELEMENT_NONE
	};

	for (i = 0; i < MAX_MAGIC_POWERS; i++)
	{
		if (i == magic_number)
		{ // zyk: found the magic index. Return the Element
			return magic_power_elements[i];
		}
	}

	return MAGICELEMENT_NONE;
}

// zyk: return the Spirit value of this magic element
zyk_main_quest_t zyk_get_magic_spirit(zyk_magic_element_t magic_element)
{
	zyk_magic_element_t magic_spirits[NUMBER_OF_MAGIC_SPIRITS + 1] = {
		-1, // Non-Elemental Magic does not have an Elemental Spirit
		QUEST_WATER_SPIRIT,
		QUEST_EARTH_SPIRIT,
		QUEST_FIRE_SPIRIT,
		QUEST_AIR_SPIRIT,
		QUEST_DARK_SPIRIT,
		QUEST_LIGHT_SPIRIT
	};

	return magic_spirits[magic_element];
}

// zyk: spawns the element effect above the player or npc when magic is cast
void zyk_spawn_magic_element_effect(gentity_t* ent, vec3_t effect_origin, int magic_number, int duration)
{
	char magic_element_effects[NUM_MAGIC_ELEMENTS][64] = {
		"env/small_electricity2",
		"env/water_drops_steam",
		"materials/gravel",
		"env/fire",
		"env/water_steam3",
		"force/rage2",
		"howler/sonic"
	};

	zyk_magic_element_t magic_element = zyk_get_magic_element(magic_number);
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "fx_runner");
	zyk_set_entity_field(new_ent, "targetname", "zyk_magic_element");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)effect_origin[0], (int)effect_origin[1], (int)effect_origin[2] + 56));

	new_ent->s.modelindex = G_EffectIndex(magic_element_effects[magic_element]);

	zyk_spawn_entity(new_ent);

	level.special_power_effects[new_ent->s.number] = ent->s.number;
	level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
}

extern void Jedi_Decloak(gentity_t *self);
void energy_modulator_think(gentity_t* self)
{
	if (self)
	{
		if (self->parent && self->parent->client)
		{
			// zyk: do this to prevent the model from disappearing in some places
			VectorCopy(self->parent->client->ps.origin, self->r.currentOrigin);
			trap->LinkEntity((sharedEntity_t*)self);

			self->s.boltToPlayer = self->parent->s.number + 1;

			if (self->parent->health < 1)
			{ // zyk: Gunner died. Free this entity
				self->think = G_FreeEntity;
			}
			else if (self->count != self->parent->client->pers.energy_modulator_mode)
			{ // zyk: changed mode. Remove this one
				self->think = G_FreeEntity;
			}
		}

		self->nextthink = level.time + FRAMETIME;
	}
}

void energy_modulator_spawn_model(gentity_t* ent, char *model_path)
{
	gentity_t* new_ent = G_Spawn();

	zyk_main_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_main_set_entity_field(new_ent, "origin", va("%d %d %d", ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2]));
	zyk_main_set_entity_field(new_ent, "zykmodelscale", "30");
	zyk_main_set_entity_field(new_ent, "model", G_NewString(model_path));
	zyk_main_spawn_entity(new_ent);

	new_ent->parent = ent;
	new_ent->count = ent->client->pers.energy_modulator_mode;
	new_ent->think = energy_modulator_think;
	new_ent->nextthink = level.time;
	ent->client->pers.energy_modulator_entity_id = new_ent->s.number;
}

// zyk: spawns the entity when turning it on or free it when turning it off
void zyk_energy_modulator(gentity_t* ent)
{
	if (ent->client->pers.energy_modulator_mode == 0)
	{ // zyk: if it is Off, turn it on and spawns the model
		ent->client->pers.energy_modulator_mode = 1;

		energy_modulator_spawn_model(ent, "models/map_objects/danger/ship_item04.md3");
	}
	else if (ent->client->pers.energy_modulator_mode == 1)
	{ // zyk: sets the new mode
		ent->client->pers.energy_modulator_mode = 2;

		energy_modulator_spawn_model(ent, "models/map_objects/danger/ship_item04_placed.md3");
	}
	else
	{ // zyk: if it is 2, turn it off
		ent->client->pers.energy_modulator_mode = 0;
	}
}

void zyk_spawn_ice_block(gentity_t *ent, int duration, int pitch, int yaw, int x_offset, int y_offset, int z_offset)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "65537");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->r.currentOrigin[0], (int)ent->r.currentOrigin[1], (int)ent->r.currentOrigin[2]));

	zyk_set_entity_field(new_ent, "angles", va("%d %d 0", pitch, yaw));

	if (x_offset == 0 && y_offset != 0)
	{
		zyk_set_entity_field(new_ent, "mins", va("%d -50 %d", y_offset * -1, y_offset * -1));
		zyk_set_entity_field(new_ent, "maxs", va("%d 50 %d", y_offset, y_offset));
	}
	else if (x_offset != 0 && y_offset == 0)
	{
		zyk_set_entity_field(new_ent, "mins", va("-50 %d %d", x_offset * -1, x_offset * -1));
		zyk_set_entity_field(new_ent, "maxs", va("50 %d %d", x_offset, x_offset));
	}
	else if (x_offset == 0 && y_offset == 0)
	{
		zyk_set_entity_field(new_ent, "mins", va("%d %d -50", z_offset * -1, z_offset * -1));
		zyk_set_entity_field(new_ent, "maxs", va("%d %d 50", z_offset, z_offset));
	}

	zyk_set_entity_field(new_ent, "model", "models/map_objects/rift/crystal_wall.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_ice_block");

	zyk_set_entity_field(new_ent, "zykmodelscale", "200");

	zyk_spawn_entity(new_ent);

	level.special_power_effects[new_ent->s.number] = ent->s.number;
	level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
}

void zyk_spawn_black_hole_model(gentity_t* ent, int duration, int model_scale)
{
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "65536");
	zyk_set_entity_field(new_ent, "origin", va("%f %f %f", ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2] - 24));

	zyk_set_entity_field(new_ent, "model", "models/map_objects/mp/sphere_1.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_magic_black_hole");

	zyk_set_entity_field(new_ent, "zykmodelscale", va("%d", model_scale));

	zyk_spawn_entity(new_ent);

	level.special_power_effects[new_ent->s.number] = ent->s.number;
	level.special_power_effects_timer[new_ent->s.number] = level.time + duration;

	VectorCopy(ent->r.currentOrigin, ent->client->pers.black_hole_origin);

	// zyk: remaps the sphere for Black Hole textures
	AddRemap("models/map_objects/mp/spheretwo", "textures/mp/black", level.time * 0.001);
	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
}

// zyk: fires the Boba Fett flame thrower
void Player_FireFlameThrower(gentity_t* self, qboolean is_magic)
{
	trace_t		tr;
	gentity_t* traceEnt = NULL;

	int entityList[MAX_GENTITIES];
	int numListedEntities;
	int e = 0;
	int damage = zyk_flame_thrower_damage.integer;

	vec3_t	tfrom, tto, fwd;
	vec3_t thispush_org, a;
	vec3_t mins, maxs, fwdangles, forward, right, center;
	vec3_t		origin, dir;

	int i;
	float visionArc = 120;
	float radius = 144;

	self->client->cloakDebReduce = level.time + zyk_flame_thrower_cooldown.integer;

	// zyk: Fire Magic power has more damage
	if (is_magic == qtrue)
	{
		damage += (1 * self->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC]);
	}

	origin[0] = self->r.currentOrigin[0];
	origin[1] = self->r.currentOrigin[1];
	origin[2] = self->r.currentOrigin[2] + 20.0f;

	dir[0] = (-1) * self->client->ps.viewangles[0];
	dir[2] = self->client->ps.viewangles[2];
	dir[1] = (-1) * (180 - self->client->ps.viewangles[1]);

	if ((self->client->pers.flame_thrower_timer - level.time) > 500)
		G_PlayEffectID(G_EffectIndex("boba/fthrw"), origin, dir);

	if ((self->client->pers.flame_thrower_timer - level.time) > 1250)
		G_Sound(self, CHAN_WEAPON, G_SoundIndex("sound/effects/fire_lp"));

	//Check for a direct usage on NPCs first
	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0] * radius / 2;
	tto[1] = tfrom[1] + fwd[1] * radius / 2;
	tto[2] = tfrom[2] + fwd[2] * radius / 2;

	trap->Trace(&tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	VectorCopy(self->client->ps.viewangles, fwdangles);
	AngleVectors(fwdangles, forward, right, NULL);
	VectorCopy(self->client->ps.origin, center);

	for (i = 0; i < 3; i++)
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	numListedEntities = trap->EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	while (e < numListedEntities)
	{
		traceEnt = &g_entities[entityList[e]];

		if (traceEnt)
		{ //not in the arc, don't consider it
			if (traceEnt->client)
			{
				VectorCopy(traceEnt->client->ps.origin, thispush_org);
			}
			else
			{
				VectorCopy(traceEnt->s.pos.trBase, thispush_org);
			}

			VectorCopy(self->client->ps.origin, tto);
			tto[2] += self->client->ps.viewheight;
			VectorSubtract(thispush_org, tto, a);
			vectoangles(a, a);

			if (!InFieldOfVision(self->client->ps.viewangles, visionArc, a))
			{ //only bother with arc rules if the victim is a client
				entityList[e] = ENTITYNUM_NONE;
			}
		}
		traceEnt = &g_entities[entityList[e]];
		if (traceEnt && traceEnt != self)
		{
			G_Damage(traceEnt, self, self, self->client->ps.viewangles, tr.endpos, damage, DAMAGE_NO_KNOCKBACK | DAMAGE_IGNORE_TEAM, MOD_LAVA);

			// zyk: make target catch fire
			if (traceEnt->client)
			{
				if (is_magic == qtrue)
				{
					traceEnt->client->pers.fire_bolt_hits_counter = (4 * self->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC]);
				}
				else
				{
					traceEnt->client->pers.fire_bolt_hits_counter = 10;
				}
				
				traceEnt->client->pers.fire_bolt_user_id = self->s.number;
				traceEnt->client->pers.fire_bolt_timer = level.time + 100;
				traceEnt->client->pers.player_statuses |= (1 << 29);
			}
		}
		e++;
	}
}

// zyk: clear effects of some special powers
void clear_special_power_effect(gentity_t* ent)
{
	if (level.special_power_effects[ent->s.number] != -1 && level.special_power_effects_timer[ent->s.number] < level.time)
	{
		level.special_power_effects[ent->s.number] = -1;

		// zyk: if it is a misc_model_breakable power, remove it right now
		if (Q_stricmp(ent->classname, "misc_model_breakable") == 0)
			G_FreeEntity(ent);
		else
			ent->think = G_FreeEntity;
	}
}

// zyk: Magic Sense
void magic_sense(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_MAGIC_SENSE]);

	ent->client->pers.current_magic_element = MAGICELEMENT_NONE;

	ent->client->pers.quest_power_status |= (1 << MAGIC_MAGIC_SENSE);
	ent->client->pers.magic_power_debounce_timer[MAGIC_MAGIC_SENSE] = 0;
	ent->client->pers.magic_power_hit_counter[MAGIC_MAGIC_SENSE] = 1;
	ent->client->pers.magic_power_timer[MAGIC_MAGIC_SENSE] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (9000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_MAGIC_SENSE]));
}

// zyk: Healing Area
void healing_area(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_HEALING_AREA]);
	int damage = 2 + ent->client->pers.skill_levels[SKILL_MAGIC_HEALING_AREA];

	ent->client->pers.current_magic_element = MAGICELEMENT_NONE;

	zyk_quest_effect_spawn(ent, ent, "zyk_magic_healing_area", "4", "env/red_cyc", 0, damage, 228, duration);

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_HEALING_AREA]));
}

// zyk: Enemy Weakening
void enemy_weakening(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_ENEMY_WEAKENING]);

	ent->client->pers.current_magic_element = MAGICELEMENT_NONE;

	ent->client->pers.quest_power_status |= (1 << MAGIC_ENEMY_WEAKENING);
	ent->client->pers.magic_power_debounce_timer[MAGIC_ENEMY_WEAKENING] = 0;
	ent->client->pers.magic_power_timer[MAGIC_ENEMY_WEAKENING] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_ENEMY_WEAKENING]));
}

// zyk: Dome of Damage
void dome_of_damage(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_DOME_OF_DAMAGE]);
	int damage = 4 * ent->client->pers.skill_levels[SKILL_MAGIC_DOME_OF_DAMAGE];

	ent->client->pers.current_magic_element = MAGICELEMENT_NONE;

	ent->client->pers.dome_of_damage_dmg = damage;

	ent->client->pers.quest_power_status |= (1 << MAGIC_DOME_OF_DAMAGE);
	ent->client->pers.magic_power_debounce_timer[MAGIC_DOME_OF_DAMAGE] = 0;
	ent->client->pers.magic_power_timer[MAGIC_DOME_OF_DAMAGE] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_DOME_OF_DAMAGE]));
}

// zyk: Water Magic
void water_magic(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC]);

	ent->client->pers.current_magic_element = MAGICELEMENT_WATER;

	ent->client->pers.quest_power_status |= (1 << MAGIC_WATER_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_WATER_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_WATER_MAGIC] = 1;
	ent->client->pers.magic_power_timer[MAGIC_WATER_MAGIC] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC]));
}

// zyk: Earth Magic
void earth_magic(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC]);

	ent->client->pers.current_magic_element = MAGICELEMENT_EARTH;

	ent->client->pers.quest_power_status |= (1 << MAGIC_EARTH_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_EARTH_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_EARTH_MAGIC] = 2;
	ent->client->pers.magic_power_timer[MAGIC_EARTH_MAGIC] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC]));
}

// zyk: Fire Magic
void fire_magic(gentity_t* ent) 
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC]);

	ent->client->pers.current_magic_element = MAGICELEMENT_FIRE;

	ent->client->pers.quest_power_status |= (1 << MAGIC_FIRE_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_FIRE_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_FIRE_MAGIC] = 1;
	ent->client->pers.magic_power_timer[MAGIC_FIRE_MAGIC] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC]));
}

// zyk: Air Magic
void air_magic(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_AIR_MAGIC]);

	ent->client->pers.current_magic_element = MAGICELEMENT_AIR;

	ent->client->pers.quest_power_status |= (1 << MAGIC_AIR_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_AIR_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_AIR_MAGIC] = 1;
	ent->client->pers.magic_power_timer[MAGIC_AIR_MAGIC] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_AIR_MAGIC]));
}

// zyk: Dark Magic
void dark_magic(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC]);

	ent->client->pers.current_magic_element = MAGICELEMENT_DARK;

	ent->client->pers.quest_power_status |= (1 << MAGIC_DARK_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_DARK_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_DARK_MAGIC] = 1;
	ent->client->pers.magic_power_timer[MAGIC_DARK_MAGIC] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC]));
}

extern void zyk_lightning_dome_detonate(gentity_t* ent);
void lightning_dome(gentity_t* ent, int damage)
{
	gentity_t* missile;
	vec3_t origin;
	trace_t	tr;

	VectorSet(origin, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2] - 22);

	trap->Trace(&tr, ent->client->ps.origin, NULL, NULL, origin, ent->s.number, MASK_SHOT, qfalse, 0, 0);

	missile = G_Spawn();

	G_SetOrigin(missile, origin);
	//In SP the impact actually travels as a missile based on the trace fraction, but we're
	//just going to be instant. -rww

	VectorCopy(tr.plane.normal, missile->pos1);

	missile->count = 9;

	missile->classname = "demp2_alt_proj";
	missile->s.weapon = WP_DEMP2;

	missile->think = zyk_lightning_dome_detonate;
	missile->nextthink = level.time;

	missile->splashDamage = missile->damage = damage;
	missile->splashMethodOfDeath = missile->methodOfDeath = MOD_DEMP2;

	missile->splashRadius = 768;

	missile->r.ownerNum = ent->s.number;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to ever bounce
	missile->bounceCount = 0;

	if (ent->s.number < level.maxclients)
		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/thunder_close1.mp3"));
}

//zyk: Light Magic
void light_magic(gentity_t* ent)
{
	int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC]);

	ent->client->pers.current_magic_element = MAGICELEMENT_LIGHT;

	ent->client->pers.quest_power_status |= (1 << MAGIC_LIGHT_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_LIGHT_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_LIGHT_MAGIC] = 1;
	ent->client->pers.magic_power_timer[MAGIC_LIGHT_MAGIC] = level.time + duration;

	ent->client->pers.quest_power_usage_timer = level.time + (12000 - (1000 * ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC]));
}

// zyk: return the bonus resistance. If > 0, target resist the same element. If < 0, target is taking extra effect from opposite element
float zyk_get_bonus_element_factor(gentity_t* attacker, gentity_t* target)
{
	if (attacker && target && attacker != target && attacker->client && target->client)
	{
		int magic_skill_levels[NUM_MAGIC_ELEMENTS];
		int target_skill_level = 0, attacker_skill_level = 0;
		zyk_magic_element_t target_element = target->client->pers.current_magic_element;
		zyk_magic_element_t attacker_element = attacker->client->pers.current_magic_element;

		magic_skill_levels[MAGICELEMENT_NONE] = 0;
		magic_skill_levels[MAGICELEMENT_WATER] = SKILL_MAGIC_WATER_MAGIC;
		magic_skill_levels[MAGICELEMENT_EARTH] = SKILL_MAGIC_EARTH_MAGIC;
		magic_skill_levels[MAGICELEMENT_FIRE] = SKILL_MAGIC_FIRE_MAGIC;
		magic_skill_levels[MAGICELEMENT_AIR] = SKILL_MAGIC_AIR_MAGIC;
		magic_skill_levels[MAGICELEMENT_DARK] = SKILL_MAGIC_DARK_MAGIC;
		magic_skill_levels[MAGICELEMENT_LIGHT] = SKILL_MAGIC_LIGHT_MAGIC;

		target_skill_level = target->client->pers.skill_levels[magic_skill_levels[target_element]];
		attacker_skill_level = attacker->client->pers.skill_levels[magic_skill_levels[attacker_element]];

		if (target_element == attacker_element && target_element != MAGICELEMENT_NONE)
		{ // zyk: being hit by magic from the same element. Resist based on current level of that elemental magic level subtracted by the attacker level
			return (0.10 * ((target_skill_level + 1) - attacker_skill_level));
		}
		
		if ((target_element == MAGICELEMENT_WATER && attacker_element == MAGICELEMENT_FIRE) || 
			(target_element == MAGICELEMENT_EARTH && attacker_element == MAGICELEMENT_AIR) ||
			(target_element == MAGICELEMENT_FIRE && attacker_element == MAGICELEMENT_WATER) ||
			(target_element == MAGICELEMENT_AIR && attacker_element == MAGICELEMENT_EARTH) ||
			(target_element == MAGICELEMENT_DARK && attacker_element == MAGICELEMENT_LIGHT) ||
			(target_element == MAGICELEMENT_LIGHT && attacker_element == MAGICELEMENT_DARK))
		{ // zyk: being hit by magic from the opposite element. Take extra bonus based on current level of that elemental magic level subtracted by the attacker level
			return (-0.10 * ((attacker_skill_level + 1) - target_skill_level));
		}
	}

	return 0.00;
}

// zyk: controls the quest powers stuff
extern void initialize_rpg_skills(gentity_t *ent, qboolean init_all);
extern int zyk_max_magic_power(gentity_t* ent);
void quest_power_events(gentity_t *ent)
{
	if (ent && ent->client)
	{
		if (ent->health > 0)
		{
			if (ent->client->pers.quest_power_status & (1 << MAGIC_MAGIC_SENSE))
			{
				if (ent->client->pers.magic_power_debounce_timer[MAGIC_MAGIC_SENSE] < level.time)
				{
					if (ent->client->pers.magic_power_hit_counter[MAGIC_MAGIC_SENSE] > 0)
					{
						ent->client->ps.forceAllowDeactivateTime = ent->client->pers.magic_power_timer[MAGIC_MAGIC_SENSE];
						ent->client->ps.fd.forcePowersActive |= (1 << FP_SEE);
						ent->client->ps.fd.forcePowerDuration[FP_SEE] = ent->client->pers.magic_power_timer[MAGIC_MAGIC_SENSE];

						G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/weapons/force/see.wav"));

						ent->client->pers.magic_power_hit_counter[MAGIC_MAGIC_SENSE]--;
					}

					ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_3;

					ent->client->pers.magic_power_debounce_timer[MAGIC_MAGIC_SENSE] = level.time + 100;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_MAGIC_SENSE] < level.time)
				{
					ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[SKILL_SENSE];
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_MAGIC_SENSE);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_ENEMY_WEAKENING))
			{
				if (ent->client->pers.magic_power_debounce_timer[MAGIC_ENEMY_WEAKENING] < level.time)
				{
					int zyk_it = 0;
					int targets_hit = 0;
					int max_distance = 250 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_ENEMY_WEAKENING]);
					int duration = 1500 + (500 * ent->client->pers.skill_levels[SKILL_MAGIC_ENEMY_WEAKENING]);

					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						gentity_t* player_ent = &g_entities[zyk_it];

						if (zyk_special_power_can_hit_target(ent, player_ent, zyk_it, 0, max_distance, qfalse, &targets_hit) == qtrue)
						{
							player_ent->client->pers.magic_power_debounce_timer[MAGIC_HIT_BY_ENEMY_WEAKENING] = 0;
							player_ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_ENEMY_WEAKENING] = level.time + duration;
							player_ent->client->pers.quest_power_status |= (1 << MAGIC_HIT_BY_ENEMY_WEAKENING);

							G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh10.mp3"));
						}
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_ENEMY_WEAKENING] = level.time + 500;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_ENEMY_WEAKENING] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_ENEMY_WEAKENING);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_HIT_BY_ENEMY_WEAKENING))
			{
				if (ent->client->pers.magic_power_debounce_timer[MAGIC_HIT_BY_ENEMY_WEAKENING] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_enemy_weakening", "0", "force/kothos_beam", 0, 0, 0, 1000);

					ent->client->pers.magic_power_debounce_timer[MAGIC_HIT_BY_ENEMY_WEAKENING] = level.time + 500;
				}

				if (ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_ENEMY_WEAKENING] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_HIT_BY_ENEMY_WEAKENING);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_DOME_OF_DAMAGE))
			{
				if (ent->client->pers.magic_power_debounce_timer[MAGIC_DOME_OF_DAMAGE] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_dome", "4", "env/dome", 0, ent->client->pers.dome_of_damage_dmg, 290, 1000);

					ent->client->pers.magic_power_debounce_timer[MAGIC_DOME_OF_DAMAGE] = level.time + 500;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_DOME_OF_DAMAGE] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_DOME_OF_DAMAGE);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_WATER_MAGIC))
			{
				gentity_t* target_ent = NULL;
				int zyk_it = 0;
				int max_distance = 250 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC]);
				int damage = 2 * ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC];

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_WATER_MAGIC] < level.time)
				{
					if (ent->client->pers.magic_power_hit_counter[MAGIC_WATER_MAGIC] > 0)
					{
						int duration = (ent->client->pers.magic_power_timer[MAGIC_WATER_MAGIC] - level.time)/2;
						int heal_amount = 20 * ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC];

						if ((ent->health + heal_amount) < ent->client->ps.stats[STAT_MAX_HEALTH])
							ent->health += heal_amount;
						else
							ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

						// zyk: ice block around the player
						zyk_spawn_ice_block(ent, duration, 0, 0, -140, 0, 0);
						zyk_spawn_ice_block(ent, duration, 0, 90, 140, 0, 0);
						zyk_spawn_ice_block(ent, duration, 0, 179, 0, -140, 0);
						zyk_spawn_ice_block(ent, duration, 0, -90, 0, 140, 0);
						zyk_spawn_ice_block(ent, duration, 90, 0, 0, 0, -140);
						zyk_spawn_ice_block(ent, duration, -90, 0, 0, 0, 140);

						G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/glass_tumble3.wav"));

						ent->client->pers.magic_power_hit_counter[MAGIC_WATER_MAGIC]--;
					}

					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						target_ent = &g_entities[zyk_it];

						if (target_ent && target_ent->client && ent != target_ent &&
							zyk_magic_effect_can_hit_target(ent, target_ent, ent->r.currentOrigin, zyk_it, 0, max_distance, qfalse))
						{
							zyk_quest_effect_spawn(ent, target_ent, "zyk_magic_water", "4", "world/waterfall3", 0, damage, 200, 1000);
							zyk_quest_effect_spawn(ent, target_ent, "zyk_magic_water_effect", "0", "env/water_impact", 0, 0, 0, 1000);
						}
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_WATER_MAGIC] = level.time + 500;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_WATER_MAGIC] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_WATER_MAGIC);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_EARTH_MAGIC))
			{
				gentity_t* target_ent = NULL;
				int zyk_it = 0;
				int targets_hit = 0;
				int max_distance = 250 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC]);
				int earthquake_damage = 10 * ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC];
				int damage = 3 * ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC];

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_EARTH_MAGIC] < level.time)
				{
					if (ent->client->pers.magic_power_hit_counter[MAGIC_EARTH_MAGIC] >= 2)
					{
						zyk_quest_effect_spawn(ent, ent, "zyk_tree_of_life", "1", "models/map_objects/yavin/tree10_b.md3", 0, 0, 0, 1600);

						ent->client->pers.magic_power_hit_counter[MAGIC_EARTH_MAGIC]--;
					}
					else if (ent->client->pers.magic_power_hit_counter[MAGIC_EARTH_MAGIC] > 0)
					{ // zyk: Tree of Life ended
						ent->client->pers.magic_power_hit_counter[MAGIC_EARTH_MAGIC]--;
					}

					G_ScreenShake(ent->client->ps.origin, ent, 10.0f, 2000, qtrue);
					G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/stone_break1.mp3"));

					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						target_ent = &g_entities[zyk_it];

						if (zyk_special_power_can_hit_target(ent, target_ent, zyk_it, 0, max_distance, qfalse, &targets_hit) == qtrue)
						{
							if (target_ent->client && target_ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
							{ // zyk: player can only be hit if he is on floor
								// zyk: if using Meditate taunt, remove it
								if (target_ent->client->ps.legsAnim == BOTH_MEDITATE && target_ent->client->ps.torsoAnim == BOTH_MEDITATE)
								{
									target_ent->client->ps.legsAnim = target_ent->client->ps.torsoAnim = BOTH_MEDITATE_END;
								}

								target_ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
								target_ent->client->ps.forceHandExtendTime = level.time + 800;
								target_ent->client->ps.velocity[2] += 300;
								target_ent->client->ps.forceDodgeAnim = 0;
								target_ent->client->ps.quickerGetup = qtrue;

								G_Damage(target_ent, ent, ent, NULL, NULL, earthquake_damage, 0, MOD_UNKNOWN);
							}

							if (target_ent->client && zyk_it < level.maxclients)
							{
								G_ScreenShake(target_ent->client->ps.origin, target_ent, 10.0f, 2000, qtrue);
							}

							G_Sound(target_ent, CHAN_AUTO, G_SoundIndex("sound/effects/stone_break1.mp3"));
						}

						if (target_ent && target_ent->client && ent != target_ent &&
							zyk_magic_effect_can_hit_target(ent, target_ent, ent->r.currentOrigin, zyk_it, 0, max_distance, qfalse))
						{
							zyk_quest_effect_spawn(ent, target_ent, "zyk_magic_earth", "4", "env/rock_smash", 0, damage, 100, 2000);
						}
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_EARTH_MAGIC] = level.time + 1600;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_EARTH_MAGIC] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_EARTH_MAGIC);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_FIRE_MAGIC))
			{
				gentity_t* target_ent = NULL;
				int zyk_it = 0;
				int max_distance = 250 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC]);
				int damage = 3 * ent->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC];

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_FIRE_MAGIC] < level.time)
				{
					if (ent->client->pers.magic_power_hit_counter[MAGIC_FIRE_MAGIC] > 0)
					{
						ent->client->pers.flame_thrower_timer = ent->client->pers.magic_power_timer[MAGIC_FIRE_MAGIC];

						ent->client->pers.magic_power_hit_counter[MAGIC_FIRE_MAGIC]--;
					}

					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						target_ent = &g_entities[zyk_it];

						if (target_ent && target_ent->client && ent != target_ent &&
							zyk_magic_effect_can_hit_target(ent, target_ent, ent->r.currentOrigin, zyk_it, 0, max_distance, qfalse))
						{
							zyk_quest_effect_spawn(ent, target_ent, "zyk_magic_fire", "4", "env/fire", 0, damage, 90, 1000);

							G_Sound(target_ent, CHAN_AUTO, G_SoundIndex("sound/effects/fire_lp.wav"));
						}
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_FIRE_MAGIC] = level.time + 500;
				}

				if (ent->client->cloakDebReduce < level.time)
				{ // zyk: fires the flame thrower
					Player_FireFlameThrower(ent, qtrue);
				}

				if (ent->client->pers.magic_power_timer[MAGIC_FIRE_MAGIC] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_FIRE_MAGIC);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_HIT_BY_FIRE))
			{
				if (ent->client->pers.magic_power_hit_counter[MAGIC_HIT_BY_FIRE] > 0 && ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_FIRE] < level.time)
				{
					gentity_t* fire_magic_user = &g_entities[ent->client->pers.magic_power_user_id[MAGIC_HIT_BY_FIRE]];

					if (fire_magic_user && fire_magic_user->client)
					{
						zyk_quest_effect_spawn(fire_magic_user, ent, "zyk_magic_fire_hit", "0", "env/fire", 0, 0, 0, 300);

						G_Damage(ent, fire_magic_user, fire_magic_user, NULL, NULL, fire_magic_user->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC], 0, MOD_UNKNOWN);

						G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/fire_lp.wav"));
					}

					ent->client->pers.magic_power_hit_counter[MAGIC_HIT_BY_FIRE]--;

					ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_FIRE] = level.time + 200;
				}
				else if (ent->client->pers.magic_power_hit_counter[MAGIC_HIT_BY_FIRE] == 0 && ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_FIRE] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_HIT_BY_FIRE);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_AIR_MAGIC))
			{
				if (ent->client->pers.magic_power_debounce_timer[MAGIC_AIR_MAGIC] < level.time)
				{
					gentity_t* target_ent = NULL;
					int zyk_it = 0;
					int targets_hit = 0;
					int max_distance = 250 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_AIR_MAGIC]);
					int damage = 1 + (ent->client->pers.skill_levels[SKILL_MAGIC_AIR_MAGIC] / 3);

					if (ent->client->pers.magic_power_hit_counter[MAGIC_AIR_MAGIC] > 0)
					{
						G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/vacuum.mp3"));

						ent->client->pers.magic_power_hit_counter[MAGIC_AIR_MAGIC]--;
					}

					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						target_ent = &g_entities[zyk_it];

						if (target_ent && target_ent->client && 
							zyk_special_power_can_hit_target(ent, target_ent, zyk_it, 0, max_distance, qfalse, &targets_hit) == qtrue)
						{
							static vec3_t forward;
							vec3_t dir;

							zyk_quest_effect_spawn(ent, target_ent, "zyk_magic_air", "4", "env/water_steam3", 0, damage, 100, 500);

							AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);

							VectorNormalize(forward);

							if (target_ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
								VectorScale(forward, 215.0, dir);
							else
								VectorScale(forward, 40.0, dir);

							VectorAdd(target_ent->client->ps.velocity, dir, target_ent->client->ps.velocity);
						}
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_AIR_MAGIC] = level.time + 50;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_AIR_MAGIC] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_AIR_MAGIC);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_HIT_BY_AIR))
			{
				if (ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_AIR] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_HIT_BY_AIR);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_DARK_MAGIC))
			{
				gentity_t* black_hole_target = NULL;
				int zyk_it = 0;
				int confusion_duration = 300 + (100 * ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC]);

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_DARK_MAGIC] < level.time)
				{
					if (ent->client->pers.magic_power_hit_counter[MAGIC_DARK_MAGIC] > 0)
					{
						int duration = ent->client->pers.magic_power_timer[MAGIC_DARK_MAGIC] - level.time;
						int damage = 6 * ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC];
						int radius = 390 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC]); // zyk: default distace for this effect is 540

						zyk_quest_effect_spawn(ent, ent, "zyk_magic_dark", "4", "ships/proton_impact", 100, damage, radius, duration);

						zyk_spawn_black_hole_model(ent, duration, 80);

						ent->client->pers.black_hole_distance = radius;

						ent->client->pers.magic_power_hit_counter[MAGIC_DARK_MAGIC]--;
					}
					
					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						black_hole_target = &g_entities[zyk_it];

						if (black_hole_target && black_hole_target->client && ent != black_hole_target &&
							zyk_magic_effect_can_hit_target(ent, black_hole_target, ent->client->pers.black_hole_origin, zyk_it, 0, ent->client->pers.black_hole_distance, qfalse))
						{
							vec3_t dir, forward;
							float target_distance = Distance(ent->client->pers.black_hole_origin, black_hole_target->client->ps.origin);
							float black_hole_suck_strength = 0.0;

							if (target_distance > 0.0)
							{
								black_hole_suck_strength = ((ent->client->pers.black_hole_distance * 0.7) / target_distance);
							}

							VectorSubtract(ent->client->pers.black_hole_origin, black_hole_target->client->ps.origin, forward);
							VectorNormalize(forward);

							// zyk: increases strength with which target is sucked into the black hole the closer he is to it
							if (black_hole_target->client->ps.groundEntityNum != ENTITYNUM_NONE)
							{
								black_hole_suck_strength *= 215.0;
							}
							else
							{
								black_hole_suck_strength *= 52.0;
							}

							// zyk: add a limit to the strength to prevent the target from being blown out of the black hole
							if (black_hole_suck_strength > 500.0)
							{
								black_hole_suck_strength = 500.0;
							}

							if (target_distance < 64.0)
							{ // zyk: very close to black hole center
								VectorScale(black_hole_target->client->ps.velocity, 0.4, black_hole_target->client->ps.velocity);
							}
							else
							{
								VectorScale(forward, black_hole_suck_strength, dir);
								VectorAdd(black_hole_target->client->ps.velocity, dir, black_hole_target->client->ps.velocity);
							}

							// zyk: removing emotes to prevent exploits
							if (black_hole_target->client->pers.player_statuses & (1 << 1))
							{
								black_hole_target->client->pers.player_statuses &= ~(1 << 1);
								black_hole_target->client->ps.forceHandExtendTime = level.time;
							}

							// zyk: if using Meditate taunt, remove it
							if (black_hole_target->client->ps.legsAnim == BOTH_MEDITATE && black_hole_target->client->ps.torsoAnim == BOTH_MEDITATE)
							{
								black_hole_target->client->ps.legsAnim = black_hole_target->client->ps.torsoAnim = BOTH_MEDITATE_END;
							}
						}
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_DARK_MAGIC] = level.time + 50;
				}
				
				if (ent->client->pers.magic_power_timer[MAGIC_DARK_MAGIC] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_DARK_MAGIC);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_LIGHT_MAGIC))
			{
				gentity_t* light_of_judgement_target = NULL;
				int zyk_it = 0;

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_LIGHT_MAGIC] < level.time)
				{
					if (ent->client->pers.magic_power_hit_counter[MAGIC_LIGHT_MAGIC] > 0)
					{
						int duration = ent->client->pers.magic_power_timer[MAGIC_LIGHT_MAGIC] - level.time;
						int radius = 390 + (50 * ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC]); // zyk: default distace for this effect is 540
						int damage = ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC];

						zyk_quest_effect_spawn(ent, ent, "zyk_magic_light_effect", "0", "ships/sd_exhaust", 500, 0, 0, duration);
						zyk_quest_effect_spawn(ent, ent, "zyk_magic_light", "4", "misc/possession", 500, damage, radius, duration);

						ent->client->pers.light_of_judgement_distance = radius;

						VectorCopy(ent->client->ps.origin, ent->client->pers.light_of_judgement_origin);

						// zyk: creates a lightning dome, it is the DEMP2 alt fire but bigger
						lightning_dome(ent, damage * 8);

						// zyk: protective bubble around the player
						ent->client->ps.eFlags |= EF_INVULNERABLE;
						ent->client->invulnerableTimer = ent->client->pers.magic_power_timer[MAGIC_LIGHT_MAGIC];

						ent->client->pers.magic_power_hit_counter[MAGIC_LIGHT_MAGIC]--;
					}

					if (Distance(ent->client->ps.origin, ent->client->pers.light_of_judgement_origin) < ent->client->pers.light_of_judgement_distance)
					{ // zyk: while inside the light, you slowly regen health
						int heal_amount = 1 * ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC];

						if ((ent->health + heal_amount) < ent->client->ps.stats[STAT_MAX_HEALTH])
						{
							ent->health += heal_amount;
						}
						else
						{
							ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
						}
					}

					for (zyk_it = 0; zyk_it < level.num_entities; zyk_it++)
					{
						light_of_judgement_target = &g_entities[zyk_it];

						if (light_of_judgement_target && light_of_judgement_target->client && ent != light_of_judgement_target &&
							zyk_magic_effect_can_hit_target(ent, light_of_judgement_target, ent->client->pers.light_of_judgement_origin, zyk_it, 0, ent->client->pers.light_of_judgement_distance, qfalse))
						{
							int mp_to_drain = 1 * ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC];
							int max_player_mp = zyk_max_magic_power(ent);

							// zyk: Elemental affinity
							mp_to_drain -= (mp_to_drain * zyk_get_bonus_element_factor(ent, light_of_judgement_target));

							// zyk: drains mp from target
							if (light_of_judgement_target->client->pers.magic_power >= mp_to_drain)
							{
								ent->client->pers.magic_power += mp_to_drain;
								light_of_judgement_target->client->pers.magic_power -= mp_to_drain;
							}
							else if (light_of_judgement_target->client->pers.magic_power > 0)
							{
								ent->client->pers.magic_power += light_of_judgement_target->client->pers.magic_power;
								light_of_judgement_target->client->pers.magic_power = 0;
							}

							if (ent->client->pers.magic_power > max_player_mp)
							{
								ent->client->pers.magic_power = max_player_mp;
							}

							// zyk: setting confuse effect
							// zyk: removing emotes to prevent exploits
							if (light_of_judgement_target->client->pers.player_statuses & (1 << 1))
							{
								light_of_judgement_target->client->pers.player_statuses &= ~(1 << 1);
								light_of_judgement_target->client->ps.forceHandExtendTime = level.time;
							}

							// zyk: if using Meditate taunt, remove it
							if (light_of_judgement_target->client->ps.legsAnim == BOTH_MEDITATE && light_of_judgement_target->client->ps.torsoAnim == BOTH_MEDITATE)
							{
								light_of_judgement_target->client->ps.legsAnim = light_of_judgement_target->client->ps.torsoAnim = BOTH_MEDITATE_END;
							}

							if (light_of_judgement_target->client->jetPackOn)
							{
								Jetpack_Off(light_of_judgement_target);
							}

							// zyk: confuses the target
							if (light_of_judgement_target->client->ps.forceDodgeAnim != BOTH_SONICPAIN_END)
							{
								light_of_judgement_target->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
								light_of_judgement_target->client->ps.forceDodgeAnim = BOTH_SONICPAIN_END;
								light_of_judgement_target->client->ps.forceHandExtendTime = level.time + 1500;

								// zyk: target cant attack while confused
								light_of_judgement_target->client->ps.weaponTime = 1500;
							}
						}
					}

					send_rpg_events(2000);

					ent->client->pers.magic_power_debounce_timer[MAGIC_LIGHT_MAGIC] = level.time + 50;
				}

				if (ent->client->pers.magic_power_timer[MAGIC_LIGHT_MAGIC] < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << MAGIC_LIGHT_MAGIC);
				}
			}
		}
		/*
		else if (!ent->NPC && ent->client->pers.quest_power_status & (1 << 10) && ent->client->pers.quest_power1_timer < level.time && 
				!(ent->client->ps.eFlags & EF_DISINTEGRATION)) 
		{ // zyk: Resurrection Power
			ent->r.contents = CONTENTS_BODY;
			ent->client->ps.pm_type = PM_NORMAL;
			ent->client->ps.fallingToDeath = 0;
			ent->client->noCorpse = qtrue;
			ent->client->ps.eFlags &= ~EF_NODRAW;
			ent->client->ps.eFlags2 &= ~EF2_HELD_BY_MONSTER;
			ent->flags = 0;
			ent->die = player_die; // zyk: must set this function again
			initialize_rpg_skills(ent);
			ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
			ent->client->ps.jetpackFuel = 100;
			ent->client->ps.cloakFuel = 100;
			ent->client->pers.quest_power_status &= ~(1 << 10);
		}
		*/
	}
}

// zyk: damages target player with Fire Bolt flames
void fire_bolt_hits(gentity_t* ent)
{
	if (ent && ent->client && ent->health > 0 && ent->client->pers.player_statuses & (1 << 29) && ent->client->pers.fire_bolt_hits_counter > 0 &&
		ent->client->pers.fire_bolt_timer < level.time)
	{
		gentity_t* fire_bolt_user = &g_entities[ent->client->pers.fire_bolt_user_id];

		zyk_quest_effect_spawn(fire_bolt_user, ent, "zyk_effect_fire_bolt_hit", "0", "env/fire", 0, 0, 0, 300);

		G_Damage(ent, fire_bolt_user, fire_bolt_user, NULL, NULL, 4, 0, MOD_UNKNOWN);

		ent->client->pers.fire_bolt_hits_counter--;
		ent->client->pers.fire_bolt_timer = level.time + 200;

		// zyk: no more do fire bolt damage if counter is 0
		if (ent->client->pers.fire_bolt_hits_counter == 0)
			ent->client->pers.player_statuses &= ~(1 << 29);
	}
}

void zyk_print_custom_quest_info(gentity_t *ent)
{
	// zyk: show mission fields when player uses /customquest to print mission fields
	if (ent->client->pers.custom_quest_print > 0 && ent->client->pers.custom_quest_print_timer < level.time)
	{
		char mission_content[MAX_STRING_CHARS];
		int i = 2 * MAX_MISSION_FIELD_LINES * (ent->client->pers.custom_quest_print - 1);
		int number_of_lines = MAX_MISSION_FIELD_LINES * (ent->client->pers.custom_quest_print - 1);
		int quest_number = ent->client->pers.custom_quest_quest_number;
		int mission_number = ent->client->pers.custom_quest_mission_number;
		qboolean stop_printing = qtrue;

		strcpy(mission_content, "");

		while (i < level.zyk_custom_quest_mission_values_count[quest_number][mission_number])
		{
			if (number_of_lines == MAX_MISSION_FIELD_LINES * ent->client->pers.custom_quest_print)
			{ // zyk: max of mission field lines per string array index
				stop_printing = qfalse;
				break;
			}

			strcpy(mission_content, va("%s^3%s: ^7%s\n", mission_content, level.zyk_custom_quest_missions[quest_number][mission_number][i], level.zyk_custom_quest_missions[quest_number][mission_number][i + 1]));
			number_of_lines++;

			i += 2;
		}

		if (stop_printing == qtrue)
		{ // zyk: all info was printed, stop printing
			ent->client->pers.custom_quest_print = 0;
		}
		else
		{
			ent->client->pers.custom_quest_print++;
		}

		// zyk: sends all mission info to client if there is info to print
		if (Q_stricmp(mission_content, "") != 0)
			trap->SendServerCommand(ent->s.number, va("print \"%s\"", mission_content));

		// zyk: interval between each time part of the info is sent
		ent->client->pers.custom_quest_print_timer = level.time + 200;
	}
}

// zyk: checks if the player has already all artifacts
extern void save_account(gentity_t *ent, qboolean save_char_file);

// zyk: backup player force powers
void player_backup_force(gentity_t *ent)
{
	int i = 0;

	ent->client->pers.zyk_saved_force_powers = ent->client->ps.fd.forcePowersKnown;

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->pers.zyk_saved_force_power_levels[i] = ent->client->ps.fd.forcePowerLevel[i];
	}
}

// zyk: restore player force powers
void player_restore_force(gentity_t *ent)
{
	int i = 0;

	if (ent->client->pers.player_statuses & (1 << 27))
	{ // zyk: do not restore force to players that died in a Duel Tournament duel, because the force was already restored
		return;
	}

	ent->client->ps.fd.forcePowersKnown = ent->client->pers.zyk_saved_force_powers;

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowerLevel[i] = ent->client->pers.zyk_saved_force_power_levels[i];
	}

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
}

// zyk: finished the duel tournament
void duel_tournament_end()
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		level.duel_players[i] = -1;
		level.duel_allies[i] = -1;
	}

	for (i = 0; i < MAX_DUEL_MATCHES; i++)
	{
		level.duel_matches[i][0] = -1;
		level.duel_matches[i][1] = -1;
		level.duel_matches[i][2] = 0;
		level.duel_matches[i][3] = 0;
	}

	if (level.duel_tournament_model_id != -1)
	{
		G_FreeEntity(&g_entities[level.duel_tournament_model_id]);
		level.duel_tournament_model_id = -1;
	}

	level.duel_tournament_mode = 0;
	level.duel_tournament_rounds = 0;
	level.duelists_quantity = 0;
	level.duel_matches_quantity = 0;
	level.duel_matches_done = 0;
	level.duelist_1_id = -1;
	level.duelist_2_id = -1;
	level.duelist_1_ally_id = -1;
	level.duelist_2_ally_id = -1;
}

// zyk: prepare duelist for duel
void duel_tournament_prepare(gentity_t *ent)
{
	int i = 0;

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
	}

	ent->client->ps.ammo[AMMO_BLASTER] = 0;
	ent->client->ps.ammo[AMMO_POWERCELL] = 0;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = 0;
	ent->client->ps.ammo[AMMO_ROCKETS] = 0;
	ent->client->ps.ammo[AMMO_THERMAL] = 0;
	ent->client->ps.ammo[AMMO_TRIPMINE] = 0;
	ent->client->ps.ammo[AMMO_DETPACK] = 0;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_NONE);
	ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

	// zyk: removing the seeker drone in case if is activated
	if (ent->client->ps.droneExistTime > (level.time + 5000))
	{
		ent->client->ps.droneExistTime = level.time + 5000;
	}

	Jedi_Decloak(ent);

	// zyk: disable jetpack
	Jetpack_Off(ent);

	ent->client->ps.jetpackFuel = 100;
	ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;

	// zyk: giving saber to the duelist
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
	ent->client->ps.weapon = WP_SABER;
	ent->s.weapon = WP_SABER;

	// zyk: reset hp and shield of duelist
	ent->health = 100;
	ent->client->ps.stats[STAT_ARMOR] = 100;

	if ((level.duelist_1_id == ent->s.number && level.duelist_1_ally_id != -1) || (level.duelist_2_id == ent->s.number && level.duelist_2_ally_id != -1) || 
		level.duelist_1_ally_id == ent->s.number || level.duelist_2_ally_id == ent->s.number)
	{
		ent->client->ps.stats[STAT_ARMOR] = 0;
	}

	player_backup_force(ent);

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		if (i != FP_LEVITATION && i != FP_SABER_OFFENSE && i != FP_SABER_DEFENSE)
		{ // zyk: cannot use any force powers, except Jump, Saber Attack and Saber Defense
			if ((ent->client->ps.fd.forcePowersActive & (1 << i)))
			{//turn it off
				WP_ForcePowerStop(ent, (forcePowers_t)i);
			}

			ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			ent->client->ps.fd.forcePowerDuration[i] = 0;
		}
	}

	// zyk: removing powerups
	ent->client->ps.powerups[PW_FORCE_BOON] = 0;
	ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
	ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;

	// zyk: removing flag that is used to test if player died in a duel
	ent->client->pers.player_statuses &= ~(1 << 27);

	// zyk: stop any movement
	VectorSet(ent->client->ps.velocity, 0, 0, 0);
}

// zyk: generate the teams and validates them
int duel_tournament_generate_teams()
{
	int i = 0;
	int number_of_teams = level.duelists_quantity;

	// zyk: validating the teams
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.duel_players[i] == -1 || (level.duel_allies[i] != -1 && (level.duel_allies[level.duel_allies[i]] != i || level.duel_players[level.duel_allies[i]] == -1)))
		{ // zyk: this team is not valid. Remove the ally
			level.duel_allies[i] = -1;
		}
	}

	// zyk: counting the teams
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.duel_allies[i] != -1 && i < level.duel_allies[i] && level.duel_allies[level.duel_allies[i]] == i)
		{ // zyk: both players added themselves as allies
			// zyk: must count both as a single player (team)
			number_of_teams--;
		}
	}

	level.duel_number_of_teams = number_of_teams;

	return number_of_teams;
}

// zyk: generates the table with all the tournament matches
void duel_tournament_generate_match_table()
{
	int i = 0;
	int last_opponent_id = -1;
	int number_of_filled_positions = 0;
	int max_filled_positions = level.duel_number_of_teams - 1; // zyk: used to fill the player in current iteration in the table. It will always be the number of duelists (or teams) minus one
	int temp_matches[MAX_DUEL_MATCHES][2];
	int temp_remaining_matches = 0;

	level.duel_matches_quantity = 0;
	level.duel_matches_done = 0;

	for (i = 0; i < MAX_DUEL_MATCHES; i++)
	{ // zyk: initializing temporary array of matches
		temp_matches[i][0] = -1;
		temp_matches[i][1] = -1;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		int j = 0;

		if (level.duel_players[i] != -1)
		{ // zyk: player joined the tournament
			// zyk: calculates matches for alone players or for the teams by using the team leader, which is the lower id
			if (level.duel_allies[i] == -1 || i < level.duel_allies[i])
			{
				last_opponent_id = -1;

				for (j = 0; j < MAX_DUEL_MATCHES; j++)
				{
					if (number_of_filled_positions >= max_filled_positions)
					{
						number_of_filled_positions = 0;
						break;
					}

					if (temp_matches[j][0] == -1)
					{
						temp_matches[j][0] = i;
						number_of_filled_positions++;
					}
					else if (temp_matches[j][1] == -1 && last_opponent_id != temp_matches[j][0] && level.duel_allies[i] != temp_matches[j][0])
					{ // zyk: will not the same opponent again (last_opponent_id) and will not add ally as opponent
						last_opponent_id = temp_matches[j][0];
						temp_matches[j][1] = i;
						number_of_filled_positions++;
						level.duel_matches_quantity++;
					}
				}
			}
		}
	}

	temp_remaining_matches = level.duel_matches_quantity;

	// zyk: generating the ramdomized array with the matches of the tournament
	for (i = 0; i < level.duel_matches_quantity; i++)
	{
		int duel_chosen_index = Q_irand(0, (temp_remaining_matches - 1));
		int j = 0;

		level.duel_matches[i][0] = temp_matches[duel_chosen_index][0];
		level.duel_matches[i][1] = temp_matches[duel_chosen_index][1];
		level.duel_matches[i][2] = 0;
		level.duel_matches[i][3] = 0;

		for (j = (duel_chosen_index + 1); j < temp_remaining_matches; j++)
		{ // zyk: updating the match table to move all duels after the duel_chosen_index one index lower
			temp_matches[j - 1][0] = temp_matches[j][0];
			temp_matches[j - 1][1] = temp_matches[j][1];
		}

		temp_remaining_matches--;
	}
}

// zyk: gives prize to the winner
void duel_tournament_prize(gentity_t *ent)
{
	if (ent->health < 1)
	{ // zyk: if he is dead, respawn him so he can receive his prize
		ClientRespawn(ent);
	}

	ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 40000;

	if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
	{
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 40000;
	}
	else if (ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
	{
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 40000;
	}
	
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL) | (1 << WP_BLASTER) | (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
	ent->client->ps.ammo[AMMO_BLASTER] = zyk_max_blaster_pack_ammo.integer;
	ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN) | (1 << HI_SEEKER) | (1 << HI_MEDPAC_BIG);

	ent->client->ps.jetpackFuel = 100;
	ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
}

void duel_tournament_generate_leaderboard(char *filename, char *netname)
{
	level.duel_leaderboard_add_ally = qfalse;
	level.duel_leaderboard_timer = level.time + 500;
	strcpy(level.duel_leaderboard_acc, filename);
	strcpy(level.duel_leaderboard_name, netname);
	level.duel_leaderboard_step = 1;
}

// zyk: determines who is the tournament winner
extern void add_credits(gentity_t *ent, int credits);
void duel_tournament_winner()
{
	gentity_t *ent = NULL;
	int max_score = -1;
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.duel_players[i] != -1)
		{
			if (level.duel_players[i] > max_score)
			{ // zyk: player is in tournament and his score is higher than max_score, so for now he is the max score
				max_score = level.duel_players[i];
				ent = &g_entities[i];
			}
			else if (level.duel_players[i] == max_score && ent && level.duel_players_hp[i] > level.duel_players_hp[ent->s.number])
			{ // zyk: this guy has the same score as the max score guy. Test the remaining hp of all duels to untie
				ent = &g_entities[i];
			}
		}
	}

	if (ent)
	{ // zyk: found a winner
		gentity_t *ally = NULL;
		char winner_info[128];

		if (level.duel_allies[ent->s.number] != -1)
		{
			ally = &g_entities[level.duel_allies[ent->s.number]];
		}

		duel_tournament_prize(ent);

		// zyk: calculating the new leaderboard if this winner is logged in his account
		if (ent->client->sess.amrpgmode > 0)
		{
			duel_tournament_generate_leaderboard(G_NewString(ent->client->sess.filename), G_NewString(ent->client->pers.netname));
		}

		if (ally)
		{
			duel_tournament_prize(ally);

			if (ally->client->sess.amrpgmode > 0)
			{
				if (ent->client->sess.amrpgmode > 0)
				{
					level.duel_leaderboard_add_ally = qtrue;
					strcpy(level.duel_leaderboard_ally_acc, ally->client->sess.filename);
					strcpy(level.duel_leaderboard_ally_name, ally->client->pers.netname);
				}
				else
				{ // zyk: if only the ally is logged, generate only for the ally
					duel_tournament_generate_leaderboard(G_NewString(ally->client->sess.filename), G_NewString(ally->client->pers.netname));
				}
			}

			strcpy(winner_info, va("%s ^7/ %s", ent->client->pers.netname, ally->client->pers.netname));
		}
		else
		{
			strcpy(winner_info, ent->client->pers.netname);
		}

		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Winner is: %s^7. Prize: force power-ups, some guns and items\"", winner_info));
	}
	else
	{
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7No one is winner\"");
	}
}

// zyk: returns the amount of hp and shield in a string, it is the total hp and shield of a team or single duelist in Duel Tournament
char *duel_tournament_remaining_health(gentity_t *ent)
{
	char health_info[128];
	gentity_t *ally = NULL;

	if (level.duel_allies[ent->s.number] != -1)
	{
		ally = &g_entities[level.duel_allies[ent->s.number]];
	}

	strcpy(health_info, "");

	if (level.duel_tournament_mode == 4)
	{
		if (!(ent->client->pers.player_statuses & (1 << 27)))
		{ // zyk: show health if the player did not die in duel
			strcpy(health_info, va(" ^1%d^7/^2%d^7 ", ent->health, ent->client->ps.stats[STAT_ARMOR]));
		}

		if (ally && !(ally->client->pers.player_statuses & (1 << 27)))
		{ // zyk: show health if the ally did not die in duel
			strcpy(health_info, va("%s ^1%d^7/^2%d^7", health_info, ally->health, ally->client->ps.stats[STAT_ARMOR]));
		}
	}

	return G_NewString(health_info);
}

// zyk: sums the score and hp score to a single duelist or to a team
void duel_tournament_give_score(gentity_t *ent, int score)
{
	gentity_t *ally = NULL;

	if (level.duel_allies[ent->s.number] != -1)
	{
		ally = &g_entities[level.duel_allies[ent->s.number]];
	}

	level.duel_players[ent->s.number] += score;
	if (level.duel_tournament_mode == 4 && !(ent->client->pers.player_statuses & (1 << 27)))
	{ // zyk: add hp score if he did not die in duel
		level.duel_players_hp[ent->s.number] += (ent->health + ent->client->ps.stats[STAT_ARMOR]);
	}

	if (ally)
	{ // zyk: both players must have the same score and the same hp score
		level.duel_players[ally->s.number] = level.duel_players[ent->s.number];

		if (level.duel_tournament_mode == 4 && !(ally->client->pers.player_statuses & (1 << 27)))
		{ // zyk: add hp score if he did not die in duel
			level.duel_players_hp[ent->s.number] += (ally->health + ally->client->ps.stats[STAT_ARMOR]);
		}

		level.duel_players_hp[ally->s.number] = level.duel_players_hp[ent->s.number];
	}
}

// zyk: sets the winner of the Duel Tournament match
void duel_tournament_set_match_winner(gentity_t *winner)
{
	gentity_t *first_duelist = &g_entities[level.duelist_1_id];
	gentity_t *second_duelist = &g_entities[level.duelist_2_id];
	gentity_t *first_duelist_ally = NULL;
	gentity_t *second_duelist_ally = NULL;

	char ally_name[36];

	strcpy(ally_name, "");

	if (level.duelist_1_ally_id != -1)
	{
		first_duelist_ally = &g_entities[level.duelist_1_ally_id];
	}

	if (level.duelist_2_ally_id != -1)
	{
		second_duelist_ally = &g_entities[level.duelist_2_ally_id];
	}

	// zyk: setting score
	if (winner && level.duel_matches[level.duel_matches_done][0] == winner->s.number)
	{ // zyk: first duelist won
		level.duel_matches[level.duel_matches_done][2]++;

		duel_tournament_give_score(winner, 3);
	}
	else if (winner && level.duel_matches[level.duel_matches_done][1] == winner->s.number)
	{ // zyk: second duelist won
		level.duel_matches[level.duel_matches_done][3]++;

		duel_tournament_give_score(winner, 3);
	}
	else
	{ // zyk: round tied
		duel_tournament_give_score(first_duelist, 1);
		duel_tournament_give_score(second_duelist, 1);
	}

	// zyk: showing round win message
	if (winner)
	{
		gentity_t *winner_ally = NULL;

		if (first_duelist_ally && level.duelist_1_id == winner->s.number)
		{
			winner_ally = first_duelist_ally;
		}
		else if (second_duelist_ally && level.duelist_2_id == winner->s.number)
		{
			winner_ally = second_duelist_ally;
		}

		if (winner_ally)
		{
			strcpy(ally_name, va("^7 / %s", winner_ally->client->pers.netname));
		}

		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s%s ^7wins! %s\"", winner->client->pers.netname, ally_name, duel_tournament_remaining_health(winner)));
	}
	else
	{
		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Tie! %s %s\"", duel_tournament_remaining_health(first_duelist), duel_tournament_remaining_health(second_duelist)));
	}

	// zyk: this match ended. Set this variable to get next match
	level.duel_matches_done++;
}

void duel_tournament_protect_duelists(gentity_t *duelist_1, gentity_t *duelist_2, gentity_t *duelist_1_ally, gentity_t *duelist_2_ally)
{
	duelist_1->client->ps.eFlags |= EF_INVULNERABLE;
	duelist_1->client->invulnerableTimer = level.time + DUEL_TOURNAMENT_PROTECTION_TIME;

	duelist_2->client->ps.eFlags |= EF_INVULNERABLE;
	duelist_2->client->invulnerableTimer = level.time + DUEL_TOURNAMENT_PROTECTION_TIME;

	if (duelist_1_ally)
	{
		duelist_1_ally->client->ps.eFlags |= EF_INVULNERABLE;
		duelist_1_ally->client->invulnerableTimer = level.time + DUEL_TOURNAMENT_PROTECTION_TIME;
	}

	if (duelist_2_ally)
	{
		duelist_2_ally->client->ps.eFlags |= EF_INVULNERABLE;
		duelist_2_ally->client->invulnerableTimer = level.time + DUEL_TOURNAMENT_PROTECTION_TIME;
	}
}

qboolean duel_tournament_valid_duelist(gentity_t *ent)
{
	if (ent && ent->client && ent->client->pers.connected == CON_CONNECTED &&
		ent->client->sess.sessionTeam != TEAM_SPECTATOR && level.duel_players[ent->s.number] != -1)
	{ // zyk: valid player
		return qtrue;
	}

	return qfalse;
}

// zyk: validates duelists in Duel Tournament
qboolean duel_tournament_validate_duelists()
{
	gentity_t *first_duelist = &g_entities[level.duelist_1_id];
	gentity_t *second_duelist = &g_entities[level.duelist_2_id];
	gentity_t *first_duelist_ally = NULL;
	gentity_t *second_duelist_ally = NULL;

	qboolean first_valid = qfalse;
	qboolean first_valid_ally = qfalse;
	qboolean second_valid = qfalse;
	qboolean second_valid_ally = qfalse;

	if (level.duelist_1_ally_id != -1)
	{
		first_duelist_ally = &g_entities[level.duelist_1_ally_id];
	}

	if (level.duelist_2_ally_id != -1)
	{
		second_duelist_ally = &g_entities[level.duelist_2_ally_id];
	}

	// zyk: removing duelists from private duels
	if (first_duelist->client->ps.duelInProgress == qtrue)
	{
		first_duelist->client->ps.stats[STAT_HEALTH] = first_duelist->health = -999;

		player_die(first_duelist, first_duelist, first_duelist, 100000, MOD_SUICIDE);
	}

	if (second_duelist->client->ps.duelInProgress == qtrue)
	{
		second_duelist->client->ps.stats[STAT_HEALTH] = second_duelist->health = -999;

		player_die(second_duelist, second_duelist, second_duelist, 100000, MOD_SUICIDE);
	}

	if (first_duelist_ally && first_duelist_ally->client->ps.duelInProgress == qtrue)
	{
		first_duelist_ally->client->ps.stats[STAT_HEALTH] = first_duelist_ally->health = -999;

		player_die(first_duelist_ally, first_duelist_ally, first_duelist_ally, 100000, MOD_SUICIDE);
	}

	if (second_duelist_ally && second_duelist_ally->client->ps.duelInProgress == qtrue)
	{
		second_duelist_ally->client->ps.stats[STAT_HEALTH] = second_duelist_ally->health = -999;

		player_die(second_duelist_ally, second_duelist_ally, second_duelist_ally, 100000, MOD_SUICIDE);
	}

	// zyk: testing if duelists are still valid
	first_valid = duel_tournament_valid_duelist(first_duelist);
	first_valid_ally = duel_tournament_valid_duelist(first_duelist_ally);
	second_valid = duel_tournament_valid_duelist(second_duelist);
	second_valid_ally = duel_tournament_valid_duelist(second_duelist_ally);

	// zyk: if the main team members (the ones saved in level.duel_matches) of each team are no longer valid, make the ally a main member
	if (first_valid == qfalse && first_valid_ally == qtrue)
	{
		level.duel_matches[level.duel_matches_done][0] = level.duelist_1_ally_id;

		level.duel_allies[level.duelist_1_ally_id] = -1;
		level.duel_allies[level.duelist_1_id] = -1;

		level.duelist_1_id = level.duelist_1_ally_id;
		level.duelist_1_ally_id = -1;

		first_duelist = &g_entities[level.duelist_1_id];
		first_duelist_ally = NULL;
	}

	if (second_valid == qfalse && second_valid_ally == qtrue)
	{
		level.duel_matches[level.duel_matches_done][1] = level.duelist_2_ally_id;

		level.duel_allies[level.duelist_2_ally_id] = -1;
		level.duel_allies[level.duelist_2_id] = -1;

		level.duelist_2_id = level.duelist_2_ally_id;
		level.duelist_2_ally_id = -1;

		second_duelist = &g_entities[level.duelist_2_id];
		second_duelist_ally = NULL;
	}

	// zyk: in only main member is valid, removes ally (if he has one)
	if (first_valid == qtrue && first_valid_ally == qfalse)
	{
		if (level.duelist_1_ally_id != -1)
		{
			level.duel_allies[level.duelist_1_ally_id] = -1;
			level.duel_allies[level.duelist_1_id] = -1;
		}

		level.duelist_1_ally_id = -1;
	}

	if (second_valid == qtrue && second_valid_ally == qfalse)
	{
		if (level.duelist_2_ally_id != -1)
		{
			level.duel_allies[level.duelist_2_ally_id] = -1;
			level.duel_allies[level.duelist_2_id] = -1;
		}

		level.duelist_2_ally_id = -1;
	}

	if ((first_valid == qtrue || first_valid_ally == qtrue) && (second_valid == qtrue || second_valid_ally == qtrue))
	{ // zyk: valid match
		return qtrue;
	}
	
	if ((first_valid == qtrue || first_valid_ally == qtrue) && second_valid == qfalse && second_valid_ally == qfalse)
	{ // zyk: only first team valid. Gives score to it
		duel_tournament_set_match_winner(first_duelist);
	}
	else if ((second_valid == qtrue || second_valid_ally == qtrue) && first_valid == qfalse && first_valid_ally == qfalse)
	{ // zyk: only second team valid. Gives score to it
		duel_tournament_set_match_winner(second_duelist);
	}
	else
	{ // zyk: both teams invalid
		duel_tournament_set_match_winner(NULL);
	}

	return qfalse;
}

// zyk: finishes the Sniper Battle
void sniper_battle_end()
{
	int i = 0;

	level.sniper_mode = 0;
	level.sniper_mode_quantity = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.sniper_players[i] != -1)
		{ // zyk: restoring default guns and force powers to this player
			gentity_t *ent = &g_entities[i];

			ent->client->ps.fd.forceDeactivateAll = 0;

			WP_InitForcePowers(ent);

			ent->client->ps.fd.forcePowerMax = zyk_max_force_power.integer;

			if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		}

		level.sniper_players[i] = -1;
	}
}

// zyk: sets the sniper gun with full ammo for players and remove everything else from them
void sniper_battle_prepare()
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		gentity_t *ent = &g_entities[i];

		if (level.sniper_players[i] != -1)
		{ // zyk: a player in the Sniper Battle. Gives disruptor with full ammo and a jetpack
			if (ent->health < 1)
			{ // zyk: respawn him if he is dead
				ClientRespawn(ent);
			}

			ent->client->ps.stats[STAT_WEAPONS] = 0;
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE) | (1 << WP_DISRUPTOR);
			ent->client->ps.weapon = WP_MELEE;

			ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;

			ent->health = 100;
			ent->client->ps.stats[STAT_ARMOR] = 100;

			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_JETPACK);
			ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

			// zyk: removing the seeker drone in case if is activated
			if (ent->client->ps.droneExistTime > (level.time + 5000))
			{
				ent->client->ps.droneExistTime = level.time + 5000;
			}

			ent->client->ps.jetpackFuel = 100;
			ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
			
			// zyk: cannot use any force powers
			ent->client->ps.fd.forcePowerLevel[FP_LEVITATION] = FORCE_LEVEL_1;
			ent->client->ps.fd.forceDeactivateAll = 1;
			ent->client->ps.fd.forcePower = 0;
			ent->client->ps.fd.forcePowerMax = 0;
		}
	}
}

// zyk: shows the winner of the Sniper Battle
void sniper_battle_winner()
{
	int i = 0;
	gentity_t *ent = NULL;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.sniper_players[i] != -1)
		{
			ent = &g_entities[i];
			break;
		}
	}

	if (ent)
	{
		ent->client->ps.fd.forcePowerMax = zyk_max_force_power.integer;

		ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 20000;
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 20000;
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 20000;

		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER) | (1 << WP_BLASTER) | (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
		ent->client->ps.ammo[AMMO_BLASTER] = zyk_max_blaster_pack_ammo.integer;
		ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN) | (1 << HI_SEEKER) | (1 << HI_MEDPAC_BIG);

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		trap->SendServerCommand(-1, va("chat \"^3Sniper Battle: ^7%s ^7is the winner! Kills: %d\"", ent->client->pers.netname, level.sniper_players[ent->s.number]));
	}
	else
	{
		trap->SendServerCommand(-1, "chat \"^3Sniper Battle: ^7No one is the winner!\"");
	}
}

// zyk: finishes the RPG LMS
void rpg_lms_end()
{
	int i = 0;

	level.rpg_lms_mode = 0;
	level.rpg_lms_quantity = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		level.rpg_lms_players[i] = -1;
	}
}

// zyk: prepares the rpg players for the battle
void rpg_lms_prepare()
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		gentity_t *ent = &g_entities[i];

		if (level.rpg_lms_players[i] != -1)
		{ // zyk: a player in the RPG LMS
			if (ent->health < 1)
			{ // zyk: respawn him if he is dead
				ClientRespawn(ent);
			}
			else
			{
				initialize_rpg_skills(ent, qtrue);
			}
		}
	}
}

// zyk: shows the winner of the RPG LMS
void rpg_lms_winner()
{
	int i = 0;
	int credits = 1000;
	gentity_t *ent = NULL;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.rpg_lms_players[i] != -1)
		{
			ent = &g_entities[i];
			break;
		}
	}

	if (ent)
	{
		add_credits(ent, credits);

		save_account(ent, qtrue);

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		trap->SendServerCommand(-1, va("chat \"^3RPG LMS: ^7%s ^7is the winner! prize: %d credits! Kills: %d\"", ent->client->pers.netname, credits, level.rpg_lms_players[ent->s.number]));
	}
	else
	{
		trap->SendServerCommand(-1, "chat \"^3RPG LMS: ^7No one is the winner!\"");
	}
}

// zyk: finishes the melee battle
void melee_battle_end()
{
	int i = 0;

	level.melee_mode = 0;
	level.melee_mode_quantity = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.melee_players[i] != -1)
		{ // zyk: restoring default guns and force powers to this player
			gentity_t *ent = &g_entities[i];

			WP_InitForcePowers(ent);

			if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		}

		level.melee_players[i] = -1;
	}

	if (level.melee_model_id != -1)
	{
		G_FreeEntity(&g_entities[level.melee_model_id]);
		level.melee_model_id = -1;
	}
}

// zyk: prepares players to fight in Melee Battle
void melee_battle_prepare()
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		gentity_t *ent = &g_entities[i];

		if (level.melee_players[i] != -1)
		{ // zyk: a player in the Melee Battle
			vec3_t origin;

			if (ent->health < 1)
			{ // zyk: respawn him if he is dead
				ClientRespawn(ent);
			}

			ent->client->ps.stats[STAT_WEAPONS] = 0;
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
			ent->client->ps.weapon = WP_MELEE;

			ent->health = 100;
			ent->client->ps.stats[STAT_ARMOR] = 100;

			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_BINOCULARS);
			ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

			// zyk: disable jetpack at the start of a Melee Battle
			if (ent->client->jetPackOn)
			{
				Jetpack_Off(ent);
			}

			// zyk: removing the seeker drone in case if is activated
			if (ent->client->ps.droneExistTime > (level.time + 5000))
			{
				ent->client->ps.droneExistTime = level.time + 5000;
			}

			// zyk: cannot use any force powers, except Jump
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PUSH);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PULL);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SPEED);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_ABSORB);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_HEAL);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PROTECT);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TELEPATHY);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_HEAL);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LIGHTNING);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_GRIP);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_DRAIN);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_RAGE);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_FORCE);

			// zyk: stop jumping to avoid falling from the platform
			ent->client->ps.fd.forcePowersActive &= ~(1 << FP_LEVITATION);

			VectorSet(origin, level.melee_mode_origin[0] - 120 + ((i % 6) * 45), level.melee_mode_origin[1] - 120 + ((i/6) * 45), level.melee_mode_origin[2] + 50);

			zyk_TeleportPlayer(ent, origin, ent->client->ps.viewangles);
		}
	}
}

// zyk: shows the winner of the Melee Battle
void melee_battle_winner()
{
	int i = 0;
	gentity_t *ent = NULL;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.melee_players[i] != -1)
		{
			ent = &g_entities[i];
			break;
		}
	}

	if (ent)
	{
		ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 20000;
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 20000;
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 20000;

		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER) | (1 << WP_BLASTER) | (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
		ent->client->ps.ammo[AMMO_BLASTER] = zyk_max_blaster_pack_ammo.integer;
		ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN) | (1 << HI_SEEKER) | (1 << HI_MEDPAC_BIG);

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7is the winner! Kills: %d\"", ent->client->pers.netname, level.melee_players[ent->s.number]));
	}
	else
	{
		trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7No one is the winner!\"");
	}
}

// zyk: calculates current weight of stuff the RPG player is carrying
void zyk_calculate_current_weight(gentity_t* ent)
{
	int rpg_inventory_weights[MAX_RPG_INVENTORY_ITEMS];
	int i = 0;
	int current_weight = 0;

	rpg_inventory_weights[RPG_INVENTORY_WP_STUN_BATON] = 12;
	rpg_inventory_weights[RPG_INVENTORY_WP_SABER] = 15;
	rpg_inventory_weights[RPG_INVENTORY_WP_BLASTER_PISTOL] = 14;
	rpg_inventory_weights[RPG_INVENTORY_WP_E11_BLASTER_RIFLE] = 18;
	rpg_inventory_weights[RPG_INVENTORY_WP_DISRUPTOR] = 18;
	rpg_inventory_weights[RPG_INVENTORY_WP_BOWCASTER] = 20;
	rpg_inventory_weights[RPG_INVENTORY_WP_REPEATER] = 22;
	rpg_inventory_weights[RPG_INVENTORY_WP_DEMP2] = 20;
	rpg_inventory_weights[RPG_INVENTORY_WP_FLECHETTE] = 24;
	rpg_inventory_weights[RPG_INVENTORY_WP_ROCKET_LAUNCHER] = 35;
	rpg_inventory_weights[RPG_INVENTORY_WP_CONCUSSION] = 40;
	rpg_inventory_weights[RPG_INVENTORY_WP_BRYAR_PISTOL] = 14;

	rpg_inventory_weights[RPG_INVENTORY_AMMO_BLASTER_PACK] = 1;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_POWERCELL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_METAL_BOLTS] = 1;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_ROCKETS] = 4;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_THERMALS] = 5;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_TRIPMINES] = 7;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_DETPACKS] = 10;

	rpg_inventory_weights[RPG_INVENTORY_ITEM_BINOCULARS] = 3;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_BACTA_CANISTER] = 4;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_SENTRY_GUN] = 6;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_SEEKER_DRONE] = 5;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_EWEB] = 22;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_BIG_BACTA] = 10;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_FORCE_FIELD] = 9;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_CLOAK] = 7;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_JETPACK] = 100;

	rpg_inventory_weights[RPG_INVENTORY_MISC_JETPACK_FUEL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_MISC_FLAME_THROWER_FUEL] = 1;

	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BACTA] = 5;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_FORCE_FIELD] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_CLOAK] = 7;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR] = 50;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = 200;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] = 200;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SABER_ARMOR] = 200;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_FLAME_THROWER] = 80;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_STUN_BATON] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DISRUPTOR] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BOWCASTER] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DEMP2] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_REPEATER] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_FLECHETTE] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_CONCUSSION] = 30;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DETPACKS] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_JETPACK] = 50;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_RADAR] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_THERMAL_VISION] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SENTRY_GUN] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SEEKER_DRONE] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_EWEB] = 30;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_ENERGY_MODULATOR] = 100;

	for (i = 0; i < MAX_RPG_INVENTORY_ITEMS; i++)
	{
		current_weight += (ent->client->pers.rpg_inventory[i] * rpg_inventory_weights[i]);
	}

	ent->client->pers.current_weight = current_weight;
}

void zyk_show_tutorial(gentity_t* ent)
{
	vec3_t effect_origin;

	if (ent->client->pers.tutorial_step == 0)
	{
		VectorSet(effect_origin, ent->r.currentOrigin[0] + 100, ent->r.currentOrigin[1] + 80, ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_DOME_OF_DAMAGE, 180000);

		VectorSet(effect_origin, ent->r.currentOrigin[0], ent->r.currentOrigin[1] + 100, ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_WATER_MAGIC, 180000);

		VectorSet(effect_origin, ent->r.currentOrigin[0] - 100, ent->r.currentOrigin[1] + 80, ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_EARTH_MAGIC, 180000);

		VectorSet(effect_origin, ent->r.currentOrigin[0] - 90, ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_FIRE_MAGIC, 180000);

		VectorSet(effect_origin, ent->r.currentOrigin[0] + 90, ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_AIR_MAGIC, 180000);

		VectorSet(effect_origin, ent->r.currentOrigin[0] - 50, ent->r.currentOrigin[1] - 70, ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_DARK_MAGIC, 180000);

		VectorSet(effect_origin, ent->r.currentOrigin[0] + 50, ent->r.currentOrigin[1] - 70, ent->r.currentOrigin[2]);
		zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_LIGHT_MAGIC, 180000);
	}

	if (ent->client->pers.tutorial_step == 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Hello %s^7. We are the Magical Spirits.\n\"", QUESTCHAR_ALL_SPIRITS, ent->client->pers.netname));
	}
	if (ent->client->pers.tutorial_step == 2)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: We will explain to you everything you keed to know.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 3)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You must defeat players or enemy npcs to get experience and levels. Levels give you skillpoints.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 4)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Use ^3/list ^7to see all info you need, like current Level, Magic, skillpoints, Stamina, etc.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 5)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: In ^3/list rpg ^7you can see the commands that lists your skills and other commands too.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 6)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: In time your Stamina will decrease. Doing actions makes it decrease faster.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 7)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: If out of Stamina, you will faint and regain a bit of it. Also, low Stamina decreases your run speed.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 8)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Use meditate (in controls Menu, set the bind for it) to regen Stamina.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 9)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Anything you get (weapons, ammo, items) use your inventory. To see it use ^3/list inventory^7\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 10)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Your inventory stuff have weight. If over the max weight, your run speed will decrease.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 11)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can either ^3/drop ^7weapons or items or sell then to the seller.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 12)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: To call the seller use ^3/callseller^7. Press the Use key on him for info on how to buy and sell stuff.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 13)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You will probably need to see info about a skill so use ^3/list <skill number> ^7to see skill info.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 14)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Your current stats (hp, shield, mp, stamina) are saved so changing map will keep them at current values.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 15)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: To upgrade or downgrade a skill, use ^3/up <skill number> ^7or ^3/down <skill number>^7\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 16)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: If you don't want to fight other players, use ^3/nofight^7\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 17)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can add allies players with ^3/allyadd ^7or use ^3/allyremove ^7to remove them.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 18)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can use ^3/callvote map mapname ^7to change to any map, including Single Player ones.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 19)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can see your quests in ^3/list quests^7. Any new mission will appear there\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 20)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: That's the reason we are here. We need your help %s^7\n\"", QUESTCHAR_ALL_SPIRITS, ent->client->pers.netname));
	}
	if (ent->client->pers.tutorial_step == 21)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Each one of us have affinity with one of the Elements of Nature, and one of us is non-elemental.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 22)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: our temples were took by evil forces, lead by the Emperor %s^7\n\"", QUESTCHAR_ALL_SPIRITS, QUESTCHAR_EMPEROR_NAME));
	}
	if (ent->client->pers.tutorial_step == 23)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: he is a servant of the demon %s^7, who came from the World of Darkness and is using the emperor to rule over everything!\n\"", QUESTCHAR_ALL_SPIRITS, QUESTCHAR_MAINVILLAIN_NAME));
	}
	if (ent->client->pers.tutorial_step == 24)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: he has minions, lesser demons, who took our temples.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 25)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: the angel %s^7 is in the World of Light, protecting it from being taken over, so he can't help us now.\n\"", QUESTCHAR_ALL_SPIRITS, QUESTCHAR_MAINGOODENTITY_NAME));
	}
	if (ent->client->pers.tutorial_step == 26)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: If you defeat one of them, one of us can reside again in it and help you in your quest.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 27)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Then you can summon the ones back in the temples with ^3/spirit <spirit number>\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 28)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: A summoned spirit will enhance magic of that type/element but you will be more vulnerable to that element from enemies.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 29)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: While summoned, the spirit will consume your mp until you use ^3/spirit ^7again to unsummon.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 30)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can try visiting many places (maps). The people there may help with side-quests.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 31)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: There are also many hidden artifacts that may help you.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 32)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Remember, you can use ^3/tutorial ^7to see all of this explanation again if you need.\n\"", QUESTCHAR_ALL_SPIRITS));
	}
	if (ent->client->pers.tutorial_step == 33)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Now go %s^7! Save everything and bring back harmony to Nature!\n\"", QUESTCHAR_ALL_SPIRITS, ent->client->pers.netname));

		// zyk: end of tutorial
		ent->client->pers.player_statuses &= ~(1 << 25);
	}
}

/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void ClearNPCGlobals( void );
void AI_UpdateGroups( void );
void ClearPlayerAlertEvents( void );
void SiegeCheckTimers(void);
void WP_SaberStartMissileBlockCheck( gentity_t *self, usercmd_t *ucmd );
extern void Jedi_Cloak( gentity_t *self );
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

int g_siegeRespawnCheck = 0;
void SetMoverState( gentity_t *ent, moverState_t moverState, int time );

extern void remove_credits(gentity_t *ent, int credits);
extern void try_finishing_race();
extern int zyk_max_skill_level(int skill_index);
extern void set_max_health(gentity_t *ent);
extern void set_max_shield(gentity_t *ent);
extern void duel_show_table(gentity_t *ent);
extern void WP_DisruptorAltFire(gentity_t *ent);
extern void G_Kill( gentity_t *ent );
extern void save_quest_file(int quest_number);
extern void zyk_set_quest_npc_abilities(gentity_t *zyk_npc);
extern void zyk_cast_magic(gentity_t* ent, int skill_index);
extern void zyk_update_inventory_quantity(gentity_t* ent, qboolean add_item, zyk_inventory_t item);

void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;
#ifdef _G_FRAME_PERFANAL
	int			iTimer_ItemRun = 0;
	int			iTimer_ROFF = 0;
	int			iTimer_ClientEndframe = 0;
	int			iTimer_GameChecks = 0;
	int			iTimer_Queues = 0;
	void		*timer_ItemRun;
	void		*timer_ROFF;
	void		*timer_ClientEndframe;
	void		*timer_GameChecks;
	void		*timer_Queues;
#endif

	if (level.gametype == GT_SIEGE &&
		g_siegeRespawn.integer &&
		g_siegeRespawnCheck < level.time)
	{ //check for a respawn wave
		gentity_t *clEnt;
		for ( i=0; i < MAX_CLIENTS; i++ )
		{
			clEnt = &g_entities[i];

			if (clEnt->inuse && clEnt->client &&
				clEnt->client->tempSpectate >= level.time &&
				clEnt->client->sess.sessionTeam != TEAM_SPECTATOR)
			{
				ClientRespawn(clEnt);
				clEnt->client->tempSpectate = 0;
			}
		}

		g_siegeRespawnCheck = level.time + g_siegeRespawn.integer * 1000;
	}

	if (gDoSlowMoDuel)
	{
		if (level.restarted)
		{
			char buf[128];
			float tFVal = 0;

			trap->Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

			tFVal = atof(buf);

			trap->Cvar_Set("timescale", "1");
			if (tFVal == 1.0f)
			{
				gDoSlowMoDuel = qfalse;
			}
		}
		else
		{
			float timeDif = (level.time - gSlowMoDuelTime); //difference in time between when the slow motion was initiated and now
			float useDif = 0; //the difference to use when actually setting the timescale

			if (timeDif < 150)
			{
				trap->Cvar_Set("timescale", "0.1f");
			}
			else if (timeDif < 1150)
			{
				useDif = (timeDif/1000); //scale from 0.1 up to 1
				if (useDif < 0.1f)
				{
					useDif = 0.1f;
				}
				if (useDif > 1.0f)
				{
					useDif = 1.0f;
				}
				trap->Cvar_Set("timescale", va("%f", useDif));
			}
			else
			{
				char buf[128];
				float tFVal = 0;

				trap->Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

				tFVal = atof(buf);

				trap->Cvar_Set("timescale", "1");
				if (timeDif > 1500 && tFVal == 1.0f)
				{
					gDoSlowMoDuel = qfalse;
				}
			}
		}
	}

	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;

	if (g_allowNPC.integer)
	{
		NAV_CheckCalcPaths();
	}

	AI_UpdateGroups();

	if (g_allowNPC.integer)
	{
		if ( d_altRoutes.integer )
		{
			trap->Nav_CheckAllFailedEdges();
		}
		trap->Nav_ClearCheckedNodes();

		//remember last waypoint, clear current one
		for ( i = 0; i < level.num_entities ; i++)
		{
			ent = &g_entities[i];

			if ( !ent->inuse )
				continue;

			if ( ent->waypoint != WAYPOINT_NONE
				&& ent->noWaypointTime < level.time )
			{
				ent->lastWaypoint = ent->waypoint;
				ent->waypoint = WAYPOINT_NONE;
			}
			if ( d_altRoutes.integer )
			{
				trap->Nav_CheckFailedNodes( (sharedEntity_t *)ent );
			}
		}

		//Look to clear out old events
		ClearPlayerAlertEvents();
	}

	g_TimeSinceLastFrame = (level.time - g_LastFrameTime);

	// get any cvar changes
	G_UpdateCvars();



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_ItemRun);
#endif

	if (level.map_music_reset_timer > 0 && level.map_music_reset_timer < level.time)
	{ // zyk: resets music to default one
		level.map_music_reset_timer = 0;

		if (level.current_map_music == MAPMUSIC_NONE)
		{
			trap->SetConfigstring(CS_MUSIC, G_NewString(level.default_map_music));
		}
		else if (level.current_map_music == MAPMUSIC_PROLOGUE)
		{
			trap->SetConfigstring(CS_MUSIC, "music/zyknewmod/sb_fateandfortune.mp3");
		}
		else if (level.current_map_music == MAPMUSIC_DESERT_CITY)
		{
			trap->SetConfigstring(CS_MUSIC, "music/zyknewmod/al-andalus.mp3");
		}
	}

	if (level.race_mode == 1 && level.race_start_timer < level.time)
	{ // zyk: Race Mode. Tests if we should start the race
		level.race_countdown = 3;
		level.race_countdown_timer = level.time;
		level.race_last_player_position = 0;
		level.race_mode = 2;
	}
	else if (level.race_mode == 2 && level.race_countdown_timer < level.time)
	{ // zyk: Race Mode. Shows the countdown messages in players screens and starts the race
		level.race_countdown_timer = level.time + 1000;

		if (level.race_countdown > 0)
		{
			trap->SendServerCommand( -1, va("cp \"^7Race starts in ^3%d\"", level.race_countdown));
			level.race_countdown--;
		}
		else if (level.race_countdown == 0)
		{
			level.race_mode = 3;
			trap->SendServerCommand( -1, "cp \"^2Go!\"");
		}
	}

	// zyk: Melee Battle
	if (level.melee_mode == 3 && level.melee_mode_timer < level.time)
	{
		melee_battle_end();
	}
	else if (level.melee_mode == 2)
	{
		if (level.melee_mode_timer < level.time)
		{
			melee_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7Time is up! No winner!\"");
		}
		else if (level.melee_mode_quantity == 1)
		{
			melee_battle_winner();

			// zyk: wait some time before ending the melee battle so the winner can escape the platform
			level.melee_mode_timer = level.time + 3000;
			level.melee_mode = 3;
		}
	}
	else if (level.melee_mode == 1 && level.melee_mode_timer < level.time)
	{
		if (level.melee_mode_quantity > 1)
		{ // zyk: if at least 2 players joined in it, start the battle
			melee_battle_prepare();
			level.melee_mode = 2;
			level.melee_mode_timer = level.time + 600000;
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7the battle has begun! The battle will have a max of 10 minutes!\"");
		}
		else
		{ // zyk: finish the battle
			melee_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7Not enough players. Melee Battle is over!\"");
		}
	}

	// zyk: Sniper Battle
	if (level.sniper_mode == 2)
	{
		if (level.sniper_mode_timer < level.time)
		{
			sniper_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Sniper Battle: ^7Time is up! No winner!\"");
		}
		else if (level.sniper_mode_quantity == 1)
		{
			sniper_battle_winner();
			sniper_battle_end();
		}
	}
	else if (level.sniper_mode == 1 && level.sniper_mode_timer < level.time)
	{
		if (level.sniper_mode_quantity > 1)
		{ // zyk: if at least 2 players joined in it, start the battle
			sniper_battle_prepare();
			level.sniper_mode = 2;
			level.sniper_mode_timer = level.time + 600000;
			trap->SendServerCommand(-1, "chat \"^3Sniper Battle: ^7the battle has begun! The battle will have a max of 10 minutes!\"");
		}
		else
		{ // zyk: finish the battle
			sniper_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Sniper Battle: ^7Not enough players. Sniper Battle is over!\"");
		}
	}

	// zyk: RPG LMS
	if (level.rpg_lms_mode == 2)
	{
		if (level.rpg_lms_timer < level.time)
		{
			rpg_lms_end();
			trap->SendServerCommand(-1, "chat \"^3RPG LMS: ^7Time is up! No winner!\"");
		}
		else if (level.rpg_lms_quantity == 1)
		{
			rpg_lms_winner();
			rpg_lms_end();
		}
	}
	else if (level.rpg_lms_mode == 1 && level.rpg_lms_timer < level.time)
	{
		if (level.rpg_lms_quantity > 1)
		{ // zyk: if at least 2 players joined in it, start the battle
			rpg_lms_prepare();
			level.rpg_lms_mode = 2;
			level.rpg_lms_timer = level.time + 600000;
			trap->SendServerCommand(-1, "chat \"^3RPG LMS: ^7the battle has begun! The battle will have a max of 10 minutes!\"");
		}
		else
		{ // zyk: finish the battle
			rpg_lms_end();
			trap->SendServerCommand(-1, "chat \"^3RPG LMS: ^7Not enough players. RPG LMS is over!\"");
		}
	}

	// zyk: Duel Tournament
	if (level.duel_tournament_mode == 4)
	{ // zyk: validations during a duel
		if (duel_tournament_validate_duelists() == qtrue)
		{
			gentity_t *first_duelist = &g_entities[level.duelist_1_id];
			gentity_t *second_duelist = &g_entities[level.duelist_2_id];
			gentity_t *first_duelist_ally = NULL;
			gentity_t *second_duelist_ally = NULL;

			if (level.duelist_1_ally_id != -1)
			{
				first_duelist_ally = &g_entities[level.duelist_1_ally_id];
			}

			if (level.duelist_2_ally_id != -1)
			{
				second_duelist_ally = &g_entities[level.duelist_2_ally_id];
			}

			if ((!(first_duelist->client->pers.player_statuses & (1 << 27)) || (first_duelist_ally && !(first_duelist_ally->client->pers.player_statuses & (1 << 27)))) &&
				 second_duelist->client->pers.player_statuses & (1 << 27) && (!second_duelist_ally || second_duelist_ally->client->pers.player_statuses & (1 << 27)))
			{ // zyk: first team wins
				duel_tournament_set_match_winner(first_duelist);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if ((!(second_duelist->client->pers.player_statuses & (1 << 27)) || (second_duelist_ally && !(second_duelist_ally->client->pers.player_statuses & (1 << 27)))) &&
				first_duelist->client->pers.player_statuses & (1 << 27) && (!first_duelist_ally || first_duelist_ally->client->pers.player_statuses & (1 << 27)))
			{ // zyk: second team wins
				duel_tournament_set_match_winner(second_duelist);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if (level.duel_tournament_timer < level.time)
			{ // zyk: duel timed out
				int first_team_health = 0;
				int second_team_health = 0;

				if (!(first_duelist->client->pers.player_statuses & (1 << 27)))
				{
					first_team_health = first_duelist->health + first_duelist->client->ps.stats[STAT_ARMOR];
				}

				if (!(second_duelist->client->pers.player_statuses & (1 << 27)))
				{
					second_team_health = second_duelist->health + second_duelist->client->ps.stats[STAT_ARMOR];
				}

				if (first_duelist_ally && !(first_duelist_ally->client->pers.player_statuses & (1 << 27)))
				{
					first_team_health += (first_duelist_ally->health + first_duelist_ally->client->ps.stats[STAT_ARMOR]);
				}

				if (second_duelist_ally && !(second_duelist_ally->client->pers.player_statuses & (1 << 27)))
				{
					second_team_health += (second_duelist_ally->health + second_duelist_ally->client->ps.stats[STAT_ARMOR]);
				}

				if (first_team_health > second_team_health)
				{ // zyk: first team wins
					duel_tournament_set_match_winner(first_duelist);
				}
				else if (first_team_health < second_team_health)
				{ // zyk: second team wins
					duel_tournament_set_match_winner(second_duelist);
				}
				else
				{ // zyk: tie
					duel_tournament_set_match_winner(NULL);
				}

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if (first_duelist->client->pers.player_statuses & (1 << 27) && (!first_duelist_ally || first_duelist_ally->client->pers.player_statuses & (1 << 27)) &&
				second_duelist->client->pers.player_statuses & (1 << 27) && (!second_duelist_ally || second_duelist_ally->client->pers.player_statuses & (1 << 27)))
			{ // zyk: tie when everyone dies at the same frame
				duel_tournament_set_match_winner(NULL);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
		}
		else
		{ // zyk: match ended because one of the teams is no longer valid
			level.duel_tournament_mode = 5;
			level.duel_tournament_timer = level.time + 1500;
		}
	}

	if (level.duel_tournament_paused == qfalse)
	{
		if (level.duel_tournament_mode == 5 && level.duel_tournament_timer < level.time)
		{ // zyk: show score table and reset duelists
			duel_show_table(NULL);

			if (level.duelist_1_id != -1)
			{
				player_restore_force(&g_entities[level.duelist_1_id]);
			}

			if (level.duelist_2_id != -1)
			{
				player_restore_force(&g_entities[level.duelist_2_id]);
			}

			if (level.duelist_1_ally_id != -1)
			{
				player_restore_force(&g_entities[level.duelist_1_ally_id]);
			}

			if (level.duelist_2_ally_id != -1)
			{
				player_restore_force(&g_entities[level.duelist_2_ally_id]);
			}

			level.duelist_1_id = -1;
			level.duelist_2_id = -1;
			level.duelist_1_ally_id = -1;
			level.duelist_2_ally_id = -1;

			level.duel_tournament_timer = level.time + 1500;
			level.duel_tournament_mode = 2;
		}
		else if (level.duel_tournament_mode == 3 && level.duel_tournament_timer < level.time)
		{
			if (duel_tournament_validate_duelists() == qtrue)
			{
				vec3_t zyk_origin, zyk_angles;
				gentity_t *duelist_1 = &g_entities[level.duelist_1_id];
				gentity_t *duelist_2 = &g_entities[level.duelist_2_id];
				gentity_t *duelist_1_ally = NULL;
				gentity_t *duelist_2_ally = NULL;
				qboolean zyk_has_respawned = qfalse;

				if (level.duelist_1_ally_id != -1)
				{
					duelist_1_ally = &g_entities[level.duelist_1_ally_id];
				}

				if (level.duelist_2_ally_id != -1)
				{
					duelist_2_ally = &g_entities[level.duelist_2_ally_id];
				}

				// zyk: respawning duelists that are still dead
				if (duelist_1->health < 1)
				{
					ClientRespawn(duelist_1);
					zyk_has_respawned = qtrue;
				}

				if (duelist_2->health < 1)
				{
					ClientRespawn(duelist_2);
					zyk_has_respawned = qtrue;
				}

				if (duelist_1_ally && duelist_1_ally->health < 1)
				{
					ClientRespawn(duelist_1_ally);
					zyk_has_respawned = qtrue;
				}

				if (duelist_2_ally && duelist_2_ally->health < 1)
				{
					ClientRespawn(duelist_2_ally);
					zyk_has_respawned = qtrue;
				}

				duel_tournament_protect_duelists(duelist_1, duelist_2, duelist_1_ally, duelist_2_ally);

				if (zyk_has_respawned == qfalse)
				{
					// zyk: setting the max time players can duel
					level.duel_tournament_timer = level.time + zyk_duel_tournament_duel_time.integer;

					// zyk: prepare the duelists to start duel
					duel_tournament_prepare(duelist_1);
					duel_tournament_prepare(duelist_2);

					if (duelist_1_ally)
					{
						duel_tournament_prepare(duelist_1_ally);
					}

					if (duelist_2_ally)
					{
						duel_tournament_prepare(duelist_2_ally);
					}

					// zyk: put the duelists along the y axis
					VectorSet(zyk_angles, 0, 90, 0);

					if (duelist_1_ally)
					{
						VectorSet(zyk_origin, level.duel_tournament_origin[0] - 50, level.duel_tournament_origin[1] - 125, level.duel_tournament_origin[2] + 1);
						zyk_TeleportPlayer(duelist_1, zyk_origin, zyk_angles);

						VectorSet(zyk_origin, level.duel_tournament_origin[0] + 50, level.duel_tournament_origin[1] - 125, level.duel_tournament_origin[2] + 1);
						zyk_TeleportPlayer(duelist_1_ally, zyk_origin, zyk_angles);
					}
					else
					{
						VectorSet(zyk_origin, level.duel_tournament_origin[0], level.duel_tournament_origin[1] - 125, level.duel_tournament_origin[2] + 1);
						zyk_TeleportPlayer(duelist_1, zyk_origin, zyk_angles);
					}

					VectorSet(zyk_angles, 0, -90, 0);

					if (duelist_2_ally)
					{
						VectorSet(zyk_origin, level.duel_tournament_origin[0] - 50, level.duel_tournament_origin[1] + 125, level.duel_tournament_origin[2] + 1);
						zyk_TeleportPlayer(duelist_2, zyk_origin, zyk_angles);

						VectorSet(zyk_origin, level.duel_tournament_origin[0] + 50, level.duel_tournament_origin[1] + 125, level.duel_tournament_origin[2] + 1);
						zyk_TeleportPlayer(duelist_2_ally, zyk_origin, zyk_angles);
					}
					else
					{
						VectorSet(zyk_origin, level.duel_tournament_origin[0], level.duel_tournament_origin[1] + 125, level.duel_tournament_origin[2] + 1);
						zyk_TeleportPlayer(duelist_2, zyk_origin, zyk_angles);
					}

					level.duel_tournament_mode = 4;
				}
				else
				{ // zyk: must wait a bit more to guarantee the player is fully respawned before teleporting him to arena
					level.duel_tournament_timer = level.time + 500;
				}
			}
			else
			{ // zyk: duelists are no longer valid, get a new match
				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
		}
		else if (level.duel_tournament_mode == 2 && level.duel_tournament_timer < level.time)
		{ // zyk: search for duelists and put them in the arena
			int zyk_it = 0;

			for (zyk_it = 0; zyk_it < MAX_CLIENTS; zyk_it++)
			{
				gentity_t *this_ent = &g_entities[zyk_it];

				// zyk: cleaning flag from player
				if (this_ent && this_ent->client)
					this_ent->client->pers.player_statuses &= ~(1 << 27);
			}

			if (level.duel_matches_done < level.duel_matches_quantity)
			{ // zyk: if there are still matches to be chosen, try to choose now
				level.duelist_1_id = level.duel_matches[level.duel_matches_done][0];
				level.duelist_2_id = level.duel_matches[level.duel_matches_done][1];

				// zyk: getting the duelist allies
				if (level.duel_allies[level.duelist_1_id] != -1)
				{
					level.duelist_1_ally_id = level.duel_allies[level.duelist_1_id];
				}

				if (level.duel_allies[level.duelist_2_id] != -1)
				{
					level.duelist_2_ally_id = level.duel_allies[level.duelist_2_id];
				}

				if (duel_tournament_validate_duelists() == qfalse)
				{ // zyk: if not valid, show score table
					level.duel_tournament_mode = 5;
					level.duel_tournament_timer = level.time + 1500;
				}
				else
				{
					gentity_t *duelist_1 = &g_entities[level.duelist_1_id];
					gentity_t *duelist_2 = &g_entities[level.duelist_2_id];
					char first_ally[36];
					char second_ally[36];

					strcpy(first_ally, "");
					strcpy(second_ally, "");

					if (level.duelist_1_ally_id != -1)
					{
						strcpy(first_ally, va("^7 / %s", g_entities[level.duelist_1_ally_id].client->pers.netname));
					}

					if (level.duelist_2_ally_id != -1)
					{
						strcpy(second_ally, va("^7 / %s", g_entities[level.duelist_2_ally_id].client->pers.netname));
					}

					level.duel_tournament_timer = level.time + 3000;
					level.duel_tournament_mode = 3;

					trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s%s ^7x %s%s\"", duelist_1->client->pers.netname, first_ally, duelist_2->client->pers.netname, second_ally));
				}
			}

			if (level.duel_matches_quantity == level.duel_matches_done)
			{ // zyk: current cycle ended. Go to next one
				level.duel_tournament_rounds++;

				if (level.duel_tournament_rounds < zyk_duel_tournament_rounds_per_match.integer)
				{
					level.duel_matches_done = 0;
				}
			}

			if (level.duel_matches_quantity == level.duel_matches_done && level.duel_tournament_mode == 2)
			{ // zyk: all matches were done. Determine the tournament winner
				duel_tournament_winner();
				duel_tournament_end();
			}
			else if (level.duelists_quantity == 0)
			{
				duel_tournament_end();
				trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7There are no duelists anymore. Tournament is over!\"");
			}
		}
		else if (level.duel_tournament_mode == 1 && level.duel_tournament_timer < level.time)
		{ // zyk: Duel tournament begins after validation on number of players
			if (level.duelists_quantity > 1 && level.duelists_quantity >= zyk_duel_tournament_min_players.integer)
			{ // zyk: must have a minimum of 2 players
				int zyk_number_of_teams = duel_tournament_generate_teams();

				if (zyk_number_of_teams > 1 && zyk_number_of_teams >= zyk_duel_tournament_min_players.integer)
				{
					level.duel_tournament_mode = 5;
					level.duel_tournament_timer = level.time + 1500;

					duel_tournament_generate_match_table();

					trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7The tournament begins!\"");
				}
				else
				{
					duel_tournament_end();
					trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Not enough teams or single duelists (minimum of %d). Tournament is over!\"", zyk_duel_tournament_min_players.integer));
				}
			}
			else
			{
				duel_tournament_end();
				trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Not enough teams or single duelists (minimum of %d). Tournament is over!\"", zyk_duel_tournament_min_players.integer));
			}
		}
	}

	// zyk: Duel Tournament Leaderboard is calculated here
	if (level.duel_leaderboard_step > 0 && level.duel_leaderboard_timer < level.time)
	{
		if (level.duel_leaderboard_step == 1)
		{
			FILE *leaderboard_file = fopen("zykmod/leaderboard.txt", "r");

			if (leaderboard_file != NULL)
			{ 
				char content[64];
				qboolean found_acc = qfalse;
				int j = 0;

				strcpy(content, "");

				while (found_acc == qfalse && fgets(content, sizeof(content), leaderboard_file) != NULL)
				{
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					if (Q_stricmp(G_NewString(content), G_NewString(level.duel_leaderboard_acc)) == 0)
					{
						found_acc = qtrue;

						// zyk: reads player name
						fgets(content, sizeof(content), leaderboard_file);
						if (content[strlen(content) - 1] == '\n')
							content[strlen(content) - 1] = '\0';

						// zyk: reads score
						fgets(content, sizeof(content), leaderboard_file);
						if (content[strlen(content) - 1] == '\n')
							content[strlen(content) - 1] = '\0';

						level.duel_leaderboard_score = atoi(content) + 1; // zyk: sets the new number of tourmanemt victories of this winner
						level.duel_leaderboard_step = 3;
						level.duel_leaderboard_timer = level.time + 500;
						level.duel_leaderboard_index = j; // zyk: current line in the file where this winner is
					}
					else
					{
						// zyk: reads player name
						fgets(content, sizeof(content), leaderboard_file);
						if (content[strlen(content) - 1] == '\n')
							content[strlen(content) - 1] = '\0';

						// zyk: reads score
						fgets(content, sizeof(content), leaderboard_file);
						if (content[strlen(content) - 1] == '\n')
							content[strlen(content) - 1] = '\0';
					}

					j++;
				}

				fclose(leaderboard_file);

				if (found_acc == qfalse)
				{ // zyk: did not find the player, saves him at the end of the file
					level.duel_leaderboard_step = 2;
					level.duel_leaderboard_timer = level.time + 500;
				}
			}
			else
			{ // zyk: if file does not exist, create it with this player in it
				level.duel_leaderboard_step = 2;
				level.duel_leaderboard_timer = level.time + 500;
			}
		}
		else if (level.duel_leaderboard_step == 2)
		{ // zyk: add the player to the end of the file with 1 tournament win
			FILE *leaderboard_file = fopen("zykmod/leaderboard.txt", "a");
			fprintf(leaderboard_file, "%s\n%s\n1\n", level.duel_leaderboard_acc, level.duel_leaderboard_name);
			fclose(leaderboard_file);

			level.duel_leaderboard_step = 0; // zyk: stop creating the leaderboard

			if (level.duel_leaderboard_add_ally == qtrue)
			{
				duel_tournament_generate_leaderboard(G_NewString(level.duel_leaderboard_ally_acc), G_NewString(level.duel_leaderboard_ally_name));
			}
		}
		else if (level.duel_leaderboard_step == 3)
		{ // zyk: determines the line where this winner must be put in the file
			if (level.duel_leaderboard_index == 0)
			{ // zyk: already the first place, go straight to next step
				level.duel_leaderboard_step = 4;
				level.duel_leaderboard_timer = level.time + 500;
			}
			else
			{
				int j = 0;
				int this_score = 0;
				char content[64];				
				FILE *leaderboard_file = fopen("zykmod/leaderboard.txt", "r");

				strcpy(content, "");

				for (j = 0; j < level.duel_leaderboard_index; j++)
				{
					// zyk: reads acc name
					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					// zyk: reads player name
					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					// zyk: reads score
					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					this_score = atoi(content);
					if (level.duel_leaderboard_score > this_score)
					{ // zyk: winner score is greater than this one, this will be the new index
						level.duel_leaderboard_index = j;
						break;
					}
				}

				fclose(leaderboard_file);

				level.duel_leaderboard_step = 4;
				level.duel_leaderboard_timer = level.time + 500;
			}
		}
		else if (level.duel_leaderboard_step == 4)
		{ // zyk: saving the new leaderboard file with the updated score of the winner
			FILE *leaderboard_file = fopen("zykmod/leaderboard.txt", "r");
			FILE *new_leaderboard_file = fopen("zykmod/new_leaderboard.txt", "w");
			int j = 0;
			char content[64];

			strcpy(content, "");

			// zyk: saving players before the winner
			for (j = 0; j < level.duel_leaderboard_index; j++)
			{
				// zyk: saving acc name
				fgets(content, sizeof(content), leaderboard_file);
				if (content[strlen(content) - 1] == '\n')
					content[strlen(content) - 1] = '\0';
				fprintf(new_leaderboard_file, "%s\n", content);

				// zyk: saving player name
				fgets(content, sizeof(content), leaderboard_file);
				if (content[strlen(content) - 1] == '\n')
					content[strlen(content) - 1] = '\0';
				fprintf(new_leaderboard_file, "%s\n", content);

				// zyk: saving score
				fgets(content, sizeof(content), leaderboard_file);
				if (content[strlen(content) - 1] == '\n')
					content[strlen(content) - 1] = '\0';
				fprintf(new_leaderboard_file, "%s\n", content);
			}

			// zyk: saving the winner
			fprintf(new_leaderboard_file, "%s\n%s\n%d\n", level.duel_leaderboard_acc, level.duel_leaderboard_name, level.duel_leaderboard_score);

			// zyk: saving the other players, except the old line of the winner
			while (fgets(content, sizeof(content), leaderboard_file) != NULL)
			{
				if (content[strlen(content) - 1] == '\n')
					content[strlen(content) - 1] = '\0';

				if (Q_stricmp(content, level.duel_leaderboard_acc) != 0)
				{
					fprintf(new_leaderboard_file, "%s\n", content);

					// zyk: saving player name
					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';
					fprintf(new_leaderboard_file, "%s\n", content);

					// zyk: saving score
					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';
					fprintf(new_leaderboard_file, "%s\n", content);
				}
				else
				{
					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';

					fgets(content, sizeof(content), leaderboard_file);
					if (content[strlen(content) - 1] == '\n')
						content[strlen(content) - 1] = '\0';
				}
			}

			fclose(leaderboard_file);
			fclose(new_leaderboard_file);

			level.duel_leaderboard_step = 5;
			level.duel_leaderboard_timer = level.time + 500;
		}
		else if (level.duel_leaderboard_step == 5)
		{ // zyk: renaming new file to leaderboard.txt
#if defined(__linux__)
			system("mv -f zykmod/new_leaderboard.txt zykmod/leaderboard.txt");
#else
			system("MOVE /Y \"zykmod\\new_leaderboard.txt\" \"zykmod\\leaderboard.txt\"");
#endif

			level.duel_leaderboard_step = 0; // zyk: stop creating the leaderboard

			if (level.duel_leaderboard_add_ally == qtrue)
			{
				duel_tournament_generate_leaderboard(G_NewString(level.duel_leaderboard_ally_acc), G_NewString(level.duel_leaderboard_ally_name));
			}
		}
	}

	if (level.load_entities_timer != 0 && level.load_entities_timer < level.time)
	{ // zyk: loading entities from the file specified in entload command, or the default file
		char content[2048];
		FILE *this_file = NULL;

		strcpy(content,"");

		// zyk: loading the entities from the file
		this_file = fopen(level.load_entities_file,"r");

		if (this_file != NULL)
		{
			while (fgets(content,sizeof(content),this_file) != NULL)
			{
				gentity_t *new_ent = G_Spawn();

				if (content[strlen(content) - 1] == '\n')
					content[strlen(content) - 1] = '\0';

				if (new_ent)
				{
					int j = 0; // zyk: the current key/value being used
					int k = 0; // zyk: current spawn string position

					while (content[k] != '\0')
					{
						int l = 0;
						char zyk_key[256];
						char zyk_value[256];

						// zyk: getting the key
						while (content[k] != ';')
						{ 
							zyk_key[l] = content[k];

							l++;
							k++;
						}
						zyk_key[l] = '\0';
						k++;

						// zyk: getting the value
						l = 0;
						while (content[k] != ';')
						{
							zyk_value[l] = content[k];

							l++;
							k++;
						}
						zyk_value[l] = '\0';
						k++;

						// zyk: copying the key and value to the spawn string array
						level.zyk_spawn_strings[new_ent->s.number][j] = G_NewString(zyk_key);
						level.zyk_spawn_strings[new_ent->s.number][j + 1] = G_NewString(zyk_value);

						j += 2;
					}

					level.zyk_spawn_strings_values_count[new_ent->s.number] = j;

					// zyk: spawns the entity
					zyk_main_spawn_entity(new_ent);
				}
			}

			fclose(this_file);
		}

		// zyk: CTF need to have the flags spawned again when an entity file is loaded
		// general initialization
		G_FindTeams();

		// make sure we have flags for CTF, etc
		if( level.gametype >= GT_TEAM ) {
			G_CheckTeamItems();
		}

		level.load_entities_timer = 0;
	}

	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	// &= EV_EVENT_BITS;
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
					// predicted events should never be set to zero
					//ent->client->ps.events[0] = 0;
					//ent->client->ps.events[1] = 0;
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				if (ent->s.eFlags & EF_SOUNDTRACKER)
				{ //don't trigger the event again..
					ent->s.event = 0;
					ent->s.eventParm = 0;
					ent->s.eType = 0;
					ent->eventTime = 0;
				}
				else
				{
					G_FreeEntity( ent );
					continue;
				}
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap->UnlinkEntity( (sharedEntity_t *)ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
			G_RunMissile( ent );
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
#if 0 //use if body dragging enabled?
			if (ent->s.eType == ET_BODY)
			{ //special case for bodies
				float grav = 3.0f;
				float mass = 0.14f;
				float bounce = 1.15f;

				G_RunExPhys(ent, grav, mass, bounce, qfalse, NULL, 0);
			}
			else
			{
				G_RunItem( ent );
			}
#else
			G_RunItem( ent );
#endif
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		//fix for self-deactivating areaportals in Siege
		if ( ent->s.eType == ET_MOVER && level.gametype == GT_SIEGE && level.intermissiontime)
		{
			if ( !Q_stricmp("func_door", ent->classname) && ent->moverState != MOVER_POS1 )
			{
				SetMoverState( ent, MOVER_POS1, level.time );
				if ( ent->teammaster == ent || !ent->teammaster )
				{
					trap->AdjustAreaPortalState( (sharedEntity_t *)ent, qfalse );
				}

				//stop the looping sound
				ent->s.loopSound = 0;
				ent->s.loopIsSoundset = qfalse;
			}
			continue;
		}

		clear_special_power_effect(ent);

		if ( i < MAX_CLIENTS )
		{
			G_CheckClientTimeouts ( ent );

			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //we're in space, check for suffocating and for exiting
                gentity_t *spacetrigger = &g_entities[ent->client->inSpaceIndex];

				if (!spacetrigger->inuse ||
					!G_PointInBounds(ent->client->ps.origin, spacetrigger->r.absmin, spacetrigger->r.absmax))
				{ //no longer in space then I suppose
                    ent->client->inSpaceIndex = 0;
				}
				else
				{ //check for suffocation
                    if (ent->client->inSpaceSuffocation < level.time)
					{ //suffocate!
						if (ent->health > 0 && ent->takedamage)
						{ //if they're still alive..
							G_Damage(ent, spacetrigger, spacetrigger, NULL, ent->client->ps.origin, Q_irand(50, 70), DAMAGE_NO_ARMOR, MOD_SUICIDE);

							if (ent->health > 0)
							{ //did that last one kill them?
								//play the choking sound
								G_EntitySound(ent, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )));

								//make them grasp their throat
								ent->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
								ent->client->ps.forceHandExtendTime = level.time + 2000;
							}
						}

						ent->client->inSpaceSuffocation = level.time + Q_irand(100, 200);
					}
				}
			}

			if (ent->client->isHacking)
			{ //hacking checks
				gentity_t *hacked = &g_entities[ent->client->isHacking];
				vec3_t angDif;

				VectorSubtract(ent->client->ps.viewangles, ent->client->hackingAngles, angDif);

				//keep him in the "use" anim
				if (ent->client->ps.torsoAnim != BOTH_CONSOLE1)
				{
					G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_CONSOLE1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
				}
				else
				{
					ent->client->ps.torsoTimer = 500;
				}
				ent->client->ps.weaponTime = ent->client->ps.torsoTimer;

				if (!(ent->client->pers.cmd.buttons & BUTTON_USE))
				{ //have to keep holding use
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!hacked || !hacked->inuse)
				{ //shouldn't happen, but safety first
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!G_PointInBounds( ent->client->ps.origin, hacked->r.absmin, hacked->r.absmax ))
				{ //they stepped outside the thing they're hacking, so reset hacking time
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (VectorLength(angDif) > 10.0f)
				{ //must remain facing generally the same angle as when we start
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
			}

			// zyk: new jetpack debounce and recharge code. It uses the new attribute jetpack_fuel in the pers struct
			//      then we scale and set it to the jetpackFuel attribute to display the fuel bar correctly to the player
			if (ent->client->jetPackOn && ent->client->jetPackDebReduce < level.time)
			{
				int jetpack_debounce_amount = 20;

				if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_JETPACK] > 0)
				{ // zyk: Jetpack Upgrade decreases fuel debounce
					jetpack_debounce_amount -= 4;
				}

				if (ent->client->pers.cmd.upmove > 0)
				{ // zyk: jetpack thrusting
					jetpack_debounce_amount *= 2;
				}

				ent->client->pers.jetpack_fuel -= jetpack_debounce_amount;

				if (ent->client->pers.jetpack_fuel <= 0)
				{ // zyk: out of fuel. Turn jetpack off
					ent->client->pers.jetpack_fuel = 0;
					Jetpack_Off(ent);
				}

				ent->client->ps.jetpackFuel = ent->client->pers.jetpack_fuel/JETPACK_SCALE;
				ent->client->jetPackDebReduce = level.time + 200; // zyk: JETPACK_DEFUEL_RATE. Original value: 200
			}

			// zyk: Duel Tournament. Do not let anyone enter or anyone leave the globe arena
			if (level.duel_tournament_mode == 4)
			{
				if (duel_tournament_is_duelist(ent) == qtrue && 
					!(ent->client->pers.player_statuses & (1 << 27)) && // zyk: did not die in his duel yet
					Distance(ent->client->ps.origin, level.duel_tournament_origin) > (DUEL_TOURNAMENT_ARENA_SIZE * zyk_duel_tournament_arena_scale.value / 100.0) &&
					ent->health > 0)
				{ // zyk: duelists cannot leave the arena after duel begins
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
				else if ((duel_tournament_is_duelist(ent) == qfalse || 
					(level.duel_players[ent->s.number] != -1 && ent->client->pers.player_statuses & (1 << 27))) && // zyk: not a duelist or died in his duel
					ent->client->sess.sessionTeam != TEAM_SPECTATOR && 
					Distance(ent->client->ps.origin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * zyk_duel_tournament_arena_scale.value / 100.0) &&
					ent->health > 0)
				{ // zyk: other players cannot enter the arena
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
			}

			if (level.melee_mode == 2 && level.melee_players[ent->s.number] != -1)
			{ // zyk: Melee Battle
				if (ent->client->ps.origin[2] < level.melee_mode_origin[2])
				{ // zyk: validating if player fell of the catwalk
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
				else if (Distance(ent->client->ps.origin, level.melee_mode_origin) > 1000.0)
				{ // zyk: validating if player is too far from the platform
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
			}

			if (ent->client->pers.race_position > 0)
			{ // zyk: Race Mode management
				if (level.race_mode == 3)
				{ // zyk: if race already started
					if (ent->client->ps.m_iVehicleNum != level.race_mode_vehicle[ent->client->pers.race_position - 1] && ent->health > 0)
					{ // zyk: if player loses his vehicle, he loses the race
						trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%s ^7lost his vehicle and so he lost the race!\n\"", ent->client->pers.netname));

						ent->client->pers.race_position = 0;

						try_finishing_race();
					}
					else if (level.race_map == 1)
					{ // zyk: t2_trip map
						if ((int)ent->client->ps.origin[0] > 3200 && (int)ent->client->ps.origin[0] < 4770 && (int)ent->client->ps.origin[1] > -11136 && (int)ent->client->ps.origin[1] < -9978)
						{ // zyk: player reached the finish line
							level.race_last_player_position++;
							ent->client->pers.race_position = 0;

							if (level.race_last_player_position == 1)
							{ // zyk: this player won the race. Send message to everyone and give his prize
								if (ent->client->sess.amrpgmode == 2)
								{ // zyk: give him credits
									add_credits(ent, 2000);
									save_account(ent, qtrue);
									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
									trap->SendServerCommand(-1, va("chat \"^3Race System: ^7Winner: %s^7 - Prize: 2000 Credits!\"", ent->client->pers.netname));
								}
								else
								{ // zyk: give him some stuff
									ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER) | (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
									ent->client->ps.ammo[AMMO_BLASTER] = zyk_max_blaster_pack_ammo.integer;
									ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
									ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
									ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN) | (1 << HI_SEEKER);
									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
									trap->SendServerCommand(-1, va("chat \"^3Race System: ^7Winner: %s^7 - Prize: Nice Stuff!\"", ent->client->pers.netname));
								}
							}
							else if (level.race_last_player_position == 2)
							{ // zyk: second place
								trap->SendServerCommand(-1, va("chat \"^3Race System: ^72nd Place - %s\"", ent->client->pers.netname));
							}
							else if (level.race_last_player_position == 3)
							{ // zyk: third place
								trap->SendServerCommand(-1, va("chat \"^3Race System: ^73rd Place - %s\"", ent->client->pers.netname));
							}
							else
							{
								trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%dth Place - %s\"", level.race_last_player_position, ent->client->pers.netname));
							}

							try_finishing_race();
						}
						else if ((int)ent->client->ps.origin[0] > -14795 && (int)ent->client->ps.origin[0] < -13830 && (int)ent->client->ps.origin[1] > -11483 && (int)ent->client->ps.origin[1] < -8474)
						{ // zyk: teleporting to get through the wall
							vec3_t origin, yaw;

							origin[0] = -14785;
							origin[1] = -9252;
							origin[2] = 1848;

							yaw[0] = 0.0f;
							yaw[1] = 179.0f;
							yaw[2] = 0.0f;

							zyk_TeleportPlayer(&g_entities[ent->client->ps.m_iVehicleNum], origin, yaw);
						}
						else if ((int)ent->client->ps.origin[0] > -18845 && (int)ent->client->ps.origin[0] < -17636 && (int)ent->client->ps.origin[1] > -7530 && (int)ent->client->ps.origin[1] < -6761)
						{ // zyk: teleporting to get through the door
							vec3_t origin, yaw;

							origin[0] = -18248;
							origin[1] = -6152;
							origin[2] = 1722;

							yaw[0] = 0.0f;
							yaw[1] = 90.0f;
							yaw[2] = 0.0f;

							zyk_TeleportPlayer(&g_entities[ent->client->ps.m_iVehicleNum], origin, yaw);
						}
					}
					else if (level.race_map == 2)
					{ // zyk: t3_stamp map
						if ((int)ent->client->ps.origin[1] > -174 && (int)ent->client->ps.origin[2] < -170)
						{ // zyk: player reached the finish line
							level.race_last_player_position++;
							ent->client->pers.race_position = 0;

							if (level.race_last_player_position == 1)
							{ // zyk: this player won the race. Send message to everyone and give his prize
								if (ent->client->sess.amrpgmode == 2)
								{ // zyk: give him credits
									add_credits(ent, 500);
									save_account(ent, qtrue);
									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
									trap->SendServerCommand(-1, va("chat \"^3Race System: ^7Winner: %s^7 - Prize: 500 Credits!\"", ent->client->pers.netname));
								}
								else
								{ // zyk: give him some stuff
									ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
									ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
									ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
									ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);
									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
									trap->SendServerCommand(-1, va("chat \"^3Race System: ^7Winner: %s^7 - Prize: Nice Stuff!\"", ent->client->pers.netname));
								}
							}
							else if (level.race_last_player_position == 2)
							{ // zyk: second place
								trap->SendServerCommand(-1, va("chat \"^3Race System: ^72nd Place - %s\"", ent->client->pers.netname));
							}
							else if (level.race_last_player_position == 3)
							{ // zyk: third place
								trap->SendServerCommand(-1, va("chat \"^3Race System: ^73rd Place - %s\"", ent->client->pers.netname));
							}
							else
							{
								trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%dth Place - %s\"", level.race_last_player_position, ent->client->pers.netname));
							}

							try_finishing_race();
						}
					}
				}
				else if (level.race_map == 1 && (int)ent->client->ps.origin[0] < -4536)
				{ // zyk: player cant start racing before the countdown timer
					ent->client->pers.race_position = 0;
					trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%s ^7lost for trying to race before it starts!\n\"", ent->client->pers.netname));

					try_finishing_race();
				}
				else if (level.race_map == 2 && (int)ent->client->ps.origin[1] < 1230)
				{ // zyk: player cant start racing before the countdown timer
					ent->client->pers.race_position = 0;
					trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%s ^7lost for trying to race before it starts!\n\"", ent->client->pers.netname));

					try_finishing_race();
				}
			}

			quest_power_events(ent);
			fire_bolt_hits(ent);

			if (zyk_chat_protection_timer.integer > 0)
			{ // zyk: chat protection. If 0, it is off. If greater than 0, set the timer to protect the player
				if (ent->client->ps.eFlags & EF_TALK && ent->client->pers.chat_protection_timer == 0)
				{
					ent->client->pers.chat_protection_timer = level.time + zyk_chat_protection_timer.integer;
				}
				else if (ent->client->ps.eFlags & EF_TALK && ent->client->pers.chat_protection_timer < level.time)
				{
					ent->client->pers.player_statuses |= (1 << 5);
				}
				else if (ent->client->pers.chat_protection_timer != 0 && !(ent->client->ps.eFlags & EF_TALK))
				{
					ent->client->pers.player_statuses &= ~(1 << 5);
					ent->client->pers.chat_protection_timer = 0;
				}
			}

			// zyk: tutorial, which teaches the player the RPG Mode features
			if (ent->client->pers.player_statuses & (1 << 25) && ent->client->pers.tutorial_timer < level.time)
			{
				zyk_show_tutorial(ent);

				// zyk: interval between messages
				ent->client->pers.tutorial_step++;
				ent->client->pers.tutorial_timer = level.time + 5000;
			}

			zyk_print_custom_quest_info(ent);

			if (ent->client->sess.amrpgmode == 2 && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ // zyk: RPG Mode skills and quests actions. Must be done if player is not at Spectator Mode
				
				/* zyk: Max Health used by jka code must be set by the max amount possible, so player can restore shield to the max possible shield as soon as
						he has the Shield Generator Upgrade
				*/
				ent->client->ps.stats[STAT_MAX_HEALTH] = 100 + (zyk_max_skill_level(SKILL_MAX_HEALTH) * RPG_MAX_HEALTH_INCREASE);

				// zyk: if player gets a medpac or shield booster, it may go above max because of STAT_MAX_HEALTH, so limit it here
				if (ent->health > ent->client->pers.max_rpg_health)
				{
					ent->health = ent->client->pers.max_rpg_health;
					ent->client->ps.stats[STAT_HEALTH] = ent->health;
				}

				if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield)
				{
					ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
				}

				if (!(ent->client->pers.player_statuses & (1 << 24)) && 
					(ent->client->pers.last_health != ent->health || 
					 ent->client->pers.last_shield != ent->client->ps.stats[STAT_ARMOR] || 
					 ent->client->pers.last_mp != ent->client->pers.magic_power || 
					 ent->client->pers.last_stamina != ent->client->pers.current_stamina))
				{
					ent->client->pers.last_health = ent->health;
					ent->client->pers.last_shield = ent->client->ps.stats[STAT_ARMOR];
					ent->client->pers.last_mp = ent->client->pers.magic_power;
					ent->client->pers.last_stamina = ent->client->pers.current_stamina;

					ent->client->pers.save_stats_changes = qtrue;
				}

				if (ent->client->pers.save_stats_changes == qtrue && ent->client->pers.save_stat_changes_timer < level.time)
				{
					save_account(ent, qtrue);

					// zyk: timer to save account to prevent too much file IO
					ent->client->pers.save_stats_changes = qfalse;
					ent->client->pers.save_stat_changes_timer = level.time + 400;
				}

				// zyk: Weapon Upgrades
				if (ent->client->ps.weapon == WP_DISRUPTOR && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DISRUPTOR] > 0 &&
					ent->client->ps.weaponTime > (weaponData[WP_DISRUPTOR].fireTime * 0.6))
				{
					ent->client->ps.weaponTime = weaponData[WP_DISRUPTOR].fireTime * 0.6;
				}

				if (ent->client->ps.weapon == WP_DEMP2 && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEMP2] > 0 &&
					ent->client->ps.weaponTime > (weaponData[WP_DEMP2].fireTime * 0.7))
				{
					ent->client->ps.weaponTime = weaponData[WP_DEMP2].fireTime * 0.7;
				}

				if (ent->client->ps.weapon == WP_BRYAR_PISTOL && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] > 0 &&
					ent->client->ps.weaponTime > (weaponData[WP_BRYAR_PISTOL].fireTime * 0.6))
				{
					ent->client->ps.weaponTime = weaponData[WP_BRYAR_PISTOL].fireTime * 0.6;
				}

				if (ent->client->ps.weapon == WP_BRYAR_OLD && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] > 0 &&
					ent->client->ps.weaponTime > (weaponData[WP_BRYAR_OLD].fireTime * 0.6))
				{
					ent->client->ps.weaponTime = weaponData[WP_BRYAR_OLD].fireTime * 0.6;
				}

				if (ent->client->ps.weapon == WP_REPEATER && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_REPEATER] > 0 && 
					ent->client->ps.weaponTime > (weaponData[WP_REPEATER].altFireTime * 0.5))
				{
					ent->client->ps.weaponTime = weaponData[WP_REPEATER].altFireTime * 0.5;
				}

				// zyk: Melee Punch Speed skill
				if (ent->client->ps.weapon == WP_MELEE && ent->client->pers.skill_levels[SKILL_MELEE_SPEED] > 0 && 
					ent->client->ps.weaponTime > (weaponData[WP_MELEE].fireTime * (1.0 - (0.2 * ent->client->pers.skill_levels[SKILL_MELEE_SPEED]))))
				{
					ent->client->ps.weaponTime = (weaponData[WP_MELEE].fireTime * (1.0 - (0.2 * ent->client->pers.skill_levels[SKILL_MELEE_SPEED])));
				}

				if (ent->client->pers.flame_thrower_timer > level.time && ent->client->cloakDebReduce < level.time)
				{ // zyk: fires the flame thrower
					Player_FireFlameThrower(ent, qfalse);
				}

				// zyk: updating RPG inventory and calculating current weight
				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_BLASTER_PACK] != ent->client->ps.ammo[AMMO_BLASTER])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_BLASTER_PACK] = ent->client->ps.ammo[AMMO_BLASTER];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_POWERCELL] != ent->client->ps.ammo[AMMO_POWERCELL])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_POWERCELL] = ent->client->ps.ammo[AMMO_POWERCELL];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_METAL_BOLTS] != ent->client->ps.ammo[AMMO_METAL_BOLTS])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_METAL_BOLTS] = ent->client->ps.ammo[AMMO_METAL_BOLTS];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_ROCKETS] != ent->client->ps.ammo[AMMO_ROCKETS])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_ROCKETS] = ent->client->ps.ammo[AMMO_ROCKETS];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_THERMALS] != ent->client->ps.ammo[AMMO_THERMAL])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_THERMALS] = ent->client->ps.ammo[AMMO_THERMAL];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_TRIPMINES] != ent->client->ps.ammo[AMMO_TRIPMINE])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_TRIPMINES] = ent->client->ps.ammo[AMMO_TRIPMINE];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_DETPACKS] != ent->client->ps.ammo[AMMO_DETPACK])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_DETPACKS] = ent->client->ps.ammo[AMMO_DETPACK];
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				// zyk: Jetpack Fuel changed. Update inventory
				if (ent->client->ps.jetpackFuel != ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_JETPACK_FUEL])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_JETPACK_FUEL] = ent->client->ps.jetpackFuel;
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				// zyk: Flame Thrower Fuel changed. Update inventory
				if (ent->client->ps.cloakFuel != ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_FLAME_THROWER_FUEL])
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_FLAME_THROWER_FUEL] = ent->client->ps.cloakFuel;
					ent->client->pers.rpg_inventory_modified = qtrue;
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_STUN_BATON] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_STUN_BATON)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_STUN_BATON);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_SABER] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_SABER);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_BLASTER_PISTOL] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_BLASTER_PISTOL);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_E11_BLASTER_RIFLE] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BLASTER)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_E11_BLASTER_RIFLE);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_DISRUPTOR] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DISRUPTOR)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_DISRUPTOR);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_BOWCASTER] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_BOWCASTER);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_REPEATER] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_REPEATER)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_REPEATER);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_DEMP2] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DEMP2)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_DEMP2);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_FLECHETTE] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_FLECHETTE)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_FLECHETTE);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_ROCKET_LAUNCHER] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_ROCKET_LAUNCHER);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_CONCUSSION] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_CONCUSSION)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_CONCUSSION);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_BRYAR_PISTOL] > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_OLD)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_WP_BRYAR_PISTOL);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_BINOCULARS] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_BINOCULARS);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_BACTA_CANISTER] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_BACTA_CANISTER);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_SENTRY_GUN] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_SENTRY_GUN);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_SEEKER_DRONE] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_SEEKER_DRONE);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_EWEB] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_EWEB);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_BIG_BACTA] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_BIG_BACTA);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_FORCE_FIELD] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_FORCE_FIELD);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_CLOAK] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_CLOAK);
				}

				if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_JETPACK] > 0 && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)))
				{
					zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_ITEM_JETPACK);
				}

				if (ent->client->pers.rpg_inventory_modified == qtrue)
				{ // zyk: save account with new updated inventory
					save_account(ent, qtrue);
				}

				zyk_calculate_current_weight(ent);
				
				if (ent->client->pers.thermal_vision == qtrue && ent->client->ps.zoomMode == 0)
				{ // zyk: if player stops using binoculars, stop the Thermal Vision
					ent->client->pers.thermal_vision = qfalse;
					ent->client->ps.fd.forcePowersActive &= ~(1 << FP_SEE);
					ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
					ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_0;

					ent->client->pers.thermal_vision_cooldown_time = level.time + 300;
				}
				else if (ent->client->pers.thermal_vision == qfalse && ent->client->ps.zoomMode == 2 && 
						ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_THERMAL_VISION] > 0)
				{ // zyk: Thermal Vision Upgrade, activate the Thermal Vision
					ent->client->pers.thermal_vision = qtrue;
					ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
					ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_3;
					ent->client->ps.fd.forcePowersActive |= (1 << FP_SEE);

					// zyk: adds some time to allow deactivating the Binoculars. Force Sense is active, so using this variable to add the cooldown time
					ent->client->ps.forceAllowDeactivateTime = level.time + 300;

					ent->client->pers.thermal_vision_cooldown_time = level.time + 300;
				}

				if (level.quest_map > QUESTMAP_NONE && zyk_allow_quests.integer > 0 && ent->client->ps.duelInProgress == qfalse && ent->health > 0 &&
					level.quest_debounce_timer < level.time && ent->client->pers.connected == CON_CONNECTED && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
				{ // zyk: control the quest events which happen in the quest maps, if player can play quests now, is alive and is not in a private duel
					level.quest_debounce_timer = level.time + 100;

					if (level.spawned_quest_stuff == qfalse)
					{
						zyk_spawn_quest_stuff(qtrue);

						level.spawned_quest_stuff = qtrue;
					}

					if (level.quest_map == QUESTMAP_HERO_HOUSE && !(ent->client->pers.main_quest_progress & (1 << QUEST_PROLOGUE)))
					{ // zyk: Hero's House, show the Prologue to the player
						vec3_t heros_house_center;

						VectorSet(heros_house_center, -64, -512, 88);

						if (Distance(ent->r.currentOrigin, heros_house_center) < 120 && ent->client->pers.quest_event_timer < level.time)
						{
							if (ent->client->pers.current_quest_event == 0)
							{
								if (!(ent->client->pers.player_settings & ((1 << SETTINGS_BOSS_MUSIC))))
								{
									zyk_set_map_music(MAPMUSIC_PROLOGUE, 500);
								}

								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7years ago, after the Great War, evil took over everything.\"");
							}
							else if (ent->client->pers.current_quest_event == 1)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7the evil king Drakon created an age of oppression and sadness for people.\"");
							}
							else if (ent->client->pers.current_quest_event == 2)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7a rebel group was formed to fight the king's forces, but failed. The rebel survivors either got arrested or fled.\"");
							}
							else if (ent->client->pers.current_quest_event == 3)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7now people wait for a hero who will bring hope and will be able to defeat King Drakon.\"");
							}
							else if (ent->client->pers.current_quest_event == 4)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7you are this hero! With your courage, you can fight for freedom!\"");
							}
							else if (ent->client->pers.current_quest_event == 5)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7for now, you know you must find more information that will help you get more power and allies for this quest.\"");
							}
							else if (ent->client->pers.current_quest_event == 6)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7going to the Desert City and talking to people there perhaps will give you the information you need.\"");
							}
							else if (ent->client->pers.current_quest_event == 7)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7you can also visit cities and places to get levels and information.\"");
							}
							else if (ent->client->pers.current_quest_event == 8)
							{
								trap->SendServerCommand(ent->s.number, "chat \"^3Prologue: ^7the Main Quest begins!\"");

								ent->client->pers.main_quest_progress |= (1 << QUEST_PROLOGUE);
								save_account(ent, qtrue);

								zyk_set_map_music(MAPMUSIC_NONE, 500);
							}
							
							ent->client->pers.current_quest_event++;
							ent->client->pers.quest_event_timer = level.time + 5000;
						}
					}
				}

				if (level.custom_quest_map > -1 && level.zyk_custom_quest_timer < level.time && ent->client->ps.duelInProgress == qfalse && ent->health > 0 && 
					(level.zyk_quest_test_origin == qfalse || Distance(ent->client->ps.origin, level.zyk_quest_mission_origin) < level.zyk_quest_radius))
				{ // zyk: Custom Quest map
					char *zyk_keys[5] = {"text", "npc", "item", "entfile", "" };
					int j = 0;
					qboolean still_has_keys = qfalse;

					for (j = 0; j < 5; j++)
					{ // zyk: testing each key and processing them when found in this mission
						char *zyk_value = zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("%s%d", zyk_keys[j], level.zyk_custom_quest_counter));

						if (Q_stricmp(zyk_value, "") != 0)
						{ // zyk: there is a value for this key
							char *zyk_map = zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("map%d", level.zyk_custom_quest_counter));

							still_has_keys = qtrue;

							if (Q_stricmp(level.zykmapname, zyk_map) == 0)
							{ // zyk: this mission step is in this map
								if (Q_stricmp(zyk_keys[j], "text") == 0)
								{ // zyk: a text message
									int zyk_timer = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("texttimer%d", level.zyk_custom_quest_counter)));

									if (zyk_timer <= 0)
									{
										zyk_timer = 5000;
									}

									trap->SendServerCommand(-1, va("chat \"%s\n\"", zyk_value));
									level.zyk_custom_quest_timer = level.time + zyk_timer;
									level.zyk_custom_quest_counter++;

									// zyk: increasing the number of steps done in this mission
									zyk_set_quest_field(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done", va("%d", atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done")) + 1));
								}
								else if (Q_stricmp(zyk_keys[j], "entfile") == 0)
								{
									char zykserverinfo[MAX_INFO_STRING] = { 0 };
									char zyk_mapname[128] = { 0 };
									int k = 0;

									// zyk: getting mapname
									trap->GetServerinfo(zykserverinfo, sizeof(zykserverinfo));
									Q_strncpyz(zyk_mapname, Info_ValueForKey(zykserverinfo, "mapname"), sizeof(zyk_mapname));
									strcpy(level.load_entities_file, va("zykmod/entities/%s/%s.txt", zyk_mapname, zyk_value));

									// zyk: cleaning entities. Only the ones from the file will be in the map
									for (k = (MAX_CLIENTS + BODY_QUEUE_SIZE); k < level.num_entities; k++)
									{
										gentity_t *target_ent = &g_entities[k];

										if (target_ent)
											G_FreeEntity(target_ent);
									}

									level.load_entities_timer = level.time + 1050;
									level.zyk_custom_quest_counter++;
									level.zyk_custom_quest_timer = level.time + 2000;

									// zyk: increasing the number of steps done in this mission
									zyk_set_quest_field(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done", va("%d", atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done")) + 1));
								}
								else if (Q_stricmp(zyk_keys[j], "npc") == 0)
								{ // zyk: npc battle
									int zyk_timer = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npctimer%d", level.zyk_custom_quest_counter)));
									int npc_count = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npccount%d", level.zyk_custom_quest_counter)));
									int npc_yaw = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcyaw%d", level.zyk_custom_quest_counter)));
									int k = 0;

									if (zyk_timer <= 0)
									{
										zyk_timer = 1000;
									}

									if (npc_count <= 0)
									{
										npc_count = 1;
									}

									for (k = 0; k < npc_count; k++)
									{
										gentity_t *zyk_npc = NULL;
										vec3_t zyk_vec;

										if (sscanf(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcorigin%d", level.zyk_custom_quest_counter)), "%f %f %f", &zyk_vec[0], &zyk_vec[1], &zyk_vec[2]) != 3)
										{ // zyk: if there was not a valid npcorigin, use the mission origin instead
											VectorCopy(level.zyk_quest_mission_origin, zyk_vec);
										}

										zyk_npc = Zyk_NPC_SpawnType(zyk_value, zyk_vec[0], zyk_vec[1], zyk_vec[2], npc_yaw);

										if (zyk_npc)
										{
											int zyk_enemy = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcenemy%d", level.zyk_custom_quest_counter)));
											int zyk_ally = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcally%d", level.zyk_custom_quest_counter)));
											int zyk_health = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npchealth%d", level.zyk_custom_quest_counter)));
											int zyk_boss = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcboss%d", level.zyk_custom_quest_counter)));

											zyk_npc->client->pers.player_statuses |= (1 << 28);

											if (zyk_health > 0)
											{ // zyk: custom npc health
												zyk_npc->NPC->stats.health = zyk_health;
												zyk_npc->client->ps.stats[STAT_MAX_HEALTH] = zyk_health;
												zyk_npc->health = zyk_health;
											}

											if (zyk_enemy > 0)
											{ // zyk: force it to be enemy
												zyk_npc->client->playerTeam = NPCTEAM_ENEMY;
												zyk_npc->client->enemyTeam = NPCTEAM_PLAYER;
											}

											if (zyk_ally > 0)
											{ // zyk: force it to be ally
												zyk_npc->client->playerTeam = NPCTEAM_PLAYER;
												zyk_npc->client->enemyTeam = NPCTEAM_ENEMY;
											}

											if (zyk_boss > 0)
											{ // zyk: set it as a Custom Quest boss
												zyk_npc->client->pers.custom_quest_boss_npc = 1;
											}

											if (zyk_npc->client->playerTeam == NPCTEAM_PLAYER)
											{ // zyk: if ally, must count this npc in the counter until mission ends
												level.zyk_quest_ally_npc_count++;
											}
											else
											{ // zyk: if any non-ally team, must count this npc in the counter and hold mission until all npcs are defeated
												level.zyk_hold_quest_mission = qtrue;
												level.zyk_quest_npc_count++;
											}

											zyk_set_quest_npc_abilities(zyk_npc);
										}
									}

									level.zyk_custom_quest_timer = level.time + zyk_timer;
									level.zyk_custom_quest_counter++;
								}
								else if (Q_stricmp(zyk_keys[j], "item") == 0)
								{ // zyk: items to find
									char *zyk_item_origin = zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("itemorigin%d", level.zyk_custom_quest_counter));
									gentity_t *new_ent = G_Spawn();

									zyk_set_entity_field(new_ent, "classname", G_NewString(zyk_value));
									zyk_set_entity_field(new_ent, "spawnflags", "262144");
									zyk_set_entity_field(new_ent, "origin", zyk_item_origin);

									zyk_spawn_entity(new_ent);

									level.zyk_quest_item_count++;

									level.zyk_custom_quest_timer = level.time + 1000;
									level.zyk_custom_quest_counter++;
									level.zyk_hold_quest_mission = qtrue;
								}
							}
							else
							{ // zyk: will test map in the next step
								level.zyk_custom_quest_counter++;
							}
						}
					}

					// zyk: no more fields to test, pass the mission
					if (still_has_keys == qfalse && level.zyk_hold_quest_mission == qfalse)
					{
						int zyk_steps = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "steps"));
						int zyk_done = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done"));
						int k = 0;
						int zyk_prize = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, "prize"));

						if (zyk_done >= zyk_steps)
						{ // zyk: completed all steps of this mission
							if (zyk_prize > 0)
							{ // zyk: add this amount of credits to all players in quest area
								for (k = 0; k < level.maxclients; k++)
								{
									gentity_t *zyk_ent = &g_entities[k];

									if (zyk_ent && zyk_ent->client && zyk_ent->client->sess.amrpgmode == 2 && zyk_ent->client->sess.sessionTeam != TEAM_SPECTATOR && 
										(level.zyk_quest_test_origin == qfalse || Distance(zyk_ent->client->ps.origin, level.zyk_quest_mission_origin) < level.zyk_quest_radius))
									{ // zyk: only players in the quest area can receive the prize
										add_credits(ent, zyk_prize);
										trap->SendServerCommand(zyk_ent->s.number, va("chat \"^3Custom Quest: ^7Got ^2%d ^7credits\n\"", zyk_prize));
									}
								}
							}

							if ((level.zyk_custom_quest_current_mission + 1) >= level.zyk_custom_quest_mission_count[level.custom_quest_map])
							{ // zyk: completed all missions, reset quest to the first mission
								level.zyk_custom_quest_main_fields[level.custom_quest_map][2] = "0";
							}
							else
							{
								level.zyk_custom_quest_main_fields[level.custom_quest_map][2] = G_NewString(va("%d", level.zyk_custom_quest_current_mission + 1));
							}

							// zyk: reset the steps done for this mission
							zyk_set_quest_field(level.custom_quest_map, level.zyk_custom_quest_current_mission, "done", "0");

							for (k = 0; k < level.zyk_custom_quest_mission_values_count[level.custom_quest_map][level.zyk_custom_quest_current_mission] / 2; k++)
							{ // zyk: goes through all keys of this mission to find the map keys with the current map and reset them
								char *zyk_map = zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("donemap%d", k));

								if (Q_stricmp(zyk_map, "") != 0)
								{
									zyk_set_quest_field(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("donemap%d", k), "zykremovekey");
								}
							}

							trap->SendServerCommand(-1, "chat \"^3Custom Quest: ^7Mission complete\n\"");
						}
						else
						{ // zyk: completed a step but not the entire mission yet, because some steps are in other maps
							for (k = 0; k < level.zyk_custom_quest_mission_values_count[level.custom_quest_map][level.zyk_custom_quest_current_mission] / 2; k++)
							{ // zyk: goes through all keys of this mission to find the map keys with the current map and set them as done
								char *zyk_map = zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("map%d", k));

								if (Q_stricmp(level.zykmapname, zyk_map) == 0)
								{
									zyk_set_quest_field(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("donemap%d", k), "yes");
								}
							}

							trap->SendServerCommand(-1, "chat \"^3Custom Quest: ^7Objectives complete\n\"");
						}

						save_quest_file(level.custom_quest_map);

						load_custom_quest_mission();
					}
				}
			}

			if (level.gametype == GT_SIEGE &&
				ent->client->siegeClass != -1 &&
				(bgSiegeClasses[ent->client->siegeClass].classflags & (1<<CFL_STATVIEWER)))
			{ //see if it's time to send this guy an update of extended info
				if (ent->client->siegeEDataSend < level.time)
				{
                    G_SiegeClientExData(ent);
					ent->client->siegeEDataSend = level.time + 1000; //once every sec seems ok
				}
			}

			if((!level.intermissiontime)&&!(ent->client->ps.pm_flags&PMF_FOLLOW) && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{
				WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
				WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
				WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
			}

			if (g_allowNPC.integer)
			{
				//This was originally intended to only be done for client 0.
				//Make sure it doesn't slow things down too much with lots of clients in game.
				NAV_FindPlayerWaypoint(i);
			}

			trap->ICARUS_MaintainTaskManager(ent->s.number);

			G_RunClient( ent );
			continue;
		}
		else if (ent->s.eType == ET_NPC)
		{
			int j;
			// turn off any expired powerups
			for ( j = 0 ; j < MAX_POWERUPS ; j++ ) {
				if ( ent->client->ps.powerups[ j ] < level.time ) {
					ent->client->ps.powerups[ j ] = 0;
				}
			}

			WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
			WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
			WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);

			quest_power_events(ent);
			fire_bolt_hits(ent);

			// zyk: npcs cannot enter the Duel Tournament arena
			if (level.duel_tournament_mode == 4 && 
				Distance(ent->r.currentOrigin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * zyk_duel_tournament_arena_scale.value / 100.0))
			{
				ent->health = 0;
				ent->client->ps.stats[STAT_HEALTH] = 0;
				if (ent->die)
				{
					ent->die(ent, ent, ent, 100, MOD_UNKNOWN);
				}
			}

			// zyk: quest npc events
			if (ent->client->pers.quest_npc > 0 && ent->health > 0 && ent->client->pers.quest_npc_timer < level.time && 
				ent->client->pers.quest_npc_current_event < MAX_QUEST_NPC_EVENTS)
			{
				if (level.quest_map == QUESTMAP_DESERT_CITY)
				{
					if (ent->client->pers.quest_npc == 1)
					{
						G_SetAnim(ent, NULL, SETANIM_BOTH, ent->client->pers.quest_npc_anims[ent->client->pers.quest_npc_current_event], SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
						ent->client->ps.torsoTimer = ent->client->pers.quest_npc_anim_duration[ent->client->pers.quest_npc_current_event];
						ent->client->ps.legsTimer = ent->client->ps.torsoTimer;

						ent->client->pers.quest_npc_timer = level.time + ent->client->pers.quest_npc_interval_timer[ent->client->pers.quest_npc_current_event];
						ent->client->pers.quest_npc_current_event++;

						if (ent->client->pers.quest_npc_current_event == 1)
						{
							VectorSet(ent->NPC->tempGoal->r.currentOrigin, ent->client->ps.origin[0] + 100, ent->client->ps.origin[1] + 100, -486);
							ent->NPC->goalEntity = ent->NPC->tempGoal;
							ent->NPC->tempBehavior = BS_INVESTIGATE;
						}
					}
				}
			}

			// zyk: abilities of custom quest npcs
			if (ent->client->pers.player_statuses & (1 << 28) && ent->health > 0)
			{
				if (ent->client->pers.quest_power_usage_timer < level.time)
				{
					int random_magic = Q_irand(0, (MAX_MAGIC_POWERS - 1));
					int first_magic_skill = SKILL_MAGIC_MAGIC_SENSE;
					int current_magic_skill = first_magic_skill;

					// zyk: adding all magic powers to this npc
					while (current_magic_skill < NUMBER_OF_SKILLS)
					{
						if (random_magic < 31 && ent->client->pers.custom_quest_magic & (1 << random_magic) && ent->client->pers.skill_levels[current_magic_skill] < 1)
						{
							ent->client->pers.skill_levels[current_magic_skill] = 2;
						}

						current_magic_skill++;
					}

					current_magic_skill = first_magic_skill + 31;

					// zyk: adding all magic powers to this npc
					while (current_magic_skill < NUMBER_OF_SKILLS)
					{
						if (random_magic >= 31 && ent->client->pers.custom_quest_more_magic & (1 << (random_magic - 31)) && ent->client->pers.skill_levels[current_magic_skill] < 1)
						{
							ent->client->pers.skill_levels[current_magic_skill] = 2;
						}

						current_magic_skill++;
					}

					// zyk: regen mp of this npc
					if (ent->client->pers.magic_power < 20)
					{
						ent->client->pers.magic_power = 500;
					}

					zyk_cast_magic(ent, first_magic_skill + random_magic);
				}
			}

			if (ent->health > 0 && Q_stricmp(ent->NPC_type, "quest_mage") == 0 && ent->enemy && ent->client->pers.quest_power_usage_timer < level.time)
			{ // zyk: powers used by the quest_mage npc
				int random_magic = Q_irand(0, MAGIC_LIGHT_MAGIC);
				int first_magic_skill = SKILL_MAGIC_MAGIC_SENSE;
				int current_magic_skill = first_magic_skill;

				// zyk: adding all magic powers to this npc
				while (current_magic_skill < NUMBER_OF_SKILLS)
				{
					if (ent->client->pers.skill_levels[current_magic_skill] < 1)
					{
						ent->client->pers.skill_levels[current_magic_skill] = zyk_max_skill_level(current_magic_skill);
					}

					current_magic_skill++;
				}

				// zyk: regen mp and level of this mage
				if (ent->client->pers.magic_power < 20)
				{
					ent->client->pers.level = 100;
					ent->client->pers.skill_levels[SKILL_MAX_MP] = zyk_max_skill_level(SKILL_MAX_MP) - 2;
					ent->client->pers.magic_power = zyk_max_magic_power(ent);
				}

				zyk_cast_magic(ent, first_magic_skill + random_magic);
			}
		}

		if (ent) // zyk: testing if ent is not NULL
			G_RunThink( ent );

		if (g_allowNPC.integer)
		{
			ClearNPCGlobals();
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ItemRun = trap->PrecisionTimer_End(timer_ItemRun);
#endif

	SiegeCheckTimers();

#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_ROFF);
#endif
	trap->ROFF_UpdateEntities();
#ifdef _G_FRAME_PERFANAL
	iTimer_ROFF = trap->PrecisionTimer_End(timer_ROFF);
#endif



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_ClientEndframe);
#endif
	// perform final fixups on the players
	ent = &g_entities[0];
	for (i=0 ; i < level.maxclients ; i++, ent++ ) {
		if ( ent->inuse ) {
			ClientEndFrame( ent );
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ClientEndframe = trap->PrecisionTimer_End(timer_ClientEndframe);
#endif



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_GameChecks);
#endif
	// see if it is time to do a tournament restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

	// cancel vote if timed out
	CheckVote();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

#ifdef _G_FRAME_PERFANAL
	iTimer_GameChecks = trap->PrecisionTimer_End(timer_GameChecks);
#endif



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_Queues);
#endif
	//At the end of the frame, send out the ghoul2 kill queue, if there is one
	G_SendG2KillQueue();

	if (gQueueScoreMessage)
	{
		if (gQueueScoreMessageTime < level.time)
		{
			SendScoreboardMessageToAllClients();

			gQueueScoreMessageTime = 0;
			gQueueScoreMessage = 0;
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_Queues = trap->PrecisionTimer_End(timer_Queues);
#endif



#ifdef _G_FRAME_PERFANAL
	Com_Printf("---------------\nItemRun: %i\nROFF: %i\nClientEndframe: %i\nGameChecks: %i\nQueues: %i\n---------------\n",
		iTimer_ItemRun,
		iTimer_ROFF,
		iTimer_ClientEndframe,
		iTimer_GameChecks,
		iTimer_Queues);
#endif

	g_LastFrameTime = level.time;
}

const char *G_GetStringEdString(char *refSection, char *refName)
{
	/*
	static char text[1024]={0};
	trap->SP_GetStringTextString(va("%s_%s", refSection, refName), text, sizeof(text));
	return text;
	*/

	//Well, it would've been lovely doing it the above way, but it would mean mixing
	//languages for the client depending on what the server is. So we'll mark this as
	//a stringed reference with @@@ and send the refname to the client, and when it goes
	//to print it will get scanned for the stringed reference indication and dealt with
	//properly.
	static char text[1024]={0};
	Com_sprintf(text, sizeof(text), "@@@%s", refName);
	return text;
}

static void G_SpawnRMGEntity( void ) {
	if ( G_ParseSpawnVars( qfalse ) )
		G_SpawnGEntityFromSpawnVars( qfalse );
}

static void _G_ROFF_NotetrackCallback( int entID, const char *notetrack ) {
	G_ROFF_NotetrackCallback( &g_entities[entID], notetrack );
}

static int G_ICARUS_PlaySound( void ) {
	T_G_ICARUS_PLAYSOUND *sharedMem = &gSharedBuffer.playSound;
	return Q3_PlaySound( sharedMem->taskID, sharedMem->entID, sharedMem->name, sharedMem->channel );
}
static qboolean G_ICARUS_Set( void ) {
	T_G_ICARUS_SET *sharedMem = &gSharedBuffer.set;
	return Q3_Set( sharedMem->taskID, sharedMem->entID, sharedMem->type_name, sharedMem->data );
}
static void G_ICARUS_Lerp2Pos( void ) {
	T_G_ICARUS_LERP2POS *sharedMem = &gSharedBuffer.lerp2Pos;
	Q3_Lerp2Pos( sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->nullAngles ? NULL : sharedMem->angles, sharedMem->duration );
}
static void G_ICARUS_Lerp2Origin( void ) {
	T_G_ICARUS_LERP2ORIGIN *sharedMem = &gSharedBuffer.lerp2Origin;
	Q3_Lerp2Origin( sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->duration );
}
static void G_ICARUS_Lerp2Angles( void ) {
	T_G_ICARUS_LERP2ANGLES *sharedMem = &gSharedBuffer.lerp2Angles;
	Q3_Lerp2Angles( sharedMem->taskID, sharedMem->entID, sharedMem->angles, sharedMem->duration );
}
static int G_ICARUS_GetTag( void ) {
	T_G_ICARUS_GETTAG *sharedMem = &gSharedBuffer.getTag;
	return Q3_GetTag( sharedMem->entID, sharedMem->name, sharedMem->lookup, sharedMem->info );
}
static void G_ICARUS_Lerp2Start( void ) {
	T_G_ICARUS_LERP2START *sharedMem = &gSharedBuffer.lerp2Start;
	Q3_Lerp2Start( sharedMem->entID, sharedMem->taskID, sharedMem->duration );
}
static void G_ICARUS_Lerp2End( void ) {
	T_G_ICARUS_LERP2END *sharedMem = &gSharedBuffer.lerp2End;
	Q3_Lerp2End( sharedMem->entID, sharedMem->taskID, sharedMem->duration );
}
static void G_ICARUS_Use( void ) {
	T_G_ICARUS_USE *sharedMem = &gSharedBuffer.use;
	Q3_Use( sharedMem->entID, sharedMem->target );
}
static void G_ICARUS_Kill( void ) {
	T_G_ICARUS_KILL *sharedMem = &gSharedBuffer.kill;
	Q3_Kill( sharedMem->entID, sharedMem->name );
}
static void G_ICARUS_Remove( void ) {
	T_G_ICARUS_REMOVE *sharedMem = &gSharedBuffer.remove;
	Q3_Remove( sharedMem->entID, sharedMem->name );
}
static void G_ICARUS_Play( void ) {
	T_G_ICARUS_PLAY *sharedMem = &gSharedBuffer.play;
	Q3_Play( sharedMem->taskID, sharedMem->entID, sharedMem->type, sharedMem->name );
}
static int G_ICARUS_GetFloat( void ) {
	T_G_ICARUS_GETFLOAT *sharedMem = &gSharedBuffer.getFloat;
	return Q3_GetFloat( sharedMem->entID, sharedMem->type, sharedMem->name, &sharedMem->value );
}
static int G_ICARUS_GetVector( void ) {
	T_G_ICARUS_GETVECTOR *sharedMem = &gSharedBuffer.getVector;
	return Q3_GetVector( sharedMem->entID, sharedMem->type, sharedMem->name, sharedMem->value );
}
static int G_ICARUS_GetString( void ) {
	T_G_ICARUS_GETSTRING *sharedMem = &gSharedBuffer.getString;
	char *crap = NULL; //I am sorry for this -rww
	char **morecrap = &crap; //and this
	int r = Q3_GetString( sharedMem->entID, sharedMem->type, sharedMem->name, morecrap );

	if ( crap )
		strcpy( sharedMem->value, crap );

	return r;
}
static void G_ICARUS_SoundIndex( void ) {
	T_G_ICARUS_SOUNDINDEX *sharedMem = &gSharedBuffer.soundIndex;
	G_SoundIndex( sharedMem->filename );
}
static int G_ICARUS_GetSetIDForString( void ) {
	T_G_ICARUS_GETSETIDFORSTRING *sharedMem = &gSharedBuffer.getSetIDForString;
	return GetIDForString( setTable, sharedMem->string );
}
static qboolean G_NAV_ClearPathToPoint( int entID, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask, int okToHitEnt ) {
	return NAV_ClearPathToPoint( &g_entities[entID], pmins, pmaxs, point, clipmask, okToHitEnt );
}
static qboolean G_NPC_ClearLOS2( int entID, const vec3_t end ) {
	return NPC_ClearLOS2( &g_entities[entID], end );
}
static qboolean	G_NAV_CheckNodeFailedForEnt( int entID, int nodeNum ) {
	return NAV_CheckNodeFailedForEnt( &g_entities[entID], nodeNum );
}

/*
============
GetModuleAPI
============
*/

gameImport_t *trap = NULL;

Q_EXPORT gameExport_t* QDECL GetModuleAPI( int apiVersion, gameImport_t *import )
{
	static gameExport_t ge = {0};

	assert( import );
	trap = import;
	Com_Printf	= trap->Print;
	Com_Error	= trap->Error;

	memset( &ge, 0, sizeof( ge ) );

	if ( apiVersion != GAME_API_VERSION ) {
		trap->Print( "Mismatched GAME_API_VERSION: expected %i, got %i\n", GAME_API_VERSION, apiVersion );
		return NULL;
	}

	ge.InitGame							= G_InitGame;
	ge.ShutdownGame						= G_ShutdownGame;
	ge.ClientConnect					= ClientConnect;
	ge.ClientBegin						= ClientBegin;
	ge.ClientUserinfoChanged			= ClientUserinfoChanged;
	ge.ClientDisconnect					= ClientDisconnect;
	ge.ClientCommand					= ClientCommand;
	ge.ClientThink						= ClientThink;
	ge.RunFrame							= G_RunFrame;
	ge.ConsoleCommand					= ConsoleCommand;
	ge.BotAIStartFrame					= BotAIStartFrame;
	ge.ROFF_NotetrackCallback			= _G_ROFF_NotetrackCallback;
	ge.SpawnRMGEntity					= G_SpawnRMGEntity;
	ge.ICARUS_PlaySound					= G_ICARUS_PlaySound;
	ge.ICARUS_Set						= G_ICARUS_Set;
	ge.ICARUS_Lerp2Pos					= G_ICARUS_Lerp2Pos;
	ge.ICARUS_Lerp2Origin				= G_ICARUS_Lerp2Origin;
	ge.ICARUS_Lerp2Angles				= G_ICARUS_Lerp2Angles;
	ge.ICARUS_GetTag					= G_ICARUS_GetTag;
	ge.ICARUS_Lerp2Start				= G_ICARUS_Lerp2Start;
	ge.ICARUS_Lerp2End					= G_ICARUS_Lerp2End;
	ge.ICARUS_Use						= G_ICARUS_Use;
	ge.ICARUS_Kill						= G_ICARUS_Kill;
	ge.ICARUS_Remove					= G_ICARUS_Remove;
	ge.ICARUS_Play						= G_ICARUS_Play;
	ge.ICARUS_GetFloat					= G_ICARUS_GetFloat;
	ge.ICARUS_GetVector					= G_ICARUS_GetVector;
	ge.ICARUS_GetString					= G_ICARUS_GetString;
	ge.ICARUS_SoundIndex				= G_ICARUS_SoundIndex;
	ge.ICARUS_GetSetIDForString			= G_ICARUS_GetSetIDForString;
	ge.NAV_ClearPathToPoint				= G_NAV_ClearPathToPoint;
	ge.NPC_ClearLOS2					= G_NPC_ClearLOS2;
	ge.NAVNEW_ClearPathBetweenPoints	= NAVNEW_ClearPathBetweenPoints;
	ge.NAV_CheckNodeFailedForEnt		= G_NAV_CheckNodeFailedForEnt;
	ge.NAV_EntIsUnlockedDoor			= G_EntIsUnlockedDoor;
	ge.NAV_EntIsDoor					= G_EntIsDoor;
	ge.NAV_EntIsBreakable				= G_EntIsBreakable;
	ge.NAV_EntIsRemovableUsable			= G_EntIsRemovableUsable;
	ge.NAV_FindCombatPointWaypoints		= CP_FindCombatPointWaypoints;
	ge.BG_GetItemIndexByTag				= BG_GetItemIndexByTag;

	return &ge;
}

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain( int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4,
	intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11 )
{
	switch ( command ) {
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2 );
		return 0;

	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;

	case GAME_CLIENT_CONNECT:
		return (intptr_t)ClientConnect( arg0, arg1, arg2 );

	case GAME_CLIENT_THINK:
		ClientThink( arg0, NULL );
		return 0;

	case GAME_CLIENT_USERINFO_CHANGED:
		ClientUserinfoChanged( arg0 );
		return 0;

	case GAME_CLIENT_DISCONNECT:
		ClientDisconnect( arg0 );
		return 0;

	case GAME_CLIENT_BEGIN:
		ClientBegin( arg0, qtrue );
		return 0;

	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;

	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;

	case GAME_CONSOLE_COMMAND:
		return ConsoleCommand();

	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );

	case GAME_ROFF_NOTETRACK_CALLBACK:
		_G_ROFF_NotetrackCallback( arg0, (const char *)arg1 );
		return 0;

	case GAME_SPAWN_RMG_ENTITY:
		G_SpawnRMGEntity();
		return 0;

	case GAME_ICARUS_PLAYSOUND:
		return G_ICARUS_PlaySound();

	case GAME_ICARUS_SET:
		return G_ICARUS_Set();

	case GAME_ICARUS_LERP2POS:
		G_ICARUS_Lerp2Pos();
		return 0;

	case GAME_ICARUS_LERP2ORIGIN:
		G_ICARUS_Lerp2Origin();
		return 0;

	case GAME_ICARUS_LERP2ANGLES:
		G_ICARUS_Lerp2Angles();
		return 0;

	case GAME_ICARUS_GETTAG:
		return G_ICARUS_GetTag();

	case GAME_ICARUS_LERP2START:
		G_ICARUS_Lerp2Start();
		return 0;

	case GAME_ICARUS_LERP2END:
		G_ICARUS_Lerp2End();
		return 0;

	case GAME_ICARUS_USE:
		G_ICARUS_Use();
		return 0;

	case GAME_ICARUS_KILL:
		G_ICARUS_Kill();
		return 0;

	case GAME_ICARUS_REMOVE:
		G_ICARUS_Remove();
		return 0;

	case GAME_ICARUS_PLAY:
		G_ICARUS_Play();
		return 0;

	case GAME_ICARUS_GETFLOAT:
		return G_ICARUS_GetFloat();

	case GAME_ICARUS_GETVECTOR:
		return G_ICARUS_GetVector();

	case GAME_ICARUS_GETSTRING:
		return G_ICARUS_GetString();

	case GAME_ICARUS_SOUNDINDEX:
		G_ICARUS_SoundIndex();
		return 0;

	case GAME_ICARUS_GETSETIDFORSTRING:
		return G_ICARUS_GetSetIDForString();

	case GAME_NAV_CLEARPATHTOPOINT:
		return G_NAV_ClearPathToPoint( arg0, (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5 );

	case GAME_NAV_CLEARLOS:
		return G_NPC_ClearLOS2( arg0, (const float *)arg1 );

	case GAME_NAV_CLEARPATHBETWEENPOINTS:
		return NAVNEW_ClearPathBetweenPoints((float *)arg0, (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5);

	case GAME_NAV_CHECKNODEFAILEDFORENT:
		return NAV_CheckNodeFailedForEnt(&g_entities[arg0], arg1);

	case GAME_NAV_ENTISUNLOCKEDDOOR:
		return G_EntIsUnlockedDoor(arg0);

	case GAME_NAV_ENTISDOOR:
		return G_EntIsDoor(arg0);

	case GAME_NAV_ENTISBREAKABLE:
		return G_EntIsBreakable(arg0);

	case GAME_NAV_ENTISREMOVABLEUSABLE:
		return G_EntIsRemovableUsable(arg0);

	case GAME_NAV_FINDCOMBATPOINTWAYPOINTS:
		CP_FindCombatPointWaypoints();
		return 0;

	case GAME_GETITEMINDEXBYTAG:
		return BG_GetItemIndexByTag(arg0, arg1);
	}

	return -1;
}
