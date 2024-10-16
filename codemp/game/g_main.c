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

// zyk: similar to TeleportPlayer(), but this one doesnt spit the player out at the destination
void zyk_TeleportPlayer(gentity_t* player, vec3_t origin, vec3_t angles) {
	gentity_t* tent;
	qboolean	isNPC = qfalse;

	if (player->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
	if (player->client->sess.sessionTeam != TEAM_SPECTATOR) {
		tent = G_TempEntity(player->client->ps.origin, EV_PLAYER_TELEPORT_OUT);
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity(origin, EV_PLAYER_TELEPORT_IN);
		tent->s.clientNum = player->s.clientNum;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap->UnlinkEntity((sharedEntity_t*)player);

	VectorCopy(origin, player->client->ps.origin);
	player->client->ps.origin[2] += 1;

	// set angles
	SetClientViewAngle(player, angles);

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// kill anything at the destination
	if (player->client->sess.sessionTeam != TEAM_SPECTATOR) {
		G_KillBox(player);
	}

	// save results of pmove
	BG_PlayerStateToEntityState(&player->client->ps, &player->s, qtrue);
	if (isNPC)
	{
		player->s.eType = ET_NPC;
	}

	// use the precise origin for linking
	VectorCopy(player->client->ps.origin, player->r.currentOrigin);

	if (player->client->sess.sessionTeam != TEAM_SPECTATOR) {
		trap->LinkEntity((sharedEntity_t*)player);
	}
}

// zyk: function to kill npcs with the name as parameter
void zyk_NPC_Kill_f(char* name)
{
	int	n = 0;
	gentity_t* player = NULL;

	for (n = level.maxclients; n < level.num_entities; n++)
	{
		player = &g_entities[n];
		if (player && player->NPC && player->client)
		{
			if (Q_stricmp(name, player->NPC_type) == 0 || Q_stricmp(name, "all") == 0)
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

gentity_t* zyk_find_entity_for_quest()
{
	int min_entity_id = (MAX_CLIENTS + BODY_QUEUE_SIZE);
	int chosen_entity_index = 0; // zyk: npc origin will be at a random map entity origin
	gentity_t* valid_entities[ENTITYNUM_MAX_NORMAL];
	int i = 0, j = 0;
	int total_valid_entities = 0;

	// zyk: initializing valid_entities
	for (i = 0; i < ENTITYNUM_MAX_NORMAL; i++)
	{
		valid_entities[i] = NULL;
	}

	// zyk: get possible entities to be used to place quest stuff
	for (i = min_entity_id; i < level.num_entities; i++)
	{
		gentity_t* current_entity = &g_entities[i];

		if (current_entity &&
			Q_stricmp(current_entity->classname, "freed") != 0 &&
			Q_stricmp(current_entity->classname, "lightsaber") != 0 &&
			Q_stricmp(current_entity->classname, "tempEntity") != 0 &&
			Q_stricmp(current_entity->classname, "light") != 0 &&
			Q_stricmp(current_entity->classname, "noclass") != 0 &&
			Q_stricmp(current_entity->classname, "NPC_goal") != 0 &&
			current_entity->s.eType != ET_MISSILE &&
			!strstr(current_entity->classname, "fx_") != 0 &&
			!strstr(current_entity->classname, "func_") &&
			!strstr(current_entity->classname, "misc_") &&
			!strstr(current_entity->classname, "trigger_") &&
			!strstr(current_entity->classname, "target_") &&
			!strstr(current_entity->classname, "zyk_") != 0)
		{ // zyk: a valid entity for quest
			total_valid_entities++;
			valid_entities[j] = current_entity;
			j++;
		}
	}

	// zyk: choose a random entity from the valid ones
	chosen_entity_index = Q_irand(0, (total_valid_entities - 1));

	return valid_entities[chosen_entity_index];
}

qboolean zyk_there_is_player_or_npc_in_spot(float x, float y, float z)
{
	int iEntityList[MAX_GENTITIES];
	int numListedEntities = 0;
	vec3_t mins, maxs;
	float radius = 80;
	int i = 0;

	mins[0] = x - radius;
	mins[1] = y - radius;
	mins[2] = z - radius;

	maxs[0] = x + radius;
	maxs[1] = y + radius;
	maxs[2] = z + radius;

	numListedEntities = trap->EntitiesInBox(mins, maxs, iEntityList, MAX_GENTITIES);

	while (i < numListedEntities)
	{
		gentity_t* this_ent = &g_entities[iEntityList[i]];

		if (this_ent && this_ent->client && this_ent->health > 0)
		{
			return qtrue;
		}

		i++;
	}

	return qfalse;
}

extern void Jedi_Cloak(gentity_t* self);
extern int zyk_max_skill_level(int skill_index);
extern int zyk_max_magic_power(gentity_t* ent);
char* zyk_get_enemy_type(int enemy_type)
{
	char* enemy_names[NUM_QUEST_NPCS];

	enemy_names[QUEST_NPC_NONE] = "";

	enemy_names[QUEST_NPC_ANGEL_OF_DEATH] = "legendary_angel_of_death";
	enemy_names[QUEST_NPC_JORMUNGANDR] = "legendary_jormungandr";
	enemy_names[QUEST_NPC_CHIMERA] = "legendary_chimera";

	enemy_names[QUEST_NPC_MAGE_MASTER] = "mage_master";
	enemy_names[QUEST_NPC_MAGE_MINISTER] = "mage_minister";
	enemy_names[QUEST_NPC_MAGE_SCHOLAR] = "mage_scholar";
	enemy_names[QUEST_NPC_FORCE_MAGE] = "force_mage";
	enemy_names[QUEST_NPC_HIGH_TRAINED_WARRIOR] = "high_trained_warrior";
	enemy_names[QUEST_NPC_MID_TRAINED_WARRIOR] = "mid_trained_warrior";
	enemy_names[QUEST_NPC_CHANGELING_SENTRY] = "changeling_sentry";
	enemy_names[QUEST_NPC_FLYING_WARRIOR] = "flying_warrior";
	enemy_names[QUEST_NPC_CHANGELING_WORM] = "changeling_worm";
	enemy_names[QUEST_NPC_HEAVY_ARMORED_WARRIOR] = "heavy_armored_warrior";
	enemy_names[QUEST_NPC_FORCE_SABER_WARRIOR] = "force_saber_warrior";
	enemy_names[QUEST_NPC_CHANGELING_HOWLER] = "changeling_howler";
	enemy_names[QUEST_NPC_LOW_TRAINED_WARRIOR] = "low_trained_warrior";

	enemy_names[QUEST_NPC_ALLY_MAGE] = "ally_mage";
	enemy_names[QUEST_NPC_ALLY_ELEMENTAL_FORCE_MAGE] = "ally_elemental_force_mage";
	enemy_names[QUEST_NPC_ALLY_FLYING_WARRIOR] = "ally_flying_warrior";
	enemy_names[QUEST_NPC_ALLY_FORCE_WARRIOR] = "ally_force_warrior";
	enemy_names[QUEST_NPC_SELLER] = "quest_seller";

	if (enemy_type > QUEST_NPC_NONE && enemy_type < NUM_QUEST_NPCS)
	{
		return G_NewString(enemy_names[enemy_type]);
	}
	
	return "";
}

int zyk_bonus_increase_for_quest_npc(zyk_quest_npc_t enemy_type)
{
	int bonus_increase[NUM_QUEST_NPCS];

	bonus_increase[QUEST_NPC_NONE] = QUEST_NPC_BONUS_INCREASE;

	bonus_increase[QUEST_NPC_ANGEL_OF_DEATH] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_JORMUNGANDR] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_CHIMERA] = QUEST_NPC_BONUS_INCREASE;

	bonus_increase[QUEST_NPC_MAGE_MASTER] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_MAGE_MINISTER] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_MAGE_SCHOLAR] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_FORCE_MAGE] = QUEST_NPC_BONUS_INCREASE * 1.2;
	bonus_increase[QUEST_NPC_HIGH_TRAINED_WARRIOR] = QUEST_NPC_BONUS_INCREASE * 1.2;
	bonus_increase[QUEST_NPC_MID_TRAINED_WARRIOR] = QUEST_NPC_BONUS_INCREASE * 1.5;
	bonus_increase[QUEST_NPC_CHANGELING_SENTRY] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_FLYING_WARRIOR] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_CHANGELING_WORM] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_HEAVY_ARMORED_WARRIOR] = QUEST_NPC_BONUS_INCREASE * 1.8;
	bonus_increase[QUEST_NPC_FORCE_SABER_WARRIOR] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_CHANGELING_HOWLER] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_LOW_TRAINED_WARRIOR] = QUEST_NPC_BONUS_INCREASE * 1.5;

	bonus_increase[QUEST_NPC_ALLY_MAGE] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_ALLY_ELEMENTAL_FORCE_MAGE] = QUEST_NPC_BONUS_INCREASE;
	bonus_increase[QUEST_NPC_ALLY_FLYING_WARRIOR] = QUEST_NPC_BONUS_INCREASE * 2;
	bonus_increase[QUEST_NPC_ALLY_FORCE_WARRIOR] = QUEST_NPC_BONUS_INCREASE * 2;
	bonus_increase[QUEST_NPC_SELLER] = QUEST_NPC_BONUS_INCREASE;

	if (enemy_type > QUEST_NPC_NONE && enemy_type < NUM_QUEST_NPCS)
	{
		return bonus_increase[enemy_type];
	}

	return QUEST_NPC_BONUS_INCREASE;
}

int zyk_max_magic_level_for_quest_npc(zyk_quest_npc_t enemy_type)
{
	int max_levels[NUM_QUEST_NPCS];

	max_levels[QUEST_NPC_NONE] = 0;

	max_levels[QUEST_NPC_ANGEL_OF_DEATH] = 16;
	max_levels[QUEST_NPC_JORMUNGANDR] = 16;
	max_levels[QUEST_NPC_CHIMERA] = 16;

	max_levels[QUEST_NPC_MAGE_MASTER] = 14;
	max_levels[QUEST_NPC_MAGE_MINISTER] = 10;
	max_levels[QUEST_NPC_MAGE_SCHOLAR] = 10;
	max_levels[QUEST_NPC_FORCE_MAGE] = 8;
	max_levels[QUEST_NPC_HIGH_TRAINED_WARRIOR] = 8;
	max_levels[QUEST_NPC_MID_TRAINED_WARRIOR] = 7;
	max_levels[QUEST_NPC_CHANGELING_SENTRY] = 7;
	max_levels[QUEST_NPC_FLYING_WARRIOR] = 7;
	max_levels[QUEST_NPC_CHANGELING_WORM] = 7;
	max_levels[QUEST_NPC_HEAVY_ARMORED_WARRIOR] = 5;
	max_levels[QUEST_NPC_FORCE_SABER_WARRIOR] = 7;
	max_levels[QUEST_NPC_CHANGELING_HOWLER] = 7;
	max_levels[QUEST_NPC_LOW_TRAINED_WARRIOR] = 4;
	
	max_levels[QUEST_NPC_ALLY_MAGE] = 10;
	max_levels[QUEST_NPC_ALLY_ELEMENTAL_FORCE_MAGE] = 8;
	max_levels[QUEST_NPC_ALLY_FLYING_WARRIOR] = 7;
	max_levels[QUEST_NPC_ALLY_FORCE_WARRIOR] = 7;
	max_levels[QUEST_NPC_SELLER] = 4;

	if (enemy_type > QUEST_NPC_NONE && enemy_type < NUM_QUEST_NPCS)
	{
		return max_levels[enemy_type];
	}

	return 0;
}

void zyk_set_magic_level_for_quest_npc(gentity_t* npc_ent, zyk_quest_npc_t enemy_type, int skill_index, int value)
{
	int max_level = zyk_max_magic_level_for_quest_npc(enemy_type);

	npc_ent->client->pers.skill_levels[skill_index] = value;

	if (value > max_level)
	{
		npc_ent->client->pers.skill_levels[skill_index] = max_level;
	}
}

void zyk_set_quest_npc_stuff(gentity_t* npc_ent, zyk_quest_npc_t quest_npc_type, int bonuses, qboolean hard_mode, int player_id)
{
	if (npc_ent && npc_ent->client)
	{
		int ally_bonus = (bonuses / zyk_bonus_increase_for_quest_npc(quest_npc_type));

		// zyk: bonus skill level based on the chance to appear
		int npc_skill_level = (bonuses / zyk_bonus_increase_for_quest_npc(quest_npc_type));

		int hp_bonus = npc_ent->NPC->stats.health * (0.01 * bonuses);
		int skill_level_bonus = 0;

		npc_ent->client->pers.quest_npc = quest_npc_type;
		npc_ent->client->pers.quest_npc_event = 0;
		npc_ent->client->pers.quest_event_timer = 0;
		npc_ent->client->pers.quest_npc_idle_timer = level.time + QUEST_NPC_IDLE_TIME;
		npc_ent->client->pers.quest_npc_caller_player_id = player_id;

		if (hard_mode == qtrue)
		{
			hp_bonus *= 2;
			skill_level_bonus += 2;
		}

		// zyk: setting quest npc health
		npc_ent->NPC->stats.health += hp_bonus;
		npc_ent->client->ps.stats[STAT_MAX_HEALTH] = npc_ent->NPC->stats.health;
		npc_ent->health = npc_ent->client->ps.stats[STAT_MAX_HEALTH];
		npc_ent->client->pers.maxHealth = npc_ent->client->ps.stats[STAT_MAX_HEALTH];

		// zyk: setting magic abilities
		if (quest_npc_type == QUEST_NPC_ANGEL_OF_DEATH)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_AIR_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_DARK_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 60 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_JORMUNGANDR)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_WATER_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_EARTH_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 60 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_CHIMERA)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_FIRE_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_LIGHT_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 60 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_MAGE_MASTER)
		{
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] = 1;
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SABER_ARMOR] = 1;
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = 1;
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_DARK_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_LIGHT_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_AIR_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_FIRE_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_EARTH_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_WATER_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_MAGIC_DOME, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAGIC_FIST] = npc_skill_level - 1 + skill_level_bonus;

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 42 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_MAGE_MINISTER)
		{
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_LIGHT_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAGIC_FIST] = npc_skill_level - 2 + skill_level_bonus;

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 16 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_MAGE_SCHOLAR)
		{
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_DARK_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAGIC_FIST] = npc_skill_level - 2 + skill_level_bonus;

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 16 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_FORCE_MAGE)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_MAGIC_DOME, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_DARK_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_LIGHT_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 10 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_HIGH_TRAINED_WARRIOR)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_WATER_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_EARTH_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_FIRE_MAGIC, npc_skill_level + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_AIR_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + 6 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_MID_TRAINED_WARRIOR)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_MAGIC_DOME, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_CHANGELING_SENTRY)
		{
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_AIR_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_FLYING_WARRIOR)
		{
			Jedi_Cloak(npc_ent);

			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_CHANGELING_WORM)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_EARTH_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_HEAVY_ARMORED_WARRIOR)
		{
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] = 1;
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SABER_ARMOR] = 1;
			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_FORCE_SABER_WARRIOR)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_WATER_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_CHANGELING_HOWLER)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_FIRE_MAGIC, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_LOW_TRAINED_WARRIOR)
		{
			if (bonuses < (QUEST_NPC_BONUS_INCREASE / 2))
			{
				npc_ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BLASTER);

				npc_ent->s.weapon = WP_MELEE;
				npc_ent->client->ps.weapon = WP_MELEE;
			}

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, npc_skill_level + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = npc_skill_level + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_ALLY_MAGE)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_DARK_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_LIGHT_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_AIR_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_FIRE_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_EARTH_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_WATER_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_MAGIC_DOME, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, ally_bonus + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAGIC_FIST] = ally_bonus - 1 + skill_level_bonus;

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = ally_bonus + 25 + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_ALLY_ELEMENTAL_FORCE_MAGE)
		{
			int random_magic_power = Q_irand(SKILL_MAGIC_WATER_MAGIC, SKILL_MAGIC_LIGHT_MAGIC);

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, random_magic_power, ally_bonus + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = ally_bonus + 10 + skill_level_bonus;
			}
		else if (quest_npc_type == QUEST_NPC_ALLY_FLYING_WARRIOR)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_AIR_MAGIC, ally_bonus + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = ally_bonus + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_ALLY_FORCE_WARRIOR)
		{
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_WATER_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_EARTH_MAGIC, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_FIRE_MAGIC, ally_bonus + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = ally_bonus + skill_level_bonus;
		}
		else if (quest_npc_type == QUEST_NPC_SELLER)
		{
			npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

			npc_ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] = 1;

			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_HEALING_AREA, ally_bonus + skill_level_bonus);
			zyk_set_magic_level_for_quest_npc(npc_ent, quest_npc_type, SKILL_MAGIC_MAGIC_DOME, ally_bonus + skill_level_bonus);

			npc_ent->client->pers.skill_levels[SKILL_MAX_MP] = ally_bonus + skill_level_bonus;

			// zyk: seller stays in the map only for this amount of time before going away
			npc_ent->client->pers.quest_seller_map_timer = level.time + (SIDE_QUEST_STUFF_TIMER * bonuses);
		}

		// zyk: setting the initial amount of magic points here because it is based on the Max MP skill
		npc_ent->client->pers.magic_power = zyk_max_magic_power(npc_ent);
	}
}

// zyk: spawns a quest npc and sets additional stuff, like levels, etc
void zyk_spawn_quest_npc(zyk_quest_npc_t quest_npc_type, int yaw, int bonuses, qboolean hard_mode, int player_id)
{
	gentity_t* npc_ent = NULL;
	qboolean spawn_quest_npc = qtrue;
	int i = 0;

	float x = 0, y = 0, z = 0;
	int npc_offset = 8;
	gentity_t* chosen_entity = NULL;

	chosen_entity = zyk_find_entity_for_quest();

	if (chosen_entity == NULL)
	{ // zyk: if for some reason there was no chosen entity, try again later
		return;
	}

	x = Q_irand(0, npc_offset);
	y = Q_irand(0, npc_offset);
	z = Q_irand(0, npc_offset);

	if (Q_irand(0, 1) == 0)
	{
		x *= -1;
	}

	if (Q_irand(0, 1) == 0)
	{
		y *= -1;
	}

	if (chosen_entity->r.svFlags & SVF_USE_CURRENT_ORIGIN)
	{
		x += chosen_entity->r.currentOrigin[0];
		y += chosen_entity->r.currentOrigin[1];
		z += chosen_entity->r.currentOrigin[2];
	}
	else
	{
		x += chosen_entity->s.origin[0];
		y += chosen_entity->s.origin[1];
		z += chosen_entity->s.origin[2];
	}

	// zyk: avoiding telefrag other npcs
	if (zyk_there_is_player_or_npc_in_spot(x, y, z) == qtrue)
	{
		return;
	}
	
	if (level.special_quest_npc_in_map & (1 << quest_npc_type))
	{ // zyk: only one special npc can be in the map
		spawn_quest_npc = qfalse;
	}

	if (spawn_quest_npc == qtrue)
	{
		npc_ent = Zyk_NPC_SpawnType(zyk_get_enemy_type(quest_npc_type), x, y, z, yaw);

		zyk_set_quest_npc_stuff(npc_ent, quest_npc_type, bonuses, hard_mode, player_id);
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

	level.legendary_artifact_step = QUEST_SECRET_INIT_STEP;
	level.legendary_artifact_debounce_timer = 0;
	level.legendary_artifact_timer = 0;

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

	// zyk: initializing bounty_quest_target_id value
	level.bounty_quest_target_id = 0;
	level.bounty_quest_choose_target = qtrue;

	level.map_music_reset_timer = 0;

	level.voting_player = -1;

	level.server_empty_change_map_timer = 0;
	level.num_fully_connected_clients = 0;

	level.energy_modulator_timer = 0;
	level.magic_armor_timer = 0;

	level.special_quest_npc_in_map = 0;
	level.reality_shift_mode = REALITY_SHIFT_NONE;
	level.reality_shift_timer = 0;

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

		for (zyk_iterator = 0; zyk_iterator < LEGENDARY_CRYSTALS_CHOSEN; zyk_iterator++)
		{
			level.legendary_crystal_chosen[zyk_iterator] = 0;
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

	level.current_map_music = MAPMUSIC_NONE;

	if (Q_stricmp(level.default_map_music, "") == 0)
	{ // zyk: if the default map music is empty (the map has no music) then set a default music
		strcpy(level.default_map_music, "music/yavin2/yavtemp2_explore.mp3");
	}

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

		zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)target_ent->r.currentOrigin[0], (int)target_ent->r.currentOrigin[1], (int)target_ent->r.currentOrigin[2]));

		new_ent->s.modelindex = G_EffectIndex(effect_path);

		zyk_spawn_entity(new_ent);

		if (new_ent && new_ent->inuse)
		{ // zyk: do not set these if for some reason the entity was not spawned
			if (damage > 0)
				new_ent->splashDamage = damage;

			if (radius > 0)
				new_ent->splashRadius = radius;

			if (start_time > 0)
				new_ent->nextthink = level.time + start_time;

			level.special_power_effects[new_ent->s.number] = ent->s.number;
			level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
		}
	}
	else
	{ // zyk: model power
		zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
		zyk_set_entity_field(new_ent, "spawnflags", spawnflags);

		zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)target_ent->r.currentOrigin[0], (int)target_ent->r.currentOrigin[1], (int)target_ent->r.currentOrigin[2]));

		zyk_set_entity_field(new_ent, "model", effect_path);

		zyk_set_entity_field(new_ent, "targetname", targetname);

		zyk_spawn_entity(new_ent);

		if (new_ent && new_ent->inuse)
		{ // zyk: do not set these if for some reason the entity was not spawned
			level.special_power_effects[new_ent->s.number] = ent->s.number;
			level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
		}
	}
}

// zyk: tests if the target player can be hit by the attacker gun/saber damage, force power or special power
qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target)
{
	if (attacker && attacker->client && target && target->client && !attacker->NPC && !target->NPC)
	{
		if (attacker->client->pers.connected == CON_CONNECTED && attacker->client->sess.sessionTeam == TEAM_SPECTATOR)
		{ // zyk: cannot hit in spectator mode
			return qfalse;
		}

		if (target->client->pers.connected == CON_CONNECTED && target->client->sess.sessionTeam == TEAM_SPECTATOR)
		{ // zyk: cannot hit in spectator mode
			return qfalse;
		}

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

		if (attacker->client->pers.player_statuses & (1 << PLAYER_STATUS_NO_FIGHT) && attacker != target)
		{ // zyk: used nofight command, cannot hit anyone
			return qfalse;
		}

		if (target->client->pers.player_statuses & (1 << PLAYER_STATUS_NO_FIGHT) && attacker != target)
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

extern int zyk_get_magic_index(int skill_index);
extern void zyk_remap_shaders(const char* oldShader, const char* newShader);

zyk_magic_element_t zyk_get_magic_element(int magic_number)
{
	int i = 0;
	zyk_magic_element_t magic_power_elements[MAX_MAGIC_POWERS] = {
		MAGICELEMENT_NONE,
		MAGICELEMENT_NONE,
		MAGICELEMENT_WATER,
		MAGICELEMENT_EARTH,
		MAGICELEMENT_FIRE,
		MAGICELEMENT_AIR,
		MAGICELEMENT_DARK,
		MAGICELEMENT_LIGHT
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

	if (new_ent && new_ent->inuse)
	{ // zyk: do not set these if for some reason the entity was not spawned
		level.special_power_effects[new_ent->s.number] = ent->s.number;
		level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
	}
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

qboolean zyk_has_resources_for_energy_modulator(gentity_t* ent)
{
	if (ent->client->pers.magic_power < 1 &&
		ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_BLUE_CRYSTAL] < 1 &&
		ent->client->ps.ammo[AMMO_POWERCELL] < 1)
	{
		return qfalse;
	}

	return qtrue;
}

// zyk: spawns the entity when turning it on or free it when turning it off
void zyk_energy_modulator(gentity_t* ent)
{
	if (zyk_has_resources_for_energy_modulator(ent) == qfalse)
	{
		ent->client->pers.energy_modulator_mode = 0;
		return;
	}

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

void zyk_spawn_black_hole_model(gentity_t* ent, int duration, int model_scale)
{
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "65536");
	zyk_set_entity_field(new_ent, "origin", va("%f %f %f", ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2] - 15));

	zyk_set_entity_field(new_ent, "model", "models/map_objects/mp/sphere_1.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_magic_black_hole");

	zyk_set_entity_field(new_ent, "zykmodelscale", va("%d", model_scale));

	zyk_spawn_entity(new_ent);

	if (new_ent && new_ent->inuse)
	{ // zyk: do not set these if for some reason the entity was not spawned
		level.special_power_effects[new_ent->s.number] = ent->s.number;
		level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
	}

	zyk_remap_shaders("models/map_objects/mp/spheretwo", "textures/mp/black");
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
			qboolean is_ally = qfalse;

			if (traceEnt->s.number < level.maxclients && !self->NPC &&
				zyk_is_ally(self, traceEnt) == qtrue)
			{ // zyk: allies will not be hit
				is_ally = qtrue;
			}

			if (OnSameTeam(self, traceEnt) == qtrue || npcs_on_same_team(self, traceEnt) == qtrue)
			{ // zyk: if one of them is npc, also check for allies
				is_ally = qtrue;
			}

			if (is_ally == qfalse)
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
					traceEnt->client->pers.player_statuses |= (1 << PLAYER_STATUS_IN_FLAMES);
				}
			}
		}
		e++;
	}
}

extern void save_account(gentity_t* ent, qboolean save_char_file);

void zyk_spawn_puzzle_effect(gentity_t *crystal_model)
{
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "fx_runner");
	zyk_set_entity_field(new_ent, "targetname", "zyk_puzzle_effect");
	zyk_set_entity_field(new_ent, "origin", va("%f %f %f", crystal_model->r.currentOrigin[0], crystal_model->r.currentOrigin[1], crystal_model->r.currentOrigin[2]));

	new_ent->s.modelindex = G_EffectIndex("env/btend");

	zyk_spawn_entity(new_ent);

	if (new_ent && new_ent->inuse)
	{ // zyk: do not set these if for some reason the entity was not spawned
		level.special_power_effects[new_ent->s.number] = 0;
		level.special_power_effects_timer[new_ent->s.number] = level.time + 1500;
	}
}

// zyk: used in puzzles to get Legendary Artifacts, like the Energy Modulator
void zyk_spawn_legendary_artifact_puzzle_model(float x, float y, float z, int model_scale, char *model_path, int crystal_number)
{
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");

	// zyk: only the usable crystals will be solid, to avoid a bug in which player cannot use the correct crystals
	if (crystal_number > 0)
		zyk_set_entity_field(new_ent, "spawnflags", "1");
	else
		zyk_set_entity_field(new_ent, "spawnflags", "0");

	zyk_set_entity_field(new_ent, "origin", va("%f %f %f", x, y, z));

	zyk_set_entity_field(new_ent, "model", G_NewString(model_path));

	zyk_set_entity_field(new_ent, "zykmodelscale", va("%d", model_scale));
	zyk_set_entity_field(new_ent, "targetname", "zyk_puzzle_model");

	if (crystal_number > 0)
	{
		zyk_set_entity_field(new_ent, "mins", "-16 -16 -16");
		zyk_set_entity_field(new_ent, "maxs", "16 16 16");
	}

	zyk_spawn_entity(new_ent);

	new_ent->count = crystal_number;
}

void zyk_spawn_legendary_artifact_model(float x, float y, float z, int model_scale)
{
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "1");
	zyk_set_entity_field(new_ent, "origin", va("%f %f %f", x, y, z));

	if (level.legendary_artifact_type == QUEST_ITEM_ENERGY_MODULATOR)
	{
		zyk_set_entity_field(new_ent, "model", "models/map_objects/danger/ship_item04.md3");
		zyk_set_entity_field(new_ent, "targetname", "zyk_energy_modulator_model");
	}
	else
	{
		zyk_set_entity_field(new_ent, "model", "models/map_objects/desert/3po_torso.md3");
		zyk_set_entity_field(new_ent, "targetname", "zyk_magic_armor_model");
	}

	zyk_set_entity_field(new_ent, "zykmodelscale", va("%d", model_scale));

	zyk_set_entity_field(new_ent, "mins", "-32 -32 -32");
	zyk_set_entity_field(new_ent, "maxs", "32 32 32");

	zyk_spawn_entity(new_ent);

	new_ent->count = 7;
}

void zyk_clear_quest_items(gentity_t* effect_ent)
{
	int i = 0;

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		gentity_t* crystal_ent = &g_entities[i];

		if (crystal_ent && Q_stricmp(crystal_ent->classname, "misc_model_breakable") == 0 && 
			Q_stricmp(crystal_ent->targetname, "zyk_quest_item") == 0 &&
			crystal_ent->count == effect_ent->s.number)
		{ // zyk: found one of the models of this crystal effect, clear it
			level.special_power_effects_timer[crystal_ent->s.number] = level.time + 100;
		}
	}
}

extern int zyk_total_skillpoints(gentity_t* ent);
int zyk_spawn_quest_item_effect(float x, float y, float z, int duration, char *quest_item_name, zyk_quest_item_t quest_item_type)
{
	gentity_t* new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "fx_runner");
	zyk_set_entity_field(new_ent, "targetname", G_NewString(quest_item_name));
	zyk_set_entity_field(new_ent, "origin", va("%f %f %f", x, y, z));

	new_ent->s.modelindex = G_EffectIndex("force/heal2");

	zyk_spawn_entity(new_ent);

	if (new_ent && new_ent->inuse)
	{ // zyk: do not set these if for some reason the entity was not spawned
		level.special_power_effects[new_ent->s.number] = 0;
		level.special_power_effects_timer[new_ent->s.number] = level.time + duration;

		return new_ent->s.number;
	}

	return -1;
}

void zyk_spawn_quest_item_model(float x, float y, float z, char* model_path, int model_scale, int duration, int quest_item_effect_id, zyk_quest_item_t quest_item_type)
{
	if (quest_item_effect_id > -1)
	{
		gentity_t* new_ent = G_Spawn();

		zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");

		// zyk: only the usable crystals will be solid, to avoid a bug in which player cannot use the correct crystals
		zyk_set_entity_field(new_ent, "spawnflags", "0");

		zyk_set_entity_field(new_ent, "origin", va("%f %f %f", x, y, z));

		zyk_set_entity_field(new_ent, "model", G_NewString(model_path));

		if (quest_item_type == QUEST_ITEM_MAGIC_ARMOR)
		{
			zyk_set_entity_field(new_ent, "angles", "90 0 0");
		}

		zyk_set_entity_field(new_ent, "zykmodelscale", va("%d", model_scale));

		zyk_set_entity_field(new_ent, "targetname", "zyk_quest_item");

		zyk_spawn_entity(new_ent);

		if (new_ent && new_ent->inuse)
		{ // zyk: do not set these if for some reason the entity was not spawned
			new_ent->count = quest_item_effect_id;

			level.special_power_effects[new_ent->s.number] = 0;
			level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
		}
	}
}

void zyk_spawn_crystal(float x, float y, float z, int duration, zyk_quest_item_t crystal_type)
{
	int crystal_effect_id = 0;

	if (crystal_type == QUEST_ITEM_SKILL_CRYSTAL)
	{
		crystal_effect_id = zyk_spawn_quest_item_effect(x, y, z, duration, "zyk_skill_crystal", crystal_type);
		zyk_spawn_quest_item_model(x, y, z, "models/map_objects/mp/crystal_blue.md3", 45, duration, crystal_effect_id, crystal_type);
	}
	else if (crystal_type == QUEST_ITEM_EXTRA_TRIES_CRYSTAL)
	{
		crystal_effect_id = zyk_spawn_quest_item_effect(x, y, z, duration, "zyk_extra_tries_crystal", crystal_type);
		zyk_spawn_quest_item_model(x, y, z, "models/map_objects/mp/crystal_green.md3", 45, duration, crystal_effect_id, crystal_type);
	}
	else if (crystal_type == QUEST_ITEM_SPECIAL_CRYSTAL)
	{
		crystal_effect_id = zyk_spawn_quest_item_effect(x, y, z, duration, "zyk_red_crystal", crystal_type);
		zyk_spawn_quest_item_model(x, y, z, "models/map_objects/mp/crystal_red.md3", 45, duration, crystal_effect_id, crystal_type);
	}
}

// zyk: spawn the model and effect used by magic crystals
void zyk_spawn_magic_crystal(int duration, zyk_quest_item_t crystal_type)
{
	float x, y, z;
	int distance_factor = 20;
	gentity_t* chosen_entity = NULL;
	
	chosen_entity = zyk_find_entity_for_quest();

	if (!chosen_entity)
	{ // zyk: if for some reason there was no chosen entity, try again later
		return;
	}

	// zyk: the distance the magic crystal is from the chosen entity origin will increase as the player gets more skillpoints
	x = Q_irand(0, distance_factor);
	y = Q_irand(0, distance_factor);
	z = Q_irand(0, distance_factor);

	if (Q_irand(0, 1) == 0)
	{
		x *= -1;
	}

	if (Q_irand(0, 1) == 0)
	{
		y *= -1;
	}

	if (Q_irand(0, 1) == 0)
	{
		z *= -1;
	}

	if (chosen_entity->r.svFlags & SVF_USE_CURRENT_ORIGIN)
	{
		x += chosen_entity->r.currentOrigin[0];
		y += chosen_entity->r.currentOrigin[1];
		z += chosen_entity->r.currentOrigin[2];
	}
	else
	{
		x += chosen_entity->s.origin[0];
		y += chosen_entity->s.origin[1];
		z += chosen_entity->s.origin[2];
	}

	zyk_spawn_crystal(x, y, z, duration, crystal_type);
}

// zyk: spawns any of the quest item types at specific coordinates in map
int zyk_spawn_quest_item(zyk_quest_item_t quest_item_type, int duration, int model_scale, float x, float y, float z)
{
	int quest_item_effect_id = -1;

	if (quest_item_type == QUEST_ITEM_ENERGY_MODULATOR)
	{
		quest_item_effect_id = zyk_spawn_quest_item_effect(x, y, z, duration, "zyk_energy_modulator_puzzle", quest_item_type);
		zyk_spawn_quest_item_model(x, y, z, "models/map_objects/danger/ship_item04.md3", model_scale, duration, quest_item_effect_id, quest_item_type);
	}
	else if (quest_item_type == QUEST_ITEM_MAGIC_ARMOR)
	{
		quest_item_effect_id = zyk_spawn_quest_item_effect(x, y, z, duration, "zyk_magic_armor_puzzle", quest_item_type);
		zyk_spawn_quest_item_model(x, y, z, "models/map_objects/desert/3po_torso.md3", model_scale, duration, quest_item_effect_id, quest_item_type);
	}
	else if (quest_item_type == QUEST_ITEM_SPIRIT_TREE)
	{
		quest_item_effect_id = zyk_spawn_quest_item_effect(x, y, z, duration, "zyk_spirit_tree", quest_item_type);

		z *= QUEST_SPIRIT_TREE_ORIGIN_Z_OFFSET;

		zyk_spawn_quest_item_model(x, y, z, "models/map_objects/yavin/tree10_b.md3", model_scale, duration, quest_item_effect_id, quest_item_type);
	}

	return quest_item_effect_id;
}

// zyk: clear effects of some special powers
void clear_special_power_effect(gentity_t* ent)
{
	qboolean clear_effect = qfalse;

	if (level.special_power_effects[ent->s.number] != -1 && level.special_power_effects_timer[ent->s.number] < level.time)
	{
		clear_effect = qtrue;
	}
	else if (level.special_power_effects[ent->s.number] != -1)
	{
		gentity_t* quest_power_user = &g_entities[level.special_power_effects[ent->s.number]];

		if (quest_power_user && quest_power_user->client && (quest_power_user->client->sess.amrpgmode == 2 || quest_power_user->NPC))
		{ // zyk: stops the magic power effect/model if player or npc is no longer using the magic power
			if (quest_power_user->client->pers.quest_power_status & (1 << MAGIC_HEALING_AREA) && 
				Q_stricmp(ent->targetname, "zyk_magic_healing_area") == 0)
			{
				level.special_power_effects_timer[ent->s.number] = level.time + 200;
			}
		}
	}

	if (clear_effect == qtrue)
	{
		level.special_power_effects[ent->s.number] = -1;

		// zyk: if it is a misc_model_breakable power, remove it right now
		if (Q_stricmp(ent->classname, "misc_model_breakable") == 0)
			G_FreeEntity(ent);
		else
			ent->think = G_FreeEntity;
	}
}

extern void zyk_add_health(gentity_t* ent, int heal_amount);
extern void zyk_set_stamina(gentity_t* ent, int amount, qboolean add);

// zyk: Healing Area
void healing_area(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_HEALING_AREA);
	ent->client->pers.magic_power_debounce_timer[MAGIC_HEALING_AREA] = 100;
	ent->client->pers.magic_power_hit_counter[MAGIC_HEALING_AREA] = 1;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/movers/objects/green_beam_start.mp3"));
}

// zyk: Magic Dome
void dome_of_damage(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_MAGIC_DOME);
	ent->client->pers.magic_power_debounce_timer[MAGIC_MAGIC_DOME] = 100;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/cairn_beam_start.mp3"));
}

// zyk: Water Magic
void water_magic(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_WATER_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_WATER_MAGIC] = level.time + 500;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/bodyfall_water1.mp3"));
}

// zyk: Earth Magic
void earth_magic(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_EARTH_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_EARTH_MAGIC] = level.time + 500;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/stone_break1.mp3"));
}

// zyk: Fire Magic
void fire_magic(gentity_t* ent) 
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_FIRE_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_FIRE_MAGIC] = level.time + 500;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/fireburst.mp3"));
}

// zyk: Air Magic
void air_magic(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_AIR_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_AIR_MAGIC] = level.time + 500;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/vacuum.mp3"));
}

// zyk: Dark Magic
void dark_magic(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_DARK_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_DARK_MAGIC] = level.time + 500;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam_off_1.mp3"));
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
	// missile->targetname = "zyk_magic_light";
	missile->s.weapon = WP_DEMP2;

	missile->think = zyk_lightning_dome_detonate;
	missile->nextthink = level.time;

	missile->splashDamage = missile->damage = damage;
	missile->splashMethodOfDeath = missile->methodOfDeath = MOD_UNKNOWN;

	missile->splashRadius = 768;

	missile->r.ownerNum = ent->s.number;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to ever bounce
	missile->bounceCount = 0;
}

// zyk: Light Magic
void light_magic(gentity_t* ent)
{
	ent->client->pers.quest_power_status |= (1 << MAGIC_LIGHT_MAGIC);
	ent->client->pers.magic_power_debounce_timer[MAGIC_LIGHT_MAGIC] = level.time + 500;
	ent->client->pers.magic_power_hit_counter[MAGIC_LIGHT_MAGIC] = 1;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/enlightenment.mp3"));
}

// zyk: calculates bonus damage factor based on element of the magic used by attacker and target magic levels of both the same element and opposite element
float zyk_get_elemental_bonus_factor(zyk_magic_t magic_power, gentity_t *attacker, gentity_t *target)
{
	float bonus_damage_factor = 1.0;

	zyk_magic_t opposite_elemental_magic[MAX_MAGIC_POWERS] = {
		-1,
		-1,
		MAGIC_FIRE_MAGIC,
		MAGIC_AIR_MAGIC,
		MAGIC_WATER_MAGIC,
		MAGIC_EARTH_MAGIC,
		MAGIC_LIGHT_MAGIC,
		MAGIC_DARK_MAGIC
	};

	if (magic_power >= 0 && magic_power < MAX_MAGIC_POWERS && opposite_elemental_magic[magic_power] != -1 && 
		attacker && target && attacker->client && target->client)
	{ /* zyk: calculates bonus damage based on the levels of the magic skill of the attacker and both the same elemental magic and the opposite elemental magic of the target, so
			    it will increase and decrease damage considering both opposite and same elements of target
	*/
		int attacker_skill_level = attacker->client->pers.skill_levels[SKILL_MAGIC_HEALING_AREA + magic_power];
		int target_skill_level = target->client->pers.skill_levels[SKILL_MAGIC_HEALING_AREA + magic_power];
		int target_opposite_element_skill_level = target->client->pers.skill_levels[SKILL_MAGIC_HEALING_AREA + opposite_elemental_magic[magic_power]];

		if (attacker_skill_level > 0)
			bonus_damage_factor = ((float)(attacker_skill_level - target_skill_level + target_opposite_element_skill_level) / (float)attacker_skill_level);
	}

	return bonus_damage_factor;
}

void zyk_stop_magic_power(gentity_t* ent, zyk_magic_t magic_number)
{
	ent->client->pers.quest_power_status &= ~(1 << magic_number);
}

void zyk_stop_all_magic_powers(gentity_t* ent)
{
	int i = 0;

	for (i = 0; i < MAX_MAGIC_POWERS; i++)
	{
		zyk_stop_magic_power(ent, i);
	}
}

// zyk: controls the quest powers stuff
extern void initialize_rpg_skills(gentity_t *ent, qboolean init_all);
void zyk_status_effects(gentity_t* ent)
{
	if (ent && ent->client && ent->health > 0)
	{
		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_POISONED))
		{
			if (ent->client->pers.poison_duration > level.time && ent->client->pers.poison_debounce_timer < level.time)
			{
				ent->client->pers.poison_debounce_timer = level.time + 250;

				zyk_quest_effect_spawn(ent, ent, "zyk_status_poison", "0", "noghri_stick/gas_cloud", 100, 0, 0, 1200);

				G_Damage(ent, ent, ent, NULL, NULL, 1, 0, MOD_UNKNOWN);

				zyk_set_stamina(ent, 50, qfalse);
			}
			else if (ent->client->pers.poison_duration <= level.time)
			{
				ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_POISONED);
			}
		}

		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_IN_FLAMES) && ent->client->pers.fire_bolt_hits_counter > 0 &&
			ent->client->pers.fire_bolt_timer < level.time)
		{
			gentity_t* fire_bolt_user = &g_entities[ent->client->pers.fire_bolt_user_id];

			zyk_quest_effect_spawn(fire_bolt_user, ent, "zyk_effect_fire_bolt_hit", "0", "env/fire", 0, 0, 0, 300);

			G_Damage(ent, fire_bolt_user, fire_bolt_user, NULL, NULL, 10, 0, MOD_UNKNOWN);

			ent->client->pers.fire_bolt_hits_counter--;
			ent->client->pers.fire_bolt_timer = level.time + 200;

			// zyk: no more do fire bolt damage if counter is 0
			if (ent->client->pers.fire_bolt_hits_counter <= 0)
			{
				ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_IN_FLAMES);
			}
		}

		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_BLEEDING))
		{
			if (ent->client->pers.bleeding_duration > level.time && ent->client->pers.bleeding_debounce_timer < level.time)
			{
				ent->client->pers.bleeding_debounce_timer = level.time + 100;

				G_Damage(ent, ent, ent, NULL, NULL, 1, 0, MOD_UNKNOWN);
			}
			else if (ent->client->pers.bleeding_duration <= level.time)
			{
				ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_BLEEDING);
			}
		}

		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_MAGIC_SHIELD))
		{
			if (ent->client->pers.magic_shield_duration > level.time)
			{
				ent->client->ps.eFlags |= EF_INVULNERABLE;
				ent->client->invulnerableTimer = ent->client->pers.magic_shield_duration;
			}
			else
			{
				ent->client->ps.eFlags &= ~EF_INVULNERABLE;
				ent->client->invulnerableTimer = 0;

				ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_MAGIC_SHIELD);
			}
		}
	}
}

void zyk_mp_usage(gentity_t* ent, int magic_skill_index)
{
	int magic_mp_usage = (ent->client->pers.skill_levels[magic_skill_index] / 2);

	if (magic_mp_usage < 1)
	{
		magic_mp_usage = 1;
	}

	if (ent->client->pers.magic_power >= magic_mp_usage)
	{
		ent->client->pers.magic_power -= magic_mp_usage;
	}
	else
	{
		ent->client->pers.magic_power = 0;
	}
}

void magic_power_events(gentity_t *ent)
{
	if (ent && ent->client)
	{
		if (ent->health > 0)
		{
			int i = 0;
			int magic_bonus = 0;

			// zyk: Magic Armor improves all magic powers
			if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] > 0)
			{
				magic_bonus = 1;
			}

			if (level.reality_shift_mode == REALITY_SHIFT_NO_MAGIC)
			{
				zyk_stop_all_magic_powers(ent);
			}

			if (ent->client->pers.magic_consumption_timer < level.time && ent->client->pers.magic_power <= 0)
			{ // zyk: run out of mp, stop all magic
				ent->client->pers.magic_power = 0;

				zyk_stop_all_magic_powers(ent);
			}

			// zyk: stop magic if skill level is not at least at level 1
			for (i = 0; i < MAX_MAGIC_POWERS; i++)
			{
				int skill_index = SKILL_MAGIC_HEALING_AREA + i;

				if (ent->client->pers.skill_levels[skill_index] < 1)
				{
					zyk_stop_magic_power(ent, i);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_HEALING_AREA))
			{
				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_HEALING_AREA);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_HEALING_AREA] < level.time)
				{
					int damage = 1 + magic_bonus;

					// zyk: effect on player position while magic is active
					zyk_spawn_magic_element_effect(ent, ent->r.currentOrigin, MAGIC_HEALING_AREA, 500);

					if (ent->client->pers.magic_power_hit_counter[MAGIC_HEALING_AREA] > 0)
					{
						zyk_quest_effect_spawn(ent, ent, "zyk_magic_healing_area", "4", "env/red_cyc", 0, damage, 228, 400);

						ent->client->pers.magic_power_hit_counter[MAGIC_HEALING_AREA]--;
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_HEALING_AREA] = level.time + 300;
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_MAGIC_DOME))
			{
				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_MAGIC_DOME);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_MAGIC_DOME] < level.time)
				{
					int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_MAGIC_DOME];

					zyk_quest_effect_spawn(ent, ent, "zyk_magic_dome", "4", "env/dome", 0, damage, 290, 400);

					ent->client->pers.magic_power_debounce_timer[MAGIC_MAGIC_DOME] = level.time + 300;
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_WATER_MAGIC))
			{
				int max_distance = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC] + magic_bonus));
				int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC];

				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_WATER_MAGIC);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_WATER_MAGIC] < level.time)
				{
					int heal_amount = ent->client->pers.skill_levels[SKILL_MAGIC_WATER_MAGIC];

					zyk_quest_effect_spawn(ent, ent, "zyk_magic_water", "4", "env/water_impact", 0, damage, max_distance, 400);

					if (ent->health > 0)
					{
						zyk_add_health(ent, heal_amount);
					}

					ent->client->pers.magic_power_debounce_timer[MAGIC_WATER_MAGIC] = level.time + 300;
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_EARTH_MAGIC))
			{
				int max_distance = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC] + magic_bonus));
				int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_EARTH_MAGIC];

				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_EARTH_MAGIC);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_EARTH_MAGIC] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_earth", "4", "env/rock_smash", 0, damage, max_distance, 400);

					ent->client->pers.magic_power_debounce_timer[MAGIC_EARTH_MAGIC] = level.time + 300;
				}
			}

			if (ent->client->pers.hit_by_magic & (1 << MAGIC_HIT_BY_EARTH))
			{
				if (ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_EARTH] < level.time)
				{
					ent->client->pers.hit_by_magic &= ~(1 << MAGIC_HIT_BY_EARTH);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_FIRE_MAGIC))
			{
				int max_distance = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC] + magic_bonus));
				int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC];

				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_FIRE_MAGIC);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_FIRE_MAGIC] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_fire", "4", "env/fire", 0, damage, max_distance, 400);

					G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/fireburst.mp3"));

					ent->client->pers.magic_power_debounce_timer[MAGIC_FIRE_MAGIC] = level.time + 300;
				}
			}

			if (ent->client->pers.hit_by_magic & (1 << MAGIC_HIT_BY_FIRE))
			{
				if (ent->client->pers.magic_power_hit_counter[MAGIC_HIT_BY_FIRE] > 0 && ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_FIRE] < level.time)
				{
					gentity_t* fire_magic_user = &g_entities[ent->client->pers.magic_power_user_id[MAGIC_HIT_BY_FIRE]];

					if (fire_magic_user && fire_magic_user->client)
					{
						int fire_damage = MAGIC_MIN_DMG + (fire_magic_user->client->pers.skill_levels[SKILL_MAGIC_FIRE_MAGIC] / 2);

						zyk_quest_effect_spawn(fire_magic_user, ent, "zyk_magic_fire", "0", "env/fire", 0, 0, 0, 300);

						fire_damage = (int)ceil(fire_damage * zyk_get_elemental_bonus_factor(MAGIC_FIRE_MAGIC, fire_magic_user, ent));

						// zyk: must do at least 1 damage
						if (fire_damage < 1)
							fire_damage = 1;

						G_Damage(ent, fire_magic_user, fire_magic_user, NULL, NULL, fire_damage, 0, MOD_UNKNOWN);

						G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/fire_lp.wav"));
					}

					ent->client->pers.magic_power_hit_counter[MAGIC_HIT_BY_FIRE]--;

					ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_FIRE] = level.time + 200;
				}
				else if (ent->client->pers.magic_power_hit_counter[MAGIC_HIT_BY_FIRE] == 0 && ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_FIRE] < level.time)
				{
					ent->client->pers.hit_by_magic &= ~(1 << MAGIC_HIT_BY_FIRE);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_AIR_MAGIC))
			{
				int max_distance = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[SKILL_MAGIC_AIR_MAGIC] + magic_bonus));
				int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_AIR_MAGIC];

				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_AIR_MAGIC);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_AIR_MAGIC] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_air", "4", "env/water_steam3", 0, damage, max_distance, 400);

					ent->client->pers.magic_power_debounce_timer[MAGIC_AIR_MAGIC] = level.time + 300;
				}
			}

			if (ent->client->pers.hit_by_magic & (1 << MAGIC_HIT_BY_AIR))
			{
				if (ent->client->pers.magic_power_target_timer[MAGIC_HIT_BY_AIR] < level.time)
				{
					ent->client->pers.hit_by_magic &= ~(1 << MAGIC_HIT_BY_AIR);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_DARK_MAGIC))
			{
				int duration = 400;
				int radius = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC] + magic_bonus));
				int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_DARK_MAGIC];

				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_DARK_MAGIC);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_DARK_MAGIC] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_dark", "4", "force/rage2", 0, damage, radius, duration);

					zyk_spawn_black_hole_model(ent, duration, (4 * damage));

					ent->client->pers.magic_power_debounce_timer[MAGIC_DARK_MAGIC] = level.time + 300;
				}
			}

			if (ent->client->pers.quest_power_status & (1 << MAGIC_LIGHT_MAGIC))
			{
				int radius = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC] + magic_bonus));
				int damage = MAGIC_MIN_DMG + magic_bonus + ent->client->pers.skill_levels[SKILL_MAGIC_LIGHT_MAGIC];

				if (ent->client->pers.magic_consumption_timer < level.time)
				{
					zyk_mp_usage(ent, SKILL_MAGIC_LIGHT_MAGIC);
				}

				if (ent->client->pers.magic_power_debounce_timer[MAGIC_LIGHT_MAGIC] < level.time)
				{
					zyk_quest_effect_spawn(ent, ent, "zyk_magic_light", "4", "howler/sonic", 0, damage, radius, 400);

					ent->client->pers.magic_power_debounce_timer[MAGIC_LIGHT_MAGIC] = level.time + 300;
				}
			}

			if (ent->client->pers.magic_consumption_timer < level.time)
			{
				ent->client->pers.magic_consumption_timer = level.time + 250;
			}
		}
	}
}

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

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))
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
	ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS);

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
		if (!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
		{ // zyk: show health if the player did not die in duel
			strcpy(health_info, va(" ^1%d^7/^2%d^7 ", ent->health, ent->client->ps.stats[STAT_ARMOR]));
		}

		if (ally && !(ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
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
	if (level.duel_tournament_mode == 4 && !(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
	{ // zyk: add hp score if he did not die in duel
		level.duel_players_hp[ent->s.number] += (ent->health + ent->client->ps.stats[STAT_ARMOR]);
	}

	if (ally)
	{ // zyk: both players must have the same score and the same hp score
		level.duel_players[ally->s.number] = level.duel_players[ent->s.number];

		if (level.duel_tournament_mode == 4 && !(ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
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

int zyk_get_item_weight(zyk_inventory_t item_index)
{
	int rpg_inventory_weights[MAX_RPG_INVENTORY_ITEMS];

	rpg_inventory_weights[RPG_INVENTORY_WP_STUN_BATON] = 5;
	rpg_inventory_weights[RPG_INVENTORY_WP_SABER] = 10;
	rpg_inventory_weights[RPG_INVENTORY_WP_BLASTER_PISTOL] = 7;
	rpg_inventory_weights[RPG_INVENTORY_WP_E11_BLASTER_RIFLE] = 10;
	rpg_inventory_weights[RPG_INVENTORY_WP_DISRUPTOR] = 12;
	rpg_inventory_weights[RPG_INVENTORY_WP_BOWCASTER] = 12;
	rpg_inventory_weights[RPG_INVENTORY_WP_REPEATER] = 14;
	rpg_inventory_weights[RPG_INVENTORY_WP_DEMP2] = 12;
	rpg_inventory_weights[RPG_INVENTORY_WP_FLECHETTE] = 15;
	rpg_inventory_weights[RPG_INVENTORY_WP_ROCKET_LAUNCHER] = 17;
	rpg_inventory_weights[RPG_INVENTORY_WP_CONCUSSION] = 18;
	rpg_inventory_weights[RPG_INVENTORY_WP_BRYAR_PISTOL] = 7;

	rpg_inventory_weights[RPG_INVENTORY_AMMO_BLASTER_PACK] = 1;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_POWERCELL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_METAL_BOLTS] = 1;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_ROCKETS] = 2;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_THERMALS] = 3;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_TRIPMINES] = 3;
	rpg_inventory_weights[RPG_INVENTORY_AMMO_DETPACKS] = 4;

	rpg_inventory_weights[RPG_INVENTORY_ITEM_BINOCULARS] = 3;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_BACTA_CANISTER] = 5;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_SENTRY_GUN] = 6;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_SEEKER_DRONE] = 5;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_EWEB] = 22;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_BIG_BACTA] = 10;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_FORCE_FIELD] = 14;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_CLOAK] = 15;
	rpg_inventory_weights[RPG_INVENTORY_ITEM_JETPACK] = 100;

	rpg_inventory_weights[RPG_INVENTORY_MISC_JETPACK_FUEL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_MISC_FLAME_THROWER_FUEL] = 1;

	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BACTA] = 5;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_FORCE_FIELD] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_CLOAK] = 8;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR] = 150;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = 225;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] = 225;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SABER_ARMOR] = 225;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_FLAME_THROWER] = 50;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_STUN_BATON] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DISRUPTOR] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_BOWCASTER] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DEMP2] = 22;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_REPEATER] = 23;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_FLECHETTE] = 23;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_CONCUSSION] = 28;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER] = 22;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_DETPACKS] = 20;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_JETPACK] = 50;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_THERMAL_VISION] = 15;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SENTRY_GUN] = 25;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_SEEKER_DRONE] = 10;
	rpg_inventory_weights[RPG_INVENTORY_UPGRADE_EWEB] = 30;
	rpg_inventory_weights[RPG_INVENTORY_MISC_BLUE_CRYSTAL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_MISC_GREEN_CRYSTAL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_MISC_RED_CRYSTAL] = 1;
	rpg_inventory_weights[RPG_INVENTORY_LEGENDARY_ENERGY_MODULATOR] = 50;
	rpg_inventory_weights[RPG_INVENTORY_LEGENDARY_QUEST_LOG] = 20;
	rpg_inventory_weights[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] = 225;
	rpg_inventory_weights[RPG_INVENTORY_MISC_MAGIC_SHIELD] = 10;
	rpg_inventory_weights[RPG_INVENTORY_MISC_SHIELD_BOOSTER] = 5;
	rpg_inventory_weights[RPG_INVENTORY_MISC_YSALAMIRI] = 10;
	rpg_inventory_weights[RPG_INVENTORY_MISC_FORCE_BOON] = 10;

	if (item_index >= 0 && item_index < MAX_RPG_INVENTORY_ITEMS)
	{
		return rpg_inventory_weights[item_index];
	}

	return 0;
}

// zyk: calculates current weight of stuff the RPG player is carrying
void zyk_calculate_current_weight(gentity_t* ent)
{
	int i = 0;
	int current_weight = 0;

	for (i = 0; i < MAX_RPG_INVENTORY_ITEMS; i++)
	{
		if (ent->client->pers.rpg_inventory[i] > 0)
		{
			current_weight += (ent->client->pers.rpg_inventory[i] * zyk_get_item_weight(i));
		}
	}

	ent->client->pers.current_weight = current_weight;
}

void zyk_spawn_magic_spirits(gentity_t* ent, int duration)
{
	vec3_t effect_origin;

	VectorSet(effect_origin, ent->r.currentOrigin[0] + 100, ent->r.currentOrigin[1] + 80, ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_MAGIC_DOME, duration);

	VectorSet(effect_origin, ent->r.currentOrigin[0], ent->r.currentOrigin[1] + 100, ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_WATER_MAGIC, duration);

	VectorSet(effect_origin, ent->r.currentOrigin[0] - 100, ent->r.currentOrigin[1] + 80, ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_EARTH_MAGIC, duration);

	VectorSet(effect_origin, ent->r.currentOrigin[0] - 90, ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_FIRE_MAGIC, duration);

	VectorSet(effect_origin, ent->r.currentOrigin[0] + 90, ent->r.currentOrigin[1], ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_AIR_MAGIC, duration);

	VectorSet(effect_origin, ent->r.currentOrigin[0] - 50, ent->r.currentOrigin[1] - 70, ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_DARK_MAGIC, duration);

	VectorSet(effect_origin, ent->r.currentOrigin[0] + 50, ent->r.currentOrigin[1] - 70, ent->r.currentOrigin[2]);
	zyk_spawn_magic_element_effect(ent, effect_origin, MAGIC_LIGHT_MAGIC, duration);
}

void zyk_show_tutorial(gentity_t* ent)
{
	if (ent->client->pers.tutorial_step >= TUTORIAL_NEW_CHAR_START && ent->client->pers.tutorial_step <= TUTORIAL_NEW_CHAR_END)
	{
		if (ent->client->pers.tutorial_step == TUTORIAL_NEW_CHAR_START)
		{
			zyk_spawn_magic_spirits(ent, TUTORIAL_DURATION);
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_NEW_CHAR_START + 1))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Hello %s^7. We are the Magical Spirits. We will explain everything you need to know.\n\"", QUESTCHAR_ALL_SPIRITS, ent->client->pers.netname));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_NEW_CHAR_START + 2))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Use ^3/list ^7to see all info. From there you can find all other commands.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_NEW_CHAR_START + 3))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: We are here because we need your help! Use ^3/list quests^7 too see all info you need to help us.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_NEW_CHAR_START + 4))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Now go %s^7! Use ^3/tutorial ^7if you need all this information again.\n\"", QUESTCHAR_ALL_SPIRITS, ent->client->pers.netname));
		}

		ent->client->pers.tutorial_step++;
	}
	else if (ent->client->pers.tutorial_step >= TUTORIAL_STAMINA_START && ent->client->pers.tutorial_step <= TUTORIAL_STAMINA_END)
	{
		if (ent->client->pers.tutorial_step == TUTORIAL_STAMINA_START)
		{
			zyk_spawn_magic_spirits(ent, 10000);

			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Doing actions decrease stamina. It is the blue bar at the right of your screen.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_STAMINA_START + 1))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Use ^2Meditate ^7taunt to regen it. If Stamina runs out, you will faint for some seconds.\n\"", QUESTCHAR_ALL_SPIRITS));
		
			ent->client->pers.quest_missions |= (1 << MAIN_QUEST_START);
		}

		ent->client->pers.tutorial_step++;
	}
	else if (ent->client->pers.tutorial_step >= TUTORIAL_INVENTORY_START && ent->client->pers.tutorial_step <= TUTORIAL_INVENTORY_END)
	{
		if (ent->client->pers.tutorial_step == TUTORIAL_INVENTORY_START)
		{
			zyk_spawn_magic_spirits(ent, 20000);

			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Use ^3/list inventory^7. Everything you pickup up is stored there. Each inventory page shows 10 items.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_INVENTORY_START + 1))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Your inventory has weight. If it goes over the max weight, your run speed will decrease and Stamina will decrease faster.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_INVENTORY_START + 2))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: When seeing your inventory, you can also see the commands to buy or sell items.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_INVENTORY_START + 3))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can either ^3/drop ^7weapons or get melee to drop items. Use ^3/jetpack ^7to drop your jetpack.\n\"", QUESTCHAR_ALL_SPIRITS));
		
			ent->client->pers.quest_missions |= (1 << MAIN_QUEST_START);
		}

		ent->client->pers.tutorial_step++;
	}
	else if (ent->client->pers.tutorial_step >= TUTORIAL_QUEST_ITEMS_START && ent->client->pers.tutorial_step <= TUTORIAL_QUEST_ITEMS_END)
	{
		if (ent->client->pers.tutorial_step == TUTORIAL_QUEST_ITEMS_START)
		{
			zyk_spawn_magic_spirits(ent, 20000);

			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You need Blue Crystals to upgrade skills. We will randomly place some of them in the map. Enemies defeated have a chance to drop them.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_QUEST_ITEMS_START + 1))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Use ^3/list force^7 or ^3/list misc ^7or ^3/list magic ^7to see skills. It also show commands to upgrade them.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_QUEST_ITEMS_START + 2))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Green crystals give extra tries for quest and Red crystals creates a Lightning Dome by pressing Use key.\n\"", QUESTCHAR_ALL_SPIRITS));
		
			ent->client->pers.quest_missions |= (1 << MAIN_QUEST_START);
		}

		ent->client->pers.tutorial_step++;
	}
	else if (ent->client->pers.tutorial_step >= TUTORIAL_MAGIC_START && ent->client->pers.tutorial_step <= TUTORIAL_MAGIC_END)
	{
		if (ent->client->pers.tutorial_step == TUTORIAL_MAGIC_START)
		{
			zyk_spawn_magic_spirits(ent, 10000);

			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: That was a Magic Power! You can learn them too.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_MAGIC_START + 1))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: To cast magic, upgrade the magic skill in ^3/list magic^7, then bind to a key like this: ^3/bind <key> magic <skill number>^7.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_MAGIC_START + 2))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You need MP (Magic Points) to cast magic. Upgrade the Max Magic Points skill to be able to have MP.\n\"", QUESTCHAR_ALL_SPIRITS));
		}
		else if (ent->client->pers.tutorial_step == (TUTORIAL_MAGIC_START + 3))
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You can regen MP by collecting crystals and using Bacta and Big Bacta holdable items.\n\"", QUESTCHAR_ALL_SPIRITS));
		
			ent->client->pers.quest_missions |= (1 << MAIN_QUEST_START);
		}

		ent->client->pers.tutorial_step++;
	}
}

void zyk_set_quest_event_timer(gentity_t* ent)
{
	int player_power_level = ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_BLUE_CRYSTAL] + zyk_total_skillpoints(ent);
	int interval_time = QUEST_NPC_SPAWN_TIME - (player_power_level * 100);

	if (ent->client->pers.player_settings & (1 << SETTINGS_DIFFICULTY))
	{
		interval_time -= (interval_time / 5);
	}

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_CREATED_ACCOUNT))
	{ //zyk: player is in tutorial for the first time. Do not spawn quest npcs yet
		interval_time = TUTORIAL_DURATION;

		ent->client->pers.quest_progress_timer = level.time + TUTORIAL_DURATION;

		ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_CREATED_ACCOUNT);
	}
	
	ent->client->pers.quest_event_timer = level.time + interval_time;
}

extern int zyk_number_of_allies_in_map(gentity_t* ent);
extern int zyk_number_of_enemies_in_map();

void zyk_update_inventory(gentity_t* ent)
{
	// zyk: loading initial inventory
	ent->client->ps.stats[STAT_WEAPONS] = 0;
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_NONE);
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;

	// zyk: weapons inventory
	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_STUN_BATON] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_SABER] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_BLASTER_PISTOL] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_E11_BLASTER_RIFLE] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_DISRUPTOR] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_BOWCASTER] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_REPEATER] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_DEMP2] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_FLECHETTE] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_ROCKET_LAUNCHER] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_CONCUSSION] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_BRYAR_PISTOL] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);

	// zyk: ammo inventory
	ent->client->ps.ammo[AMMO_BLASTER] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_BLASTER_PACK];
	ent->client->ps.ammo[AMMO_POWERCELL] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_POWERCELL];
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_METAL_BOLTS];
	ent->client->ps.ammo[AMMO_ROCKETS] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_ROCKETS];
	ent->client->ps.ammo[AMMO_THERMAL] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_THERMALS];
	ent->client->ps.ammo[AMMO_TRIPMINE] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_TRIPMINES];
	ent->client->ps.ammo[AMMO_DETPACK] = ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_DETPACKS];

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_THERMALS] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_TRIPMINES] > 0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_DETPACKS] > 0 || ent->client->ps.hasDetPackPlanted)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);

	// zyk: holdable items inventory
	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_BINOCULARS] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_BACTA_CANISTER] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_SENTRY_GUN] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_SEEKER_DRONE] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_EWEB] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_BIG_BACTA] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_FORCE_FIELD] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_CLOAK] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);

	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_ITEM_JETPACK] > 0)
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

	// zyk: jetpack fuel
	ent->client->pers.jetpack_fuel = ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_JETPACK_FUEL];

	if (ent->client->pers.jetpack_fuel < 100)
	{
		ent->client->ps.jetpackFuel = ent->client->pers.jetpack_fuel;
	}
	else
	{
		ent->client->ps.jetpackFuel = 100;
	}

	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)))
	{ // zyk: if player has no items left, deselect the held item
		ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	// zyk: if RPG player does not have this weapon, set melee
	if (ent->health > 0 && !(ent->client->ps.stats[STAT_WEAPONS] & (1 << ent->client->ps.weapon)))
	{
		ent->s.weapon = WP_MELEE;
		ent->client->ps.weapon = WP_MELEE;

		ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_RESET_TO_MELEE);
	}

	// zyk: PLAYER_STATUS_RESET_TO_MELEE used because, after setting it PLAYER_STATUS_RESET_TO_MELEE, client still sends pers.cmd.weapon as the old weapon
	if (ent->client->ps.weapon == ent->client->pers.cmd.weapon && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_RESET_TO_MELEE))
	{
		ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_RESET_TO_MELEE);
	}

	// zyk: if player no longer has Cloak Item and is cloaked, decloaks
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) && ent->client->ps.powerups[PW_CLOAKED])
	{
		Jedi_Decloak(ent);
	}
}

void zyk_start_main_quest_spirits_event(gentity_t* ent)
{
	gentity_t* tree_ent = NULL;

	ent->client->pers.quest_spirits_event_step = 1;
	ent->client->pers.quest_spirits_event_timer = level.time + 2000;

	if (ent->client->pers.quest_spirit_tree_id > -1)
	{
		tree_ent = &g_entities[ent->client->pers.quest_spirit_tree_id];
	}

	if (tree_ent)
	{
		zyk_TeleportPlayer(ent, tree_ent->s.origin, ent->client->ps.viewangles);
	}
}

void zyk_set_starting_quest_progress(gentity_t* ent)
{
	ent->client->pers.quest_progress = MAX_QUEST_PROGRESS / 20;

	if (ent->client->pers.player_settings & (1 << SETTINGS_DIFFICULTY))
	{ // zyk: Hard Mode
		ent->client->pers.quest_progress = MAX_QUEST_PROGRESS / 100;
	}
}

void zyk_show_quest_riddle(gentity_t* ent)
{
	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] == 0)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Its size is immense, and having its energy is a must... it keeps life on Earth, on its power we can trust...\n\"", QUESTCHAR_SELLER));
	}
	else if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] == 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: One can feel warm, with the power of its energy... its power can also be evil, burning to ashes all the harmony...\n\"", QUESTCHAR_SELLER));
	}
	else if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] == 2)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: The harsh feeling of anger, hurting deep into the soul... if one is consumed by it, their life will fall into its bowl...\n\"", QUESTCHAR_SELLER));
	}
	else if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] == 3)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: It has a pure essence, it can create life... but it can also be furious, like a sharp cut of a knife...\n\"", QUESTCHAR_SELLER));
	}
	else if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] == 4)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"%s^7: The pure feeling of affection, even the evil ones can sustain... if one can feel and share, their life will not be in vain...\n\"", QUESTCHAR_SELLER));
	}
}

int zyk_spirit_tree_wither(float x, float y, float z)
{
	int i = 0;
	int total_decrease = 0;
	vec3_t tree_origin;
	gentity_t* npc_ent = NULL;

	VectorSet(tree_origin, x, y, z);

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		npc_ent = &g_entities[i];

		if (npc_ent && npc_ent->client && npc_ent->NPC && npc_ent->health > 0 &&
			npc_ent->client->pers.quest_npc >= QUEST_NPC_MAGE_MASTER && npc_ent->client->pers.quest_npc <= QUEST_NPC_LOW_TRAINED_WARRIOR)
		{
			float npc_distance_to_tree = Distance(tree_origin, npc_ent->r.currentOrigin);

			// zyk: quest enemies will make tree wither based on the distance to the tree and their health
			if (npc_distance_to_tree > 0.0)
			{
				total_decrease += (int)ceil((QUEST_SPIRIT_TREE_WITHER_RATE / npc_distance_to_tree) * npc_ent->health);
			}
			else
			{
				total_decrease += (npc_ent->health * QUEST_SPIRIT_TREE_WITHER_RATE);
			}
		}
	}

	return total_decrease;
}

void zyk_spirit_tree_events(gentity_t* ent)
{
	gentity_t* tree_ent = NULL;
	int tree_duration = 2000000000 - level.time; // zyk: a very long duration so the tree will not disappear

	// zyk: get the Spirit Tree entity or spawn one
	if (ent->client->pers.quest_spirit_tree_id > -1)
	{
		tree_ent = &g_entities[ent->client->pers.quest_spirit_tree_id];
	}
	else
	{
		int quest_progress_percentage = 0;
		int quest_spirit_tree_scale = 0;

		float tree_x = ent->client->ps.origin[0];
		float tree_y = ent->client->ps.origin[1];
		float tree_z = ent->client->ps.origin[2];

		quest_progress_percentage = (ent->client->pers.quest_progress * 100.0) / MAX_QUEST_PROGRESS;
		quest_spirit_tree_scale = (QUEST_SPIRIT_TREE_DEFAULT_SCALE + quest_progress_percentage) / 2;

		ent->client->pers.quest_spirit_tree_id = zyk_spawn_quest_item(QUEST_ITEM_SPIRIT_TREE, tree_duration, quest_spirit_tree_scale, tree_x, tree_y, tree_z);

		if (ent->client->pers.quest_spirit_tree_id > -1)
		{
			tree_ent = &g_entities[ent->client->pers.quest_spirit_tree_id];

			G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/holocron.wav"));
		}
	}

	if (tree_ent && Q_stricmp(tree_ent->classname, "fx_runner") == 0 && Q_stricmp(tree_ent->targetname, "zyk_spirit_tree") == 0)
	{
		int quest_progress_change = 0;
		int quest_progress_percentage = 0;
		int quest_spirit_tree_scale = 0;
		int distance_to_tree = Distance(ent->client->ps.origin, tree_ent->s.origin);

		float tree_x = tree_ent->s.origin[0];
		float tree_y = tree_ent->s.origin[1];
		float tree_z = tree_ent->s.origin[2];

		quest_progress_change += (QUEST_SPIRIT_TREE_REGEN_RATE + ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_BLUE_CRYSTAL] +
			(ent->client->pers.quest_defeated_enemies - QUEST_MIN_ENEMIES_TO_DEFEAT));

		if (distance_to_tree < QUEST_SPIRIT_TREE_RADIUS)
		{
			trap->SendServerCommand(ent->s.number, "cp \"Your Spirit Tree\n\"");
		}

		if (ent->client->ps.forceHandExtend == HANDEXTEND_TAUNT &&
			ent->client->ps.forceDodgeAnim == BOTH_MEDITATE &&
			distance_to_tree < QUEST_SPIRIT_TREE_RADIUS)
		{
			// zyk: meditating inside the tree makes it regen faster
			quest_progress_change *= 2;
		}

		quest_progress_change -= zyk_spirit_tree_wither(tree_x, tree_y, tree_z);

		if (ent->client->pers.player_settings & (1 << SETTINGS_DIFFICULTY))
		{ // zyk: Hard Mode
			if (quest_progress_change > 0)
			{
				quest_progress_change /= 2;
			}
		}

		ent->client->pers.quest_progress += quest_progress_change;

		if (ent->client->pers.quest_progress >= MAX_QUEST_PROGRESS)
		{
			ent->client->pers.quest_progress = MAX_QUEST_PROGRESS;

			// zyk: completed the second part of the quest
			if (ent->client->pers.quest_progress == MAX_QUEST_PROGRESS &&
				ent->client->pers.quest_masters_defeated == QUEST_MASTERS_TO_DEFEAT)
			{
				zyk_start_main_quest_spirits_event(ent);
			}
		}
		else if (ent->client->pers.quest_progress <= 0)
		{ // zyk: if Spirit Tree is completely withered, player dies and loses a quest try
			zyk_set_starting_quest_progress(ent);

			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: The Spirit Tree is completely withered!\n\"", QUESTCHAR_ALL_SPIRITS));

			ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

			player_die(ent, ent, ent, 100000, MOD_SUICIDE);
		}

		quest_progress_percentage = (ent->client->pers.quest_progress * 100.0) / MAX_QUEST_PROGRESS;
		quest_spirit_tree_scale = (QUEST_SPIRIT_TREE_DEFAULT_SCALE + quest_progress_percentage) / 2;

		if (quest_spirit_tree_scale != tree_ent->s.iModelScale)
		{
			gentity_t* tree_model = NULL;
			int j = 0;

			for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
			{
				tree_model = &g_entities[j];

				if (tree_model && Q_stricmp(tree_model->targetname, "zyk_quest_item") == 0 &&
					tree_model->count == tree_ent->s.number)
				{ // zyk: found the Spirit Tree model
					break;
				}

				tree_model = NULL;
			}

			// zyk: change size of the tree based on the quest progress
			if (tree_model)
			{
				float tree_model_x = tree_ent->s.origin[0];
				float tree_model_y = tree_ent->s.origin[1];
				float tree_model_z = tree_ent->s.origin[2] + (quest_spirit_tree_scale * QUEST_SPIRIT_TREE_ORIGIN_Z_OFFSET);

				zyk_set_entity_field(tree_model, "origin", va("%f %f %f", tree_model_x, tree_model_y, tree_model_z));
				zyk_set_entity_field(tree_model, "zykmodelscale", va("%d", quest_spirit_tree_scale));

				zyk_spawn_entity(tree_model);
			}
		}
	}
	else
	{ // zyk: if for some reason this entity is no longer the tree, reset the id
		ent->client->pers.quest_spirit_tree_id = -1;
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
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

int g_siegeRespawnCheck = 0;
void SetMoverState( gentity_t *ent, moverState_t moverState, int time );

extern void remove_credits(gentity_t *ent, int credits);
extern void try_finishing_race();
extern void set_max_health(gentity_t *ent);
extern void set_max_shield(gentity_t *ent);
extern void duel_show_table(gentity_t *ent);
extern void WP_DisruptorAltFire(gentity_t *ent);
extern void G_Kill( gentity_t *ent );
extern void zyk_update_inventory_quantity(gentity_t* ent, qboolean add_item, zyk_inventory_t item, int amount);
extern void zyk_cast_magic(gentity_t* ent, int skill_index);
extern void WP_FireMelee(gentity_t* ent, qboolean alt_fire);

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

			if ((!(first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) || 
				(first_duelist_ally && !(first_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))) &&
				 second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS) && 
				(!second_duelist_ally || second_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
			{ // zyk: first team wins
				duel_tournament_set_match_winner(first_duelist);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if ((!(second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) || 
				(second_duelist_ally && !(second_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))) &&
				first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS) && 
				(!first_duelist_ally || first_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
			{ // zyk: second team wins
				duel_tournament_set_match_winner(second_duelist);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if (level.duel_tournament_timer < level.time)
			{ // zyk: duel timed out
				int first_team_health = 0;
				int second_team_health = 0;

				if (!(first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
				{
					first_team_health = first_duelist->health + first_duelist->client->ps.stats[STAT_ARMOR];
				}

				if (!(second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
				{
					second_team_health = second_duelist->health + second_duelist->client->ps.stats[STAT_ARMOR];
				}

				if (first_duelist_ally && !(first_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
				{
					first_team_health += (first_duelist_ally->health + first_duelist_ally->client->ps.stats[STAT_ARMOR]);
				}

				if (second_duelist_ally && !(second_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
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
			else if (first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS) && 
				(!first_duelist_ally || first_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) &&
				second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS) && 
				(!second_duelist_ally || second_duelist_ally->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
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
					this_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS);
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

	if (level.load_entities_timer == 0 && level.legendary_artifact_step > QUEST_SECRET_INIT_STEP &&
		level.legendary_artifact_debounce_timer < level.time)
	{ // zyk: map has an legendary artifact
		if (level.legendary_artifact_timer < level.time)
		{ // zyk: time to solve puzzle run out
			level.legendary_artifact_step = QUEST_SECRET_CLEAR_STEP;
		}

		if (level.legendary_artifact_step == QUEST_SECRET_SPAWN_CRYSTALS_STEP)
		{ // zyk: spawns the crystal models
			int crystal_scale = 90;
			int model_x = level.legendary_artifact_origin[0];
			int model_y = level.legendary_artifact_origin[1];
			int model_z = level.legendary_artifact_origin[2];

			zyk_spawn_legendary_artifact_puzzle_model(model_x + 76, model_y + 42, model_z, crystal_scale, "models/map_objects/mp/crystal_red.md3", 1);

			zyk_spawn_legendary_artifact_puzzle_model(model_x + 76, model_y - 42, model_z, crystal_scale, "models/map_objects/mp/crystal_green.md3", 2);

			zyk_spawn_legendary_artifact_puzzle_model(model_x - 76, model_y + 42, model_z, crystal_scale, "models/map_objects/mp/crystal_blue.md3", 3);

			zyk_spawn_legendary_artifact_puzzle_model(model_x - 76, model_y - 42, model_z, crystal_scale, "models/map_objects/mp/crystal_red.md3", 4);
			zyk_spawn_legendary_artifact_puzzle_model(model_x - 76, model_y - 42, model_z, crystal_scale, "models/map_objects/mp/crystal_green.md3", 0);

			zyk_spawn_legendary_artifact_puzzle_model(model_x, model_y + 84, model_z, crystal_scale, "models/map_objects/mp/crystal_red.md3", 5);
			zyk_spawn_legendary_artifact_puzzle_model(model_x, model_y + 84, model_z, crystal_scale, "models/map_objects/mp/crystal_blue.md3", 0);

			zyk_spawn_legendary_artifact_puzzle_model(model_x, model_y - 84, model_z, crystal_scale, "models/map_objects/mp/crystal_green.md3", 6);
			zyk_spawn_legendary_artifact_puzzle_model(model_x, model_y - 84, model_z, crystal_scale, "models/map_objects/mp/crystal_blue.md3", 0);

			level.legendary_artifact_step = QUEST_SECRET_START_PUZZLE_STEP;

			level.legendary_artifact_debounce_timer = level.time + 2000;
		}
		else if (level.legendary_artifact_step >= QUEST_SECRET_START_PUZZLE_STEP && level.legendary_artifact_step < QUEST_SECRET_CHOSEN_CRYSTALS_STEP)
		{ // zyk: starts the memory puzzle by spawning effects the player must remember
			int chosen_crystal = Q_irand(1, 6);
			int j = 0;

			level.legendary_crystal_chosen[level.legendary_artifact_step - QUEST_SECRET_START_PUZZLE_STEP] = chosen_crystal;

			level.legendary_artifact_step++;

			for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
			{ // zyk: show effect in the chosen crystal position
				gentity_t* crystal_ent = &g_entities[j];

				if (crystal_ent && Q_stricmp(crystal_ent->targetname, "zyk_puzzle_model") == 0 && crystal_ent->count == chosen_crystal)
				{
					zyk_spawn_puzzle_effect(crystal_ent);

					G_Sound(crystal_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));
				}
			}

			level.legendary_artifact_debounce_timer = level.time + 1500;
		}
		else if (level.legendary_artifact_step == QUEST_SECRET_CORRECT_CRYSTALS_STEP)
		{ // zyk: player solved the puzzle, spawn the legendary artifact
			int legendary_artifact_scale = 30;

			int model_x = level.legendary_artifact_origin[0];
			int model_y = level.legendary_artifact_origin[1];
			int model_z = level.legendary_artifact_origin[2];

			if (level.legendary_artifact_type == QUEST_ITEM_MAGIC_ARMOR)
			{
				legendary_artifact_scale = 80;
			}

			zyk_spawn_legendary_artifact_model(model_x, model_y, model_z, legendary_artifact_scale);

			level.legendary_artifact_step = QUEST_SECRET_SECRET_ITEM_SPAWNED_STEP;
		}
		else if (level.legendary_artifact_step == QUEST_SECRET_CLEAR_STEP)
		{ // zyk: clear the crystals
			int j = 0;

			for (j = 0; j < level.maxclients; j++)
			{
				gentity_t* player_ent = &g_entities[j];

				if (player_ent && player_ent->client)
				{
					player_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_GOT_PUZZLE_CRYSTAL);
				}
			}

			for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
			{ // zyk: show effect in the chosen crystal position
				gentity_t* crystal_ent = &g_entities[j];

				if (crystal_ent && Q_stricmp(crystal_ent->targetname, "zyk_puzzle_model") == 0)
				{
					G_FreeEntity(crystal_ent);
				}
			}

			level.legendary_artifact_step = QUEST_SECRET_INIT_STEP;
		}
	}

	if (level.map_music_reset_timer > 0 && level.map_music_reset_timer < level.time)
	{ // zyk: resets music to default one
		level.map_music_reset_timer = 0;

		if (level.current_map_music == MAPMUSIC_NONE)
		{
			trap->SetConfigstring(CS_MUSIC, G_NewString(level.default_map_music));
		}
		else if (level.current_map_music == MAPMUSIC_QUEST)
		{
			trap->SetConfigstring(CS_MUSIC, "music/kor_lite/korrib_action.mp3");
		}
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
				else if (!G_PointInBounds( ent->client->ps.origin, hacked->r.absmin, hacked->r.absmax ) || 
					(Q_stricmp(hacked->targetname, "zyk_energy_modulator_puzzle") == 0 &&
					(Distance(ent->client->ps.origin, hacked->s.origin) >= QUEST_ITEM_DISTANCE))) // zyk: quest item effects
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
				int jetpack_debounce_amount = JETPACK_FUEL_USAGE;

				if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_JETPACK] > 0)
				{ // zyk: Jetpack Upgrade decreases fuel debounce
					jetpack_debounce_amount /= 2;
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

				ent->client->ps.jetpackFuel = ent->client->pers.jetpack_fuel / JETPACK_SCALE;

				if (ent->client->sess.amrpgmode == 2)
				{
					ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_JETPACK_FUEL] = ent->client->pers.jetpack_fuel;

					if (ent->client->pers.jetpack_fuel < 100)
					{
						ent->client->ps.jetpackFuel = ent->client->pers.jetpack_fuel;
					}
					else
					{
						ent->client->ps.jetpackFuel = 100;
					}
				}

				ent->client->jetPackDebReduce = level.time + 250; // zyk: JETPACK_DEFUEL_RATE. Original value: 200
			}

			// zyk: Duel Tournament. Do not let anyone enter or anyone leave the globe arena
			if (level.duel_tournament_mode == 4)
			{
				if (duel_tournament_is_duelist(ent) == qtrue && 
					!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) && // zyk: did not die in his duel yet
					Distance(ent->client->ps.origin, level.duel_tournament_origin) > (DUEL_TOURNAMENT_ARENA_SIZE * zyk_duel_tournament_arena_scale.value / 100.0) &&
					ent->health > 0)
				{ // zyk: duelists cannot leave the arena after duel begins
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
				else if ((duel_tournament_is_duelist(ent) == qfalse || 
					(level.duel_players[ent->s.number] != -1 && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))) && // zyk: not a duelist or died in his duel
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
				}
				else if (level.race_map == 1 && (int)ent->client->ps.origin[0] < -4536)
				{ // zyk: player cant start racing before the countdown timer
					ent->client->pers.race_position = 0;
					trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%s ^7lost for trying to race before it starts!\n\"", ent->client->pers.netname));

					try_finishing_race();
				}
			}

			magic_power_events(ent);
			zyk_status_effects(ent);

			if (zyk_chat_protection_timer.integer > 0)
			{ // zyk: chat protection. If 0, it is off. If greater than 0, set the timer to protect the player
				if (ent->client->ps.eFlags & EF_TALK && ent->client->pers.chat_protection_timer == 0)
				{
					ent->client->pers.chat_protection_timer = level.time + zyk_chat_protection_timer.integer;
				}
				else if (ent->client->ps.eFlags & EF_TALK && ent->client->pers.chat_protection_timer < level.time)
				{
					ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_CHAT_PROTECTION);
				}
				else if (ent->client->pers.chat_protection_timer != 0 && !(ent->client->ps.eFlags & EF_TALK))
				{
					ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_CHAT_PROTECTION);
					ent->client->pers.chat_protection_timer = 0;
				}
			}

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

				if (!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SELF_KILL)) &&
					(ent->client->pers.last_health != ent->health || 
					 ent->client->pers.last_shield != ent->client->ps.stats[STAT_ARMOR] || 
					 ent->client->pers.last_mp != ent->client->pers.magic_power || 
					 ent->client->pers.last_stamina != ent->client->pers.current_stamina))
				{
					ent->client->pers.last_health = ent->health;
					ent->client->pers.last_shield = ent->client->ps.stats[STAT_ARMOR];
					ent->client->pers.last_mp = ent->client->pers.magic_power;
					ent->client->pers.last_stamina = ent->client->pers.current_stamina;
				}

				// zyk: tutorial, which teaches the player the RPG Mode features
				if (ent->client->pers.tutorial_step > TUTORIAL_NONE && ent->client->pers.tutorial_step < MAX_TUTORIAL_STEPS && ent->client->pers.tutorial_timer < level.time)
				{
					zyk_show_tutorial(ent);

					// zyk: interval between messages
					ent->client->pers.tutorial_timer = level.time + 5000;
				}

				if (ent->client->pers.save_stat_changes_timer < level.time)
				{
					save_account(ent, qtrue);

					// zyk: timer to save account to prevent too much file IO
					ent->client->pers.save_stat_changes_timer = level.time + 500;
				}

				// zyk: Weapon Upgrades. Do not change weaponTime if player has no stamina
				if (ent->client->pers.stamina_out_timer < level.time)
				{
					if (ent->client->ps.weapon == WP_BLASTER && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRADE_E11_BLASTER1) &&
						ent->client->ps.weaponTime > (weaponData[WP_BLASTER].fireTime * 0.7))
					{
						ent->client->ps.weaponTime = weaponData[WP_BLASTER].fireTime * 0.7;
					}

					if (ent->client->ps.weapon == WP_DISRUPTOR && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DISRUPTOR] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRADE_DISRUPTOR1) && 
						ent->client->ps.weaponTime > (weaponData[WP_DISRUPTOR].fireTime * 0.6))
					{
						ent->client->ps.weaponTime = weaponData[WP_DISRUPTOR].fireTime * 0.6;
					}

					if (ent->client->ps.weapon == WP_DEMP2 && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEMP2] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRADE_DEMP21) &&
						ent->client->ps.weaponTime > (weaponData[WP_DEMP2].fireTime * 0.5))
					{
						ent->client->ps.weaponTime = weaponData[WP_DEMP2].fireTime * 0.5;
					}

					if (ent->client->ps.weapon == WP_BRYAR_PISTOL && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRAGE_BLASTER_PISTOL1) && 
						ent->client->ps.weaponTime > (weaponData[WP_BRYAR_PISTOL].fireTime * 0.6))
					{
						ent->client->ps.weaponTime = weaponData[WP_BRYAR_PISTOL].fireTime * 0.6;
					}

					if (ent->client->ps.weapon == WP_BRYAR_OLD && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRAGE_BRYAR_PISTOL1) &&
						ent->client->ps.weaponTime > (weaponData[WP_BRYAR_OLD].fireTime * 0.6))
					{
						ent->client->ps.weaponTime = weaponData[WP_BRYAR_OLD].fireTime * 0.6;
					}

					if (ent->client->ps.weapon == WP_REPEATER && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_REPEATER] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRADE_REPEATER1) &&
						ent->client->ps.weaponTime > (weaponData[WP_REPEATER].altFireTime * 0.5))
					{
						ent->client->ps.weaponTime = weaponData[WP_REPEATER].altFireTime * 0.5;
					}

					if (ent->client->ps.weapon == WP_ROCKET_LAUNCHER && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER] > 0 &&
						ent->client->pers.active_inventory_upgrades & (1 << INV_UPGRADE_ROCKET2) &&
						ent->client->ps.weaponTime > (weaponData[WP_ROCKET_LAUNCHER].altFireTime * 0.5))
					{
						ent->client->ps.weaponTime = weaponData[WP_ROCKET_LAUNCHER].altFireTime * 0.5;
					}

					// zyk: Melee Punch Speed skill
					if (ent->client->ps.weapon == WP_MELEE && ent->client->pers.skill_levels[SKILL_MELEE_SPEED] > 0 &&
						ent->client->ps.weaponTime > (weaponData[WP_MELEE].fireTime * (1.0 - (0.15 * ent->client->pers.skill_levels[SKILL_MELEE_SPEED]))))
					{
						ent->client->ps.weaponTime = (weaponData[WP_MELEE].fireTime * (1.0 - (0.15 * ent->client->pers.skill_levels[SKILL_MELEE_SPEED])));
					}

					if (ent->client->pers.flame_thrower_timer > level.time && ent->client->cloakDebReduce < level.time)
					{ // zyk: fires the flame thrower
						Player_FireFlameThrower(ent, qfalse);
					}
				}
				else if (ent->client->pers.is_getting_up == qfalse && (ent->client->pers.stamina_out_timer - level.time) < 1200)
				{ // zyk: setting the getup anim
					G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_GETUP1, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);

					ent->client->pers.is_getting_up = qtrue;
				}

				if (ent->client->pers.special_crystal_timer < level.time && 
					ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_RED_CRYSTAL] > 0 && 
					ent->client->pers.special_crystal_counter >= RED_CRYSTAL_MAX_CHARGE)
				{ // zyk: Red crystal is fully charged
					int red_crystal_dmg = 10 + ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_GREEN_CRYSTAL];

					if (red_crystal_dmg > 90)
					{
						red_crystal_dmg = 90;
					}

					// zyk: creates a lightning dome, it is the DEMP2 alt fire but bigger
					lightning_dome(ent, red_crystal_dmg);

					G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/thunder_close1.mp3"));

					ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_RED_CRYSTAL]--;

					ent->client->pers.special_crystal_counter = 0;
					ent->client->pers.special_crystal_timer = level.time + 200;

				}
				else if (!(ent->client->pers.cmd.buttons & BUTTON_USE))
				{ // zyk: not holding Use, reset charge
					ent->client->pers.special_crystal_counter = 0;
				}

				// zyk: updating RPG inventory and calculating current weight
				if (ent->client->pers.inventory_update_timer < level.time)
				{
					zyk_update_inventory(ent);
					zyk_calculate_current_weight(ent);

					ent->client->pers.inventory_update_timer = level.time + 100;
				}
				
				if (ent->client->pers.thermal_vision == qtrue && ent->client->ps.zoomMode == 0)
				{ // zyk: if player stops using binoculars, stop the Thermal Vision
					ent->client->pers.thermal_vision = qfalse;

					ent->client->ps.fd.forcePowersActive &= ~(1 << FP_SEE);

					// zyk: loading Sense value
					if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SEE)) && ent->client->pers.skill_levels[SKILL_SENSE] > 0)
						ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
					if (ent->client->pers.skill_levels[SKILL_SENSE] == 0)
						ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);

					ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[SKILL_SENSE];

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

				// zyk: show current magic power when player gains or loses a certain amount of mp
				if ((ent->client->pers.magic_power == 0 && ent->client->pers.magic_power != ent->client->pers.last_magic_power_shown) ||
					(ent->client->pers.magic_power / MAGIC_CHANGE_AMOUNT) != (ent->client->pers.last_magic_power_shown / MAGIC_CHANGE_AMOUNT))
				{
					gentity_t* plum;
					vec3_t plum_origin;

					ent->client->pers.last_magic_power_shown = ent->client->pers.magic_power;

					// zyk: use score plums to show the current mp
					VectorSet(plum_origin, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2] + DEFAULT_MAXS_2);

					plum = G_TempEntity(plum_origin, EV_SCOREPLUM);

					// only send this temp entity to a single client
					plum->r.svFlags |= SVF_SINGLECLIENT;
					plum->r.singleClient = ent->s.number;

					plum->s.otherEntityNum = ent->s.number;
					plum->s.time = ent->client->pers.magic_power;
				}

				// zyk: spawn magic crystals and side quest stuff
				if (ent->client->pers.skill_crystal_timer > 0 && ent->client->pers.skill_crystal_timer < level.time && 
					!(ent->client->pers.player_settings & (1 << SETTINGS_MAGIC_CRYSTALS)))
				{
					int main_quest_progress = (ent->client->pers.quest_defeated_enemies / 2) +
						(ent->client->pers.quest_masters_defeated * 2) +
						(((ent->client->pers.quest_progress * 1.0) / MAX_QUEST_PROGRESS) * 10);

					int side_quest_item_chance_modifier = ENERGY_MODULATOR_PARTS * QUEST_LOG_PARTS;

					int side_quest_item_chance = ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_BLUE_CRYSTAL] - side_quest_item_chance_modifier +
						ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_RED_CRYSTAL] + 
						ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_GREEN_CRYSTAL] -
						(ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_ENERGY_MODULATOR] * (side_quest_item_chance_modifier / ENERGY_MODULATOR_PARTS)) -
						(ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] * side_quest_item_chance_modifier) -
						(ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] * (side_quest_item_chance_modifier / QUEST_LOG_PARTS)) +
						(main_quest_progress / 5);

					int side_quest_item_duration = side_quest_item_chance * SIDE_QUEST_STUFF_TIMER;

					if (ent->client->pers.player_settings & (1 << SETTINGS_DIFFICULTY))
					{ // zyk: Hard Mode
						side_quest_item_chance /= 2;
						side_quest_item_duration /= 2;
					}
					
					if (Q_irand(0, 99) < 9)
					{ // zyk: crystals
						zyk_quest_item_t crystal_type = Q_irand(QUEST_ITEM_SKILL_CRYSTAL, QUEST_ITEM_SPECIAL_CRYSTAL);

						zyk_spawn_magic_crystal(60000, crystal_type);
					}
					
					if (Q_irand(0, 99) < side_quest_item_chance && level.energy_modulator_timer < level.time)
					{ // zyk: Energy Modulator side quest
						float puzzle_x, puzzle_y, puzzle_z;
						gentity_t* chosen_entity = NULL;

						chosen_entity = zyk_find_entity_for_quest();

						if (chosen_entity)
						{
							if (chosen_entity->r.svFlags & SVF_USE_CURRENT_ORIGIN)
							{
								puzzle_x = chosen_entity->r.currentOrigin[0];
								puzzle_y = chosen_entity->r.currentOrigin[1];
								puzzle_z = chosen_entity->r.currentOrigin[2];
							}
							else
							{
								puzzle_x = chosen_entity->s.origin[0];
								puzzle_y = chosen_entity->s.origin[1];
								puzzle_z = chosen_entity->s.origin[2];
							}

							if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_ENERGY_MODULATOR] < ENERGY_MODULATOR_PARTS)
							{
								zyk_spawn_quest_item(QUEST_ITEM_ENERGY_MODULATOR, side_quest_item_duration, 30, puzzle_x, puzzle_y, puzzle_z);
								level.energy_modulator_timer = level.time + side_quest_item_duration;
							}
						}
					}

					if (Q_irand(0, 99) < side_quest_item_chance && level.magic_armor_timer < level.time)
					{ // zyk: Magic Armor side quest
						float puzzle_x, puzzle_y, puzzle_z;
						gentity_t* chosen_entity = NULL;

						chosen_entity = zyk_find_entity_for_quest();

						if (chosen_entity)
						{
							if (chosen_entity->r.svFlags & SVF_USE_CURRENT_ORIGIN)
							{
								puzzle_x = chosen_entity->r.currentOrigin[0];
								puzzle_y = chosen_entity->r.currentOrigin[1];
								puzzle_z = chosen_entity->r.currentOrigin[2];
							}
							else
							{
								puzzle_x = chosen_entity->s.origin[0];
								puzzle_y = chosen_entity->s.origin[1];
								puzzle_z = chosen_entity->s.origin[2];
							}

							if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] == 0)
							{
								zyk_spawn_quest_item(QUEST_ITEM_MAGIC_ARMOR, side_quest_item_duration, 80, puzzle_x, puzzle_y, puzzle_z);
								level.magic_armor_timer = level.time + side_quest_item_duration;
							}
						}
					}

					if (Q_irand(0, 99) < side_quest_item_chance)
					{ // zyk: Quest Log side quest
						float puzzle_x, puzzle_y, puzzle_z;
						gentity_t* chosen_entity = NULL;

						chosen_entity = zyk_find_entity_for_quest();

						if (chosen_entity)
						{
							if (chosen_entity->r.svFlags & SVF_USE_CURRENT_ORIGIN)
							{
								puzzle_x = chosen_entity->r.currentOrigin[0];
								puzzle_y = chosen_entity->r.currentOrigin[1];
								puzzle_z = chosen_entity->r.currentOrigin[2];
							}
							else
							{
								puzzle_x = chosen_entity->s.origin[0];
								puzzle_y = chosen_entity->s.origin[1];
								puzzle_z = chosen_entity->s.origin[2];
							}

							if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] < QUEST_LOG_PARTS)
							{
								zyk_spawn_quest_npc(QUEST_NPC_SELLER, 0, side_quest_item_chance, qfalse, -1);
							}
						}
					}

					ent->client->pers.skill_crystal_timer = level.time + RPG_MAGIC_CRYSTAL_MIN_SPAWN_TIME;
				}

				// zyk: Seller events
				if (ent->client->pers.quest_seller_event_step > QUEST_SELLER_STEP_NONE && ent->client->pers.quest_seller_event_timer < level.time)
				{
					if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] < QUEST_LOG_PARTS)
					{
						if (ent->client->pers.quest_seller_event_step == QUEST_SELLER_STEP_TALKED)
						{
							trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Hi! I am the seller. Answer my riddle in chat and I will give you a part of my Quest Log!\n\"", QUESTCHAR_SELLER));
						}
						else if (ent->client->pers.quest_seller_event_step == QUEST_SELLER_RIDDLE_START)
						{ // zyk: Seller riddles
							zyk_show_quest_riddle(ent);
						}
						else if (ent->client->pers.quest_seller_event_step == QUEST_SELLER_END_STEP)
						{
							ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_QUEST_LOG] += 1;

							add_credits(ent, 1000);
							G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

							trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Correct answer! Receive this part of the Quest Log and 1000 credits. Use ^3/list questlog^7.\n\"", QUESTCHAR_SELLER));
						}
					}
					else
					{
						trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Hello again! I hope my Quest Log helped you!\n\"", QUESTCHAR_SELLER));

						ent->client->pers.quest_seller_event_step = NUM_QUEST_SELLER_STEPS;
					}

					if (ent->client->pers.quest_seller_event_step != QUEST_SELLER_RIDDLE_ANSWER)
					{
						ent->client->pers.quest_seller_event_step++;
						ent->client->pers.quest_seller_event_timer = level.time + 5000;
					}

					if (ent->client->pers.quest_seller_event_step >= NUM_QUEST_SELLER_STEPS)
					{
						ent->client->pers.quest_seller_event_step = QUEST_SELLER_STEP_NONE;
					}
				}

				// zyk: control the quest events
				if (level.load_entities_timer == 0 && 
					zyk_allow_quests.integer > 0 && !(ent->client->pers.player_settings & (1 << SETTINGS_RPG_QUESTS)) && 
					ent->client->ps.duelInProgress == qfalse && ent->health > 0 && 
					ent->client->pers.connected == CON_CONNECTED && ent->client->sess.sessionTeam != TEAM_SPECTATOR
					)
				{
					// zyk: spirits events of the quest
					if (ent->client->pers.quest_spirits_event_step > 0 && ent->client->pers.quest_spirits_event_timer < level.time)
					{
						if (!(ent->client->pers.quest_missions & (1 << MAIN_QUEST_SECOND_PART_COMPLETE)))
						{
							if (ent->client->pers.quest_spirits_event_step == 1)
							{
								gentity_t* tree_ent = NULL;

								zyk_spawn_magic_spirits(ent, QUEST_FINAL_EVENT_TIMER + 5000);

								if (ent->client->pers.quest_spirit_tree_id > -1)
								{
									tree_ent = &g_entities[ent->client->pers.quest_spirit_tree_id];

									if (tree_ent)
									{
										G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/thunder_close1.mp3"));

										zyk_quest_effect_spawn(tree_ent, tree_ent, "zyk_spirit_tree_energy", "0", "ships/sd_exhaust", 100, 0, 0, QUEST_FINAL_EVENT_TIMER);
									}
								}
							}
							else
							{
								if (ent->client->pers.quest_spirits_event_step == 2)
								{
									int j = 0;

									for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
									{
										gentity_t* npc_ent = &g_entities[j];

										if (npc_ent && npc_ent->client && npc_ent->NPC &&
											npc_ent->client->pers.quest_npc >= QUEST_NPC_MAGE_MASTER && npc_ent->client->pers.quest_npc < QUEST_NPC_ALLY_MAGE)
										{ // zyk: one of the quest enemies
											zyk_NPC_Kill_f(npc_ent->NPC_type);
										}
									}

									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: We can now defeat the remaining of Brotherhood of Mages.\n\"", QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 3)
								{
									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: But is is not over yet! The Elemental Demon is coming!\n\"",
										QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 4)
								{
									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: The Mages actions unbalanced Nature and awakened him. He is attracted by magic usage and Spirit Trees.\n\"", QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 5)
								{
									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: You must defeat him for the victory to be complete!\n\"", QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 6)
								{
									ent->client->pers.quest_missions |= (1 << MAIN_QUEST_SECOND_PART_COMPLETE);
								}
							}

							ent->client->pers.quest_spirits_event_step++;

							if (ent->client->pers.quest_spirits_event_step >= 7)
							{
								ent->client->pers.quest_spirits_event_step = 0;
							}
						}
						else if (!(ent->client->pers.quest_missions & (1 << MAIN_QUEST_THIRD_PART_COMPLETE)))
						{
							if (ent->client->pers.quest_spirits_event_step == 1)
							{
								zyk_spawn_magic_spirits(ent, QUEST_FINAL_EVENT_TIMER);
							}
							else
							{
								if (ent->client->pers.quest_spirits_event_step == 2)
								{
									zyk_NPC_Kill_f("all");

									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Nice! You did it! Our victory is complete.\n\"", QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 3)
								{
									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/interface/secret_area.mp3"));

									zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_MISC_BLUE_CRYSTAL, 10);
									zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_MISC_GREEN_CRYSTAL, 10);
									zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_MISC_RED_CRYSTAL, 10);

									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Receive these crystals!\n\"", QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 4)
								{
									trap->SendServerCommand(ent->s.number, va("chat \"%s^7: Nature is balanced again. Thank you.\n\"", QUESTCHAR_ALL_SPIRITS));
								}
								else if (ent->client->pers.quest_spirits_event_step == 5)
								{
									ent->client->pers.quest_missions |= (1 << MAIN_QUEST_THIRD_PART_COMPLETE);
								}
							}

							ent->client->pers.quest_spirits_event_step++;

							if (ent->client->pers.quest_spirits_event_step >= 6)
							{
								ent->client->pers.quest_spirits_event_step = 0;
							}
						}

						ent->client->pers.quest_spirits_event_timer = level.time + 5000;
					}

					// zyk: Spirit Tree regen and wither
					if (!(ent->client->pers.quest_progress == MAX_QUEST_PROGRESS &&
						ent->client->pers.quest_masters_defeated == QUEST_MASTERS_TO_DEFEAT) &&
						ent->client->pers.quest_defeated_enemies >= QUEST_MIN_ENEMIES_TO_DEFEAT &&
						ent->client->pers.quest_progress_timer < level.time)
					{
						zyk_spirit_tree_events(ent);

						ent->client->pers.quest_progress_timer = level.time + 1000;
					}
					else if (ent->client->pers.quest_missions & (1 << MAIN_QUEST_SECOND_PART_COMPLETE) && 
							!(ent->client->pers.quest_missions & (1 << MAIN_QUEST_THIRD_PART_COMPLETE)) &&
							ent->client->pers.quest_spirits_event_step == 0 &&
							ent->client->pers.quest_progress_timer < level.time)
					{ // zyk: final battle
						qboolean hard_difficulty = qfalse;

						if (ent->client->pers.player_settings & (1 << SETTINGS_DIFFICULTY))
						{
							hard_difficulty = qtrue;
						}

						if (!(level.special_quest_npc_in_map & (1 << QUEST_NPC_ANGEL_OF_DEATH)) && !(ent->client->pers.quest_missions & (1 << MAIN_QUEST_ANGEL_OF_DEATH)))
						{
							zyk_spawn_quest_npc(QUEST_NPC_ANGEL_OF_DEATH, ent->client->ps.viewangles[YAW], ent->client->pers.quest_defeated_enemies, hard_difficulty, -1);
						}
						else if (!(level.special_quest_npc_in_map & (1 << QUEST_NPC_JORMUNGANDR)) && !(ent->client->pers.quest_missions & (1 << MAIN_QUEST_JORMUNGANDR)))
						{
							zyk_spawn_quest_npc(QUEST_NPC_JORMUNGANDR, ent->client->ps.viewangles[YAW], ent->client->pers.quest_defeated_enemies, hard_difficulty, -1);
						}
						else if (!(level.special_quest_npc_in_map & (1 << QUEST_NPC_CHIMERA)) && !(ent->client->pers.quest_missions & (1 << MAIN_QUEST_CHIMERA)))
						{
							zyk_spawn_quest_npc(QUEST_NPC_CHIMERA, ent->client->ps.viewangles[YAW], ent->client->pers.quest_defeated_enemies, hard_difficulty, -1);
						}
						else if (ent->client->pers.quest_missions & (1 << MAIN_QUEST_ANGEL_OF_DEATH) && 
							ent->client->pers.quest_missions & (1 << MAIN_QUEST_JORMUNGANDR) && 
							ent->client->pers.quest_missions & (1 << MAIN_QUEST_CHIMERA))
						{// zyk: starting the final events
							ent->client->pers.quest_spirits_event_step = 1;

							level.reality_shift_mode = REALITY_SHIFT_NONE;
						}

						ent->client->pers.quest_progress_timer = level.time + 1000;
					}

					// zyk: quest npcs
					if (ent->client->pers.quest_event_timer < level.time && 
						(ent->client->pers.quest_defeated_enemies > 0 || ent->client->pers.quest_missions & (1 << MAIN_QUEST_START)))
					{
						qboolean hard_difficulty = qfalse;

						if (ent->client->pers.player_settings & (1 << SETTINGS_DIFFICULTY))
						{
							hard_difficulty = qtrue;
						}

						zyk_set_quest_event_timer(ent);

						if (!(ent->client->pers.quest_progress == MAX_QUEST_PROGRESS &&
							ent->client->pers.quest_masters_defeated == QUEST_MASTERS_TO_DEFEAT))
						{
							int enemy_type = 0;
							zyk_quest_npc_t stronger_enemy_type = QUEST_NPC_LOW_TRAINED_WARRIOR - (ent->client->pers.quest_defeated_enemies / 5);
							
							if (stronger_enemy_type < QUEST_NPC_MAGE_MINISTER)
							{
								stronger_enemy_type = QUEST_NPC_MAGE_MINISTER;
							}

							if (ent->client->pers.quest_defeated_enemies < QUEST_MIN_ENEMIES_TO_DEFEAT)
							{
								enemy_type = Q_irand(stronger_enemy_type, QUEST_NPC_LOW_TRAINED_WARRIOR);
							}
							else
							{
								int mage_master_chance = 
									(ent->client->pers.quest_defeated_enemies - QUEST_MIN_ENEMIES_TO_DEFEAT) + 
									(ent->client->pers.quest_masters_defeated * 4) + 
									((ent->client->pers.quest_progress * 20.0) / MAX_QUEST_PROGRESS);

								enemy_type = Q_irand(QUEST_NPC_MAGE_MINISTER, QUEST_NPC_LOW_TRAINED_WARRIOR);

								if (Q_irand(0, 99) < mage_master_chance)
								{
									enemy_type = QUEST_NPC_MAGE_MASTER;
								}
							}

							if (zyk_number_of_enemies_in_map() < (zyk_max_quest_npcs.integer / 2))
							{
								zyk_spawn_quest_npc(enemy_type, ent->client->ps.viewangles[YAW], ent->client->pers.quest_defeated_enemies, hard_difficulty, -1);
							}
						}

						if (!(ent->client->pers.quest_missions & (1 << MAIN_QUEST_THIRD_PART_COMPLETE)) && 
							zyk_number_of_allies_in_map(NULL) < (zyk_max_quest_npcs.integer / 2) &&
							Q_irand(0, 99) < (1 + ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_BLUE_CRYSTAL] + zyk_number_of_enemies_in_map() - (zyk_number_of_allies_in_map(ent) * 4)))
						{ // zyk: spawn an ally
							int ally_type = Q_irand(QUEST_NPC_ALLY_MAGE, QUEST_NPC_ALLY_FORCE_WARRIOR);
							int ally_bonus = (ent->client->pers.quest_defeated_enemies / 2) + ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_BLUE_CRYSTAL];

							zyk_spawn_quest_npc(ally_type, 0, ally_bonus, qfalse, ent->s.number);
						}
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

			magic_power_events(ent);
			zyk_status_effects(ent);

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

			if (ent->health > 0)
			{
				if ((ent->client->pers.mind_tricker_player_ids1 > 0 || ent->client->pers.mind_tricker_player_ids2 > 0) &&
					ent->client->pers.mind_trick_effect_timer < level.time)
				{ // zyk: a mind tricked npc. Show effect on head
					gentity_t* plum;
					vec3_t plum_origin;
					int player_it = 0;

					for (player_it = 0; player_it < level.maxclients; player_it++)
					{
						if (ent->client->pers.mind_tricker_player_ids1 & (1 << player_it) || ent->client->pers.mind_tricker_player_ids2 & (1 << player_it))
						{
							VectorSet(plum_origin, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2] + DEFAULT_MAXS_2);

							plum = G_TempEntity(plum_origin, EV_SCOREPLUM);

							// only send this temp entity to a single client
							plum->r.svFlags |= SVF_SINGLECLIENT;
							plum->r.singleClient = player_it;

							plum->s.otherEntityNum = player_it;
							plum->s.time = player_it;
						}
					}

					ent->client->pers.mind_trick_effect_timer = level.time + 500;
				}
				
				// zyk: npcs with magic powers
				if (ent->client->pers.quest_npc > QUEST_NPC_NONE && ent->client->pers.quest_event_timer < level.time)
				{
					if (ent->client->pers.quest_npc == QUEST_NPC_SELLER || 
						(ent->client->pers.quest_npc >= QUEST_NPC_ANGEL_OF_DEATH && ent->client->pers.quest_npc <= QUEST_NPC_CHIMERA))
					{ // zyk: there can only be one of these npcs in the map
						level.special_quest_npc_in_map |= (1 << ent->client->pers.quest_npc);
					}

					if (ent->enemy)
					{
						int first_magic_skill = SKILL_MAGIC_HEALING_AREA;
						int random_magic = Q_irand(0, MAGIC_LIGHT_MAGIC);
						int magic_skill_index = first_magic_skill + random_magic;
						int quest_npc_enemy_dist = Distance(ent->client->ps.origin, ent->enemy->r.currentOrigin);

						// zyk: must add a little interval to avoid performance issues
						ent->client->pers.quest_event_timer = level.time + 200;

						if (ent->client->pers.skill_levels[magic_skill_index] > 0)
						{
							int magic_cast_dist = 0;
							int magic_bonus = 0;

							// zyk: Magic Armor improves all magic powers
							if (ent->client->pers.rpg_inventory[RPG_INVENTORY_LEGENDARY_MAGIC_ARMOR] > 0)
							{
								magic_bonus = 1;
							}

							magic_cast_dist = MAGIC_MIN_RANGE + (MAGIC_RANGE_BONUS * (ent->client->pers.skill_levels[magic_skill_index] + magic_bonus));

							if (quest_npc_enemy_dist < Q_irand(0, magic_cast_dist) && !(ent->client->pers.quest_power_status & (1 << random_magic)))
							{
								zyk_cast_magic(ent, magic_skill_index);

								ent->client->pers.quest_event_timer = level.time + (1000 * Q_irand(4, 8));

								if (ent->client->pers.quest_npc == QUEST_NPC_MAGE_MASTER)
								{ // zyk: master mage npc will use magic more often
									ent->client->pers.quest_event_timer -= 2000;
								}
							}
							else if (ent->client->pers.quest_power_status & (1 << random_magic))
							{ // zyk: active power, stop using the magic power
								zyk_cast_magic(ent, magic_skill_index);
							}
						}

						if (ent->client->pers.quest_npc == QUEST_NPC_MAGE_MASTER && Q_irand(0, 3) == 0)
						{
							Jedi_Cloak(ent);
						}
						else if (ent->client->pers.quest_npc == QUEST_NPC_MAGE_SCHOLAR && Q_irand(0, 99) < 10)
						{ // zyk: chance to use Melee, so he can use Magic Fist
							WP_FireMelee(ent, qfalse);
						}
						else if (ent->client->pers.quest_npc >= QUEST_NPC_ANGEL_OF_DEATH && ent->client->pers.quest_npc <= QUEST_NPC_CHIMERA &&
								level.reality_shift_timer < level.time)
						{
							if (Q_irand(0, 99) < REALITY_SHIFT_MODE_CHANCE)
							{
								level.reality_shift_mode = REALITY_SHIFT_NONE;

								trap->SendServerCommand(-1, va("chat \"^1Elemental Beasts: ^7Reality Shift - Normal\n\""));
							}
							else if (Q_irand(0, 99) < REALITY_SHIFT_MODE_CHANCE)
							{
								level.reality_shift_mode = REALITY_SHIFT_NO_FORCE;

								trap->SendServerCommand(-1, va("chat \"^1Elemental Beasts: ^7Reality Shift - No Force\n\""));
							}
							else if (Q_irand(0, 99) < REALITY_SHIFT_MODE_CHANCE)
							{
								level.reality_shift_mode = REALITY_SHIFT_LOWER_PHYSICAL_DAMAGE;

								trap->SendServerCommand(-1, va("chat \"^1Elemental Beasts: ^7Reality Shift - Lower Physical Damage\n\""));
							}
							else if (Q_irand(0, 99) < REALITY_SHIFT_MODE_CHANCE)
							{
								level.reality_shift_mode = REALITY_SHIFT_NO_MAGIC;

								trap->SendServerCommand(-1, va("chat \"^1Elemental Beasts: ^7Reality Shift - No Magic\n\""));
							}
							else if (Q_irand(0, 99) < REALITY_SHIFT_MODE_CHANCE)
							{
								level.reality_shift_mode = REALITY_SHIFT_LOW_GRAVITY;

								trap->SendServerCommand(-1, va("chat \"^1Elemental Beasts: ^7Reality Shift - Low Gravity\n\""));
							}
							else if (Q_irand(0, 99) < REALITY_SHIFT_MODE_CHANCE)
							{
								level.reality_shift_mode = REALITY_SHIFT_HIGH_GRAVITY;

								trap->SendServerCommand(-1, va("chat \"^1Elemental Beasts: ^7Reality Shift - High Gravity\n\""));
							}

							level.reality_shift_timer = level.time + Q_irand(5000, 12000);
						}

						// zyk: has an enemy. Reset the idle timer
						ent->client->pers.quest_npc_idle_timer = level.time + QUEST_NPC_IDLE_TIME;
					}
					else
					{
						if (ent->client->pers.quest_npc_idle_timer < level.time)
						{ // zyk: find another map spot
							vec3_t npc_origin, npc_angles;
							float x = 0, y = 0, z = 0;
							int npc_offset = 8;
							gentity_t* chosen_entity = NULL;

							chosen_entity = zyk_find_entity_for_quest();

							if (chosen_entity)
							{ // zyk: if for some reason there was no chosen entity, try again later
								x = Q_irand(0, npc_offset);
								y = Q_irand(0, npc_offset);
								z = Q_irand(0, npc_offset);

								if (Q_irand(0, 1) == 0)
								{
									x *= -1;
								}

								if (Q_irand(0, 1) == 0)
								{
									y *= -1;
								}

								if (chosen_entity->r.svFlags & SVF_USE_CURRENT_ORIGIN)
								{
									x += chosen_entity->r.currentOrigin[0];
									y += chosen_entity->r.currentOrigin[1];
									z += chosen_entity->r.currentOrigin[2];
								}
								else
								{
									x += chosen_entity->s.origin[0];
									y += chosen_entity->s.origin[1];
									z += chosen_entity->s.origin[2];
								}

								// zyk: avoiding telefrag
								if (zyk_there_is_player_or_npc_in_spot(x, y, z) == qfalse)
								{
									VectorSet(npc_origin, x, y, z);
									VectorSet(npc_angles, 0, 0, 0);
									zyk_TeleportPlayer(ent, npc_origin, npc_angles);
								}

								ent->client->pers.quest_npc_idle_timer = level.time + QUEST_NPC_IDLE_TIME;
							}
							else
							{
								ent->client->pers.quest_npc_idle_timer = level.time + 1000;
							}
						}
						else if (ent->client->pers.quest_power_status > 0)
						{ // zyk: stop using all magic
							zyk_stop_all_magic_powers(ent);
						}
					}
				}

				if (ent->client->pers.quest_npc == QUEST_NPC_SELLER && ent->client->pers.quest_seller_map_timer < level.time)
				{ // zyk: the time for the Seller to stay in the map run out. He will go away
					gentity_t* player_ent = NULL;
					int player_it = 0;

					for (player_it = 0; player_it < level.maxclients; player_it++)
					{
						player_ent = &g_entities[player_it];

						if (player_ent && player_ent->client && player_ent->client->sess.amrpgmode == 2 && 
							player_ent->client->pers.quest_seller_event_step > QUEST_SELLER_STEP_NONE && player_ent->client->pers.quest_seller_event_step < QUEST_SELLER_END_STEP)
						{
							player_ent->client->pers.quest_seller_event_step = QUEST_SELLER_STEP_NONE;

							trap->SendServerCommand(player_ent->s.number, va("chat \"%s^7: I must go away now. See you later!\n\"", QUESTCHAR_SELLER));
						}
					}

					if (ent->client->pers.quest_power_status > 0)
					{ // zyk: stop using all magic
						zyk_stop_all_magic_powers(ent);
					}

					ent->think = G_FreeEntity;
					ent->nextthink = level.time;
				}

				if (Q_stricmp(ent->NPC_type, "quest_mage") == 0 && ent->enemy)
				{
					int random_magic = Q_irand(0, MAGIC_LIGHT_MAGIC);
					int first_magic_skill = SKILL_MAGIC_HEALING_AREA;
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
						ent->client->pers.skill_levels[SKILL_MAX_MP] = zyk_max_skill_level(SKILL_MAX_MP) - 2;
						ent->client->pers.magic_power = zyk_max_magic_power(ent);
					}

					zyk_cast_magic(ent, first_magic_skill + random_magic);
				}
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
