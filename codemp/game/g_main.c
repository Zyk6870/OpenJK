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

	// zyk: initializing quest_map value
	level.quest_map = 0;
	level.custom_quest_map = -1;
	level.zyk_custom_quest_effect_id = -1;

	// zyk: initializing quest_note_id value
	level.quest_note_id = -1;
	level.universe_quest_note_id = -1;

	// zyk: initializing quest_effect_id value
	level.quest_effect_id = -1;

	level.chaos_portal_id = -1;

	// zyk: initializing bounty_quest_target_id value
	level.bounty_quest_target_id = 0;
	level.bounty_quest_choose_target = qtrue;

	// zyk: initializing guardian quest values
	level.guardian_quest = 0;
	level.initial_map_guardian_weapons = 0;

	level.boss_battle_music_reset_timer = 0;

	level.voting_player = -1;

	level.server_empty_change_map_timer = 0;
	level.num_fully_connected_clients = 0;

	level.guardian_quest_timer = 0;

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

		// zyk: initializing quest_crystal_id value
		for (zyk_iterator = 0; zyk_iterator < 3; zyk_iterator++)
		{
			level.quest_crystal_id[zyk_iterator] = -1;
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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "yavin1b", 8) == 0)
			level.quest_map = 1;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "yavin2", 7) == 0)
			level.quest_map = 10;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "hoth2", 6) == 0)
			level.quest_map = 5;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "hoth3", 6) == 0)
			level.quest_map = 20;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t1_danger", 10) == 0)
			level.quest_map = 18;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t1_fatal", 9) == 0)
			level.quest_map = 13;

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
		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t1_sour", 8) == 0)
			level.quest_map = 2;

		zyk_create_info_player_deathmatch(9828,-5521,153,90);
		zyk_create_info_player_deathmatch(9845,-5262,153,153);
	}
	else if (Q_stricmp(zyk_mapname, "t1_surprise") == 0)
	{
		int i = 0;
		gentity_t *ent;
		qboolean found_bugged_switch = qfalse;

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t1_surprise", 12) == 0)
			level.quest_map = 3;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t2_rogue", 9) == 0)
			level.quest_map = 7;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t2_trip", 8) == 0)
			level.quest_map = 17;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t3_hevil", 9) == 0)
			level.quest_map = 8;

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
		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t3_bounty", 10) == 0)
			level.quest_map = 6;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "t3_rift", 8) == 0)
			level.quest_map = 4;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "taspir1", 8) == 0)
			level.quest_map = 25;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "kor1", 5) == 0)
			level.quest_map = 9;

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
	else if (Q_stricmp(zyk_mapname, "mp/duel5") == 0 && g_gametype.integer == GT_FFA)
	{
		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "mp/duel5", 9) == 0)
			level.quest_map = 11;
	}
	else if (Q_stricmp(zyk_mapname, "mp/duel8") == 0 && g_gametype.integer == GT_FFA)
	{
		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "mp/duel8", 9) == 0)
			level.quest_map = 14;
	}
	else if (Q_stricmp(zyk_mapname, "mp/duel9") == 0 && g_gametype.integer == GT_FFA)
	{
		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "mp/duel9", 9) == 0)
			level.quest_map = 15;
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_korriban") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove some entities
		int i = 0;
		gentity_t *ent;

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "mp/siege_korriban", 18) == 0)
			level.quest_map = 12;

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

		// zyk: making case sensitive comparing so only low case quest map names will be set to play quests. This allows building these maps without conflicting with quests
		if (Q_strncmp(zyk_mapname, "mp/siege_desert", 16) == 0)
			level.quest_map = 24;

		for (i = 0; i < level.num_entities; i++)
		{
			ent = &g_entities[i];
			if (Q_stricmp( ent->targetname, "rebel_obj_2_doors") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->targetname, "shield") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "gatedestroy_doors") == 0)
			{
				G_FreeEntity( ent );
			}
			if (i >= 153 && i <= 160)
			{
				G_FreeEntity( ent );
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

	if (Q_stricmp(level.default_map_music, "") == 0)
	{ // zyk: if the default map music is empty (the map has no music) then set a default music
		if (level.quest_map == 1)
			strcpy(level.default_map_music,"music/yavin1/swamp_explore.mp3");
		else if (level.quest_map == 7)
			strcpy(level.default_map_music,"music/t2_rogue/narshaada_explore.mp3");
		else if (level.quest_map == 10)
			strcpy(level.default_map_music,"music/yavin2/yavtemp2_explore.mp3");
		else if (level.quest_map == 13)
			strcpy(level.default_map_music,"music/t1_fatal/tunnels_explore.mp3");
		else
			strcpy(level.default_map_music,"music/hoth2/hoth2_explore.mp3");
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
			if( (Q_stricmp( name, player->NPC_type ) == 0 || Q_stricmp( name, "all" ) == 0) && player->client->pers.guardian_invoked_by_id == -1)
			{ // zyk: do not kill guardians
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
	if (ent->client->pers.player_settings & (1 << 14)) // Custom
		trap->SetConfigstring( CS_MUSIC, "music/boss_custom.mp3" );
	else if (ent->client->pers.player_settings & (1 << 24)) // Korriban Action
		trap->SetConfigstring( CS_MUSIC, "music/kor_lite/korrib_action.mp3" );
	else if (ent->client->pers.player_settings & (1 << 25)) // MP Duel
		trap->SetConfigstring( CS_MUSIC, "music/mp/duel.mp3" );
	else // Hoth2 Action
		trap->SetConfigstring( CS_MUSIC, "music/hoth2/hoth2_action.mp3" );
}

// zyk: spawns a RPG quest boss and set his HP based in the quantity of allies the quest player has now
extern void clean_effect();
extern gentity_t *NPC_SpawnType( gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle ); // zyk: used in boss battles
extern void do_scale(gentity_t *ent, int new_size);
void spawn_boss(gentity_t *ent,int x,int y,int z,int yaw,char *boss_name,int gx,int gy,int gz,int gyaw,int guardian_mode)
{
	vec3_t player_origin;
	vec3_t player_yaw;
	vec3_t boss_spawn_point;
	gentity_t *npc_ent = NULL;
	int i = 0;
	float boss_bonus_hp = 0;

	// zyk: removing trip mines and detpacks near the boss to prevent 1-hit-kill exploits
	if ((guardian_mode >= 1 && guardian_mode <= 7) || guardian_mode == 11 || guardian_mode >= 14)
		VectorSet(boss_spawn_point, gx, gy, gz);
	else
		VectorSet(boss_spawn_point, x, y, z);

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		gentity_t *this_ent = &g_entities[i];

		if (this_ent && Q_stricmp(this_ent->classname, "laserTrap") == 0 && 
			Distance(this_ent->r.currentOrigin, boss_spawn_point) < 384.0)
		{
			this_ent->think = G_FreeEntity;
			this_ent->nextthink = level.time;
		}
		else if (this_ent && Q_stricmp(this_ent->classname, "detpack") == 0 &&
			Distance(this_ent->r.currentOrigin, boss_spawn_point) < 384.0)
		{
			this_ent->think = G_FreeEntity;
			this_ent->nextthink = level.time;
		}
	}

	// zyk: removing scale from allies and setting them with the guardian_mode value
	// zyk: this function will be called several times during some boss battles, so after the first call the quest player will already have guardian_mode set
	// zyk: in this case, dont reset allies, otherwise dead allies would be able to again heal players in boss battle and damage the boss
	if (ent->client->pers.guardian_mode == 0)
	{
		for (i = 0; i < level.maxclients; i++)
		{
			gentity_t *allied_player = &g_entities[i];

			if (zyk_is_ally(ent, allied_player) == qtrue && allied_player->client->sess.amrpgmode == 2 && allied_player->health > 0)
			{
				allied_player->client->pers.guardian_mode = guardian_mode;
				do_scale(allied_player, 100);
				allied_player->client->noclip = qfalse;
			}
		}
	}

	ent->client->pers.guardian_mode = guardian_mode;

	player_origin[0] = x;
	player_origin[1] = y;
	player_origin[2] = z;
	player_yaw[0] = 0;
	player_yaw[1] = yaw;
	player_yaw[2] = 0;

	// zyk: dont teleport player in these boss battles
	if (guardian_mode != 17 && guardian_mode != 18)
		zyk_TeleportPlayer(ent,player_origin,player_yaw);

	if ((guardian_mode >= 1 && guardian_mode <= 7) || guardian_mode == 11 || guardian_mode >= 14)
		npc_ent = Zyk_NPC_SpawnType(boss_name,gx,gy,gz,gyaw);
	else
		npc_ent = NPC_SpawnType(ent,boss_name,NULL,qfalse);

	if (ent->client->pers.universe_quest_counter & (1 << 29))
	{ // zyk: Challenge Mode increases boss hp more
		boss_bonus_hp = 1.1 * (1 + zyk_number_of_allies(ent, qtrue));
	}
	else
	{
		boss_bonus_hp = 0.25 * zyk_number_of_allies(ent, qtrue);
	}

	if (npc_ent)
	{
		npc_ent->NPC->stats.health += (npc_ent->NPC->stats.health * boss_bonus_hp);
		npc_ent->client->ps.stats[STAT_MAX_HEALTH] = npc_ent->NPC->stats.health;
		npc_ent->health = npc_ent->client->ps.stats[STAT_MAX_HEALTH];

		npc_ent->client->pers.guardian_invoked_by_id = ent-g_entities;
		npc_ent->client->pers.hunter_quest_messages = 0;
		npc_ent->client->pers.light_quest_messages = 0;
		npc_ent->client->pers.light_quest_timer = level.time + 7000;
		npc_ent->client->pers.guardian_timer = level.time + 5000;
		npc_ent->client->pers.universe_quest_timer = level.time + 11000;
		npc_ent->client->pers.guardian_mode = guardian_mode;

		if (guardian_mode == 15 || guardian_mode == 21)
		{ // zyk: Ymir and Thor and Soul of Sorrow can have shield
			npc_ent->client->ps.stats[STAT_ARMOR] = npc_ent->health;
		}

		if (ent->client->pers.universe_quest_counter & (1 << 29))
		{ // zyk: if quest player is in Challenge Mode, bosses always use the improved version of powers (Universe Power)
			npc_ent->client->pers.quest_power_status |= (1 << 13);

			// zyk: they also start using magic a bit earlier
			npc_ent->client->pers.light_quest_timer = level.time + 5000;
			npc_ent->client->pers.guardian_timer = level.time + 3000;
			npc_ent->client->pers.universe_quest_timer = level.time + 9000;
		}
	}

	if (guardian_mode != 14)
	{
		clean_effect();
		zyk_NPC_Kill_f("all");
	}

	// zyk: boss battle music
	zyk_start_boss_battle_music(ent);

	// zyyk: removing noclip from the player
	ent->client->noclip = qfalse;
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

		if (Q_stricmp(targetname, "zyk_quest_effect_drain") == 0)
			G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/arc_lp.wav"));

		if (Q_stricmp(targetname, "zyk_quest_effect_sand") == 0)
			ent->client->pers.quest_power_effect1_id = new_ent->s.number;
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

// zyk: if this player or npc has immunity power, returns qtrue and shows Immunity effect
qboolean zyk_check_immunity_power(gentity_t *ent)
{
	if (ent && ent->client && ent->client->pers.quest_power_status & (1 << 0))
	{
		if (ent->client->pers.immunity_power_effect_cooldown < level.time)
		{
			zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_immunity", "0", "scepter/invincibility", 0, 0, 0, 300);

			ent->client->pers.immunity_power_effect_cooldown = level.time + 500;
		}

		return qtrue;
	}

	return qfalse;
}

qboolean zyk_can_hit_boss_battle_target(gentity_t *attacker, gentity_t *target)
{
	if (attacker->client->pers.guardian_mode == target->client->pers.guardian_mode ||
		(attacker->NPC && attacker->client->pers.guardian_mode == 0) ||
		(!attacker->NPC && attacker->client->pers.guardian_mode > 0 && target->NPC))
	{
		return qtrue;
	}

	return qfalse;
}

// zyk: tests if the target player can be hit by the attacker gun/saber damage, force power or special power
qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target)
{
	if (attacker && attacker->client && target && target->client && !attacker->NPC && !target->NPC)
	{ // zyk: in a boss battle, non-quest players cannot hit quest players and vice-versa
		if (attacker->client->sess.amrpgmode == 2 && attacker->client->pers.guardian_mode > 0 && 
			(target->client->sess.amrpgmode != 2 || target->client->pers.guardian_mode == 0))
		{
			return qfalse;
		}

		if (target->client->sess.amrpgmode == 2 && target->client->pers.guardian_mode > 0 && 
			(attacker->client->sess.amrpgmode != 2 || attacker->client->pers.guardian_mode == 0))
		{
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

qboolean zyk_unique_ability_can_hit_target(gentity_t *attacker, gentity_t *target)
{
	int i = target->s.number;

	if (attacker && target && attacker->s.number != i && target->client && target->health > 0 && zyk_can_hit_target(attacker, target) == qtrue &&
		(i > MAX_CLIENTS || (target->client->pers.connected == CON_CONNECTED && target->client->sess.sessionTeam != TEAM_SPECTATOR &&
			target->client->ps.duelInProgress == qfalse)))
	{ // zyk: target is a player or npc that can be hit by the attacker
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

		if (is_ally == 0 &&
			zyk_can_hit_boss_battle_target(attacker, target))
		{ // zyk: players in bosses can only hit bosses and their helper npcs. Players not in boss battles
		  // can only hit normal enemy npcs and npcs spawned by bosses but not the bosses themselves. Unique-using npcs can hit everyone that are not their allies
			return qtrue;
		}
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

			if (is_ally == 0 && !(zyk_check_immunity_power(target)) && 
				zyk_can_hit_boss_battle_target(attacker, target))
			{ // zyk: Cannot hit target with Immunity Power. Players in bosses can only hit bosses and their helper npcs. Players not in boss battles
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

// zyk: Earthquake
void earthquake(gentity_t *ent, int stun_time, int strength, int distance)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
		distance += (distance/2);

	for ( i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			if (player_ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
			{ // zyk: player can only be hit if he is on floor
				// zyk: if using Meditate taunt, remove it
				if (player_ent->client->ps.legsAnim == BOTH_MEDITATE && player_ent->client->ps.torsoAnim == BOTH_MEDITATE)
				{
					player_ent->client->ps.legsAnim = player_ent->client->ps.torsoAnim = BOTH_MEDITATE_END;
				}

				player_ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
				player_ent->client->ps.forceHandExtendTime = level.time + stun_time;
				player_ent->client->ps.velocity[2] += strength;
				player_ent->client->ps.forceDodgeAnim = 0;
				player_ent->client->ps.quickerGetup = qtrue;

				G_Damage(player_ent,ent,ent,NULL,NULL,strength/5,0,MOD_UNKNOWN);
			}

			if (i < level.maxclients)
			{
				G_ScreenShake(player_ent->client->ps.origin, player_ent,  10.0f, 4000, qtrue);
			}
			
			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/stone_break1.mp3"));
		}
	}
}

// zyk: Flame Burst
void flame_burst(gentity_t *ent, int duration)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 3000;
	}

	ent->client->pers.flame_thrower = level.time + duration;
	ent->client->pers.quest_power_status |= (1 << 12);
}

// zyk: Blowing Wind
void blowing_wind(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += 200;
	}

	ent->client->pers.quest_debounce1_timer = 0;

	for ( i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_user3_id = ent->s.number;
			player_ent->client->pers.quest_power_status |= (1 << 8);
			player_ent->client->pers.quest_target6_timer = level.time + duration;

			// zyk: gives fall kill to the owner of this power
			player_ent->client->ps.otherKiller = ent->s.number;
			player_ent->client->ps.otherKillerTime = level.time + duration;
			player_ent->client->ps.otherKillerDebounceTime = level.time + 100;
							
			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/vacuum.mp3"));
		}
	}
}

// zyk: Reverse Wind
void reverse_wind(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += 200;
	}

	ent->client->pers.quest_debounce1_timer = 0;

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_user3_id = ent->s.number;
			player_ent->client->pers.quest_power_status |= (1 << 20);
			player_ent->client->pers.quest_target6_timer = level.time + duration;

			// zyk: gives fall kill to the owner of this power
			player_ent->client->ps.otherKiller = ent->s.number;
			player_ent->client->ps.otherKillerTime = level.time + duration;
			player_ent->client->ps.otherKillerDebounceTime = level.time + 100;

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/vacuum.mp3"));
		}
	}
}

// zyk: Poison Mushrooms
void poison_mushrooms(gentity_t *ent, int min_distance, int max_distance)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
		min_distance = 0;

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, min_distance, max_distance, qfalse, &targets_hit) == qtrue && 
			(i < MAX_CLIENTS || player_ent->client->NPC_class != CLASS_VEHICLE))
		{
			player_ent->client->pers.quest_power_user2_id = ent->s.number;
			player_ent->client->pers.quest_power_status |= (1 << 4);
			player_ent->client->pers.quest_target3_timer = level.time + 200;
			player_ent->client->pers.quest_power_hit_counter = 40;

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/air_burst.mp3"));
		}
	}
}

// zyk: Chaos Power
void chaos_power(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];
					
		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_user1_id = ent->s.number;
			player_ent->client->pers.quest_power_status |= (1 << 1);
			player_ent->client->pers.quest_power_hit3_counter = duration/200;
			player_ent->client->pers.quest_target1_timer = level.time + 200;

			// zyk: removing emotes to prevent exploits
			if (player_ent->client->pers.player_statuses & (1 << 1))
			{
				player_ent->client->pers.player_statuses &= ~(1 << 1);
				player_ent->client->ps.forceHandExtendTime = level.time;
			}

			// zyk: if using Meditate taunt, remove it
			if (player_ent->client->ps.legsAnim == BOTH_MEDITATE && player_ent->client->ps.torsoAnim == BOTH_MEDITATE)
			{
				player_ent->client->ps.legsAnim = player_ent->client->ps.torsoAnim = BOTH_MEDITATE_END;
			}

			if (player_ent->client->jetPackOn)
			{
				Jetpack_Off(player_ent);
			}

			// zyk:  setting anim
			player_ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
			player_ent->client->ps.forceDodgeAnim = BOTH_SONICPAIN_END;
			player_ent->client->ps.forceHandExtendTime = level.time + duration;
			player_ent->client->ps.electrifyTime = level.time + duration;

			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_chaos", "0", "ships/heavydmg", 0, 0, 0, 1000);
		}
	}
}

// zyk: Magic Sense
void magic_sense(gentity_t *ent, int duration)
{
	if (ent->client->pers.quest_power_status & (1 << 13))
	{ // zyk: Universe Power
		duration += 1000;
	}

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 8 &&
		ent->client->pers.unique_skill_duration > level.time && !(ent->client->pers.player_statuses & (1 << 21)) &&
		!(ent->client->pers.player_statuses & (1 << 22)) && !(ent->client->pers.player_statuses & (1 << 23)))
	{ // zyk: Magic Master Unique Skill
		duration += 1000;
	}

	// zyk: Magic Sense gets more duration based on Sense skill level
	duration += (ent->client->pers.skill_levels[4] * 1000);

	// zyk: Magic Sense gets more duration based on Improvements skill level
	duration += (ent->client->pers.skill_levels[55] * 1000);

	ent->client->ps.forceAllowDeactivateTime = level.time + duration;
	ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[4];
	ent->client->ps.fd.forcePowersActive |= (1 << FP_SEE);
	ent->client->ps.fd.forcePowerDuration[FP_SEE] = level.time + duration;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/weapons/force/see.wav"));
}

// zyk: Lightning Dome
extern void zyk_lightning_dome_detonate( gentity_t *ent );
void lightning_dome(gentity_t *ent, int damage)
{
	gentity_t *missile;
	vec3_t origin;
	trace_t	tr;

	VectorSet(origin, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2] - 22);

	trap->Trace( &tr, ent->client->ps.origin, NULL, NULL, origin, ent->s.number, MASK_SHOT, qfalse, 0, 0);

	missile = G_Spawn();

	G_SetOrigin(missile, origin);
	//In SP the impact actually travels as a missile based on the trace fraction, but we're
	//just going to be instant. -rww

	VectorCopy( tr.plane.normal, missile->pos1 );

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 3) // zyk: Armored Soldier Lightning Shield has less radius
		missile->count = 6;
	else
		missile->count = 9;

	missile->classname = "demp2_alt_proj";
	missile->s.weapon = WP_DEMP2;

	missile->think = zyk_lightning_dome_detonate;
	missile->nextthink = level.time;

	// zyk: damage is level based
	damage = (int)ceil(damage * (0.5 + ((ent->client->pers.level * 1.0) / 200.0)));

	// zyk: Magic Master Unique Skill increases damage
	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 8 && ent->client->pers.unique_skill_duration > level.time &&
		!(ent->client->pers.player_statuses & (1 << 21)) && !(ent->client->pers.player_statuses & (1 << 22)) && 
		!(ent->client->pers.player_statuses & (1 << 23)))
	{
		damage *= 2;
	}

	missile->splashDamage = missile->damage = damage;
	missile->splashMethodOfDeath = missile->methodOfDeath = MOD_DEMP2;

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 3) // zyk: Armored Soldier Lightning Shield has less radius
		missile->splashRadius = 512;
	else
		missile->splashRadius = 768;

	missile->r.ownerNum = ent->s.number;

	missile->dflags = DAMAGE_DEATH_KNOCKBACK;
	missile->clipmask = MASK_SHOT | CONTENTS_LIGHTSABER;

	// we don't want it to ever bounce
	missile->bounceCount = 0;

	if (ent->s.number < level.maxclients)
		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/thunder_close1.mp3"));
}

// zyk: Ultra Strength. Increases damage and resistance to damage
void ultra_strength(gentity_t *ent, int duration)
{
	ent->client->pers.quest_power_status |= (1 << 3);
	ent->client->pers.quest_power2_timer = level.time + duration;

	if (ent->s.number < level.maxclients)
		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/ambience/thunder1.mp3"));
}

// zyk: Ultra Resistance. Increases resistance to damage
void ultra_resistance(gentity_t *ent, int duration)
{
	ent->client->pers.quest_power_status |= (1 << 7);
	ent->client->pers.quest_power3_timer = level.time + duration;

	if (ent->s.number < level.maxclients)
		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/enlightenment.mp3"));
}

// zyk: Immunity Power. Becomes immune against other special powers
void immunity_power(gentity_t *ent, int duration)
{
	ent->client->pers.quest_power_status |= (1 << 0);
	ent->client->pers.quest_power1_timer = level.time + duration;

	ent->client->pers.immunity_power_effect_cooldown = level.time + 500;

	zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_immunity", "0", "scepter/invincibility", 0, 0, 0, 300);

	if (ent->s.number < level.maxclients)
		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/boon.mp3"));
}

// zyk: Enemy Weakening
void enemy_nerf(gentity_t *ent, int distance)
{
	int i = 0;
	int targets_hit = 0;
	int duration = 12000;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 4000;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_target7_timer = level.time + duration;
			player_ent->client->pers.quest_power_status |= (1 << 21);

			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_enemy_nerf", "0", "force/kothos_beam", 0, 0, 0, 1000);

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh10.mp3"));
		}
	}
}

// zyk: used by Duelist Vertical DFA ability
void zyk_vertical_dfa_effect(gentity_t *ent)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "fx_runner");
	zyk_set_entity_field(new_ent, "spawnflags", "4");
	zyk_set_entity_field(new_ent, "targetname", "zyk_vertical_dfa");

	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->r.currentOrigin[0], (int)ent->r.currentOrigin[1], (int)ent->r.currentOrigin[2] - 20));

	new_ent->s.modelindex = G_EffectIndex("ships/proton_impact");

	zyk_spawn_entity(new_ent);

	new_ent->splashDamage = 130;

	new_ent->splashRadius = 600;

	level.special_power_effects[new_ent->s.number] = ent->s.number;
	level.special_power_effects_timer[new_ent->s.number] = level.time + 600;
}

void zyk_bomb_model_think(gentity_t *ent)
{
	// zyk: bomb timer seconds to explode. Each call to this function decrease counter until it reaches 0
	ent->count--;

	if (ent->count == 0)
	{ // zyk: explodes the bomb
		zyk_quest_effect_spawn(ent->parent, ent, "zyk_timed_bomb_explosion", "4", "explosions/hugeexplosion1", 0, 480, 430, 800);

		ent->think = G_FreeEntity;
		ent->nextthink = level.time + 500;
	}
	else
	{
		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/mpalarm.wav"));
		ent->nextthink = level.time + 1000;
	}
}

void zyk_add_bomb_model(gentity_t *ent)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "0");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->r.currentOrigin[0], (int)ent->r.currentOrigin[1], (int)ent->r.currentOrigin[2] - 20));

	zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/bomb_new_deact.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_timed_bomb");

	zyk_set_entity_field(new_ent, "count", "3");

	new_ent->parent = ent;
	new_ent->think = zyk_bomb_model_think;
	new_ent->nextthink = level.time + 1000;

	zyk_spawn_entity(new_ent);

	ent->wait = level.time + 5000;

	G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/cloth1.mp3"));
}

void zyk_ice_bomb_ice_think(gentity_t *ent)
{
	ent->nextthink = level.time + 100;

	if (ent->parent && ent->parent->client && ent->parent->client->sess.amrpgmode == 2 && ent->parent->client->pers.rpg_class == 2 && 
		ent->parent->client->pers.ice_bomb_counter == 3)
	{ // zyk: keeps hitting enemies until time out
		if (ent->wait < level.time)
		{
			ent->think = G_FreeEntity;
		}
		else
		{
			int i = 0;

			for (i = 0; i < level.num_entities; i++)
			{
				gentity_t *player_ent = &g_entities[i];

				if (player_ent && player_ent->client && player_ent->health > 0 &&
					(ent->parent->client->pers.guardian_mode == player_ent->client->pers.guardian_mode ||
					   ((ent->parent->client->pers.guardian_mode == 12 || ent->parent->client->pers.guardian_mode == 13) && player_ent->NPC &&
						(Q_stricmp(player_ent->NPC_type, "guardian_of_universe") || Q_stricmp(player_ent->NPC_type, "quest_reborn") ||
							Q_stricmp(player_ent->NPC_type, "quest_reborn_blue") || Q_stricmp(player_ent->NPC_type, "quest_reborn_red") ||
							Q_stricmp(player_ent->NPC_type, "quest_reborn_boss") || Q_stricmp(player_ent->NPC_type, "quest_mage"))
						)
					) && 
					zyk_can_hit_target(ent->parent, player_ent) == qtrue &&
					(i > MAX_CLIENTS || 
					 (player_ent->client->pers.connected == CON_CONNECTED && player_ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
					  player_ent->client->ps.duelInProgress == qfalse)
					) &&
					Distance(ent->s.origin, player_ent->client->ps.origin) < 90 && 
					!(player_ent->client->pers.player_statuses & (1 << 24)))
				{ // zyk: hit by the ice, make him trapped in the ice
					player_ent->client->pers.stun_baton_less_speed_timer = ent->wait;

					player_ent->client->pers.player_statuses |= (1 << 24);

					G_Damage(player_ent, ent->parent, ent->parent, NULL, NULL, 20, 0, MOD_UNKNOWN);

					G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/glass_tumble3.wav"));
				}
			}
		}
	}
}

void zyk_spawn_ice_bomb_ice(gentity_t *ent, int x_offset, int y_offset)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "0");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->r.currentOrigin[0] + x_offset, (int)ent->r.currentOrigin[1] + y_offset, (int)ent->r.currentOrigin[2]));

	zyk_set_entity_field(new_ent, "angles", "-89 0 0");

	zyk_set_entity_field(new_ent, "model", "models/map_objects/rift/crystal_wall.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_ice_bomb_ice");

	new_ent->parent = ent->parent;
	new_ent->think = zyk_ice_bomb_ice_think;
	new_ent->nextthink = level.time + 100;

	zyk_spawn_entity(new_ent);

	// zyk: ice duration
	new_ent->wait = level.time + 4000;
}

void zyk_ice_bomb_think(gentity_t *ent)
{
	ent->nextthink = level.time + 100;

	if (ent->parent && ent->parent->client && ent->parent->client->sess.amrpgmode == 2 && ent->parent->client->pers.rpg_class == 2 && 
		(ent->parent->client->pers.ice_bomb_counter == 2 || ent->parent->client->pers.unique_skill_duration < level.time))
	{ // zyk: Bounty Hunter detonated the bomb or unique duration run out. Explodes the bomb
		ent->parent->client->pers.ice_bomb_counter = 3;

		zyk_spawn_ice_bomb_ice(ent, 0, 0);
		zyk_spawn_ice_bomb_ice(ent, 0, -80);
		zyk_spawn_ice_bomb_ice(ent, 0, 80);
		zyk_spawn_ice_bomb_ice(ent, -80, 0);
		zyk_spawn_ice_bomb_ice(ent, -80, -80);
		zyk_spawn_ice_bomb_ice(ent, -80, 80);
		zyk_spawn_ice_bomb_ice(ent, 80, 0);
		zyk_spawn_ice_bomb_ice(ent, 80, -80);
		zyk_spawn_ice_bomb_ice(ent, 80, 80);

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/glass_tumble3.wav"));

		ent->think = G_FreeEntity;
	}
}

// zyk: Bounty Hunter Ice Bomb
void zyk_ice_bomb(gentity_t *ent)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "0");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->r.currentOrigin[0], (int)ent->r.currentOrigin[1], (int)ent->r.currentOrigin[2] - 22));

	zyk_set_entity_field(new_ent, "model", "models/map_objects/imperial/cargo_sm.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_ice_bomb");

	new_ent->parent = ent;
	new_ent->think = zyk_ice_bomb_think;
	new_ent->nextthink = level.time + 100;

	zyk_spawn_entity(new_ent);

	G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/cloth1.mp3"));
}

void zyk_spawn_ice_element(gentity_t *ent, gentity_t *player_ent)
{
	int i = 0;
	int initial_angle = -179;

	for (i = 0; i < 4; i++)
	{
		gentity_t *new_ent = G_Spawn();

		zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
		zyk_set_entity_field(new_ent, "spawnflags", "65537");
		zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)player_ent->r.currentOrigin[0], (int)player_ent->r.currentOrigin[1], (int)player_ent->r.currentOrigin[2]));

		zyk_set_entity_field(new_ent, "angles", va("0 %d 0", initial_angle + (i * 89)));

		zyk_set_entity_field(new_ent, "mins", "-70 -70 -70");
		zyk_set_entity_field(new_ent, "maxs", "70 70 70");

		zyk_set_entity_field(new_ent, "model", "models/map_objects/rift/crystal_wall.md3");

		zyk_set_entity_field(new_ent, "targetname", "zyk_elemental_ice");

		zyk_spawn_entity(new_ent);

		level.special_power_effects[new_ent->s.number] = ent->s.number;
		level.special_power_effects_timer[new_ent->s.number] = level.time + 4000;
	}
}

// zyk: Elemental Attack
void elemental_attack(gentity_t *ent)
{
	int i = 0;
	int targets_hit = 0;
	int min_distance = 100;
	int damage = 16;

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, min_distance, 500, qfalse, &targets_hit) == qtrue)
		{
			// zyk: first element, Ice
			zyk_spawn_ice_element(ent, player_ent);

			// zyk: second element, Fire. Multiplies by 2.5 because of the damage reduction in G_Damage()
			zyk_quest_effect_spawn(ent, player_ent, "zyk_elemental_fire", "4", "env/flame_jet", 1000, 2.5 * damage, 35, 2500);

			// zyk: third element, Earth. Multiplies by 2.5 because of the damage reduction in G_Damage()
			zyk_quest_effect_spawn(ent, player_ent, "zyk_elemental_earth", "4", "env/rock_smash", 2500, 2.5 * damage, 35, 4000);

			// zyk: fourth element, Wind
			player_ent->client->pers.quest_power_status |= (1 << 5);
			player_ent->client->pers.quest_power_hit_counter = -179;
			player_ent->client->pers.quest_target4_timer = level.time + 7000;

			// zyk: target is hit by the Ice element for 4 seconds
			player_ent->client->pers.quest_power_status |= (1 << 26);
			player_ent->client->pers.quest_target11_timer = level.time + 4000;

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/glass_tumble3.wav"));
		}
	}
}

// zyk: No Attack ability
void zyk_no_attack(gentity_t *ent)
{
	int i = 0;

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (player_ent && player_ent->client && ent != player_ent &&
			zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue &&
			Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 300)
		{
			G_Damage(player_ent, ent, ent, NULL, NULL, 15, 0, MOD_UNKNOWN);

			player_ent->client->ps.weaponTime = 3000;
			player_ent->client->ps.electrifyTime = level.time + 3000;
			player_ent->client->pers.no_attack_timer = level.time + 3000;

			if (player_ent->client->ps.weaponstate == WEAPON_CHARGING ||
				player_ent->client->ps.weaponstate == WEAPON_CHARGING_ALT)
			{
				player_ent->client->ps.weaponstate = WEAPON_READY;
			}
		}
	}

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/hologram_off.mp3"));
}

// zyk: Super Beam ability
void zyk_super_beam(gentity_t *ent, int angle_yaw)
{
	gentity_t *new_ent = G_Spawn();

	if (angle_yaw == 0)
		angle_yaw = 1;

	zyk_set_entity_field(new_ent, "classname", "fx_runner");
	zyk_set_entity_field(new_ent, "spawnflags", "0");
	zyk_set_entity_field(new_ent, "targetname", "zyk_super_beam");

	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->client->ps.origin[0], (int)ent->client->ps.origin[1], ((int)ent->client->ps.origin[2] + ent->client->ps.viewheight)));
	
	zyk_set_entity_field(new_ent, "angles", va("%d %d 0", (int)ent->client->ps.viewangles[0], angle_yaw));

	new_ent->s.modelindex = G_EffectIndex("env/hevil_bolt");

	new_ent->parent = ent;

	zyk_spawn_entity(new_ent);

	level.special_power_effects[new_ent->s.number] = ent->s.number;
	level.special_power_effects_timer[new_ent->s.number] = level.time + 2000;
}

// zyk: Force Storm ability
extern void Boba_FlyStop(gentity_t *self);
extern void Jedi_Decloak(gentity_t *self);
void zyk_force_storm(gentity_t *ent)
{
	int i = 0;

	zyk_quest_effect_spawn(ent, ent, "zyk_force_storm", "4", "env/huge_lightning", 0, 20, 120, 3000);

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (player_ent && player_ent->client && ent != player_ent &&
			zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue &&
			Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 380)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_force_storm", "4", "env/huge_lightning", 0, 20, 120, 3000);

			// zyk: decrease enemy movement speed
			player_ent->client->pers.stun_baton_less_speed_timer = level.time + 2500;

			if (!player_ent->NPC)
			{ //disable jetpack temporarily
				if (player_ent->client->jetPackOn)
					Jetpack_Off(player_ent);
				player_ent->client->jetPackToggleTime = level.time + 5000;
			}
			else if (player_ent->NPC && player_ent->client->NPC_class == CLASS_BOBAFETT)
			{ // zyk: also disables npc jetpack
				Boba_FlyStop(player_ent);
			}

			if (player_ent->client->ps.powerups[PW_CLOAKED])
			{ // zyk: disables cloak of enemies
				Jedi_Decloak(player_ent);
			}
		}
	}
}

// zyk: Force Scream ability
void force_scream(gentity_t *ent)
{
	zyk_quest_effect_spawn(ent, ent, "zyk_effect_scream", "4", "howler/sonic", 0, 18, 300, 6000);

	ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
	ent->client->ps.forceDodgeAnim = BOTH_FORCE_RAGE;
	ent->client->ps.forceHandExtendTime = level.time + 4500;

	G_Sound(ent, CHAN_VOICE, G_SoundIndex("sound/chars/howler/howl.mp3"));
}

void zyk_force_dash_effect(gentity_t *ent)
{
	zyk_quest_effect_spawn(ent, ent, "zyk_effect_force_dash", "0", "force/rage2", 0, 0, 0, 200);
}

// zyk: Fast Dash ability
void zyk_force_dash(gentity_t *ent)
{
	G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_FORCELONGLEAP_START, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh9.mp3"));

	ent->client->pers.fast_dash_timer = 0;
}

// zyk: Healing Water
void healing_water(gentity_t *ent, int heal_amount)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
		heal_amount += 30;

	if ((ent->health + heal_amount) < ent->client->ps.stats[STAT_MAX_HEALTH])
		ent->health += heal_amount;
	else
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

	G_Sound( ent, CHAN_ITEM, G_SoundIndex("sound/weapons/force/heal.wav") );
}

// zyk: Sleeping Flowers
void sleeping_flowers(gentity_t *ent, int stun_time, int distance)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += 100;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			// zyk: removing emotes to prevent exploits
			if (player_ent->client->pers.player_statuses & (1 << 1))
			{
				player_ent->client->pers.player_statuses &= ~(1 << 1);
				player_ent->client->ps.forceHandExtendTime = level.time;
			}

			// zyk: if using Meditate taunt, remove it
			if (player_ent->client->ps.legsAnim == BOTH_MEDITATE && player_ent->client->ps.torsoAnim == BOTH_MEDITATE)
			{
				player_ent->client->ps.legsAnim = player_ent->client->ps.torsoAnim = BOTH_MEDITATE_END;
			}

			player_ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
			player_ent->client->ps.forceHandExtendTime = level.time + stun_time;
			player_ent->client->ps.velocity[2] += 150;
			player_ent->client->ps.forceDodgeAnim = 0;
			player_ent->client->ps.quickerGetup = qtrue;

			player_ent->client->pers.quest_power_status |= (1 << 24);
			player_ent->client->pers.quest_target9_timer = level.time + stun_time;

			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_sleeping", "0", "force/heal2", 0, 0, 0, 800);

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/air_burst.mp3"));
		}
	}
}

// zyk: Water Attack
void water_attack(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		damage += 10;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_acid", "4", "env/water_impact", 200, damage, 40, 9000);
		}
	}
}

// zyk Shifting Sand
void shifting_sand(gentity_t *ent, int distance)
{
	int time_to_teleport = 1800;
	int i = 0;
	int targets_hit = 0;
	int min_distance = distance;
	int enemy_dist = 0;
	gentity_t *this_enemy = NULL;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance *= 1.5;
		min_distance = distance;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{ // zyk: teleport to the nearest enemy
			enemy_dist = Distance(ent->client->ps.origin, player_ent->client->ps.origin);

			if (enemy_dist < min_distance)
			{
				min_distance = enemy_dist;
				this_enemy = player_ent;
			}
		}
	}

	if (this_enemy)
	{ // zyk: found an enemy
		ent->client->pers.quest_power_status |= (1 << 17);

		ent->client->pers.quest_power_user4_id = this_enemy->s.number;

		// zyk: used to bring the player back if he gets stuck
		VectorCopy(ent->client->ps.origin, ent->client->pers.teleport_angles);
	}

	ent->client->pers.quest_power5_timer = level.time + time_to_teleport;
	zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_sand", "0", "env/sand_spray", 0, 0, 0, time_to_teleport);
}

// zyk: Time Power
extern void display_yellow_bar(gentity_t *ent, int duration);
void time_power(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];
					
		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_status |= (1 << 2);
			player_ent->client->pers.quest_target2_timer = level.time + duration;

			if (i < MAX_CLIENTS)
			{ // zyk: player hit by this power
				if (player_ent->client->pers.quest_power_usage_timer < level.time)
				{
					player_ent->client->pers.quest_power_usage_timer = level.time + duration;
				}
				else
				{ // zyk: already used a power, so increase the cooldown time
					player_ent->client->pers.quest_power_usage_timer += duration;
				}

				display_yellow_bar(player_ent, (player_ent->client->pers.quest_power_usage_timer - level.time));
			}
			else if (player_ent->NPC)
			{ // zyk: npc or boss must also not be able to use magic
				player_ent->client->pers.light_quest_timer += duration;
				player_ent->client->pers.guardian_timer += duration;
				player_ent->client->pers.universe_quest_timer += duration;
			}

			player_ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
			player_ent->client->ps.forceDodgeAnim = player_ent->client->ps.torsoAnim;
			player_ent->client->ps.forceHandExtendTime = level.time + duration;

			// zyk: disabled force powers
			player_ent->client->ps.fd.forceDeactivateAll = 1;

			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_time", "0", "misc/genrings", 0, 0, 0, duration);

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/electric_beam_lp.wav"));
		}
	}
}

// zyk: Water Splash. Damages the targets and heals the user
void water_splash(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		damage += 5;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_watersplash", "4", "world/waterfall3", 0, damage, 200, 2500);

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/ambience/yavin/waterfall_medium_lp.wav"));
		}
	}
}

// zyk: Rockfall
void rock_fall(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += (distance/2);
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qtrue, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_rockfall", "4", "env/rockfall_noshake", 0, damage, 100, 8000);
		}
	}
}

// zyk: Dome of Damage
void dome_of_damage(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += 100;
		zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_dome", "4", "env/dome", 1000, damage, 290, 8000);
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qtrue, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_dome", "4", "env/dome", 1000, damage, 290, 8000);
		}
	}
}

// zyk: Magic Shield
void magic_shield(gentity_t *ent, int duration)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 1500;
	}

	ent->client->pers.quest_power_status |= (1 << 11);
	ent->client->pers.quest_power4_timer = level.time + duration;
	ent->client->invulnerableTimer = level.time + duration;
}

// zyk: Tree of Life
void tree_of_life(gentity_t *ent)
{
	ent->client->pers.quest_power_status |= (1 << 19);
	ent->client->pers.quest_power6_timer = level.time;
	ent->client->pers.quest_power_hit2_counter = 4;

	zyk_quest_effect_spawn(ent, ent, "zyk_tree_of_life", "1", "models/map_objects/yavin/tree10_b.md3", 0, 0, 0, 4000);
}

// zyk: Magic Disable
void magic_disable(gentity_t *ent, int distance)
{
	int i = 0;
	int targets_hit = 0;
	int duration = 6000;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 2000;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_magic_disable", "0", "env/small_electricity2", 0, 0, 0, 1500);

			if (i < MAX_CLIENTS)
			{ // zyk: player hit by this power
				if (player_ent->client->pers.quest_power_usage_timer < level.time)
				{
					player_ent->client->pers.quest_power_usage_timer = level.time + duration;
				}
				else
				{ // zyk: already used a power, so increase the cooldown time
					player_ent->client->pers.quest_power_usage_timer += duration;
				}

				display_yellow_bar(player_ent, (player_ent->client->pers.quest_power_usage_timer - level.time));
			}
			else
			{ // zyk: npc or boss dont get affected that much
				player_ent->client->pers.light_quest_timer += (duration/2);
				player_ent->client->pers.guardian_timer += (duration/2);
				player_ent->client->pers.universe_quest_timer += (duration/2);
			}

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh10.mp3"));
		}
	}
}

// zyk: Ice Stalagmite
void ice_stalagmite(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;
	int min_distance = 50;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		damage += 30;
		min_distance = 0;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, min_distance, distance, qfalse, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_ice_stalagmite", "0", "models/map_objects/hoth/stalagmite_small.md3", 0, 0, 0, 2000);

			G_Damage(player_ent,ent,ent,NULL,player_ent->client->ps.origin,damage,DAMAGE_NO_PROTECTION,MOD_UNKNOWN);
		}
	}
}

// zyk: Ice Boulder
void ice_boulder(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += 50;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 50, distance, qfalse, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_ice_boulder", "1", "models/map_objects/hoth/rock_b.md3", 0, 20, 50, 4000);

			G_Damage(player_ent,ent,ent,NULL,player_ent->client->ps.origin,damage,DAMAGE_NO_PROTECTION,MOD_UNKNOWN);

			player_ent->client->pers.quest_power_status |= (1 << 25);
			player_ent->client->pers.quest_target10_timer = level.time + 4000;
		}
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

// zyk: Ice Block
void ice_block(gentity_t *ent, int duration)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 1000;
	}

	zyk_spawn_ice_block(ent, duration, 0, 0, -140, 0, 0);
	zyk_spawn_ice_block(ent, duration, 0, 90, 140, 0, 0);
	zyk_spawn_ice_block(ent, duration, 0, 179, 0, -140, 0);
	zyk_spawn_ice_block(ent, duration, 0, -90, 0, 140, 0);
	zyk_spawn_ice_block(ent, duration, 90, 0, 0, 0, -140);
	zyk_spawn_ice_block(ent, duration, -90, 0, 0, 0, 140);

	ent->client->pers.quest_power_status |= (1 << 22);
	ent->client->pers.quest_power7_timer = level.time + duration;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/glass_tumble3.wav"));
}

// zyk: Ultra Drain
void ultra_drain(gentity_t *ent, int radius, int damage, int duration)
{
	zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_drain", "4", "misc/possession", 1000, damage, radius, duration);
}

// zyk: Magic Explosion
void magic_explosion(gentity_t *ent, int radius, int damage, int duration)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_explosion", "4", "explosions/hugeexplosion1", 1500, damage, radius, duration + 1000);
	}

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 8 && ent->client->pers.unique_skill_duration > level.time &&
		!(ent->client->pers.player_statuses & (1 << 21)) && !(ent->client->pers.player_statuses & (1 << 22)) && 
		!(ent->client->pers.player_statuses & (1 << 23)))
	{ // zyk: Magic Master Unique Skill increases damage
		zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_explosion", "4", "explosions/hugeexplosion1", 500, damage * 2, radius, duration);
	}
	else
	{
		zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_explosion", "4", "explosions/hugeexplosion1", 500, damage, radius, duration);
	}
}

// zyk: Healing Area
void healing_area(gentity_t *ent, int damage, int duration)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		damage += 1;
	}

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 8 && ent->client->pers.unique_skill_duration > level.time &&
		!(ent->client->pers.player_statuses & (1 << 21)) && !(ent->client->pers.player_statuses & (1 << 22)))
	{ // zyk: Magic Master Unique Skill increases damage
		zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_healing", "4", "env/red_cyc", 0, damage * 2, 228, duration);
	}
	else
	{
		zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_healing", "4", "env/red_cyc", 0, damage, 228, duration);
	}
}

// zyk: Slow Motion
void slow_motion(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 3000;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_status |= (1 << 6);
			player_ent->client->pers.quest_target5_timer = level.time + duration;

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh10.mp3"));
		}
	}
}

// zyk: Ultra Speed
void ultra_speed(gentity_t *ent, int duration)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 3000;
	}

	ent->client->pers.quest_power_status |= (1 << 9);
	ent->client->pers.quest_power3_timer = level.time + duration;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh1.mp3"));
}

// zyk: Fast and Slow
void fast_and_slow(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		duration += 2000;
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_status |= (1 << 6);
			player_ent->client->pers.quest_target5_timer = level.time + duration;

			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh10.mp3"));
		}
	}

	ent->client->pers.quest_power_status |= (1 << 9);
	ent->client->pers.quest_power3_timer = level.time + duration;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/woosh1.mp3"));
}

// zyk: spawns the circle of fire around the player
void ultra_flame_circle(gentity_t *ent, char *targetname, char *spawnflags, char *effect_path, int start_time, int damage, int radius, int duration, int xoffset, int yoffset)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent,"classname","fx_runner");
	zyk_set_entity_field(new_ent,"spawnflags",spawnflags);
	zyk_set_entity_field(new_ent,"targetname",targetname);
	zyk_set_entity_field(new_ent,"origin",va("%d %d %d",(int)ent->r.currentOrigin[0] + xoffset,(int)ent->r.currentOrigin[1] + yoffset,(int)ent->r.currentOrigin[2]));

	new_ent->s.modelindex = G_EffectIndex( effect_path );

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

// zyk: Ultra Flame
void ultra_flame(gentity_t *ent, int distance, int damage)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		ultra_flame_circle(ent,"zyk_quest_effect_flame","4", "env/flame_jet", 200, damage, 35, 5000, 30, 30);
		ultra_flame_circle(ent,"zyk_quest_effect_flame","4", "env/flame_jet", 200, damage, 35, 5000, -30, 30);
		ultra_flame_circle(ent,"zyk_quest_effect_flame","4", "env/flame_jet", 200, damage, 35, 5000, 30, -30);
		ultra_flame_circle(ent,"zyk_quest_effect_flame","4", "env/flame_jet", 200, damage, 35, 5000, -30, -30);
	}

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			zyk_quest_effect_spawn(ent, player_ent, "zyk_quest_effect_flame", "4", "env/flame_jet", 200, damage, 35, 20000);
		}
	}
}

// zyk: spawns the flames around the player
void flaming_area_flames(gentity_t *ent, char *targetname, char *spawnflags, char *effect_path, int start_time, int damage, int radius, int duration, int xoffset, int yoffset)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "fx_runner");
	zyk_set_entity_field(new_ent, "spawnflags", spawnflags);
	zyk_set_entity_field(new_ent, "targetname", targetname);
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->r.currentOrigin[0] + xoffset, (int)ent->r.currentOrigin[1] + yoffset, (int)ent->r.currentOrigin[2]));

	new_ent->s.modelindex = G_EffectIndex(effect_path);

	zyk_spawn_entity(new_ent);

	if (damage > 0)
		new_ent->splashDamage = damage;

	if (radius > 0)
		new_ent->splashRadius = radius;

	if (start_time > 0)
		new_ent->nextthink = level.time + start_time;

	G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/fire_lp.wav"));

	level.special_power_effects[new_ent->s.number] = ent->s.number;
	level.special_power_effects_timer[new_ent->s.number] = level.time + duration;
}

// zyk: Flaming Area
void flaming_area(gentity_t *ent, int damage)
{
	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		damage *= 1.4;
	}

	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, -60, -60);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, -60, 0);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, -60, 60);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, 0, -60);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, 0, 0);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, 0, 60);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, 50, -60);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, 60, 0);
	flaming_area_flames(ent, "zyk_quest_effect_flaming_area", "4", "env/fire", 0, damage, 60, 6000, 60, 60);
}

// zyk: Hurricane
void hurricane(gentity_t *ent, int distance, int duration)
{
	int i = 0;
	int targets_hit = 0;

	// zyk: Universe Power
	if (ent->client->pers.quest_power_status & (1 << 13))
	{
		distance += 100;
		duration += 1000;
	}

	ent->client->pers.quest_debounce1_timer = 0;

	for ( i = 0; i < level.num_entities; i++)
	{
		gentity_t *player_ent = &g_entities[i];

		if (zyk_special_power_can_hit_target(ent, player_ent, i, 0, distance, qfalse, &targets_hit) == qtrue)
		{
			player_ent->client->pers.quest_power_status |= (1 << 5);
			player_ent->client->pers.quest_power_hit_counter = -179;
			player_ent->client->pers.quest_target4_timer = level.time + duration;

			// zyk: gives fall kill to the owner of this power
			player_ent->client->ps.otherKiller = ent->s.number;
			player_ent->client->ps.otherKillerTime = level.time + duration;
			player_ent->client->ps.otherKillerDebounceTime = level.time + 100;
							
			G_Sound(player_ent, CHAN_AUTO, G_SoundIndex("sound/effects/vacuum.mp3"));
		}
	}
}

// zyk: fires the Boba Fett flame thrower
void Player_FireFlameThrower( gentity_t *self )
{
	trace_t		tr;
	gentity_t	*traceEnt = NULL;

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

	// zyk: Flame Burst magic power has more damage
	if (self->client->pers.quest_power_status & (1 << 12))
	{
		damage += 2;
	}

	origin[0] = self->r.currentOrigin[0];
	origin[1] = self->r.currentOrigin[1];
	origin[2] = self->r.currentOrigin[2] + 20.0f;

	dir[0] = (-1) * self->client->ps.viewangles[0];
	dir[2] = self->client->ps.viewangles[2];
	dir[1] = (-1) * (180 - self->client->ps.viewangles[1]);

	if ((self->client->pers.flame_thrower - level.time) > 500)
		G_PlayEffectID( G_EffectIndex("boba/fthrw"), origin, dir);

	if ((self->client->pers.flame_thrower - level.time) > 1250)
		G_Sound( self, CHAN_WEAPON, G_SoundIndex("sound/effects/fire_lp") );

	//Check for a direct usage on NPCs first
	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0]*radius/2;
	tto[1] = tfrom[1] + fwd[1]*radius/2;
	tto[2] = tfrom[2] + fwd[2]*radius/2;

	trap->Trace( &tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID, qfalse, 0, 0 );

	VectorCopy( self->client->ps.viewangles, fwdangles );
	AngleVectors( fwdangles, forward, right, NULL );
	VectorCopy( self->client->ps.origin, center );

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	numListedEntities = trap->EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

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
			else if (traceEnt->client && (traceEnt->client->sess.amrpgmode == 2 || traceEnt->NPC) && 
					 self->client->pers.quest_power_status & (1 << 12) && zyk_check_immunity_power(traceEnt))
			{ // zyk: Immunity Power protects from Flame Burst
				entityList[e] = ENTITYNUM_NONE;
			}
		}
		traceEnt = &g_entities[entityList[e]];
		if (traceEnt && traceEnt != self)
		{
			G_Damage( traceEnt, self, self, self->client->ps.viewangles, tr.endpos, damage, DAMAGE_NO_KNOCKBACK|DAMAGE_IGNORE_TEAM, MOD_LAVA );
		}
		e++;
	}
}

// zyk: clear effects of some special powers
void clear_special_power_effect(gentity_t *ent)
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

// zyk: shows a text message from the file based on the language set by the player. Can receive additional arguments to concat in the final string
void zyk_text_message(gentity_t *ent, char *filename, qboolean show_in_chat, qboolean broadcast_message, ...)
{
	va_list argptr;
	char content[MAX_STRING_CHARS];
	const char *file_content;
	static char string[MAX_STRING_CHARS];
	char language[128];
	char console_cmd[64];
	int client_id = -1;
	FILE *text_file = NULL;

	strcpy(content, "");
	strcpy(string, "");
	strcpy(console_cmd, "print");

	if (broadcast_message == qfalse)
		client_id = ent->s.number;

	if (show_in_chat == qtrue)
		strcpy(console_cmd, "chat");

	if (ent->client->pers.player_settings & (1 << 5))
	{
		strcpy(language, "custom");
	}
	else
	{
		strcpy(language, "english");
	}

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

qboolean magic_master_has_this_power(gentity_t *ent, int selected_power)
{
	if (selected_power == MAGIC_HEALING_WATER && !(ent->client->pers.defeated_guardians & (1 << 4)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_WATER_SPLASH && !(ent->client->pers.defeated_guardians & (1 << 4)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_WATER_ATTACK && !(ent->client->pers.defeated_guardians & (1 << 4)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_EARTHQUAKE && !(ent->client->pers.defeated_guardians & (1 << 5)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ROCKFALL && !(ent->client->pers.defeated_guardians & (1 << 5)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_SHIFTING_SAND && !(ent->client->pers.defeated_guardians & (1 << 5)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_SLEEPING_FLOWERS && !(ent->client->pers.defeated_guardians & (1 << 6)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_POISON_MUSHROOMS && !(ent->client->pers.defeated_guardians & (1 << 6)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_TREE_OF_LIFE && !(ent->client->pers.defeated_guardians & (1 << 6)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_MAGIC_SHIELD && !(ent->client->pers.defeated_guardians & (1 << 7)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_DOME_OF_DAMAGE && !(ent->client->pers.defeated_guardians & (1 << 7)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_MAGIC_DISABLE && !(ent->client->pers.defeated_guardians & (1 << 7)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ULTRA_SPEED && !(ent->client->pers.defeated_guardians & (1 << 8)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_SLOW_MOTION && !(ent->client->pers.defeated_guardians & (1 << 8)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_FAST_AND_SLOW && !(ent->client->pers.defeated_guardians & (1 << 8)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_FLAME_BURST && !(ent->client->pers.defeated_guardians & (1 << 9)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ULTRA_FLAME && !(ent->client->pers.defeated_guardians & (1 << 9)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_FLAMING_AREA && !(ent->client->pers.defeated_guardians & (1 << 9)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_BLOWING_WIND && !(ent->client->pers.defeated_guardians & (1 << 10)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_HURRICANE && !(ent->client->pers.defeated_guardians & (1 << 10)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_REVERSE_WIND && !(ent->client->pers.defeated_guardians & (1 << 10)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ULTRA_RESISTANCE && !(ent->client->pers.defeated_guardians & (1 << 11)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ULTRA_STRENGTH && !(ent->client->pers.defeated_guardians & (1 << 11)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ENEMY_WEAKENING && !(ent->client->pers.defeated_guardians & (1 << 11)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ICE_STALAGMITE && !(ent->client->pers.defeated_guardians & (1 << 12)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ICE_BOULDER && !(ent->client->pers.defeated_guardians & (1 << 12)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_ICE_BLOCK && !(ent->client->pers.defeated_guardians & (1 << 12)) &&
		ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_HEALING_AREA && ent->client->pers.skill_levels[55] < 2)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_MAGIC_EXPLOSION && ent->client->pers.skill_levels[55] < 3)
	{
		return qfalse;
	}
	else if (selected_power == MAGIC_LIGHTNING_DOME && ent->client->pers.skill_levels[55] < 4)
	{
		return qfalse;
	}
	else if (selected_power < MAGIC_MAGIC_SENSE || selected_power >= MAX_MAGIC_POWERS)
	{ // zyk: if, for some reason, there is an invalid selected power value, does not allow it
		return qfalse;
	}
	else if (ent->client->sess.magic_disabled_powers & (1 << selected_power))
	{ // zyk: this power was disabled by the player
		return qfalse;
	}

	return qtrue;
}

void zyk_print_special_power(gentity_t *ent, int selected_power, char direction)
{
	if (selected_power == MAGIC_MAGIC_SENSE)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^7Magic Sense   ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_HEALING_WATER)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^4Healing Water       ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_WATER_SPLASH)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^4Water Splash        ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_WATER_ATTACK)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^4Water Attack        ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_EARTHQUAKE)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^3Earthquake          ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ROCKFALL)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^3Rockfall            ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_SHIFTING_SAND)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^3Shifting Sand        ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_SLEEPING_FLOWERS)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^2Sleeping Flowers    ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_POISON_MUSHROOMS)
	{	
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^2Poison Mushrooms    ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_TREE_OF_LIFE)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^2Tree of Life         ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_MAGIC_SHIELD)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^5Magic Shield        ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_DOME_OF_DAMAGE)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^5Dome of Damage      ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_MAGIC_DISABLE)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^5Magic Disable          ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ULTRA_SPEED)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^6Ultra Speed         ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_SLOW_MOTION)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^6Slow Motion         ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_FAST_AND_SLOW)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^6Fast and Slow        ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_FLAME_BURST)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^1Flame Burst         ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ULTRA_FLAME)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^1Ultra Flame         ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_FLAMING_AREA)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^1Flaming Area         ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_BLOWING_WIND)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^7Blowing Wind        ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_HURRICANE)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^7Hurricane           ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_REVERSE_WIND)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^7Reverse Wind         ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ULTRA_RESISTANCE)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^3Ultra Resistance    ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ULTRA_STRENGTH)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^3Ultra Strength      ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ENEMY_WEAKENING)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^3Enemy Weakening      ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ICE_STALAGMITE)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^5Ice Stalagmite      ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ICE_BOULDER)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^5Ice Boulder         ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_ICE_BLOCK)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^1%c ^5Ice Block            ^3MP: ^7%d\"", direction, ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_HEALING_AREA)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^7Healing Area        ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_MAGIC_EXPLOSION)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^7Magic Explosion     ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
	else if (selected_power == MAGIC_LIGHTNING_DOME)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"^1%c ^7Lightning Dome      ^3MP: ^7%d\"",direction,ent->client->pers.magic_power));
	}
}

// zyk: returns the amount of magic powers that are enabled with /magic command
int zyk_number_of_enabled_magic_powers(gentity_t *ent)
{
	int i = 0;
	int number_of_enabled_powers = 0;

	for (i = MAGIC_MAGIC_SENSE; i < MAX_MAGIC_POWERS; i++)
	{
		if (!(ent->client->sess.magic_disabled_powers & (1 << i)) && magic_master_has_this_power(ent, i) == qtrue)
		{
			number_of_enabled_powers++;
		}
	}

	return number_of_enabled_powers;
}

void zyk_show_magic_master_powers(gentity_t *ent, qboolean next_power)
{
	if (zyk_number_of_enabled_magic_powers(ent) == 0)
	{
		return;
	}

	if (next_power == qtrue)
	{
		do
		{
			ent->client->sess.selected_special_power++;
			if (ent->client->sess.selected_special_power >= MAX_MAGIC_POWERS)
				ent->client->sess.selected_special_power = MAGIC_MAGIC_SENSE;
		} while (magic_master_has_this_power(ent, ent->client->sess.selected_special_power) == qfalse);
	}
	else
	{
		do
		{
			ent->client->sess.selected_special_power--;
			if (ent->client->sess.selected_special_power < MAGIC_MAGIC_SENSE)
				ent->client->sess.selected_special_power = MAX_MAGIC_POWERS - 1;
		} while (magic_master_has_this_power(ent, ent->client->sess.selected_special_power) == qfalse);
	}

	zyk_print_special_power(ent,ent->client->sess.selected_special_power,'^');
}

void zyk_show_left_magic_master_powers(gentity_t *ent, qboolean next_power)
{
	if (zyk_number_of_enabled_magic_powers(ent) == 0)
	{
		return;
	}

	if (next_power == qtrue)
	{
		do
		{
			ent->client->sess.selected_left_special_power++;
			if (ent->client->sess.selected_left_special_power >= MAX_MAGIC_POWERS)
				ent->client->sess.selected_left_special_power = MAGIC_MAGIC_SENSE;
		} while (magic_master_has_this_power(ent, ent->client->sess.selected_left_special_power) == qfalse);
	}
	else
	{
		do
		{
			ent->client->sess.selected_left_special_power--;
			if (ent->client->sess.selected_left_special_power < MAGIC_MAGIC_SENSE)
				ent->client->sess.selected_left_special_power = MAX_MAGIC_POWERS - 1;
		} while (magic_master_has_this_power(ent, ent->client->sess.selected_left_special_power) == qfalse);
	}

	zyk_print_special_power(ent,ent->client->sess.selected_left_special_power,'<');
}

void zyk_show_right_magic_master_powers(gentity_t *ent, qboolean next_power)
{
	if (zyk_number_of_enabled_magic_powers(ent) == 0)
	{
		return;
	}

	if (next_power == qtrue)
	{
		do
		{
			ent->client->sess.selected_right_special_power++;
			if (ent->client->sess.selected_right_special_power >= MAX_MAGIC_POWERS)
				ent->client->sess.selected_right_special_power = MAGIC_MAGIC_SENSE;
		} while (magic_master_has_this_power(ent, ent->client->sess.selected_right_special_power) == qfalse);
	}
	else
	{
		do
		{
			ent->client->sess.selected_right_special_power--;
			if (ent->client->sess.selected_right_special_power < MAGIC_MAGIC_SENSE)
				ent->client->sess.selected_right_special_power = MAX_MAGIC_POWERS - 1;
		} while (magic_master_has_this_power(ent, ent->client->sess.selected_right_special_power) == qfalse);
	}

	zyk_print_special_power(ent,ent->client->sess.selected_right_special_power,'>');
}

// zyk: controls the quest powers stuff
extern void initialize_rpg_skills(gentity_t *ent);
void quest_power_events(gentity_t *ent)
{
	if (ent && ent->client)
	{
		if (ent->health > 0)
		{
			if (ent->client->pers.quest_power_status & (1 << 0) && ent->client->pers.quest_power1_timer < level.time)
			{ // zyk: Immunity Power
				ent->client->pers.quest_power_status &= ~(1 << 0);
			}

			if (ent->client->pers.quest_power_status & (1 << 1))
			{ // zyk: Chaos Power
				if (ent->client->pers.quest_power_hit3_counter > 0 && ent->client->pers.quest_target1_timer < level.time)
				{ // zyk: Chaos Power hit
					gentity_t *chaos_user = &g_entities[ent->client->pers.quest_power_user1_id];

					G_Damage(ent, chaos_user, chaos_user, NULL, NULL, 8, 0, MOD_UNKNOWN);
					ent->client->pers.quest_power_hit3_counter--;
					ent->client->pers.quest_target1_timer = level.time + 200;
				}
				
				if (ent->client->pers.quest_power_hit3_counter == 0)
				{ // zyk: end of Chaos Power
					ent->client->pers.quest_power_status &= ~(1 << 1);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 2) && ent->client->pers.quest_target2_timer < level.time)
			{ // zyk: Time Power. Remove it from target when duration ends
				ent->client->pers.quest_power_status &= ~(1 << 2);
			}

			if (ent->client->pers.quest_power_status & (1 << 3) && ent->client->pers.quest_power2_timer < level.time)
			{ // zyk: Ultra Strength
				ent->client->pers.quest_power_status &= ~(1 << 3);
			}

			if (ent->client->pers.quest_power_status & (1 << 4))
			{ // zyk: Poison Mushrooms
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 4);
				}

				if (ent->client->pers.quest_power_hit_counter > 0 && ent->client->pers.quest_target3_timer < level.time)
				{
					gentity_t *poison_mushrooms_user = &g_entities[ent->client->pers.quest_power_user2_id];

					if (poison_mushrooms_user && poison_mushrooms_user->client)
					{
						zyk_quest_effect_spawn(poison_mushrooms_user, ent, "zyk_quest_effect_poison", "0", "noghri_stick/gas_cloud", 0, 0, 0, 300);

						// zyk: Universe Power
						if (poison_mushrooms_user->client->pers.quest_power_status & (1 << 13))
							G_Damage(ent,poison_mushrooms_user,poison_mushrooms_user,NULL,NULL,6,0,MOD_UNKNOWN);
						else
							G_Damage(ent,poison_mushrooms_user,poison_mushrooms_user,NULL,NULL,4,0,MOD_UNKNOWN);
					}

					ent->client->pers.quest_power_hit_counter--;
					ent->client->pers.quest_target3_timer = level.time + 200;
				}
				else if (ent->client->pers.quest_power_hit_counter == 0 && ent->client->pers.quest_target3_timer < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << 4);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 5))
			{ // zyk: Hurricane
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 5);
				}

				if (ent->client->pers.quest_target4_timer > level.time)
				{
					static vec3_t forward;
					vec3_t blow_dir;

					if (ent->client->pers.quest_debounce1_timer < level.time)
					{
						ent->client->pers.quest_debounce1_timer = level.time + 50;

						VectorSet(blow_dir, -70, ent->client->pers.quest_power_hit_counter, 0);

						AngleVectors(blow_dir, forward, NULL, NULL);

						VectorNormalize(forward);

						VectorSet(ent->client->ps.velocity, forward[0] * 450.0, forward[1] * 450.0, forward[2] * 100.0);

						ent->client->pers.quest_power_hit_counter += 8;
						if (ent->client->pers.quest_power_hit_counter >= 180)
							ent->client->pers.quest_power_hit_counter -= 359;
					}
				}
				else
				{
					ent->client->pers.quest_power_status &= ~(1 << 5);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 6))
			{ // zyk: Slow Motion
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 6);
				}

				if (ent->client->pers.quest_target5_timer < level.time)
				{ // zyk: Slow Motion run out
					ent->client->pers.quest_power_status &= ~(1 << 6);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 7) && ent->client->pers.quest_power3_timer < level.time)
			{ // zyk: Ultra Resistance
				ent->client->pers.quest_power_status &= ~(1 << 7);
			}

			if (ent->client->pers.quest_power_status & (1 << 8))
			{ // zyk: Blowing Wind
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 8);
				}

				if (ent->client->pers.quest_target6_timer > level.time)
				{
					gentity_t *blowing_wind_user = &g_entities[ent->client->pers.quest_power_user3_id];

					if (ent->client->pers.quest_debounce1_timer < level.time)
					{
						ent->client->pers.quest_debounce1_timer = level.time + 50;

						if (blowing_wind_user && blowing_wind_user->client)
						{
							static vec3_t forward;
							vec3_t dir;

							AngleVectors(blowing_wind_user->client->ps.viewangles, forward, NULL, NULL);

							VectorNormalize(forward);

							if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
								VectorScale(forward, 215.0, dir);
							else
								VectorScale(forward, 40.0, dir);

							VectorAdd(ent->client->ps.velocity, dir, ent->client->ps.velocity);
						}
					}
				}
				else
				{
					ent->client->pers.quest_power_status &= ~(1 << 8);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 9) && ent->client->pers.quest_power3_timer < level.time)
			{ // zyk: Ultra Speed
				ent->client->pers.quest_power_status &= ~(1 << 9);
			}

			if (ent->client->pers.quest_power_status & (1 << 11))
			{ // zyk: Magic Shield
				if (ent->client->pers.quest_power4_timer < level.time)
				{ // zyk: Magic Shield run out
					ent->client->pers.quest_power_status &= ~(1 << 11);
				}
				else
				{
					ent->client->ps.eFlags |= EF_INVULNERABLE;
					ent->client->invulnerableTimer = ent->client->pers.quest_power4_timer;
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 12))
			{ // zyk: Flame Burst
				if (ent->client->pers.flame_thrower < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << 12);
				}
				else if (ent->client->cloakDebReduce < level.time)
				{ // zyk: fires the flame thrower
					Player_FireFlameThrower(ent);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 17))
			{ // zyk: Shifting Sand
				if (ent->client->pers.quest_power5_timer < level.time)
				{ // zyk: after this time, teleports to the new location and add effect there too
					if (Distance(ent->client->ps.origin, g_entities[ent->client->pers.quest_power_effect1_id].s.origin) < 100)
					{ // zyk: only teleports if the player is near the effect
						vec3_t origin;
						int random_x = Q_irand(0, 1);
						int random_y = Q_irand(0, 1);

						if (random_x == 0)
							random_x = -1;
						if (random_y == 0)
							random_y = -1;

						gentity_t *this_enemy = &g_entities[ent->client->pers.quest_power_user4_id];

						origin[0] = this_enemy->client->ps.origin[0] + (Q_irand(70, 100) * random_x);
						origin[1] = this_enemy->client->ps.origin[1] + (Q_irand(70, 100) * random_y);
						origin[2] = this_enemy->client->ps.origin[2] + Q_irand(40, 100);

						zyk_TeleportPlayer(ent, origin, ent->client->ps.viewangles);

						VectorCopy(ent->client->ps.origin, ent->client->pers.teleport_point);
					}

					ent->client->pers.quest_power_status &= ~(1 << 17);
					ent->client->pers.quest_power_status |= (1 << 18);

					ent->client->pers.quest_power5_timer = level.time + 4000;
					zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_sand", "0", "env/sand_spray", 0, 0, 0, 2000);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 18))
			{ // zyk: Shifting Sand after the teleport, validating if player is not suck
				if (ent->client->pers.quest_power5_timer < level.time)
				{
					if (VectorCompare(ent->client->ps.origin, ent->client->pers.teleport_point) == qtrue)
					{ // zyk: stuck, teleport back
						zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_sand", "0", "env/sand_spray", 0, 0, 0, 1000);
						zyk_TeleportPlayer(ent, ent->client->pers.teleport_angles, ent->client->ps.viewangles);
						zyk_quest_effect_spawn(ent, ent, "zyk_quest_effect_sand", "0", "env/sand_spray", 0, 0, 0, 1000);
					}

					ent->client->pers.quest_power_status &= ~(1 << 18);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 19))
			{ // zyk: Tree of Life
				if (ent->client->pers.quest_power_hit2_counter > 0)
				{
					if (ent->client->pers.quest_power6_timer < level.time)
					{
						int heal_amount = 20;

						// zyk: Universe Power
						if (ent->client->pers.quest_power_status & (1 << 13))
						{
							heal_amount = 40;
						}

						if ((ent->health + heal_amount) < ent->client->ps.stats[STAT_MAX_HEALTH])
							ent->health += heal_amount;
						else
							ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

						ent->client->pers.quest_power_hit2_counter--;
						ent->client->pers.quest_power6_timer = level.time + 1000;

						G_Sound(ent, CHAN_ITEM, G_SoundIndex("sound/weapons/force/heal.wav"));
					}
				}
				else
				{
					ent->client->pers.quest_power_status &= ~(1 << 19);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 20))
			{ // zyk: Reverse Wind
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 20);
				}

				if (ent->client->pers.quest_target6_timer > level.time)
				{
					gentity_t *reverse_wind_user = &g_entities[ent->client->pers.quest_power_user3_id];

					if (ent->client->pers.quest_debounce1_timer < level.time)
					{
						ent->client->pers.quest_debounce1_timer = level.time + 50;

						if (reverse_wind_user && reverse_wind_user->client)
						{
							vec3_t dir, forward;

							VectorSubtract(reverse_wind_user->client->ps.origin, ent->client->ps.origin, forward);
							VectorNormalize(forward);

							if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
								VectorScale(forward, 215.0, dir);
							else
								VectorScale(forward, 46.0, dir);

							VectorAdd(ent->client->ps.velocity, dir, ent->client->ps.velocity);
						}
					}
				}
				else
				{
					ent->client->pers.quest_power_status &= ~(1 << 20);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 21))
			{ // zyk: Enemy Weakening
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 21);
				}

				if (ent->client->pers.quest_target7_timer < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << 21);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 22))
			{ // zyk: Ice Block
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 22);
				}

				if (ent->client->pers.quest_power7_timer < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << 22);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 23))
			{ // zyk: hit by Flaming Area
				if (ent->client->pers.quest_power_status & (1 << 0))
				{ // zyk: testing for Immunity Power in target player
					ent->client->pers.quest_power_status &= ~(1 << 23);
				}

				if (ent->client->pers.quest_power_hit4_counter > 0 && ent->client->pers.quest_target8_timer < level.time)
				{
					gentity_t *flaming_area_user = &g_entities[ent->client->pers.quest_power_user5_id];

					if (flaming_area_user && flaming_area_user->client)
					{
						zyk_quest_effect_spawn(flaming_area_user, ent, "zyk_quest_effect_flaming_area_hit", "0", "env/fire", 0, 0, 0, 300);

						// zyk: Universe Power
						if (flaming_area_user->client->pers.quest_power_status & (1 << 13))
							G_Damage(ent, flaming_area_user, flaming_area_user, NULL, NULL, 3, 0, MOD_UNKNOWN);
						else
							G_Damage(ent, flaming_area_user, flaming_area_user, NULL, NULL, 2, 0, MOD_UNKNOWN);
					}

					ent->client->pers.quest_power_hit4_counter--;
					ent->client->pers.quest_target8_timer = level.time + 200;
				}
				else if (ent->client->pers.quest_power_hit4_counter == 0 && ent->client->pers.quest_target8_timer < level.time)
				{
					ent->client->pers.quest_power_status &= ~(1 << 23);
				}
			}

			if (ent->client->pers.quest_power_status & (1 << 24) && ent->client->pers.quest_target9_timer < level.time)
			{ // zyk: hit by Sleeping Flowers
				ent->client->pers.quest_power_status &= ~(1 << 24);
			}

			if (ent->client->pers.quest_power_status & (1 << 25) && ent->client->pers.quest_target10_timer < level.time)
			{ // zyk: hit by Ice Boulder
				ent->client->pers.quest_power_status &= ~(1 << 25);
			}

			if (ent->client->pers.quest_power_status & (1 << 26) && ent->client->pers.quest_target11_timer < level.time)
			{ // zyk: hit by Elemental Attack
				ent->client->pers.quest_power_status &= ~(1 << 26);
			}
		}
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
	}
}

// zyk: damages target player with poison hits
void poison_dart_hits(gentity_t *ent)
{
	if (ent && ent->client && ent->health > 0 && ent->client->pers.player_statuses & (1 << 20) && ent->client->pers.poison_dart_hit_counter > 0 && 
		ent->client->pers.poison_dart_hit_timer < level.time)
	{
		gentity_t *poison_user = &g_entities[ent->client->pers.poison_dart_user_id];

		G_Damage(ent,poison_user,poison_user,NULL,NULL,5,0,MOD_UNKNOWN);

		ent->client->pers.poison_dart_hit_counter--;
		ent->client->pers.poison_dart_hit_timer = level.time + 200;

		// zyk: no more do poison damage if counter is 0
		if (ent->client->pers.poison_dart_hit_counter == 0)
			ent->client->pers.player_statuses &= ~(1 << 20);
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

		if (fire_bolt_user->client->pers.unique_skill_duration > level.time && !(fire_bolt_user->client->pers.player_statuses & (1 << 21)) &&
			!(fire_bolt_user->client->pers.player_statuses & (1 << 22)) && !(fire_bolt_user->client->pers.player_statuses & (1 << 23)))
		{ // zyk: Unique Skill increases damage
			G_Damage(ent, fire_bolt_user, fire_bolt_user, NULL, NULL, 6, 0, MOD_UNKNOWN);
		}
		else
		{
			G_Damage(ent, fire_bolt_user, fire_bolt_user, NULL, NULL, 4, 0, MOD_UNKNOWN);
		}

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

// zyk: tests if player already finished the Revelations mission of Universe Quest
void first_second_act_objective(gentity_t *ent)
{
	int i = 0;
	int count = 0;

	for (i = 1; i < 3; i++)
	{
		if (ent->client->pers.universe_quest_counter & (1 << i))
			count++;
	}

	if (count == 2)
	{
		ent->client->pers.universe_quest_progress = 9;

		if (ent->client->pers.universe_quest_counter & (1 << 29))
		{ // zyk: if player is in Challenge Mode, do not remove this bit value
			ent->client->pers.universe_quest_counter = 0;
			ent->client->pers.universe_quest_counter |= (1 << 29);
		}
		else
		{
			ent->client->pers.universe_quest_counter = 0;
		}
	}
}

// zyk: spawns the prison door in first Universe Quest mission
void zyk_spawn_catwalk_prison(int x, int y, int z, int pitch, int yaw)
{
	gentity_t *door_ent = &g_entities[109];
	qboolean door_catwalk_ok = qfalse;
	qboolean prison_key_ok = qfalse;
	gentity_t *new_ent = NULL;
	int i = 0;

	if (door_ent && Q_stricmp(door_ent->classname, "func_static") == 0)
	{ // zyk: removes the map last door
		G_FreeEntity(door_ent);
	}

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		door_ent = &g_entities[i];

		if (door_ent && Q_stricmp(door_ent->targetname, "zyk_sage_prison") == 0)
		{
			door_catwalk_ok = qtrue;
		}

		if (door_ent && door_ent->spawnflags & 131072)
		{
			prison_key_ok = qtrue;
		}
	}

	// zyk: spawning the catwalk door
	if (door_catwalk_ok == qfalse)
	{
		new_ent = G_Spawn();

		zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
		zyk_set_entity_field(new_ent, "spawnflags", "65537");
		zyk_set_entity_field(new_ent, "origin", va("%d %d %d", x, y, z));

		zyk_set_entity_field(new_ent, "angles", va("%d %d 0", pitch, yaw));

		zyk_set_entity_field(new_ent, "mins", "-8 -64 -64");
		zyk_set_entity_field(new_ent, "maxs", "8 64 64");

		zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

		zyk_set_entity_field(new_ent, "targetname", "zyk_sage_prison");

		zyk_spawn_entity(new_ent);
	}

	// zyk: spawning the key
	if (prison_key_ok == qfalse)
	{
		new_ent = G_Spawn();

		zyk_set_entity_field(new_ent, "classname", "weapon_stun_baton");
		zyk_set_entity_field(new_ent, "spawnflags", "131072");
		zyk_set_entity_field(new_ent, "origin", "-2149 5087 -2759");

		zyk_spawn_entity(new_ent);
	}
}

// zyk: spawns reborns in first Universe Quest mission
void zyk_spawn_quest_reborns()
{
	Zyk_NPC_SpawnType("quest_reborn_blue", -2205, 5008, -2758, 179);
	Zyk_NPC_SpawnType("quest_reborn_blue", -2205, 5175, -2758, 179);
	Zyk_NPC_SpawnType("quest_reborn_blue", -2255, 5008, -2758, 179);
	Zyk_NPC_SpawnType("quest_reborn_blue", -2255, 5175, -2758, 179);

	Zyk_NPC_SpawnType("quest_reborn", -3204, 2630, -2982, -90);
	Zyk_NPC_SpawnType("quest_reborn", -3311, 2630, -2982, -90);
	Zyk_NPC_SpawnType("quest_reborn_blue", -3080, 2687, -2965, -90);

	Zyk_NPC_SpawnType("quest_reborn", -2569, 1316, -2246, -90);
	Zyk_NPC_SpawnType("quest_reborn_blue", -2619, 1316, -2246, -90);
	Zyk_NPC_SpawnType("quest_reborn", -2669, 1316, -2246, -90);

	Zyk_NPC_SpawnType("quest_reborn_blue", -4545, 3162, -2758, -90);
	Zyk_NPC_SpawnType("quest_reborn", -4590, 3162, -2758, -90);
	Zyk_NPC_SpawnType("quest_reborn", -4490, 3162, -2758, -90);

	Zyk_NPC_SpawnType("quest_reborn_blue", 1700, -10, -3130, 179);
	Zyk_NPC_SpawnType("quest_reborn_blue", 1700, -50, -3130, 179);
}

// zyk: if for some reason a sage was not spawned, try to spawn him now
void zyk_validate_sages(gentity_t *ent)
{
	int i = 0;
	qboolean sage_of_light_ok = qfalse;
	qboolean sage_of_darkness_ok = qfalse;
	qboolean sage_of_eternity_ok = qfalse;
	gentity_t *npc_ent = NULL;

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		npc_ent = &g_entities[i];

		if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "sage_of_light") == 0)
		{
			sage_of_light_ok = qtrue;
		}
		else if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "sage_of_darkness") == 0)
		{
			sage_of_darkness_ok = qtrue;
		}
		else if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "sage_of_eternity") == 0)
		{
			sage_of_eternity_ok = qtrue;
		}
	}

	if (sage_of_light_ok == qfalse)
	{
		npc_ent = Zyk_NPC_SpawnType("sage_of_light", 2750, -115, -3806, 179);

		if (npc_ent)
		{ // zyk: sets the player id who must protect this sage
			npc_ent->client->pers.universe_quest_objective_control = ent - g_entities;
		}
	}
	if (sage_of_darkness_ok == qfalse)
	{
		npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", 2750, -39, -3806, 179);

		if (npc_ent)
		{ // zyk: sets the player id who must protect this sage
			npc_ent->client->pers.universe_quest_objective_control = ent - g_entities;
		}
	}
	if (sage_of_eternity_ok == qfalse)
	{
		npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", 2750, 39, -3806, 179);

		if (npc_ent)
		{ // zyk: sets the player id who must protect this sage
			npc_ent->client->pers.universe_quest_objective_control = ent - g_entities;
		}
	}
}

// zyk: checks if the player has already all artifacts
extern void save_account(gentity_t *ent, qboolean save_char_file);
extern int number_of_artifacts(gentity_t *ent);
void universe_quest_artifacts_checker(gentity_t *ent)
{
	if (number_of_artifacts(ent) == 8)
	{ // zyk: after collecting all artifacts, go to next objective
		zyk_text_message(ent, "universe/mission_2/mission_2_got_all_artifacts", qtrue, qtrue, ent->client->pers.netname);

		if (ent->client->pers.universe_quest_counter & (1 << 29))
		{ // zyk: if player is in Challenge Mode, do not remove this bit value
			ent->client->pers.universe_quest_counter = 0;
			ent->client->pers.universe_quest_counter |= (1 << 29);
		}
		else
			ent->client->pers.universe_quest_counter = 0;

		ent->client->pers.universe_quest_progress = 3;
		ent->client->pers.universe_quest_timer = level.time + 1000;
		ent->client->pers.universe_quest_objective_control = 4; // zyk: fourth Universe Quest objective
		ent->client->pers.universe_quest_messages = 0;
		
		save_account(ent, qtrue);
	}
}

// zyk: checks if the player got all crystals
extern int number_of_crystals(gentity_t *ent);
extern void quest_get_new_player(gentity_t *ent);
void universe_crystals_check(gentity_t *ent)
{
	if (number_of_crystals(ent) == 3)
	{
		ent->client->pers.universe_quest_progress = 10;
		if (ent->client->pers.universe_quest_counter & (1 << 29))
		{ // zyk: if player is in Challenge Mode, do not remove this bit value
			ent->client->pers.universe_quest_counter = 0;
			ent->client->pers.universe_quest_counter |= (1 << 29);
		}
		else
			ent->client->pers.universe_quest_counter = 0;

		save_account(ent, qtrue);
		quest_get_new_player(ent);
	}
	else
	{
		save_account(ent, qtrue);
	}
}

extern void clean_note_model();
void zyk_try_get_dark_quest_note(gentity_t *ent, int note_bitvalue)
{
	if (ent->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && ent->client->pers.guardian_mode == 0 && 
		!(ent->client->pers.hunter_quest_progress & (1 << note_bitvalue)) && ent->client->pers.can_play_quest == 1 &&
		level.quest_note_id != -1 && (int)Distance(ent->client->ps.origin, g_entities[level.quest_note_id].r.currentOrigin) < 40)
	{
		zyk_text_message(ent, "dark/found_note", qtrue, qfalse);
		ent->client->pers.hunter_quest_progress |= (1 << note_bitvalue);
		clean_note_model();
		save_account(ent, qtrue);
		quest_get_new_player(ent);
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

	// zyk: setting Immunity Power so every status power on the duelist will be cancelled
	ent->client->pers.quest_power_status |= (1 << 0);
	ent->client->pers.quest_power1_timer = level.duel_tournament_timer;

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
				initialize_rpg_skills(ent);
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

gentity_t *zyk_quest_item(char *item_path, int x, int y, int z, char *mins, char *maxs)
{
	gentity_t *new_ent = G_Spawn();

	if (!strstr(item_path, ".md3"))
	{// zyk: effect
		zyk_set_entity_field(new_ent, "classname", "fx_runner");
		zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");
		zyk_set_entity_field(new_ent, "origin", va("%d %d %d", x, y, z));

		new_ent->s.modelindex = G_EffectIndex(item_path);

		zyk_spawn_entity(new_ent);
	}
	else
	{ // zyk: model
		zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
		zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");
		zyk_set_entity_field(new_ent, "origin", va("%d %d %d", x, y, z));

		if (Q_stricmp(mins, "") != 0 && Q_stricmp(maxs, "") != 0)
		{
			zyk_set_entity_field(new_ent, "spawnflags", "65537");
			zyk_set_entity_field(new_ent, "mins", mins);
			zyk_set_entity_field(new_ent, "maxs", maxs);
		}

		if (z == -10000)
		{ // zyk: catwalk in t3_rift quest missions. Must scale it
			zyk_set_entity_field(new_ent, "zykmodelscale", "150");
		}

		zyk_set_entity_field(new_ent, "model", item_path);

		zyk_spawn_entity(new_ent);
	}

	return new_ent;
}

// zyk: remaps quest items to the values passed as args
void zyk_remap_quest_item(char *old_remap, char *new_remap)
{
	float f = level.time * 0.001;

	AddRemap(old_remap, new_remap, f);
	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
}

void zyk_trial_room_models()
{
	gentity_t *new_ent = G_Spawn();

	// zyk: catwalk to block entrance
	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "65537");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -3770, 4884, 120));

	zyk_set_entity_field(new_ent, "angles", va("%d %d 0", 90, 0));

	zyk_set_entity_field(new_ent, "mins", "-24 -192 -192");
	zyk_set_entity_field(new_ent, "maxs", "24 192 192");
	zyk_set_entity_field(new_ent, "zykmodelscale", "300");

	zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

	zyk_spawn_entity(new_ent);

	// zyk: adding catwalks to block the central lava
	new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "65537");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -4470, 4628, -65));

	zyk_set_entity_field(new_ent, "mins", "-256 -256 -32");
	zyk_set_entity_field(new_ent, "maxs", "256 256 32");
	zyk_set_entity_field(new_ent, "zykmodelscale", "400");

	zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

	zyk_spawn_entity(new_ent);

	new_ent = G_Spawn();

	zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
	zyk_set_entity_field(new_ent, "spawnflags", "65537");
	zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -4470, 5140, -65));

	zyk_set_entity_field(new_ent, "mins", "-256 -256 -32");
	zyk_set_entity_field(new_ent, "maxs", "256 256 32");
	zyk_set_entity_field(new_ent, "zykmodelscale", "400");

	zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

	zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

	zyk_spawn_entity(new_ent);
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
extern gentity_t *load_crystal_model(int x,int y,int z, int yaw, int crystal_number);
extern void clean_crystal_model(int crystal_number);
extern qboolean dark_quest_collected_notes(gentity_t *ent);
extern qboolean light_quest_defeated_guardians(gentity_t *ent);
extern void set_max_health(gentity_t *ent);
extern void set_max_shield(gentity_t *ent);
extern gentity_t *load_effect(int x,int y,int z, int spawnflags, char *fxFile);
extern void duel_show_table(gentity_t *ent);
extern void WP_DisruptorAltFire(gentity_t *ent);
extern int zyk_max_magic_power(gentity_t *ent);
extern void G_Kill( gentity_t *ent );
extern void save_quest_file(int quest_number);
extern void zyk_set_quest_npc_abilities(gentity_t *zyk_npc);

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

	if (level.boss_battle_music_reset_timer > 0 && level.boss_battle_music_reset_timer < level.time)
	{ // zyk: resets music to default one
		level.boss_battle_music_reset_timer = 0;
		trap->SetConfigstring( CS_MUSIC, G_NewString(level.default_map_music) );
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
			qboolean is_in_boss = qfalse;

			for (zyk_it = 0; zyk_it < MAX_CLIENTS; zyk_it++)
			{
				gentity_t *this_ent = &g_entities[zyk_it];

				// zyk: cleaning flag from player
				if (this_ent && this_ent->client)
					this_ent->client->pers.player_statuses &= ~(1 << 27);

				if (this_ent && this_ent->client && this_ent->client->pers.connected == CON_CONNECTED &&
					this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->sess.amrpgmode == 2 &&
					this_ent->client->pers.guardian_mode > 0)
				{ // zyk: validating if there is someone fighting a quest boss
					is_in_boss = qtrue;
				}
			}

			if (is_in_boss == qfalse && level.duel_matches_done < level.duel_matches_quantity)
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

			if (is_in_boss == qtrue)
			{
				level.duel_tournament_timer = level.time + 15000;
				trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7Waiting for the quest player to finish boss battle!\"");
			}
			else if (level.duel_matches_quantity == level.duel_matches_done && level.duel_tournament_mode == 2)
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

	// zyk: Guardian of Map abilities
	if (level.guardian_quest > 0)
	{
		gentity_t *npc_ent = &g_entities[level.guardian_quest];

		if (npc_ent && npc_ent->client && npc_ent->health > 0)
		{		
			npc_ent->client->ps.stats[STAT_WEAPONS] = level.initial_map_guardian_weapons;

			if (npc_ent->client->pers.hunter_quest_timer < level.time)
			{
				if (npc_ent->client->pers.hunter_quest_messages == 0)
				{
					healing_area(npc_ent, 5, 5000);
					trap->SendServerCommand(-1, "chat \"^3Guardian of Map: ^7Healing Area!\"");
					npc_ent->client->pers.hunter_quest_messages++;
				}
				else if (npc_ent->client->pers.hunter_quest_messages == 1)
				{
					magic_explosion(npc_ent, 320, 130, 900);
					trap->SendServerCommand(-1, "chat \"^3Guardian of Map: ^7Magic Explosion!\"");
					npc_ent->client->pers.hunter_quest_messages++;
				}
				else if (npc_ent->client->pers.hunter_quest_messages == 2)
				{
					lightning_dome(npc_ent, 70);
					trap->SendServerCommand(-1, "chat \"^3Guardian of Map: ^7Lightning Dome!\"");
					npc_ent->client->pers.hunter_quest_messages = 0;
				}

				npc_ent->client->pers.hunter_quest_timer = level.time + Q_irand(6000, 9000);
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
				int jetpack_debounce_amount = 18;

				if (ent->client->sess.amrpgmode == 2)
				{ // zyk: RPG Mode jetpack skill. Each level decreases fuel debounce
					if (ent->client->pers.rpg_class == 2)
					{ // zyk: Bounty Hunter can have a more efficient jetpack
						jetpack_debounce_amount -= ((ent->client->pers.skill_levels[34] * 2) + (ent->client->pers.skill_levels[55]));
					}
					else
					{
						jetpack_debounce_amount -= (ent->client->pers.skill_levels[34] * 2);
					}

					if (ent->client->pers.secrets_found & (1 << 17)) // zyk: Jetpack Upgrade decreases fuel usage
						jetpack_debounce_amount -= 2;
				}

				if (ent->client->pers.cmd.upmove > 0)
				{ // zyk: jetpack thrusting
					jetpack_debounce_amount *= 2;
				}

				ent->client->pers.jetpack_fuel -= jetpack_debounce_amount;

				if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 8 && ent->client->pers.jetpack_fuel <= 0 && 
					ent->client->pers.magic_power >= 10 && ent->client->pers.skill_levels[55] > 0)
				{ // zyk: Magic Master Improvements skill allows recovering jetpack fuel with magic
					ent->client->pers.jetpack_fuel = (100 * ent->client->pers.skill_levels[55]);
					ent->client->pers.magic_power -= 10;
					send_rpg_events(2000);
				}

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
			poison_dart_hits(ent);
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
				if (ent->client->pers.tutorial_step > 1)
				{ // zyk: after last message, tutorial ends
					ent->client->pers.player_statuses &= ~(1 << 25);
				}
				else
				{
					zyk_text_message(ent, va("tutorial/%d", ent->client->pers.tutorial_step), qtrue, qfalse);
				}

				// zyk: interval between messages
				ent->client->pers.tutorial_step++;
				ent->client->pers.tutorial_timer = level.time + 7000;
			}

			zyk_print_custom_quest_info(ent);

			if (ent->client->sess.amrpgmode == 2 && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ // zyk: RPG Mode skills and quests actions. Must be done if player is not at Spectator Mode
				// zyk: Weapon Upgrades
				if (ent->client->ps.weapon == WP_DISRUPTOR && ent->client->pers.skill_levels[21] == 2 && ent->client->ps.weaponTime > (weaponData[WP_DISRUPTOR].fireTime * 1.0)/1.4)
				{
					ent->client->ps.weaponTime = (weaponData[WP_DISRUPTOR].fireTime * 1.0)/1.4;
				}

				// zyk: Stealth Attacker using his Unique Skill, increase firerate of disruptor
				if (ent->client->ps.weapon == WP_DISRUPTOR && ent->client->pers.rpg_class == 5 && ent->client->pers.skill_levels[38] > 0 && 
					ent->client->pers.unique_skill_duration > level.time && !(ent->client->pers.player_statuses & (1 << 21)) && 
					ent->client->ps.weaponTime > (weaponData[WP_DISRUPTOR].fireTime * 1.0)/3.0)
				{
					ent->client->ps.weaponTime = (weaponData[WP_DISRUPTOR].fireTime * 1.0)/3.0;
				}

				if (ent->client->ps.weapon == WP_REPEATER && ent->client->pers.skill_levels[23] == 2 && ent->client->ps.weaponTime > weaponData[WP_REPEATER].altFireTime/2)
				{
					ent->client->ps.weaponTime = weaponData[WP_REPEATER].altFireTime/2;
				}

				// zyk: Monk class has a faster melee fireTime
				if (ent->client->pers.rpg_class == 4 && ent->client->ps.weapon == WP_MELEE && ent->client->pers.skill_levels[55] > 0 && 
					ent->client->ps.weaponTime > (weaponData[WP_MELEE].fireTime * 2.25)/(ent->client->pers.skill_levels[55] + 1))
				{
					ent->client->ps.weaponTime = (weaponData[WP_MELEE].fireTime * 2.25)/(ent->client->pers.skill_levels[55] + 1);
				}
				else if (ent->client->pers.rpg_class == 8 && ent->client->pers.unique_skill_duration > level.time && ent->client->pers.player_statuses & (1 << 21) &&
					ent->client->ps.weaponTime > (weaponData[WP_MELEE].fireTime * 0.8))
				{ // zyk: Magic Master Faster Bolt ability makes melee faster to shoot bolts faster
					ent->client->ps.weaponTime = weaponData[WP_MELEE].fireTime * 0.8;
				}

				if (ent->client->pers.flame_thrower > level.time && ent->client->cloakDebReduce < level.time)
				{ // zyk: fires the flame thrower
					Player_FireFlameThrower(ent);
				}

				if (ent->client->pers.rpg_class == 2)
				{
					// zyk: if the bounty hunter still has sentries, give the item to him
					if (ent->client->pers.bounty_hunter_sentries > 0)
						ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

					if (ent->client->pers.thermal_vision == qtrue && ent->client->ps.zoomMode == 0)
					{ // zyk: if the bounty hunter stops using binoculars, stop the Thermal Vision
						ent->client->pers.thermal_vision = qfalse;
						ent->client->ps.fd.forcePowersActive &= ~(1 << FP_SEE);
						ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
						ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_0;

						ent->client->pers.thermal_vision_cooldown_time = level.time + 300;
					}
					else if (ent->client->pers.thermal_vision == qfalse && ent->client->ps.zoomMode == 2 && ent->client->pers.secrets_found & (1 << 1))
					{ // zyk: Bounty Hunter with Upgrade, activate the Thermal Vision
						ent->client->pers.thermal_vision = qtrue;
						ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
						ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_3;
						ent->client->ps.fd.forcePowersActive |= (1 << FP_SEE);

						// zyk: adds some time to allow deactivating the Binoculars. Force Sense is active, so using this variable to add the cooldown time
						ent->client->ps.forceAllowDeactivateTime = level.time + 300;

						ent->client->pers.thermal_vision_cooldown_time = level.time + 300;
					}

					if (ent->client->pers.secrets_found & (1 << 1) && ent->client->ps.weapon == WP_BRYAR_PISTOL && ent->client->ps.weaponTime > (weaponData[WP_BRYAR_PISTOL].fireTime * 0.3))
					{ // zyk: Bounty Hunter Upgrade makes his pistol shoot faster
						ent->client->ps.weaponTime = weaponData[WP_BRYAR_PISTOL].fireTime * 0.3;
					}
				}
				else if (ent->client->pers.rpg_class == 3 && ent->client->pers.player_statuses & (1 << 21) && ent->client->pers.lightning_shield_timer < level.time)
				{ // zyk: Armored Soldier Lightning Shield damage to enemies nearby
					int player_it = 0;

					for (player_it = 0; player_it < level.num_entities; player_it++)
					{
						gentity_t* player_ent = &g_entities[player_it];

						if (player_ent && player_ent->client && ent != player_ent &&
							zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue && Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 60)
						{
							G_Damage(player_ent, ent, ent, NULL, NULL, 8, 0, MOD_UNKNOWN);

							player_ent->client->ps.electrifyTime = level.time + 500;
						}
					}

					ent->client->pers.lightning_shield_timer = level.time + 200;
				}
				else if (ent->client->pers.rpg_class == 4 && 
						(ent->client->pers.player_statuses & (1 << 22) || ent->client->pers.player_statuses & (1 << 23)) &&
						 ent->client->pers.monk_unique_timer < level.time)
				{ // zyk: Monk unique abilities
					int player_it = 0;
					int push_scale = 100;

					for (player_it = 0; player_it < level.num_entities; player_it++)
					{
						gentity_t *player_ent = &g_entities[player_it];

						if (player_ent && player_ent->client && ent != player_ent &&
							zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue)
						{ // zyk: can only hit the target if he is not knocked down yet
							if (ent->client->pers.player_statuses & (1 << 22) && 
								Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 80 &&
								player_ent->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN)
							{ // zyk: Spin Kick
								vec3_t dir;

								VectorSubtract(player_ent->client->ps.origin, ent->client->ps.origin, dir);
								VectorNormalize(dir);

								G_Damage(player_ent, ent, ent, NULL, NULL, 25, 0, MOD_MELEE);

								// zyk: removing emotes to prevent exploits
								if (player_ent->client->pers.player_statuses & (1 << 1))
								{
									player_ent->client->pers.player_statuses &= ~(1 << 1);
									player_ent->client->ps.forceHandExtendTime = level.time;
								}

								player_ent->client->ps.velocity[0] = dir[0] * push_scale;
								player_ent->client->ps.velocity[1] = dir[1] * push_scale;
								player_ent->client->ps.velocity[2] = 250;

								player_ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
								player_ent->client->ps.forceHandExtendTime = level.time + 1000;
								player_ent->client->ps.forceDodgeAnim = 0;
								player_ent->client->ps.quickerGetup = qtrue;

								G_Sound(ent, CHAN_AUTO, G_SoundIndex(va("sound/weapons/melee/punch%d", Q_irand(1, 4))));
							}
							else if (ent->client->pers.player_statuses & (1 << 23) && 
									 ent->health > 0 && 
									 Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 300)
							{ // zyk: Meditation Drain
								int heal_amount = 8;

								G_Damage(player_ent, ent, ent, NULL, NULL, heal_amount/3, 0, MOD_MELEE);

								player_ent->client->ps.electrifyTime = level.time + 1000;

								if ((ent->health + heal_amount) < ent->client->pers.max_rpg_health)
								{
									ent->health += heal_amount;
								}
								else if (ent->health < ent->client->pers.max_rpg_health)
								{
									ent->health = ent->client->pers.max_rpg_health;
								}
								else if ((ent->client->ps.stats[STAT_ARMOR] + heal_amount) < ent->client->pers.max_rpg_shield)
								{
									ent->client->ps.stats[STAT_ARMOR] += heal_amount;
								}
								else
								{
									ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
								}
							}
						}
					}

					ent->client->pers.monk_unique_timer = level.time + 200;
				}
				else if (ent->client->pers.rpg_class == 5)
				{ // zyk: Stealth Attacker abilities
					if (ent->client->pers.player_statuses & (1 << 23) && ent->client->pers.monk_unique_timer < level.time)
					{ // zyk: Aimed Shot ability. Fires the full charged sniper shot
						if (ent->health > 0)
						{
							WP_DisruptorAltFire(ent);
						}

						ent->client->pers.monk_unique_timer = level.time + 2000;
					}

					if (ent->client->pers.thermal_vision == qtrue && ent->client->ps.zoomMode == 0)
					{ // zyk: if the stealth attacker stops using sniper scope, stop the Thermal Detector
						ent->client->pers.thermal_vision = qfalse;
						ent->client->ps.fd.forcePowersActive &= ~(1 << FP_SEE);
						ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
						ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_0;
					}
					else if (ent->client->pers.thermal_vision == qfalse && ent->client->ps.zoomMode == 1 && ent->client->pers.secrets_found & (1 << 7))
					{ // zyk: Stealth Attacker with Upgrade, activate the Thermal Detector
						ent->client->pers.thermal_vision = qtrue;
						ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
						ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_1;
						ent->client->ps.fd.forcePowersActive |= (1 << FP_SEE);
					}
				}
				else if (ent->client->pers.rpg_class == 6 && ent->client->pers.player_statuses & (1 << 22) && ent->client->pers.vertical_dfa_timer > 0 &&
						ent->client->pers.vertical_dfa_timer < level.time && ent->client->ps.groundEntityNum != ENTITYNUM_NONE)
				{ // zyk: Duelist abilities. Vertical DFA should appear when player hits the ground
					ent->client->pers.vertical_dfa_timer = 0;

					zyk_vertical_dfa_effect(ent);
				}

				if (level.quest_map > 0 && ent->client->ps.duelInProgress == qfalse && ent->health > 0)
				{ // zyk: control the quest events which happen in the quest maps, if player can play quests now, is alive and is not in a private duel
					// zyk: fixing exploit in boss battles. If player is in a vehicle, kill the player
					if (ent->client->pers.guardian_mode > 0 && ent->client->ps.m_iVehicleNum > 0)
						G_Kill( ent );

					if (ent->client->pers.can_play_quest == 1 && ent->client->pers.quest_afk_timer < level.time)
					{ // zyk: player afk in quest for this amount of time
						ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

						player_die(ent, ent, ent, 100000, MOD_SUICIDE);

						trap->SendServerCommand(-1, va("chat \"^3Quest System: ^7%s ^7afk for %d seconds.\"", ent->client->pers.netname, (zyk_quest_afk_timer.integer/1000)));
					}

					if (level.quest_map == 1)
					{
						zyk_try_get_dark_quest_note(ent, 4);

						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 4)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->r.currentOrigin[0] > 1962 && (int) ent->r.currentOrigin[0] < 2162 && (int) ent->r.currentOrigin[1] > 3989 && (int) ent->r.currentOrigin[1] < 4189 && (int) ent->r.currentOrigin[2] >= 360 && (int) ent->r.currentOrigin[2] <= 369)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_water", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,2062,4089,361,90,"guardian_boss_1",2062,4189,500,90,1);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}

						if (ent->client->pers.universe_quest_progress == 3 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 4 && 
							ent->client->pers.universe_quest_timer < level.time && (int) ent->client->ps.origin[0] > 2720 && (int) ent->client->ps.origin[0] < 2840 && 
							(int) ent->client->ps.origin[1] > 3944 && (int) ent->client->ps.origin[1] < 3988 && (int) ent->client->ps.origin[2] == 1432)
						{ // zyk: fourth Universe Quest objective
							if ((ent->client->pers.universe_quest_messages >= 0 && ent->client->pers.universe_quest_messages <= 3) || ent->client->pers.universe_quest_messages == 5 || 
								 ent->client->pers.universe_quest_messages == 8 || (ent->client->pers.universe_quest_messages >= 14 && ent->client->pers.universe_quest_messages <= 16))
							{
								zyk_text_message(ent, va("universe/mission_3/mission_3_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
							}
							else
							{
								zyk_text_message(ent, va("universe/mission_3/mission_3_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
							}

							ent->client->pers.universe_quest_messages++;
							ent->client->pers.universe_quest_timer = level.time + 5000;

							if (ent->client->pers.universe_quest_messages == 17)
							{ // zyk: complete the objective
								ent->client->pers.universe_quest_objective_control = -1;
								if (ent->client->pers.universe_quest_counter & (1 << 29))
								{ // zyk: if player is in Challenge Mode, do not remove this bit value
									ent->client->pers.universe_quest_counter = 0;
									ent->client->pers.universe_quest_counter |= (1 << 29);
								}
								else
									ent->client->pers.universe_quest_counter = 0;
								ent->client->pers.universe_quest_progress = 4;
								clean_note_model();
								save_account(ent, qtrue);
								quest_get_new_player(ent);
							}
						}

						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 3 && ent->client->pers.universe_quest_timer < level.time)
						{ // zyk: third Universe Quest objective, sages should appear in this map
							gentity_t *npc_ent = NULL;
							int change_player = 0;

							if (ent->client->pers.universe_quest_messages == 0)
							{
								if (!(ent->client->pers.universe_quest_counter & (1 << 3)))
								{
									zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);
								}
								
								npc_ent = Zyk_NPC_SpawnType("sage_of_light",2780,4034,1583,0);
							}
							else if (ent->client->pers.universe_quest_messages == 1)
								npc_ent = Zyk_NPC_SpawnType("sage_of_eternity",2780,3966,1583,0);
							else if (ent->client->pers.universe_quest_messages == 2)
								npc_ent = Zyk_NPC_SpawnType("sage_of_darkness",2780,3904,1583,0);
							else if (ent->client->pers.universe_quest_messages == 6)
								zyk_text_message(ent, "universe/mission_2/mission_2_sage_of_light", qtrue, qfalse);
							else if (ent->client->pers.universe_quest_messages == 8)
							{
								ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 3000;

								zyk_text_message(ent, "universe/mission_2/mission_2_sage_of_eternity", qtrue, qfalse, ent->client->pers.netname);
								ent->client->pers.universe_quest_counter |= (1 << 1);
								save_account(ent, qtrue);

								universe_quest_artifacts_checker(ent);

								change_player = 1;
							}
							else if (ent->client->pers.universe_quest_messages == 9)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_got_artifact_sage", qtrue, qfalse);
								change_player = 1;
							}
							else if (ent->client->pers.universe_quest_messages == 12)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_sage_of_darkness", qtrue, qfalse, ent->client->pers.netname);
							}
							
							if (npc_ent)
							{
								npc_ent->client->pers.universe_quest_objective_control = ent->client->pers.universe_quest_messages;
							}

							if (ent->client->pers.universe_quest_messages == 3 && ent->client->pers.universe_quest_artifact_holder_id == -1 && !(ent->client->pers.universe_quest_counter & (1 << 3)))
							{
								npc_ent = Zyk_NPC_SpawnType("quest_mage",-396,-287,-150,-153);
								if (npc_ent)
								{ // zyk: spawning the quest_mage artifact holder
									npc_ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 5500;
									npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;

									ent->client->pers.universe_quest_artifact_holder_id = 3;
								}
							}

							if (ent->client->pers.universe_quest_messages < 3)
								ent->client->pers.universe_quest_messages++;

							// zyk: after displaying the message, sets this to 15 (above 12, the last message possible) so only when player talks to a sage the message appears again
							if (ent->client->pers.universe_quest_messages > 3)
								ent->client->pers.universe_quest_messages = 15;

							ent->client->pers.universe_quest_timer = level.time + 1000;

							if (change_player == 1)
								quest_get_new_player(ent);
						}
					}
					else if (level.quest_map == 2)
					{
						zyk_try_get_dark_quest_note(ent, 5);
					}
					else if (level.quest_map == 3)
					{
						zyk_try_get_dark_quest_note(ent, 6);
					}
					else if (level.quest_map == 4)
					{
						zyk_try_get_dark_quest_note(ent, 7);

						if (level.chaos_portal_id != -1)
						{ // zyk: portal to the Realm of Souls
							gentity_t *chaos_portal = &g_entities[level.chaos_portal_id];

							if (chaos_portal && (int)Distance(chaos_portal->s.origin, ent->client->ps.origin) < 40)
							{
								vec3_t origin;
								vec3_t angles;

								origin[0] = 2230.0f;
								origin[1] = 3425.0f;
								origin[2] = -9950.0f;
								angles[0] = 0.0f;
								angles[1] = 0.0f;
								angles[2] = 0.0f;

								zyk_TeleportPlayer(ent, origin, angles);
							}

							if ((ent->client->pers.can_play_quest == 0 || ent->client->pers.universe_quest_messages > 50) && (int)ent->client->ps.origin[2] < -11000)
							{ // zyk: player cannot leave the arena
								G_Kill(ent);
							}
						}

						if (ent->client->pers.universe_quest_progress == 17 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, third mission of Time Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, 2077, 3425, 973);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 400)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 1000;
								}
								else if (ent->client->pers.universe_quest_messages > 0 && ent->client->pers.universe_quest_messages < 10)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 1000;
								}
								else if (ent->client->pers.universe_quest_messages > 11)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages == 1)
								{
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_red.md3", 2200, 3374, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 0;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 2)
								{
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_green.md3", 2200, 3482, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 1;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_blue.md3", 2077, 3294, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 2;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{ // zyk: yellow crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_red.md3", 2077, 3562, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 3;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{ // zyk: yellow crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_green.md3", 2077, 3562, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 3;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{ // zyk: cyan crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_green.md3", 1945, 3374, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 4;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 7)
								{ // zyk: cyan crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_blue.md3", 1945, 3374, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 4;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 8)
								{ // zyk: purple crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_red.md3", 1945, 3482, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 5;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 9)
								{ // zyk: purple crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_blue.md3", 1945, 3482, 952, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 5;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 11)
								{ // zyk: tests if player solved the puzzle correctly and opens the gate
									zyk_quest_item("env/btend", 2336, 3425, 947, "", "");
									zyk_quest_item("env/huge_lightning", 2336, 3425, 952, "", "");
									zyk_quest_item("env/lbolt1", 2336, 3425, 952, "", "");

									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", 2200, 3425, 952, 0);
								else if (ent->client->pers.universe_quest_messages >= 14 && ent->client->pers.universe_quest_messages <= 22)
								{
									if (ent->client->pers.universe_quest_messages == 14 || ent->client->pers.universe_quest_messages == 15 || ent->client->pers.universe_quest_messages == 18 ||
										ent->client->pers.universe_quest_messages == 21)
									{
										zyk_text_message(ent, va("universe/mission_17_time/mission_17_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_17_time/mission_17_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 23)
								{
									ent->client->pers.universe_quest_progress = 18;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 18 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, fourth mission of Time Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, 2336, 3425, -10000);

								if (ent->client->pers.universe_quest_messages == 50 && Distance(ent->client->ps.origin, zyk_quest_point) < 500)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages > 50 && ent->client->pers.universe_quest_messages < 61)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages >= 61 && ent->client->pers.universe_quest_messages < 80)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 2000;
								}

								if (ent->client->pers.universe_quest_messages < 49)
								{
									zyk_quest_item("models/map_objects/factory/catw2_b.md3", 2336 + 192 * (ent->client->pers.hunter_quest_messages - 3), 3425 + 192 * ((ent->client->pers.universe_quest_messages / 7) - 3), -10000, "-96 -96 -8", "96 96 8");

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;

									ent->client->pers.hunter_quest_messages = (ent->client->pers.hunter_quest_messages + 1) % 7;

									// zyk: remapping the catwalk models to have a glass texture
									if (ent->client->pers.universe_quest_messages == 2)
									{
										zyk_remap_quest_item("textures/factory/cat_floor_b", "textures/factory/env_glass");
										zyk_remap_quest_item("textures/factory/basic2_tiled_b", "textures/factory/env_glass");
									}
								}
								else if (ent->client->pers.universe_quest_messages == 49)
								{ // zyk: opens the gate to the Realm of Souls
									gentity_t *zyk_portal_ent;

									zyk_quest_item("env/btend", 2336, 3425, 947, "", "");
									zyk_quest_item("env/huge_lightning", 2336, 3425, 952, "", "");
									zyk_portal_ent = zyk_quest_item("env/lbolt1", 2336, 3425, 952, "", "");

									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));

									level.chaos_portal_id = zyk_portal_ent->s.number;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages == 52)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_ragnos", 2336, 3425, -9950, 179);

									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_objective_control = -200000;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 53)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_53", qtrue, qfalse, ent->client->pers.netname);

									npc_ent = Zyk_NPC_SpawnType("quest_ragnos", 2073, 3140, -9950, 90);

									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_objective_control = Q_irand(0, 2);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 54)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_54", qtrue, qfalse, ent->client->pers.netname);

									npc_ent = Zyk_NPC_SpawnType("quest_ragnos", 2600, 3600, -9950, -135);

									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_objective_control = Q_irand(0, 2);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 55)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_55", qtrue, qfalse);

									npc_ent = Zyk_NPC_SpawnType("quest_ragnos", 2700, 3425, -9950, 179);

									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_objective_control = Q_irand(0, 2);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 56)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_56", qtrue, qfalse);

									npc_ent = Zyk_NPC_SpawnType("quest_ragnos", 1900, 3800, -9950, 0);

									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_objective_control = Q_irand(0, 2);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 57)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_57", qtrue, qfalse, ent->client->pers.netname);

									npc_ent = Zyk_NPC_SpawnType("quest_ragnos", 2336, 4000, -9950, 179);

									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_objective_control = Q_irand(0, 2);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 58)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_58", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 59)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_59", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 60)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_60", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 61)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_61", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 62)
								{
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_red.md3", 2236, 3365, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 1;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 63)
								{
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_green.md3", 2236, 3485, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 2;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 64)
								{
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_blue.md3", 2336, 3265, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 3;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 65)
								{ // zyk: yellow crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_red.md3", 2336, 3585, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 4;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 66)
								{ // zyk: yellow crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_green.md3", 2336, 3585, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 4;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 67)
								{ // zyk: cyan crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_green.md3", 2436, 3365, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 5;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 68)
								{ // zyk: cyan crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_blue.md3", 2436, 3365, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 5;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 69)
								{ // zyk: purple crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_red.md3", 2436, 3485, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 6;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 70)
								{ // zyk: purple crystal
									gentity_t *crystal_ent = zyk_quest_item("models/map_objects/mp/crystal_blue.md3", 2436, 3485, -9960, "-16 -16 -16", "16 16 16");

									if (crystal_ent)
									{
										crystal_ent->count = 6;
									}
								}
								else if (ent->client->pers.universe_quest_messages > 70 && ent->client->pers.universe_quest_messages < 80)
								{
									int chosen_quest_item = Q_irand(1, 6);
									gentity_t *effect_ent = NULL;

									if (chosen_quest_item == 1)
									{
										effect_ent = zyk_quest_item("env/btend", 2236, 3365, -9960, "", "");
									}
									else if (chosen_quest_item == 2)
									{
										effect_ent = zyk_quest_item("env/btend", 2236, 3485, -9960, "", "");
									}
									else if (chosen_quest_item == 3)
									{
										effect_ent = zyk_quest_item("env/btend", 2336, 3265, -9960, "", "");
									}
									else if (chosen_quest_item == 4)
									{
										effect_ent = zyk_quest_item("env/btend", 2336, 3585, -9960, "", "");
									}
									else if (chosen_quest_item == 5)
									{
										effect_ent = zyk_quest_item("env/btend", 2436, 3365, -9960, "", "");
									}
									else if (chosen_quest_item == 6)
									{
										effect_ent = zyk_quest_item("env/btend", 2436, 3485, -9960, "", "");
									}

									// zyk: setting to 0 because it will be used to solve the puzzle
									ent->client->pers.hunter_quest_messages = 0;

									// zyk: setting the chosen crystal in the puzzle order
									level.quest_puzzle_order[ent->client->pers.universe_quest_messages - 71] = chosen_quest_item;

									if (effect_ent)
									{
										level.special_power_effects[effect_ent->s.number] = ent->s.number;
										level.special_power_effects_timer[effect_ent->s.number] = level.time + 2000;

										G_Sound(effect_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tram_boost.mp3"));
									}
								}
								else if (ent->client->pers.universe_quest_messages == 81)
								{
									gentity_t *effect_ent = zyk_quest_item("env/lbolt1", 2336, 3425, -9960, "", "");

									if (effect_ent)
									{
										level.special_power_effects[effect_ent->s.number] = ent->s.number;
										level.special_power_effects_timer[effect_ent->s.number] = level.time + 5000;

										G_Sound(effect_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tram_boost.mp3"));
									}

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 3000;
								}
								else if (ent->client->pers.universe_quest_messages == 82)
								{
									zyk_text_message(ent, "universe/mission_18_time/mission_18_time_82", qtrue, qfalse, ent->client->pers.netname);

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 83)
								{
									ent->client->pers.universe_quest_progress = 19;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 19 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, fifth mission of Time Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, 2336, 3425, -10000);

								if (ent->client->pers.universe_quest_messages == 50 && Distance(ent->client->ps.origin, zyk_quest_point) < 500)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 50 && (int)ent->client->ps.origin[2] < -5000)
								{ // zyk: player passed the former mission and is falling
									vec3_t origin;
									vec3_t angles;

									origin[0] = 2230.0f;
									origin[1] = 3425.0f;
									origin[2] = -9950.0f;
									angles[0] = 0.0f;
									angles[1] = 0.0f;
									angles[2] = 0.0f;

									// zyk: stopping the fall
									VectorSet(ent->client->ps.velocity, 0, 0, 0);

									zyk_TeleportPlayer(ent, origin, angles);
								}
								else if (ent->client->pers.universe_quest_messages > 50 && ent->client->pers.universe_quest_messages < 66)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages < 49)
								{
									zyk_quest_item("models/map_objects/factory/catw2_b.md3", 2336 + 192 * (ent->client->pers.hunter_quest_messages - 3), 3425 + 192 * ((ent->client->pers.universe_quest_messages / 7) - 3), -10000, "-96 -96 -8", "96 96 8");

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;

									ent->client->pers.hunter_quest_messages = (ent->client->pers.hunter_quest_messages + 1) % 7;

									// zyk: remapping the catwalk models to have a glass texture
									if (ent->client->pers.universe_quest_messages == 2)
									{
										zyk_remap_quest_item("textures/factory/cat_floor_b", "textures/factory/env_glass");
										zyk_remap_quest_item("textures/factory/basic2_tiled_b", "textures/factory/env_glass");
									}
								}
								else if (ent->client->pers.universe_quest_messages == 49)
								{ // zyk: opens the gate to the Realm of Souls
									gentity_t *zyk_portal_ent;

									zyk_quest_item("env/btend", 2336, 3425, 947, "", "");
									zyk_quest_item("env/huge_lightning", 2336, 3425, 952, "", "");
									zyk_portal_ent = zyk_quest_item("env/lbolt1", 2336, 3425, 952, "", "");

									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));

									level.chaos_portal_id = zyk_portal_ent->s.number;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages == 52)
									npc_ent = Zyk_NPC_SpawnType("soul_of_sorrow", 2336, 3425, -9950, 179);
								else if (ent->client->pers.universe_quest_messages >= 53 && ent->client->pers.universe_quest_messages <= 65)
								{
									if (ent->client->pers.universe_quest_messages == 53 || ent->client->pers.universe_quest_messages == 54 || ent->client->pers.universe_quest_messages == 56 ||
										ent->client->pers.universe_quest_messages == 64)
									{
										zyk_text_message(ent, va("universe/mission_19_time/mission_19_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_19_time/mission_19_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 66)
								{
									ent->client->pers.universe_quest_progress = 20;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 20 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, boss battle mission of Time Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, 2336, 3425, -10000);

								if (ent->client->pers.universe_quest_messages == 50 && Distance(ent->client->ps.origin, zyk_quest_point) < 500)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 50 && (int)ent->client->ps.origin[2] < -5000)
								{ // zyk: player passed the former mission and is falling
									vec3_t origin;
									vec3_t angles;

									origin[0] = 2230.0f;
									origin[1] = 3425.0f;
									origin[2] = -9950.0f;
									angles[0] = 0.0f;
									angles[1] = 0.0f;
									angles[2] = 0.0f;

									// zyk: stopping the fall
									VectorSet(ent->client->ps.velocity, 0, 0, 0);

									zyk_TeleportPlayer(ent, origin, angles);
								}
								else if (ent->client->pers.universe_quest_messages > 50 && ent->client->pers.universe_quest_messages < 53)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages < 49)
								{
									zyk_quest_item("models/map_objects/factory/catw2_b.md3", 2336 + 192 * (ent->client->pers.hunter_quest_messages - 3), 3425 + 192 * ((ent->client->pers.universe_quest_messages / 7) - 3), -10000, "-96 -96 -8", "96 96 8");

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;

									ent->client->pers.hunter_quest_messages = (ent->client->pers.hunter_quest_messages + 1) % 7;

									// zyk: remapping the catwalk models to have a glass texture
									if (ent->client->pers.universe_quest_messages == 2)
									{
										zyk_remap_quest_item("textures/factory/cat_floor_b", "textures/factory/env_glass");
										zyk_remap_quest_item("textures/factory/basic2_tiled_b", "textures/factory/env_glass");
									}
								}
								else if (ent->client->pers.universe_quest_messages == 49)
								{ // zyk: opens the gate to the Realm of Souls
									gentity_t *zyk_portal_ent;

									zyk_quest_item("env/btend", 2336, 3425, 947, "", "");
									zyk_quest_item("env/huge_lightning", 2336, 3425, 952, "", "");
									zyk_portal_ent = zyk_quest_item("env/lbolt1", 2336, 3425, 952, "", "");

									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));

									level.chaos_portal_id = zyk_portal_ent->s.number;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages == 52)
								{
									spawn_boss(ent, 2200, 3425, -9960, 0, "soul_of_sorrow", 2336, 3425, -9950, 179, 21);
								}
								else if (ent->client->pers.universe_quest_messages == 54)
								{
									ent->client->pers.universe_quest_progress = 21;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 21 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, final mission of Time Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, 2336, 3425, -10000);

								if (ent->client->pers.universe_quest_messages == 50 && Distance(ent->client->ps.origin, zyk_quest_point) < 500)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 50 && (int)ent->client->ps.origin[2] < -5000)
								{ // zyk: player passed the former mission and is falling
									vec3_t origin;
									vec3_t angles;

									origin[0] = 2230.0f;
									origin[1] = 3425.0f;
									origin[2] = -9950.0f;
									angles[0] = 0.0f;
									angles[1] = 0.0f;
									angles[2] = 0.0f;

									// zyk: stopping the fall
									VectorSet(ent->client->ps.velocity, 0, 0, 0);

									zyk_TeleportPlayer(ent, origin, angles);
								}
								else if (ent->client->pers.universe_quest_messages > 50 && ent->client->pers.universe_quest_messages < 72)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages < 49)
								{
									zyk_quest_item("models/map_objects/factory/catw2_b.md3", 2336 + 192 * (ent->client->pers.hunter_quest_messages - 3), 3425 + 192 * ((ent->client->pers.universe_quest_messages / 7) - 3), -10000, "-96 -96 -8", "96 96 8");

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;

									ent->client->pers.hunter_quest_messages = (ent->client->pers.hunter_quest_messages + 1) % 7;

									// zyk: remapping the catwalk models to have a glass texture
									if (ent->client->pers.universe_quest_messages == 2)
									{
										zyk_remap_quest_item("textures/factory/cat_floor_b", "textures/factory/env_glass");
										zyk_remap_quest_item("textures/factory/basic2_tiled_b", "textures/factory/env_glass");
									}
								}
								else if (ent->client->pers.universe_quest_messages == 49)
								{ // zyk: opens the gate to the Realm of Souls
									gentity_t *zyk_portal_ent;

									zyk_quest_item("env/btend", 2336, 3425, 947, "", "");
									zyk_quest_item("env/huge_lightning", 2336, 3425, 952, "", "");
									zyk_portal_ent = zyk_quest_item("env/lbolt1", 2336, 3425, 952, "", "");

									G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));

									level.chaos_portal_id = zyk_portal_ent->s.number;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages == 52)
									npc_ent = Zyk_NPC_SpawnType("soul_of_sorrow", 2336, 3425, -9950, 179);
								else if (ent->client->pers.universe_quest_messages == 58)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", 2136, 3425, -9950, 0);
								else if (ent->client->pers.universe_quest_messages >= 53 && ent->client->pers.universe_quest_messages != 58 && ent->client->pers.universe_quest_messages <= 71)
								{
									if (ent->client->pers.universe_quest_messages == 53 || ent->client->pers.universe_quest_messages == 54 || ent->client->pers.universe_quest_messages == 56 ||
										ent->client->pers.universe_quest_messages == 59 || ent->client->pers.universe_quest_messages == 65 || ent->client->pers.universe_quest_messages == 67 || 
										ent->client->pers.universe_quest_messages == 69)
									{
										zyk_text_message(ent, va("universe/mission_21_time/mission_21_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_21_time/mission_21_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 72)
								{ // zyk: teleports the player outside the Realm of Souls
									vec3_t origin;
									vec3_t angles;

									origin[0] = 2200.0f;
									origin[1] = 3425.0f;
									origin[2] = 952.0f;
									angles[0] = 0.0f;
									angles[1] = 0.0f;
									angles[2] = 0.0f;

									zyk_TeleportPlayer(ent, origin, angles);

									ent->client->pers.universe_quest_progress = 22;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
						}
					}
					else if (level.quest_map == 5)
					{
						// zyk: Dark Quest Note
						zyk_try_get_dark_quest_note(ent, 8);

						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 3 && !(ent->client->pers.universe_quest_counter & (1 << 9)) && ent->client->pers.universe_quest_timer < level.time)
						{
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);

								npc_ent = Zyk_NPC_SpawnType("quest_mage",724,5926,951,31);
								if (npc_ent)
								{
									npc_ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 5500;

									npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;
									ent->client->pers.universe_quest_artifact_holder_id = 9;
								}
							}

							if (ent->client->pers.universe_quest_messages < 1)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
						}

						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 12)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->r.currentOrigin[0] > -5648 && (int) ent->r.currentOrigin[0] < -5448 && (int) ent->r.currentOrigin[1] > 11448 && (int) ent->r.currentOrigin[1] < 11648 && (int) ent->r.currentOrigin[2] >= 980 && (int) ent->r.currentOrigin[2] <= 1000)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_ice", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,-4652,11607,991,179,"guardian_boss_10",-5623,11598,991,0,16);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}
					}
					else if (level.quest_map == 6)
					{
						// zyk: Dark Quest Note
						zyk_try_get_dark_quest_note(ent, 9);

						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 3 && !(ent->client->pers.universe_quest_counter & (1 << 6)) && ent->client->pers.universe_quest_timer < level.time)
						{ // zyk: Universe Quest artifact
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_red",2120,-1744,39,90);
							}
							else if (ent->client->pers.universe_quest_messages == 1)
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_red",2318,-1744,39,90);
							else if (ent->client->pers.universe_quest_messages == 2)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_mage",2214,-1744,39,90);
								if (npc_ent)
								{
									npc_ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 5500;

									npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;
									ent->client->pers.universe_quest_artifact_holder_id = 6;
								}
							}

							if (ent->client->pers.universe_quest_messages < 3)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
						}
						else if (ent->client->pers.universe_quest_progress == 18 && ent->client->pers.can_play_quest == 1 && 
								 ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: Universe Quest, Settle the Score mission
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.universe_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -3263, 1330, 73, -90);
								else if (ent->client->pers.universe_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -3139, 1330, 73, -90);
								else if (ent->client->pers.universe_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4367, 1019, 73, 90);
								else if (ent->client->pers.universe_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4304, 1019, 73, 90);
								else if (ent->client->pers.universe_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4930, 1359, 57, -90);
								else if (ent->client->pers.universe_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4930, 1235, 73, 90);
								else if (ent->client->pers.universe_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -6697, 699, -38, 0);
								else if (ent->client->pers.universe_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -6697, 461, -38, 0);
								else if (ent->client->pers.universe_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -5160, 473, 25, -45);
								else if (ent->client->pers.universe_quest_messages == 9)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -5236, 473, 25, -45);
								else if (ent->client->pers.universe_quest_messages == 10)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -1193, -216, 37, 179);
								else if (ent->client->pers.universe_quest_messages == 11)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -1193, -98, 37, 179);
								else if (ent->client->pers.universe_quest_messages == 12)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2018, -526, 37, 90);
								else if (ent->client->pers.universe_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2121, -526, 37, 90);
								else if (ent->client->pers.universe_quest_messages == 14)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 534, -513, 345, 90);
								else if (ent->client->pers.universe_quest_messages == 15)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 651, -513, 345, 90);
								else if (ent->client->pers.universe_quest_messages == 16)
									npc_ent = Zyk_NPC_SpawnType("ymir_boss", -4193, 771, 401, 135);
								else if (ent->client->pers.universe_quest_messages == 17)
								{
									vec3_t zyk_quest_point;

									VectorSet(zyk_quest_point, -4193, 771, 401);

									if (Distance(ent->client->ps.origin, zyk_quest_point) < 80)
									{
										int j = 0;

										for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
										{
											npc_ent = &g_entities[j];

											if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "quest_mage") == 0 && npc_ent->die)
											{
												npc_ent->health = 0;
												npc_ent->client->ps.stats[STAT_HEALTH] = 0;
												if (npc_ent->die)
												{
													npc_ent->die(npc_ent, npc_ent, npc_ent, 100, MOD_UNKNOWN);
												}
											}
											else if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "ymir_boss") == 0)
											{ // zyk: placing him in his original spot
												vec3_t npc_origin, npc_angles;

												VectorSet(npc_origin, -4193, 771, 401);
												VectorSet(npc_angles, 0, 135, 0);

												zyk_TeleportPlayer(npc_ent, npc_origin, npc_angles);
											}
										}

										ent->client->pers.universe_quest_messages = 18;
									}
								}

								if (ent->client->pers.universe_quest_messages == 18)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -4328, 750, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 19)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -4338, 705, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 20)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -4279, 646, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 21)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -4207, 631, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 22)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -4038, 807, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 23)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -4070, 843, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 24)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -4116, 881, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 25)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -4174, 920, 401, -135);
								}

								if (ent->client->pers.universe_quest_messages >= 26 && ent->client->pers.universe_quest_messages <= 28)
								{
									zyk_text_message(ent, va("universe/mission_18_sages/mission_18_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 29)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4185, 856, 451, -135);
								else if (ent->client->pers.universe_quest_messages == 30)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4144, 804, 451, -135);
								else if (ent->client->pers.universe_quest_messages == 31)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4103, 777, 451, -135);
								else if (ent->client->pers.universe_quest_messages == 32)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4277, 757, 451, 45);
								else if (ent->client->pers.universe_quest_messages == 33)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4236, 719, 451, 45);
								else if (ent->client->pers.universe_quest_messages == 34)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4195, 685, 451, 45);
								else if (ent->client->pers.universe_quest_messages == 35)
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -4300, 860, 451, -45);
								else if (ent->client->pers.universe_quest_messages >= 36 && ent->client->pers.universe_quest_messages <= 45)
								{
									if (ent->client->pers.universe_quest_messages == 41)
									{
										npc_ent = Zyk_NPC_SpawnType("sage_of_universe", -4122, 703, 451, 135);
									}

									if (ent->client->pers.universe_quest_messages == 36 || ent->client->pers.universe_quest_messages == 39)
									{
										zyk_text_message(ent, va("universe/mission_18_sages/mission_18_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_18_sages/mission_18_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 46)
								{
									int j = 0;

									for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
									{
										npc_ent = &g_entities[j];

										if (npc_ent && npc_ent->NPC && (Q_stricmp(npc_ent->NPC_type, "ymir_boss") == 0 || Q_stricmp(npc_ent->NPC_type, "thor_boss") == 0))
										{
											vec3_t npc_origin, npc_angles;

											VectorSet(npc_origin, -5871, 1440, 150);
											VectorSet(npc_angles, 0, 0, 0);

											zyk_TeleportPlayer(npc_ent, npc_origin, npc_angles);
										}
									}
								}
								else if (ent->client->pers.universe_quest_messages == 47)
								{
									ent->client->pers.universe_quest_progress = 19;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages < 16 && npc_ent)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;
								}
								else if (ent->client->pers.universe_quest_messages == 16 && npc_ent)
								{ // zyk: try to spawn Ymir again if npc_ent is NULL
									npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;
								}
								else if (ent->client->pers.universe_quest_messages > 17 && ent->client->pers.universe_quest_messages < 26 && npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages > 25 && ent->client->pers.universe_quest_messages < 29)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages > 28 && ent->client->pers.universe_quest_messages < 35 && npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 200;
								}
								else if (ent->client->pers.universe_quest_messages == 35 && npc_ent)
								{ // zyk: try to spawn Thor again if npc_ent is NULL
									npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;
								}
								else if (ent->client->pers.universe_quest_messages > 35 && ent->client->pers.universe_quest_messages < 41)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 41 && npc_ent)
								{ // zyk: try to spawn sage of universe again if npc_ent is NULL
									int j = 0;

									npc_ent->client->pers.universe_quest_messages = -2000;

									ultra_drain(npc_ent, 450, 30, 8000);

									for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
									{
										npc_ent = &g_entities[j];

										if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "quest_mage") == 0 && npc_ent->die)
										{
											npc_ent->health = 0;
											npc_ent->client->ps.stats[STAT_HEALTH] = 0;
											if (npc_ent->die)
											{
												npc_ent->die(npc_ent, npc_ent, npc_ent, 100, MOD_UNKNOWN);
											}
										}
									}

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages > 41 && ent->client->pers.universe_quest_messages < 47)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 19 && ent->client->pers.can_play_quest == 1 &&
								ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: Universe Quest Sages Sequel, The Crystal of Magic mission
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -4193, 771, 401);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 80)
								{
									ent->client->pers.universe_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -4328, 750, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -4338, 705, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -4279, 646, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -4207, 631, 401, 45);
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -4038, 807, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -4070, 843, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 7)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -4116, 881, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 8)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -4174, 920, 401, -135);
								}
								else if (ent->client->pers.universe_quest_messages == 9)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe", -4122, 703, 451, 135);
								}
								else if (ent->client->pers.universe_quest_messages >= 10 && ent->client->pers.universe_quest_messages <= 26)
								{
									if (ent->client->pers.universe_quest_messages == 10 || ent->client->pers.universe_quest_messages == 16 || ent->client->pers.universe_quest_messages == 21 || 
										ent->client->pers.universe_quest_messages == 25)
									{
										zyk_text_message(ent, va("universe/mission_19_sages/mission_19_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_19_sages/mission_19_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 27)
								{
									ent->client->pers.universe_quest_progress = 20;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages > 0 && npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages > 9 && ent->client->pers.universe_quest_messages < 27)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 20 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: Universe Quest Sages Sequel, boss battle
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -5849, 1438, 57);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 200)
								{
									ent->client->pers.universe_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("ymir_boss", -5849, 1638, 57, -90);
								}
								else if (ent->client->pers.universe_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -5849, 1238, 57, 90);
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									zyk_text_message(ent, "universe/mission_20_sages/mission_20_sages_ymir", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									zyk_text_message(ent, "universe/mission_20_sages/mission_20_sages_thor", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{
									int j = 0;

									gentity_t *new_ent = G_Spawn();

									zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
									zyk_set_entity_field(new_ent, "spawnflags", "65537");
									zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -5467, 1438, 70));

									zyk_set_entity_field(new_ent, "angles", va("%d %d 0", 90, 0));

									zyk_set_entity_field(new_ent, "mins", "-8 -64 -64");
									zyk_set_entity_field(new_ent, "maxs", "8 64 64");

									zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

									zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

									zyk_spawn_entity(new_ent);

									new_ent = G_Spawn();

									zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
									zyk_set_entity_field(new_ent, "spawnflags", "65537");
									zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -6248, 1438, 70));

									zyk_set_entity_field(new_ent, "angles", va("%d %d 0", 90, 0));

									zyk_set_entity_field(new_ent, "mins", "-8 -64 -64");
									zyk_set_entity_field(new_ent, "maxs", "8 64 64");

									zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

									zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

									zyk_spawn_entity(new_ent);

									for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
									{
										npc_ent = &g_entities[j];

										if (npc_ent && npc_ent->NPC)
										{
											G_FreeEntity(npc_ent);
										}
									}

									npc_ent = NULL;

									spawn_boss(ent, -5849, 1438, 57, 179, "ymir_boss", -5849, 1638, 57, -90, 15);
									spawn_boss(ent, -5849, 1438, 57, 179, "thor_boss", -5849, 1238, 57, 90, 15);
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{ // zyk: sage of universe heals the hero during the battle
									ent->health += 20;
									ent->client->ps.stats[STAT_ARMOR] += 20;
									ent->client->pers.magic_power += 20;

									if (ent->health > ent->client->pers.max_rpg_health)
										ent->health = ent->client->pers.max_rpg_health;

									if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield)
										ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;

									if (ent->client->pers.magic_power > zyk_max_magic_power(ent))
									{
										ent->client->pers.magic_power = zyk_max_magic_power(ent);
									}

									send_rpg_events(2000);

									G_Sound(ent, CHAN_ITEM, G_SoundIndex("sound/weapons/force/heal.wav"));

									zyk_text_message(ent, "universe/mission_20_sages/mission_20_sages_sage", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 7)
								{ // zyk: Hero defeated both bosses
									ent->client->pers.universe_quest_progress = 21;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages > 0 && npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages > 2 && ent->client->pers.universe_quest_messages < 6)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									ent->client->pers.universe_quest_timer = level.time + 20000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 21 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: Universe Quest Sages Sequel, final mission
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -5849, 1438, 57);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 100)
								{
									ent->client->pers.universe_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -6049, 1638, 57, -90);
								}
								else if (ent->client->pers.universe_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -5949, 1638, 57, -90);
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -5849, 1638, 57, -90);
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -5749, 1638, 57, -90);
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -6049, 1238, 57, 90);
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -5949, 1238, 57, 90);
								}
								else if (ent->client->pers.universe_quest_messages == 7)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -5849, 1238, 57, 90);
								}
								else if (ent->client->pers.universe_quest_messages == 8)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -5749, 1238, 57, 90);
								}
								else if (ent->client->pers.universe_quest_messages == 9)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe", -6049, 1438, 57, 0);
								}
								else if (ent->client->pers.universe_quest_messages >= 10 && ent->client->pers.universe_quest_messages <= 30)
								{
									if (ent->client->pers.universe_quest_messages == 10 || ent->client->pers.universe_quest_messages == 11 || ent->client->pers.universe_quest_messages == 17 ||
										ent->client->pers.universe_quest_messages == 26 || ent->client->pers.universe_quest_messages == 27)
									{
										zyk_text_message(ent, va("universe/mission_21_sages/mission_21_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_21_sages/mission_21_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 31)
								{
									ent->client->pers.universe_quest_progress = 22;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages > 0 && npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages > 9 && ent->client->pers.universe_quest_messages < 31)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 16 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, Confrontation mission
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.universe_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("ymir_boss", -5849, 1638, 57, -90);
								else if (ent->client->pers.universe_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -5849, 1238, 57, 90);
								else if (ent->client->pers.universe_quest_messages == 2)
								{
									vec3_t zyk_quest_point;

									VectorSet(zyk_quest_point, -5849, 1438, 57);

									if (Distance(ent->client->ps.origin, zyk_quest_point) < 500)
									{
										int j = 0;

										for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
										{
											npc_ent = &g_entities[j];

											if (npc_ent && npc_ent->client && npc_ent->NPC && (Q_stricmp(npc_ent->NPC_type, "ymir_boss") == 0 || Q_stricmp(npc_ent->NPC_type, "thor_boss") == 0))
											{
												npc_ent->client->pers.universe_quest_messages = -10000;
											}
										}

										npc_ent = NULL;
										ent->client->pers.universe_quest_messages = 3;
									}
								}

								if (ent->client->pers.universe_quest_messages == 3)
								{
									zyk_text_message(ent, "universe/mission_16_guardians/mission_16_guardians_3", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -5849, 1438, 57, 0);
									zyk_text_message(ent, "universe/mission_16_guardians/mission_16_guardians_4", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{
									int j = 0;

									Zyk_NPC_SpawnType("quest_mage", -5949, 1438, 57, 0);

									for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
									{
										npc_ent = &g_entities[j];

										if (npc_ent && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "guardian_of_universe") == 0 && npc_ent->client && npc_ent->client->pers.guardian_timer < level.time)
										{
											ultra_drain(npc_ent, 450, 30, 8000);

											npc_ent->client->pers.guardian_timer = level.time + 15000;
										}
									}
								}
								else if (ent->client->pers.universe_quest_messages >= 6 && ent->client->pers.universe_quest_messages <= 13)
								{
									if (ent->client->pers.universe_quest_messages == 6 || ent->client->pers.universe_quest_messages == 12)
									{
										zyk_text_message(ent, va("universe/mission_16_guardians/mission_16_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_16_guardians/mission_16_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 14)
								{
									ent->client->pers.universe_quest_progress = 17;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages < 2 && npc_ent)
								{ // zyk: try to spawn Ymir and Thor again if npc_ent is NULL
									npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

									npc_ent->client->pers.universe_quest_messages = -2000;

									npc_ent->client->pers.universe_quest_objective_control = ent->s.number;

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 200;
								}
								else if (ent->client->pers.universe_quest_messages > 2 && ent->client->pers.universe_quest_messages < 5)
								{
									if (npc_ent)
									{ // zyk: Guardian of Universe
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

										npc_ent->client->pers.universe_quest_messages = -10000;
										npc_ent->client->pers.universe_quest_objective_control = ent->s.number;
									}

									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 3000;
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{ // zyk: spawning mages to help Ymir and Thor
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages > 5 && ent->client->pers.universe_quest_messages < 14)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 15 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: Universe Quest, first mission in Thor Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.universe_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -3263, 1330, 73, -90);
								else if (ent->client->pers.universe_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -3139, 1330, 73, -90);
								else if (ent->client->pers.universe_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4367, 1019, 73, 90);
								else if (ent->client->pers.universe_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4304, 1019, 73, 90);
								else if (ent->client->pers.universe_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4930, 1359, 57, -90);
								else if (ent->client->pers.universe_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -4930, 1235, 73, 90);
								else if (ent->client->pers.universe_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -6697, 699, -38, 0);
								else if (ent->client->pers.universe_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -6697, 461, -38, 0);
								else if (ent->client->pers.universe_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -5160, 473, 25, -45);
								else if (ent->client->pers.universe_quest_messages == 9)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -5236, 473, 25, -45);
								else if (ent->client->pers.universe_quest_messages == 10)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -1193, -216, 37, 179);
								else if (ent->client->pers.universe_quest_messages == 11)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -1193, -98, 37, 179);
								else if (ent->client->pers.universe_quest_messages == 12)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2018, -526, 37, 90);
								else if (ent->client->pers.universe_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2121, -526, 37, 90);
								else if (ent->client->pers.universe_quest_messages == 14)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 534, -513, 345, 90);
								else if (ent->client->pers.universe_quest_messages == 15)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 651, -513, 345, 90);
								else if (ent->client->pers.universe_quest_messages == 16)
									npc_ent = Zyk_NPC_SpawnType("ymir_boss", -5849, 1538, 57, -90);
								else if (ent->client->pers.universe_quest_messages == 17)
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -5849, 1338, 57, 90);

								if (npc_ent)
								{
									if (ent->client->pers.universe_quest_messages == 16 || ent->client->pers.universe_quest_messages == 17)
									{
										npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
									}
									else
									{ // zyk: a mage. Set a value that will be used for the sentences spoken from him
										npc_ent->client->pers.universe_quest_objective_control = Q_irand(0, 3);
									}

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}
								
								if (ent->client->pers.universe_quest_messages == 23)
								{
									vec3_t zyk_quest_point;

									VectorSet(zyk_quest_point, -5849, 1538, 57);

									if (Distance(ent->client->ps.origin, zyk_quest_point) < 200)
									{
										ent->client->pers.universe_quest_messages = 24;
									}
								}

								if (ent->client->pers.universe_quest_messages >= 18 && ent->client->pers.universe_quest_messages != 23 && ent->client->pers.universe_quest_messages <= 34)
								{
									if (ent->client->pers.universe_quest_messages == 18 || ent->client->pers.universe_quest_messages == 19 || ent->client->pers.universe_quest_messages == 22 || 
										ent->client->pers.universe_quest_messages == 28 || ent->client->pers.universe_quest_messages == 29 || ent->client->pers.universe_quest_messages == 32 || 
										ent->client->pers.universe_quest_messages == 34)
									{
										zyk_text_message(ent, va("universe/mission_15_thor/mission_15_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_15_thor/mission_15_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 35)
								{
									ent->client->pers.universe_quest_progress = 16;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages < 18 && npc_ent)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;
								}
								else if (ent->client->pers.universe_quest_messages >= 18 && ent->client->pers.universe_quest_messages != 23 && ent->client->pers.universe_quest_messages < 35)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 16 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: Universe Quest Thor Sequel, Ymir boss battle
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -5849, 1438, 57);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 200)
								{
									ent->client->pers.universe_quest_messages++;
								}
								
								if (ent->client->pers.universe_quest_messages == 1)
								{
									int j = 0;

									gentity_t *new_ent = G_Spawn();

									zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
									zyk_set_entity_field(new_ent, "spawnflags", "65537");
									zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -5467, 1438, 70));

									zyk_set_entity_field(new_ent, "angles", va("%d %d 0", 90, 0));

									zyk_set_entity_field(new_ent, "mins", "-8 -64 -64");
									zyk_set_entity_field(new_ent, "maxs", "8 64 64");

									zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

									zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

									zyk_spawn_entity(new_ent);

									new_ent = G_Spawn();

									zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
									zyk_set_entity_field(new_ent, "spawnflags", "65537");
									zyk_set_entity_field(new_ent, "origin", va("%d %d %d", -6248, 1438, 70));

									zyk_set_entity_field(new_ent, "angles", va("%d %d 0", 90, 0));

									zyk_set_entity_field(new_ent, "mins", "-8 -64 -64");
									zyk_set_entity_field(new_ent, "maxs", "8 64 64");

									zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

									zyk_set_entity_field(new_ent, "targetname", "zyk_quest_models");

									zyk_spawn_entity(new_ent);

									for (j = (MAX_CLIENTS + BODY_QUEUE_SIZE); j < level.num_entities; j++)
									{
										npc_ent = &g_entities[j];

										if (npc_ent && npc_ent->NPC)
										{
											G_FreeEntity(npc_ent);
										}
									}

									npc_ent = NULL;

									spawn_boss(ent, -5849, 1438, 57, 179, "ymir_boss", -6049, 1438, 57, 0, 19);
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{ // zyk: Hero defeated boss
									zyk_NPC_Kill_f("all");
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									ent->client->pers.universe_quest_progress = 17;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 3)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 3000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 17 && ent->client->pers.can_play_quest == 1 &&
								 ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: Universe Quest, The New Leader mission of Thor Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 1)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -5849, 1438, 57, 0);

								if (npc_ent)
								{
									npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -5849, 1438, 57);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 1 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 16)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 4 || ent->client->pers.universe_quest_messages == 12 ||
										ent->client->pers.universe_quest_messages == 15)
									{
										zyk_text_message(ent, va("universe/mission_17_thor/mission_17_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_17_thor/mission_17_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 17)
								{
									ent->client->pers.universe_quest_progress = 18;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
					}
					else if (level.quest_map == 7)
					{
						zyk_try_get_dark_quest_note(ent, 10);

						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 7)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > 1820 && (int) ent->client->ps.origin[0] < 2020 && (int) ent->client->ps.origin[1] > 1968 && (int) ent->client->ps.origin[1] < 2168 && (int) ent->client->ps.origin[2] == 728)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_intelligence", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,1920,2068,729,-90,"guardian_boss_4",1920,982,729,90,4);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}
					}
					else if (level.quest_map == 8)
					{
						if (ent->client->pers.universe_quest_progress == 4 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 5 && ent->client->pers.universe_quest_timer < level.time)
						{ // zyk: fifth mission of Universe Quest
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
								Zyk_NPC_SpawnType("quest_reborn_red",785,-510,1177,-179);
							else if (ent->client->pers.universe_quest_messages == 1)
								Zyk_NPC_SpawnType("quest_reborn_red",253,-510,1177,-179);
							else if (ent->client->pers.universe_quest_messages == 2)
								Zyk_NPC_SpawnType("quest_reborn_boss",512,-315,1177,-90);
							else if (ent->client->pers.universe_quest_messages == 3)
								npc_ent = Zyk_NPC_SpawnType("sage_of_universe",507,-623,537,90);
							else if (ent->client->pers.universe_quest_messages >= 5 && ent->client->pers.universe_quest_messages <= 17)
							{
								if (ent->client->pers.universe_quest_messages != 8 && ent->client->pers.universe_quest_messages != 10 && ent->client->pers.universe_quest_messages != 11 &&
									ent->client->pers.universe_quest_messages != 16)
								{
									zyk_text_message(ent, va("universe/mission_4/mission_4_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
								}
								else
								{
									zyk_text_message(ent, va("universe/mission_4/mission_4_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 18)
							{
								zyk_text_message(ent, "universe/mission_4/mission_4_end", qtrue, qfalse, ent->client->pers.netname);

								if (ent->client->pers.universe_quest_counter & (1 << 29))
								{ // zyk: if player is in Challenge Mode, do not remove this bit value
									ent->client->pers.universe_quest_counter = 0;
									ent->client->pers.universe_quest_counter |= (1 << 29);
								}
								else
								{
									ent->client->pers.universe_quest_counter = 0;
								}

								ent->client->pers.universe_quest_progress = 5;
								ent->client->pers.universe_quest_objective_control = -1;
								ent->client->pers.universe_quest_messages = 0;

								save_account(ent, qtrue);

								quest_get_new_player(ent);
							}

							if (npc_ent)
							{ // zyk: Sage of Universe, set this player id on him so we can test it to see if the player found him
								npc_ent->client->pers.universe_quest_objective_control = ent-g_entities;
							}

							if (ent->client->pers.universe_quest_messages != 4 && ent->client->pers.universe_quest_messages < 18)
							{
								ent->client->pers.universe_quest_messages++;
							}

							ent->client->pers.universe_quest_timer = level.time + 5000;
						}
					}
					else if (level.quest_map == 9)
					{
						if (ent->client->pers.universe_quest_progress == 0 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_timer < level.time && ent->client->pers.universe_quest_objective_control != -1)
						{ // zyk: first objective of Universe Quest
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_spawn_catwalk_prison(1803, -33, -3135, 90, 0);
								zyk_text_message(ent, "universe/mission_0/mission_0_0", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 1)
							{
								zyk_validate_sages(ent);
								zyk_text_message(ent, "universe/mission_0/mission_0_1", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 2)
							{
								zyk_spawn_quest_reborns();
								zyk_text_message(ent, "universe/mission_0/mission_0_2", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 3)
							{
								zyk_spawn_catwalk_prison(1803, -33, -3135, 90, 0);
								zyk_text_message(ent, "universe/mission_0/mission_0_3", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 7)
							{
								zyk_validate_sages(ent);
								zyk_text_message(ent, "universe/mission_0/mission_0_sage_0", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 8)
							{
								zyk_text_message(ent, "universe/mission_0/mission_0_sage_1", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 9)
							{
								zyk_text_message(ent, "universe/mission_0/mission_0_sage_2", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 10)
							{ // zyk: battle against the red reborns
								if (ent->client->pers.light_quest_messages > 1)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_reborn_red", 2467, -318 + (50 * ent->client->pers.light_quest_messages), -3800, 0);
									ent->client->pers.light_quest_messages--;
								}
								else if (ent->client->pers.light_quest_messages == 1 && ent->client->pers.universe_quest_objective_control == 1)
								{
									ent->client->pers.universe_quest_messages = 9;
									ent->client->pers.light_quest_messages = 0;
									npc_ent = Zyk_NPC_SpawnType("quest_reborn_boss", 2467, -68, -3800, 0);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 12)
							{
								zyk_text_message(ent, "universe/mission_0/mission_0_end", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 13)
							{
								ent->client->pers.universe_quest_progress = 1;
								save_account(ent, qtrue);
								quest_get_new_player(ent);
							}
							else if (ent->client->pers.universe_quest_messages == 14)
							{ // zyk: player failed mission
								quest_get_new_player(ent);
							}

							if (ent->client->pers.universe_quest_progress == 0)
							{
								if (npc_ent && Q_stricmp(npc_ent->NPC_type, "quest_reborn") != 0 && 
									Q_stricmp(npc_ent->NPC_type, "quest_reborn_blue") != 0)
								{ // zyk: sets the player id who must kill this quest reborn
									npc_ent->client->pers.universe_quest_objective_control = ent-g_entities;
								}

								if (ent->client->pers.universe_quest_messages == 4 && (int)ent->client->ps.origin[0] > 1400 &&
									(int)ent->client->ps.origin[0] < 1800 && (int)ent->client->ps.origin[1] > -200 &&
									(int)ent->client->ps.origin[1] < 200 && (int)ent->client->ps.origin[2] > -3200 &&
									(int)ent->client->ps.origin[2] < -3100)
								{ // zyk: player reached the prison gate, shows message
									zyk_text_message(ent, "universe/mission_0/mission_0_prison_door", qtrue, qfalse, ent->client->pers.netname);
									ent->client->pers.universe_quest_messages = 5;
								}
								else if (ent->client->pers.universe_quest_messages == 6 && (int)ent->client->ps.origin[0] > 1400 &&
									(int)ent->client->ps.origin[0] < 1800 && (int)ent->client->ps.origin[1] > -200 &&
									(int)ent->client->ps.origin[1] < 200 && (int)ent->client->ps.origin[2] > -3200 &&
									(int)ent->client->ps.origin[2] < -3100)
								{ // zyk: player reached the sages, remove door
									int zyk_it = 0;
									ent->client->pers.universe_quest_messages = 7;

									for (zyk_it = (MAX_CLIENTS + BODY_QUEUE_SIZE); zyk_it < level.num_entities; zyk_it++)
									{
										gentity_t *catwalk_ent = &g_entities[zyk_it];

										if (catwalk_ent && Q_stricmp(catwalk_ent->classname, "misc_model_breakable") == 0 && 
											Q_stricmp(catwalk_ent->targetname, "zyk_sage_prison") == 0)
										{ // zyk: removes prison door
											G_FreeEntity(catwalk_ent);
											break;
										}
									}
								}
								else if ((ent->client->pers.universe_quest_messages < 4 || ent->client->pers.universe_quest_messages > 6) && 
										 ent->client->pers.universe_quest_messages != 10 && ent->client->pers.universe_quest_messages != 11)
								{
									ent->client->pers.universe_quest_messages++;
								}

								if (ent->client->pers.universe_quest_messages < 10)
									ent->client->pers.universe_quest_timer = level.time + 3000;
								else
									ent->client->pers.universe_quest_timer = level.time + 1000;
							}
						}
						else if (ent->client->pers.universe_quest_progress == 1 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_timer < level.time && ent->client->pers.universe_quest_objective_control > -1)
						{ // zyk: second Universe Quest mission
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_messages == 3)
							{ // zyk: starts conversation when player gets near the sages
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, 2750, -39, -3806);

								if (Distance(ent->client->ps.origin, zyk_quest_point) < 300)
								{
									ent->client->pers.universe_quest_messages = 4;
								}
							}

							if (ent->client->pers.universe_quest_messages == 0)
							{
								gentity_t *door_ent = &g_entities[109];

								npc_ent = Zyk_NPC_SpawnType("sage_of_light", 2750, -115, -3806, 179);

								if (door_ent && Q_stricmp(door_ent->classname, "func_static") == 0)
								{ // zyk: removes the map last door
									G_FreeEntity(door_ent);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 1)
								npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", 2750, -39, -3806, 179);
							else if (ent->client->pers.universe_quest_messages == 2)
								npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", 2750, 39, -3806, 179);

							if (ent->client->pers.universe_quest_messages > 3 && ent->client->pers.universe_quest_messages < 35)
							{
								if (ent->client->pers.universe_quest_messages == 4 || ent->client->pers.universe_quest_messages == 5 || ent->client->pers.universe_quest_messages == 6 || 
									ent->client->pers.universe_quest_messages == 8 || ent->client->pers.universe_quest_messages == 16 || ent->client->pers.universe_quest_messages == 32)
								{
									zyk_text_message(ent, va("universe/mission_1/mission_1_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
								}
								else
								{
									zyk_text_message(ent, va("universe/mission_1/mission_1_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 35)
							{
								zyk_text_message(ent, "universe/mission_1/mission_1_end", qtrue, qfalse);
								
								ent->client->pers.universe_quest_progress = 2;
								
								save_account(ent, qtrue);

								quest_get_new_player(ent);
							}

							if (npc_ent)
							{ // zyk: sages cannot be killed
								npc_ent->client->pers.universe_quest_objective_control = -2000;
							}

							if (ent->client->pers.universe_quest_messages < 3)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 1000;
							}
							else if (ent->client->pers.universe_quest_messages > 3)
							{ // zyk: universe_quest_messages will be 4 or higher when player reaches and press USE key on one of the sages
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 4500;
							}
						}

						if (ent->client->pers.universe_quest_messages < 7)
						{ // zyk: universe_quest_messages must be less than 7. If it is at least 7, player is in the universe quest missions in this map
							zyk_try_get_dark_quest_note(ent, 12);
						}
					}
					else if (level.quest_map == 10)
					{   
						// zyk: battle against the Guardian of Light
						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && light_quest_defeated_guardians(ent) == qtrue && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -1350 && (int) ent->client->ps.origin[0] < -630 && (int) ent->client->ps.origin[1] > -1900 && (int) ent->client->ps.origin[1] < -1400 && (int) ent->client->ps.origin[2] > 5 && (int) ent->client->ps.origin[2] < 56)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages < 3)
								{
									zyk_text_message(ent, va("light/guardian_%d", ent->client->pers.light_quest_messages), qtrue, qfalse);
								}
								else if (ent->client->pers.light_quest_messages == 3)
								{
									spawn_boss(ent,-992,-1802,25,90,"guardian_boss_9",0,0,0,0,8);
								}

								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}

						// zyk: battle against the Guardian of Darkness
						if (ent->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && dark_quest_collected_notes(ent) == qtrue && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -200 && (int) ent->client->ps.origin[0] < 100 && (int) ent->client->ps.origin[1] > 252 && (int) ent->client->ps.origin[1] < 552 && (int) ent->client->ps.origin[2] == -231)
						{
							if (ent->client->pers.hunter_quest_timer < level.time)
							{
								if (ent->client->pers.hunter_quest_messages < 3)
								{
									zyk_text_message(ent, va("dark/guardian_%d", ent->client->pers.hunter_quest_messages), qtrue, qfalse);
								}
								else if (ent->client->pers.hunter_quest_messages == 3)
								{
									spawn_boss(ent,-34,402,-231,90,"guardian_of_darkness",0,0,0,0,9);
								}
								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 3000;
							}
						}

						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 6)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > 412 && (int) ent->client->ps.origin[0] < 612 && (int) ent->client->ps.origin[1] > 4729 && (int) ent->client->ps.origin[1] < 4929 && (int) ent->client->ps.origin[2] >= 55 && (int) ent->client->ps.origin[2] <= 64)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_forest", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,119,4819,33,0,"guardian_boss_3",512,4829,62,179,3);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}

						if (ent->client->pers.eternity_quest_progress < NUMBER_OF_ETERNITY_QUEST_OBJECTIVES && ent->client->pers.eternity_quest_timer < level.time && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -676 && (int) ent->client->ps.origin[0] < -296 && (int) ent->client->ps.origin[1] > 1283 && (int) ent->client->ps.origin[1] < 1663 && (int) ent->client->ps.origin[2] > 60 && (int) ent->client->ps.origin[2] < 120)
						{ // zyk: Eternity Quest
							if (ent->client->pers.eternity_quest_progress < (NUMBER_OF_ETERNITY_QUEST_OBJECTIVES - 1))
							{
								zyk_text_message(ent, va("eternity/riddle_%d", ent->client->pers.eternity_quest_progress), qtrue, qfalse);

								ent->client->pers.eternity_quest_timer = level.time + 30000;
							}
							else if (ent->client->pers.eternity_quest_progress == (NUMBER_OF_ETERNITY_QUEST_OBJECTIVES - 1))
							{
								if (ent->client->pers.eternity_quest_timer == 0)
								{
									zyk_text_message(ent, "eternity/answered_all", qtrue, qfalse);
									ent->client->pers.eternity_quest_timer = level.time + 3000;
								}
								else
								{ // zyk: Guardian of Eternity battle
									spawn_boss(ent,-994,2975,25,90,"guardian_of_eternity",0,0,0,0,10);
								}
							}
						}

						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_objective_control == 3 && ent->client->pers.universe_quest_timer < level.time && ent->client->pers.universe_quest_messages < 8 && !(ent->client->pers.universe_quest_counter & (1 << 8)))
						{
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages > 0 && ent->client->pers.universe_quest_messages < 7)
							{
								zyk_text_message(ent, va("universe/mission_2/mission_2_artifact_guardian_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 7)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_ragnos",-1462,1079,2062,0);
								if (npc_ent)
								{
									npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;
									ent->client->pers.universe_quest_artifact_holder_id = npc_ent-g_entities;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
							ent->client->pers.universe_quest_messages++;
							ent->client->pers.universe_quest_timer = level.time + 5000;
						}

						if (ent->client->pers.universe_quest_progress == 15 && ent->client->pers.can_play_quest == 1 && 
							ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: first mission of Sages Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 4)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -952, -1782, 25, -90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -992, -1782, 25, -90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -1032, -1782, 25, -90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -992, -1782, 25);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 4 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 31)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 7 || 
										ent->client->pers.universe_quest_messages == 9 || ent->client->pers.universe_quest_messages == 10 || ent->client->pers.universe_quest_messages == 12 || 
										ent->client->pers.universe_quest_messages == 24 || ent->client->pers.universe_quest_messages == 27 || ent->client->pers.universe_quest_messages == 28 || 
										ent->client->pers.universe_quest_messages == 29 || ent->client->pers.universe_quest_messages == 31)
									{
										zyk_text_message(ent, va("universe/mission_15_sages/mission_15_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_15_sages/mission_15_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 32)
								{
									ent->client->pers.universe_quest_progress = 16;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 16 && ent->client->pers.can_play_quest == 1 &&
								 ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, second mission of Time Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -769, -2590, 70);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 150)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 2000;
								}
								else if (ent->client->pers.universe_quest_messages > 0 && ent->client->pers.universe_quest_messages < 18)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 2000;
								}
								else if (ent->client->pers.universe_quest_messages > 17 && ent->client->pers.universe_quest_messages < 30)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages == 1)
									zyk_quest_item("models/map_objects/mp/crystal_red.md3", -755, -2500, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 2)
									zyk_quest_item("models/map_objects/mp/crystal_green.md3", -755, -2530, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 3)
									zyk_quest_item("models/map_objects/mp/crystal_blue.md3", -755, -2560, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 4) // zyk: yellow crystal
									zyk_quest_item("models/map_objects/mp/crystal_green.md3", -755, -2590, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 5) // zyk: yellow crystal
									zyk_quest_item("models/map_objects/mp/crystal_red.md3", -755, -2590, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 6)
									zyk_quest_item("models/map_objects/mp/crystal_green.md3", -755, -2620, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 7)
									zyk_quest_item("models/map_objects/mp/crystal_red.md3", -755, -2650, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 8)
									zyk_quest_item("models/map_objects/mp/crystal_blue.md3", -755, -2680, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 9) // zyk: cyan crystal
									zyk_quest_item("models/map_objects/mp/crystal_green.md3", -795, -2500, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 10) // zyk: cyan crystal
									zyk_quest_item("models/map_objects/mp/crystal_blue.md3", -795, -2500, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 11)
									zyk_quest_item("models/map_objects/mp/crystal_red.md3", -795, -2530, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 12)
									zyk_quest_item("models/map_objects/mp/crystal_green.md3", -795, -2560, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 13) // zyk: purple crystal
									zyk_quest_item("models/map_objects/mp/crystal_red.md3", -795, -2590, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 14) // zyk: purple crystal
									zyk_quest_item("models/map_objects/mp/crystal_blue.md3", -795, -2590, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 15)
									zyk_quest_item("models/map_objects/mp/crystal_red.md3", -795, -2620, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 16)
									zyk_quest_item("models/map_objects/mp/crystal_green.md3", -795, -2650, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 17)
									zyk_quest_item("models/map_objects/mp/crystal_blue.md3", -795, -2680, 60, "", "");
								else if (ent->client->pers.universe_quest_messages == 18)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -700, -2590, 70, 179);
								else if (ent->client->pers.universe_quest_messages >= 19 && ent->client->pers.universe_quest_messages <= 29)
								{
									if (ent->client->pers.universe_quest_messages == 19 || ent->client->pers.universe_quest_messages == 20 || ent->client->pers.universe_quest_messages == 22 ||
										ent->client->pers.universe_quest_messages == 28)
									{
										zyk_text_message(ent, va("universe/mission_16_time/mission_16_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_16_time/mission_16_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 30)
								{
									VectorSet(zyk_quest_point, -700, -2590, 70);

									if (Distance(ent->client->ps.origin, zyk_quest_point) < 70)
									{
										ent->client->pers.universe_quest_messages++;
										ent->client->pers.universe_quest_timer = level.time + 5000;

										zyk_text_message(ent, "universe/mission_16_time/mission_16_time_30", qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 31)
								{
									ent->client->pers.universe_quest_progress = 17;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
						}

						if (ent->client->pers.universe_quest_artifact_holder_id == -3 && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_timer < level.time && ent->client->ps.powerups[PW_FORCE_BOON])
						{ // zyk: player got the artifact, save it to his account
							ent->client->pers.universe_quest_artifact_holder_id = -1;
							ent->client->pers.universe_quest_counter |= (1 << 8);
							save_account(ent, qtrue);

							zyk_text_message(ent, "universe/mission_2/mission_2_artifact_guardian_end", qtrue, qfalse, ent->client->pers.netname);

							universe_quest_artifacts_checker(ent);

							quest_get_new_player(ent);
						}
						else if (ent->client->pers.universe_quest_artifact_holder_id > -1 && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && ent->client->ps.hasLookTarget == qtrue && ent->client->ps.lookTarget == ent->client->pers.universe_quest_artifact_holder_id)
						{ // zyk: found quest_ragnos npc, set artifact effect on player
							ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 10000;
							ent->client->pers.universe_quest_timer = level.time + 5000;

							G_FreeEntity(&g_entities[ent->client->pers.universe_quest_artifact_holder_id]);

							ent->client->pers.universe_quest_artifact_holder_id = -3;

							zyk_text_message(ent, "universe/mission_2/mission_2_artifact_guardian_found", qtrue, qfalse, ent->client->pers.netname);
						}
					}
					else if (level.quest_map == 11)
					{   
						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 9)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -100 && (int) ent->client->ps.origin[0] < 100 && (int) ent->client->ps.origin[1] > -95 && (int) ent->client->ps.origin[1] < 105 && (int) ent->client->ps.origin[2] == -375)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_fire", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,0,5,-374,-90,"guardian_boss_6",0,-269,-374,90,6);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}
					}
					else if (level.quest_map == 12)
					{
						if (ent->client->pers.universe_quest_progress == 8 && ent->client->pers.can_play_quest == 1 && !(ent->client->pers.universe_quest_counter & (1 << 1)))
						{ // zyk: Revelations mission of Universe Quest
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 8)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -1082, 4337, 505, 45);
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -912, 4503, 505, -135);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -892, 4340, 505, 135);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe", -1091, 4520, 505, -45);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -1112, 4418, 505, 0);
								else if (ent->client->pers.hunter_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -882, 4418, 505, 179);
								else if (ent->client->pers.hunter_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -993, 4273, 505, 90);
								else if (ent->client->pers.hunter_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -988, 4543, 505, -90);

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -988, 4418, 525);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 8 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 20)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 3 || 
										ent->client->pers.universe_quest_messages == 7 || ent->client->pers.universe_quest_messages == 15 || ent->client->pers.universe_quest_messages == 20)
									{
										zyk_text_message(ent, va("universe/mission_8/mission_8_part_1_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_8/mission_8_part_1_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 21)
								{
									zyk_text_message(ent, "universe/mission_8/mission_8_part_1_21", qtrue, qfalse);

									ent->client->pers.universe_quest_counter |= (1 << 1);
									first_second_act_objective(ent);
									save_account(ent, qtrue);
									quest_get_new_player(ent);
								}
							}
						}

						if (ent->client->pers.universe_quest_progress == 15 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, first mission of Guardians Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 5)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -1112, 4418, 505, 0);
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -882, 4418, 505, 179);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -993, 4273, 505, 90);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -988, 4543, 505, -90);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -1091, 4520, 505, -45);

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -988, 4418, 525);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 5 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								
								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 20)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 7 ||
										ent->client->pers.universe_quest_messages == 12 || ent->client->pers.universe_quest_messages == 18 || ent->client->pers.universe_quest_messages == 20)
									{
										zyk_text_message(ent, va("universe/mission_15_guardians/mission_15_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_15_guardians/mission_15_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 21)
								{
									ent->client->pers.universe_quest_progress = 16;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 17 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, trials mission of Guardians Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 5)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -4270, 4684, -6, 45);
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -4270, 5084, -6, -45);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -3870, 4684, -6, 135);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -3870, 5084, -6, -135);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -4270, 4884, -6, 0);

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -4070, 4884, -6);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 5 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 10)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 7)
									{
										zyk_text_message(ent, va("universe/mission_17_guardians/mission_17_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_17_guardians/mission_17_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 11)
								{
									ent->client->pers.universe_quest_progress = 18;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 18 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, boss battle against light quest bosses in Guardians Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;
								int amount_of_bosses_in_map = 1;

								VectorSet(zyk_quest_point, -4070, 4884, -6);

								if (ent->client->pers.universe_quest_counter & (1 << 29))
								{ // zyk: Challenge Mode requires fighting more than one at once
									amount_of_bosses_in_map = 2;
								}

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 200)
								{
									ent->client->pers.hunter_quest_messages = 0;
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages > 0 && ent->client->pers.universe_quest_messages < 10 && 
										 ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages == 1 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{ // zyk: closing the passage from where the player came so he cannot exit the trials room
									zyk_trial_room_models();

									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_1", -4270, 4884, 150, 0, 17);

									// zyk: counts how many bosses defeated
									ent->client->pers.light_quest_messages = 0;

									// zyk: counts how many bosses in the map
									ent->client->pers.hunter_quest_messages = 1;
								}
								else if (ent->client->pers.universe_quest_messages == 2 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_2", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 3 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_3", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 4 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_4", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 5 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_5", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 6 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_6", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 7 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_7", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 8 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_8", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 9 && ent->client->pers.hunter_quest_messages < amount_of_bosses_in_map)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_10", -4270, 4884, 150, 0, 17);
									ent->client->pers.hunter_quest_messages++;
								}
								else if (ent->client->pers.universe_quest_messages == 11)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 2000;
									zyk_text_message(ent, "universe/mission_18_guardians/mission_18_guardians_win", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 12)
								{
									ent->client->pers.universe_quest_progress = 19;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 19 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, great moment mission of Guardians Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 5)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -4270, 4684, -6, 45);
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -4270, 5084, -6, -45);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -3870, 4684, -6, 135);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -3870, 5084, -6, -135);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -4270, 4884, -6, 0);

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -4070, 4884, -6);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 5 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 9)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 7 || 
										ent->client->pers.universe_quest_messages == 8)
									{
										zyk_text_message(ent, va("universe/mission_19_guardians/mission_19_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_19_guardians/mission_19_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 10)
								{
									ent->client->pers.universe_quest_progress = 20;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 20 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, final boss battle in Guardians Sequel
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -4070, 4884, -6);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 200)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages > 0 && ent->client->pers.universe_quest_messages < 5)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 500;
								}

								if (ent->client->pers.universe_quest_messages == 1)
								{ // zyk: closing the passage from where the player came so he cannot exit the trials room
									zyk_trial_room_models();

									spawn_boss(ent, -3970, 4884, -5, 179, "guardian_of_universe", -4270, 4884, 150, 0, 18);

									// zyk: counts how many bosses defeated
									ent->client->pers.light_quest_messages = 0;
								}
								else if (ent->client->pers.universe_quest_messages == 2)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_boss_9", -4270, 4784, 150, 0, 18);
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_of_darkness", -4270, 4984, 150, 0, 18);
								}
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									spawn_boss(ent, -4070, 4884, -5, 179, "guardian_of_eternity", -4170, 4884, 150, 0, 18);
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 2000;
									zyk_text_message(ent, "universe/mission_20_guardians/mission_20_guardians_win", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 7)
								{
									ent->client->pers.universe_quest_progress = 21;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 21 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_counter & (1 << 1))
						{ // zyk: Universe Quest, final mission of Guardians Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 5)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -4270, 4684, -6, 45);
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -4270, 5084, -6, -45);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -3870, 4684, -6, 135);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -3870, 5084, -6, -135);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -4270, 4884, -6, 0);

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -4070, 4884, -6);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 5 && Distance(ent->client->ps.origin, zyk_quest_point) < 200))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 13)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 9 || ent->client->pers.universe_quest_messages == 10 ||
										ent->client->pers.universe_quest_messages == 11)
									{
										zyk_text_message(ent, va("universe/mission_21_guardians/mission_21_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_21_guardians/mission_21_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 14)
								{
									ent->client->pers.universe_quest_progress = 22;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 15 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 3))
						{ // zyk: Universe Quest, first mission of Time Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 9)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -1082, 4337, 505, 45);
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -912, 4503, 505, -135);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -892, 4340, 505, 135);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe", -1091, 4520, 505, -45);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", -1112, 4418, 505, 0);
								else if (ent->client->pers.hunter_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", -882, 4418, 505, 179);
								else if (ent->client->pers.hunter_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -993, 4273, 505, 90);
								else if (ent->client->pers.hunter_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", -988, 4543, 505, -90);
								else if (ent->client->pers.hunter_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -988, 4393, 505, 90);

								if (npc_ent)
								{
									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -988, 4393, 505);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 9 && Distance(ent->client->ps.origin, zyk_quest_point) < 300))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 36)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 5 || 
										ent->client->pers.universe_quest_messages == 10 || ent->client->pers.universe_quest_messages == 13 || ent->client->pers.universe_quest_messages == 19 || 
										ent->client->pers.universe_quest_messages == 20 || ent->client->pers.universe_quest_messages == 24 || ent->client->pers.universe_quest_messages == 33 || 
										ent->client->pers.universe_quest_messages == 35)
									{
										zyk_text_message(ent, va("universe/mission_15_time/mission_15_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_15_time/mission_15_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 37)
								{
									ent->client->pers.universe_quest_progress = 16;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 7 && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && 
								 ent->client->pers.universe_quest_timer < level.time)
						{ // zyk: Guardian of Universe battle
							if ((int) ent->client->ps.origin[0] > 2746 && (int) ent->client->ps.origin[0] < 3123 && (int) ent->client->ps.origin[1] > 4728 && (int) ent->client->ps.origin[1] < 4994 && (int) ent->client->ps.origin[2] == 24)
							{
								if (ent->client->pers.universe_quest_messages == 0)
									zyk_text_message(ent, "universe/mission_7/mission_7_0", qtrue, qfalse, ent->client->pers.netname);
								else if (ent->client->pers.universe_quest_messages == 1)
									zyk_text_message(ent, "universe/mission_7/mission_7_1", qtrue, qfalse);
								else if (ent->client->pers.universe_quest_messages == 2)
									zyk_text_message(ent, "universe/mission_7/mission_7_2", qtrue, qfalse, ent->client->pers.netname);
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									spawn_boss(ent,2742,4863,25,0,"guardian_of_universe",0,0,0,0,13);
								}

								if (ent->client->pers.universe_quest_messages < 4)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
							
							if (ent->client->pers.universe_quest_messages == 5)
							{
								zyk_NPC_Kill_f("all"); // zyk: killing the guardian spawns
								zyk_text_message(ent, "universe/mission_7/mission_7_5", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 6)
							{
								zyk_text_message(ent, "universe/mission_7/mission_7_6", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 7)
							{
								zyk_text_message(ent, "universe/mission_7/mission_7_7", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 8)
							{
								zyk_text_message(ent, "universe/mission_7/mission_7_8", qtrue, qfalse);
								
								ent->client->pers.universe_quest_progress = 8;
								ent->client->pers.universe_quest_messages = 0;

								save_account(ent, qtrue);

								ent->client->pers.quest_power_status |= (1 << 13);

								quest_get_new_player(ent);
							}

							if (ent->client->pers.universe_quest_messages > 4 && ent->client->pers.universe_quest_messages < 8)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
						}
					}
					else if (level.quest_map == 13)
					{
						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 5)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -2249 && (int) ent->client->ps.origin[0] < -2049 && (int) ent->client->ps.origin[1] > -4287 && (int) ent->client->ps.origin[1] < -4087 && (int) ent->client->ps.origin[2] == 3644)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_earth", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,-2149,-4387,3645,90,"guardian_boss_2",-2149,-4037,3645,-90,2);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}

						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 3 && ent->client->pers.universe_quest_timer < level.time && ent->client->pers.universe_quest_messages < 1 && !(ent->client->pers.universe_quest_counter & (1 << 5)))
						{
							gentity_t *npc_ent = NULL;
							npc_ent = Zyk_NPC_SpawnType("quest_mage",-584,296,5977,0);
							if (npc_ent)
							{
								npc_ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 5500;

								npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;
								ent->client->pers.universe_quest_artifact_holder_id = 5;
							}
							zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);
						
							ent->client->pers.universe_quest_messages++;
							ent->client->pers.universe_quest_timer = level.time + 5000;
						}
					}
					else if (level.quest_map == 14)
					{
						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 11)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -100 && (int) ent->client->ps.origin[0] < 100 && (int) ent->client->ps.origin[1] > 1035 && (int) ent->client->ps.origin[1] < 1235 && (int) ent->client->ps.origin[2] == 24)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_resistance", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,0,1135,25,-90,"guardian_boss_8",0,905,25,90,11);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}
					}
					else if (level.quest_map == 15)
					{
						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 10)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > -253 && (int) ent->client->ps.origin[0] < -53 && (int) ent->client->ps.origin[1] > -555 && (int) ent->client->ps.origin[1] < -355 && (int) ent->client->ps.origin[2] == 216)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_wind", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,-156,-298,217,-90,"guardian_boss_7",-156,-584,300,90,7);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}
					}
					else if (level.quest_map == 17)
					{
						if (ent->client->pers.universe_quest_progress == 8 && ent->client->pers.can_play_quest == 1 && !(ent->client->pers.universe_quest_counter & (1 << 2)) && ent->client->pers.universe_quest_timer < level.time && (int) ent->client->ps.origin[0] > -18684 && (int) ent->client->ps.origin[0] < -17485 && (int) ent->client->ps.origin[1] > 17652 && (int) ent->client->ps.origin[1] < 18781 && (int) ent->client->ps.origin[2] > 1505 && (int) ent->client->ps.origin[2] < 1850)
						{ // zyk: nineth Universe Quest mission. Guardian of Time part
							if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 19)
							{
								if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 3)
								{
									zyk_text_message(ent, va("universe/mission_8/mission_8_part_2_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
								}
								else
								{
									zyk_text_message(ent, va("universe/mission_8/mission_8_part_2_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 20)
							{
								zyk_text_message(ent, "universe/mission_8/mission_8_part_2_20", qtrue, qfalse, ent->client->pers.netname);

								ent->client->pers.universe_quest_counter |= (1 << 2);
								first_second_act_objective(ent);
								save_account(ent, qtrue);
								quest_get_new_player(ent);
							}

							if (ent->client->pers.universe_quest_messages < 20)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
						}

						if (ent->client->pers.universe_quest_progress == 9 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_timer < level.time)
						{ // zyk: Universe Quest crystals mission
							gentity_t *npc_ent = NULL;

							if (!(ent->client->pers.universe_quest_counter & (1 << 0)))
							{ // zyk: Crystal of Destiny
								if (ent->client->pers.universe_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-3702,-2251,2232,90);
								else if (ent->client->pers.universe_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-3697,-2191,2232,91);
								else if (ent->client->pers.universe_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-3692,-2335,2232,-91);
								else if (ent->client->pers.universe_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-3813,-2252,2488,179);
								else if (ent->client->pers.universe_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-2588,-2252,2488,4);
								else if (ent->client->pers.universe_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-3702,-2252,2488,100);
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									load_crystal_model(-3702,-2251,2211,90,0);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 0)
							{ // zyk: if player has this crystal, try the next one
								ent->client->pers.universe_quest_messages = 6;
							}

							if (!(ent->client->pers.universe_quest_counter & (1 << 1)))
							{ // zyk: Crystal of Truth
								if (ent->client->pers.universe_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-14365,-696,1554,-179);
								else if (ent->client->pers.universe_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-14358,-284,1565,-179);
								else if (ent->client->pers.universe_quest_messages == 9)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-13880,-520,1557,-179);
								else if (ent->client->pers.universe_quest_messages == 10)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-15361,-600,1851,-179);
								else if (ent->client->pers.universe_quest_messages == 11)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",-14152,-523,1455,-179);
								else if (ent->client->pers.universe_quest_messages == 12)
								{
									load_crystal_model(-14152,-523,1455,-179,1);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 6)
							{ // zyk: if player has this crystal, try the next one
								ent->client->pers.universe_quest_messages = 12;
							}

							if (!(ent->client->pers.universe_quest_counter & (1 << 2)))
							{ // zyk: Crystal of Time
								if (ent->client->pers.universe_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",19904,-2740,1672,90);
								else if (ent->client->pers.universe_quest_messages == 14)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",19903,-2684,1672,91);
								else if (ent->client->pers.universe_quest_messages == 15)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",19751,-2669,1672,87);
								else if (ent->client->pers.universe_quest_messages == 16)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",20039,-2674,1672,86);
								else if (ent->client->pers.universe_quest_messages == 17)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",20073,-2326,1672,89);
								else if (ent->client->pers.universe_quest_messages == 18)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",19713,-2316,1672,83);
								else if (ent->client->pers.universe_quest_messages == 19)
								{
									load_crystal_model(19904,-2740,1651,90,2);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 12)
							{ // zyk: if player has this crystal, do the crystal check
								ent->client->pers.universe_quest_messages = 19;
							}

							if (ent->client->pers.universe_quest_messages < 20)
							{ // zyk: only increments to next step if npc was properly placed in the map or a crystal was placed
								if (npc_ent || ent->client->pers.universe_quest_messages == 6 || ent->client->pers.universe_quest_messages == 12 ||
									ent->client->pers.universe_quest_messages == 19)
								{
									ent->client->pers.universe_quest_messages++;
								}

								ent->client->pers.universe_quest_timer = level.time + 1200;
							}
							else
							{
								ent->client->pers.universe_quest_timer = level.time + 200;

								if (level.quest_crystal_id[0] != -1 && 
									(int)Distance(ent->client->ps.origin,g_entities[level.quest_crystal_id[0]].r.currentOrigin) < 50)
								{
									ent->client->pers.universe_quest_counter |= (1 << 0);
									zyk_text_message(ent, "universe/mission_9/mission_9_0", qtrue, qfalse);
									clean_crystal_model(0);
									universe_crystals_check(ent);
								}
								else if (level.quest_crystal_id[1] != -1 && 
									(int)Distance(ent->client->ps.origin,g_entities[level.quest_crystal_id[1]].r.currentOrigin) < 50)
								{
									ent->client->pers.universe_quest_counter |= (1 << 1);
									zyk_text_message(ent, "universe/mission_9/mission_9_1", qtrue, qfalse);
									clean_crystal_model(1);
									universe_crystals_check(ent);
								}
								else if (level.quest_crystal_id[2] != -1 && 
									(int)Distance(ent->client->ps.origin,g_entities[level.quest_crystal_id[2]].r.currentOrigin) < 50)
								{
									ent->client->pers.universe_quest_counter |= (1 << 2);
									zyk_text_message(ent, "universe/mission_9/mission_9_2", qtrue, qfalse);
									clean_crystal_model(2);
									universe_crystals_check(ent);
								}
							}
						}

						if (ent->client->pers.universe_quest_progress == 10 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_timer < level.time && 
							(int) ent->client->ps.origin[0] > -18684 && (int) ent->client->ps.origin[0] < -17485 && (int) ent->client->ps.origin[1] > 17652 && 
							(int) ent->client->ps.origin[1] < 18781 && (int) ent->client->ps.origin[2] > 1505 && (int) ent->client->ps.origin[2] < 1850)
						{ // zyk: eleventh objective of Universe Quest. Setting Guardian of Time free
							if (ent->client->pers.universe_quest_messages == 1)
								zyk_text_message(ent, "universe/mission_10/mission_10_1", qtrue, qfalse, ent->client->pers.netname);
							else if (ent->client->pers.universe_quest_messages == 2)
								Zyk_NPC_SpawnType("guardian_of_time",-18084,17970,1658,-90);
							else if (ent->client->pers.universe_quest_messages >= 3 && ent->client->pers.universe_quest_messages <= 17)
							{
								zyk_text_message(ent, va("universe/mission_10/mission_10_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 18)
							{
								zyk_text_message(ent, "universe/mission_10/mission_10_18", qtrue, qfalse, ent->client->pers.netname);

								ent->client->pers.universe_quest_progress = 11;
								save_account(ent, qtrue);
								quest_get_new_player(ent);
							}
							
							if (ent->client->pers.universe_quest_progress == 10 && ent->client->pers.universe_quest_messages < 18)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
						}

						if (ent->client->pers.universe_quest_progress == 11 && ent->client->pers.can_play_quest == 1)
						{ // zyk: mission of the battle for the temple in Universe Quest
							if (ent->client->pers.universe_quest_timer < level.time && (int) ent->client->ps.origin[0] > 7658 && (int) ent->client->ps.origin[0] < 17707 && (int) ent->client->ps.origin[1] > 5160 && (int) ent->client->ps.origin[1] < 12097 && (int) ent->client->ps.origin[2] > 1450 && (int) ent->client->ps.origin[2] < 5190)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.universe_quest_messages == 0)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time",9869,11957,1593,-52);
									zyk_text_message(ent, "universe/mission_11/mission_11_0", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe",9800,11904,1593,-52);
									zyk_text_message(ent, "universe/mission_11/mission_11_1", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("sage_of_light",9744,11862,1593,-52);
								else if (ent->client->pers.universe_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness",9687,11818,1593,-52);
								else if (ent->client->pers.universe_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity",9626,11771,1593,-52);
								else if (ent->client->pers.universe_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe",9562,11722,1593,-52);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
									zyk_text_message(ent, "universe/mission_11/mission_11_2", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9",9498,11673,1593,-52);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 7)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness",9433,11623,1593,-52);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.universe_quest_messages == 8)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity",9335,11547,1593,-52);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}

								if (npc_ent)
									npc_ent->client->pers.universe_quest_objective_control = ent->s.number;

								if (ent->client->pers.universe_quest_messages < 9)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 3000;
								}
							}

							if (ent->client->pers.hunter_quest_timer < level.time)
							{ // zyk: spawns the npcs at the temple
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
								{
									int ent_iterator = 0;
									gentity_t *this_ent = NULL;

									// zyk: cleaning entities, except the spawn points. This will prevent server from crashing in this mission
									for (ent_iterator = level.maxclients; ent_iterator < level.num_entities; ent_iterator++)
									{
										this_ent = &g_entities[ent_iterator];

										if (this_ent && Q_stricmp( this_ent->classname, "info_player_deathmatch" ) != 0)
											G_FreeEntity(this_ent);
									}

									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10212,8937,1593,-179);
								}
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10212,8828,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10212,8680,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10212,8550,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10212,8355,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10212,8164,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9280,9246,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9528,9425,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9779,9426,1593,-179);
								else if (ent->client->pers.hunter_quest_messages == 9)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9954,10016,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 10)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9450,10524,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 12)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9294,10583,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9305,10418,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 14)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9065,10415,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 15)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",9069,10244,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 16)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10094,9199,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 17)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10139,9282,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 18)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10199,9392,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 19)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10200,9777,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 20)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10220,8239,1800,-179);
								else if (ent->client->pers.hunter_quest_messages == 21)
									npc_ent = Zyk_NPC_SpawnType("quest_super_soldier",10123,8343,1800,-179);

								if (npc_ent)
									npc_ent->client->pers.universe_quest_objective_control = ent->s.number;

								if (ent->client->pers.hunter_quest_messages < 11)
								{
									ent->client->pers.hunter_quest_messages++;
									ent->client->pers.hunter_quest_timer = level.time + 1500;
								}
								else if (ent->client->pers.hunter_quest_messages > 11 && ent->client->pers.hunter_quest_messages < 22)
								{
									ent->client->pers.hunter_quest_messages++;
									ent->client->pers.hunter_quest_timer = level.time + 2000;
								}
								else if (ent->client->pers.hunter_quest_messages == 40)
								{ // zyk: completed the mission
									ent->client->pers.universe_quest_progress = 12;
									save_account(ent, qtrue);
									quest_get_new_player(ent);
								}
								else if (ent->client->pers.hunter_quest_messages == 50)
								{ // zyk: failed the mission
									quest_get_new_player(ent);
								}
							}
						}

						if (ent->client->pers.universe_quest_progress == 12 && ent->client->pers.can_play_quest == 1)
						{ // zyk: Universe Quest, The Final Revelation mission
							if (ent->client->pers.universe_quest_timer < level.time && (int) ent->client->ps.origin[0] > 9658 && (int) ent->client->ps.origin[0] < 12707 && (int) ent->client->ps.origin[1] > 8160 && (int) ent->client->ps.origin[1] < 9097 && (int) ent->client->ps.origin[2] > 1450 && (int) ent->client->ps.origin[2] < 2190)
							{
								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 24)
								{
									if (ent->client->pers.universe_quest_messages == 3 || ent->client->pers.universe_quest_messages == 6 || ent->client->pers.universe_quest_messages == 15 || 
										ent->client->pers.universe_quest_messages == 22)
									{
										zyk_text_message(ent, va("universe/mission_12/mission_12_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_12/mission_12_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 25)
								{
									ent->client->pers.universe_quest_progress = 13;
									save_account(ent, qtrue);
									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_progress == 12 && ent->client->pers.universe_quest_messages < 26)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}

							if (ent->client->pers.hunter_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time",10894,8521,1700,0);
								}
								else if (ent->client->pers.hunter_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("master_of_evil",11433,8499,1700,179);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe",10840,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity",10956,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9",11085,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness",11234,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 6)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe",10840,8416,1700,90);
								}
								else if (ent->client->pers.hunter_quest_messages == 7)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light",10956,8416,1700,90);
								}
								else if (ent->client->pers.hunter_quest_messages == 8)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness",11085,8416,1700,90);
								}
								else if (ent->client->pers.hunter_quest_messages == 9)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity",11234,8416,1700,90);
								}

								if (ent->client->pers.hunter_quest_messages < 10)
								{
									ent->client->pers.hunter_quest_messages++;
									ent->client->pers.hunter_quest_timer = level.time + 1500;
								}
							}
						}

						if (ent->client->pers.universe_quest_progress == 13 && ent->client->pers.can_play_quest == 1)
						{ // zyk: Universe Quest, the choosing mission
							if (ent->client->pers.universe_quest_timer < level.time && (int) ent->client->ps.origin[0] > 9758 && (int) ent->client->ps.origin[0] < 14000 && (int) ent->client->ps.origin[1] > 8160 && (int) ent->client->ps.origin[1] < 9097 && (int) ent->client->ps.origin[2] > 1450 && (int) ent->client->ps.origin[2] < 2190)
							{
								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 27)
								{
									zyk_text_message(ent, va("universe/mission_13/mission_13_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 29)
								{ // zyk: completed the mission
									ent->client->pers.universe_quest_progress = 14;
									save_account(ent, qtrue);
									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_progress == 13 && ent->client->pers.universe_quest_messages < 28)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}

							if (ent->client->pers.hunter_quest_timer < level.time)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time",10894,8521,1700,0);
								}
								else if (ent->client->pers.hunter_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("master_of_evil",11433,8499,1700,179);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe",10840,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity",10956,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9",11085,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness",11234,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 6)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe",10840,8416,1700,90);
								}
								else if (ent->client->pers.hunter_quest_messages == 7)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light",10956,8416,1700,90);
								}
								else if (ent->client->pers.hunter_quest_messages == 8)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness",11085,8416,1700,90);
								}
								else if (ent->client->pers.hunter_quest_messages == 9)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity",11234,8416,1700,90);
								}

								if (ent->client->pers.hunter_quest_messages < 10)
								{
									ent->client->pers.hunter_quest_messages++;
									ent->client->pers.hunter_quest_timer = level.time + 1500;
								}

								if (npc_ent)
								{
									npc_ent->client->pers.universe_quest_messages = -2000;
								}
							}
						}

						if (level.chaos_portal_id != -1)
						{ // zyk: portal at the last universe quest mission. Teleports players to Sacred Dimension
							gentity_t *chaos_portal = &g_entities[level.chaos_portal_id];

							if (chaos_portal && (int)Distance(chaos_portal->s.origin,ent->client->ps.origin) < 40)
							{
								vec3_t origin;
								vec3_t angles;
								int npc_iterator = 0;
								gentity_t *this_ent = NULL;

								// zyk: cleaning vehicles to prevent some exploits
								for (npc_iterator = level.maxclients; npc_iterator < level.num_entities; npc_iterator++)
								{
									this_ent = &g_entities[npc_iterator];

									if (this_ent && this_ent->NPC && this_ent->client->NPC_class == CLASS_VEHICLE && this_ent->die)
										this_ent->die(this_ent, this_ent, this_ent, 100, MOD_UNKNOWN);
								}

								origin[0] = -1915.0f;
								origin[1] = -26945.0f;
								origin[2] = 300.0f;
								angles[0] = 0.0f;
								angles[1] = -179.0f;
								angles[2] = 0.0f;

								zyk_TeleportPlayer(ent,origin,angles);
							}
						}

						if (ent->client->pers.universe_quest_progress == 14 && ent->client->pers.can_play_quest == 1)
						{ // zyk: Universe Quest final mission
							if (ent->client->pers.guardian_mode == 0 && ent->client->pers.universe_quest_timer < level.time)
							{
								gentity_t *new_ent = NULL;

								if (ent->client->pers.universe_quest_messages == 1)
									new_ent = load_crystal_model(12614,8430,1497,179,0);
								else if (ent->client->pers.universe_quest_messages == 2)
									new_ent = load_crystal_model(12614,8570,1497,179,1);
								else if (ent->client->pers.universe_quest_messages == 3)
									new_ent = load_crystal_model(12734,8500,1497,179,2);
								else if (ent->client->pers.universe_quest_messages == 4)
								{
									new_ent = load_effect(12614,8430,1512,0,"env/vbolt");
									G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tram_boost.mp3"));
								}
								else if (ent->client->pers.universe_quest_messages == 5)
								{
									new_ent = load_effect(12614,8570,1512,0,"env/vbolt");
									G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tram_boost.mp3"));
								}
								else if (ent->client->pers.universe_quest_messages == 6)
								{
									new_ent = load_effect(12734,8500,1512,0,"env/vbolt");
									G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tram_boost.mp3"));
								}
								else if (ent->client->pers.universe_quest_messages == 7)
									new_ent = load_effect(12668,8500,1510,0,"env/btend");
								else if (ent->client->pers.universe_quest_messages == 8)
								{
									new_ent = load_effect(12668,8500,1512,0,"env/hevil_bolt");
									G_Sound(new_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tractorbeam.mp3"));

									level.chaos_portal_id = new_ent->s.number;
								}
								else if (ent->client->pers.universe_quest_messages == 9)
								{ // zyk: teleports the quest player to the Sacred Dimension
									if ((int) ent->client->ps.origin[0] > -2415 && (int) ent->client->ps.origin[0] < -1415 && (int) ent->client->ps.origin[1] > -27445 && (int) ent->client->ps.origin[1] < -26445 && (int) ent->client->ps.origin[2] > 100 && (int) ent->client->ps.origin[2] < 400)
									{
										ent->client->pers.universe_quest_messages = 10;
									}
								}
								else if (ent->client->pers.universe_quest_messages >= 11 && ent->client->pers.universe_quest_messages <= 17)
								{
									if (ent->client->pers.universe_quest_messages == 12)
									{
										zyk_text_message(ent, va("universe/mission_14/mission_14_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_14/mission_14_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 18)
								{ // zyk: here starts the battle against the Guardian of Chaos
									int npc_iterator = 0;
									gentity_t *this_ent = NULL;

									// zyk: cleaning npcs that are not the quest ones
									for (npc_iterator = MAX_CLIENTS; npc_iterator < level.num_entities; npc_iterator++)
									{
										this_ent = &g_entities[npc_iterator];

										if (this_ent && this_ent->NPC && this_ent->die && Q_stricmp( this_ent->NPC_type, "sage_of_light" ) != 0 && Q_stricmp( this_ent->NPC_type, "sage_of_darkness" ) != 0 && Q_stricmp( this_ent->NPC_type, "sage_of_eternity" ) != 0 && Q_stricmp( this_ent->NPC_type, "sage_of_universe" ) != 0 && Q_stricmp( this_ent->NPC_type, "guardian_of_time" ) != 0 && Q_stricmp( this_ent->NPC_type, "guardian_boss_9" ) != 0 && Q_stricmp( this_ent->NPC_type, "guardian_of_darkness" ) != 0 && Q_stricmp( this_ent->NPC_type, "guardian_of_eternity" ) != 0 && Q_stricmp( this_ent->NPC_type, "guardian_of_universe" ) != 0 && Q_stricmp( this_ent->NPC_type, "master_of_evil" ) != 0 && Q_stricmp( this_ent->NPC_type, "jawa_seller" ) != 0)
											this_ent->die(this_ent, this_ent, this_ent, 100, MOD_UNKNOWN);
									}

									spawn_boss(ent,-3136,-26946,200,179,"guardian_of_chaos",-4228,-26946,393,0,14);
								}
								else if (ent->client->pers.universe_quest_messages == 20)
								{
									vec3_t origin;
									vec3_t angles;

									int npc_iterator = 0;
									char npc_name[128];
									gentity_t *npc_ent = NULL;

									origin[0] = -1915.0f;
									origin[1] = -26945.0f;
									origin[2] = 300.0f;
									angles[0] = 0.0f;
									angles[1] = -179.0f;
									angles[2] = 0.0f;

									strcpy(npc_name,"");

									if (ent->client->pers.universe_quest_counter & (1 << 0))
										strcpy(npc_name,"sage_of_universe");
									else if (ent->client->pers.universe_quest_counter & (1 << 1))
										strcpy(npc_name,"guardian_of_universe");
									else if (ent->client->pers.universe_quest_counter & (1 << 2))
										strcpy(npc_name,"master_of_evil");
									else if (ent->client->pers.universe_quest_counter & (1 << 3))
										strcpy(npc_name,"guardian_of_time");

									for (npc_iterator = 0; npc_iterator < level.num_entities; npc_iterator++)
									{
										npc_ent = &g_entities[npc_iterator];

										if (Q_stricmp( npc_ent->NPC_type, npc_name ) == 0)
										{
											zyk_TeleportPlayer(npc_ent,origin,angles);
											break;
										}
									}

									zyk_text_message(ent, "universe/mission_14/mission_14_boss_defeated", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages >= 21 && ent->client->pers.universe_quest_messages <= 25)
								{
									if (ent->client->pers.universe_quest_counter & (1 << 0))
										zyk_text_message(ent, va("universe/mission_14/mission_14_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									else if (ent->client->pers.universe_quest_counter & (1 << 1))
										zyk_text_message(ent, va("universe/mission_14/mission_14_guardians_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									else if (ent->client->pers.universe_quest_counter & (1 << 2))
										zyk_text_message(ent, va("universe/mission_14/mission_14_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									else if (ent->client->pers.universe_quest_counter & (1 << 3))
										zyk_text_message(ent, va("universe/mission_14/mission_14_time_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 26)
								{ // zyk: enf of this mission
									vec3_t origin;
									vec3_t angles;

									origin[0] = 12500.0f;
									origin[1] = 8500.0f;
									origin[2] = 1520.0f;
									angles[0] = 0.0f;
									angles[1] = 179.0f;
									angles[2] = 0.0f;

									zyk_TeleportPlayer(ent,origin,angles);

									ent->client->pers.universe_quest_progress = 15;
									save_account(ent, qtrue);
									quest_get_new_player(ent);
								}

								if (new_ent)
								{
									new_ent->targetname = "zyk_quest_models";
									new_ent = NULL;
								}

								if (ent->client->pers.universe_quest_messages > 0 && ent->client->pers.universe_quest_messages < 9)
								{
									ent->client->pers.universe_quest_messages++;

									// zyk: teleport can instantly teleport player, so no delay should be added
									if (ent->client->pers.universe_quest_messages < 9)
										ent->client->pers.universe_quest_timer = level.time + 2000;
								}
								else if (ent->client->pers.universe_quest_messages > 9 && ent->client->pers.universe_quest_messages < 19)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages > 19 && ent->client->pers.universe_quest_messages < 26)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}

							if (ent->client->pers.guardian_mode == 0 && ent->client->pers.hunter_quest_timer < level.time && (int) ent->client->ps.origin[0] > 10000 && (int) ent->client->ps.origin[0] < 14000 && (int) ent->client->ps.origin[1] > 8160 && (int) ent->client->ps.origin[1] < 9097 && (int) ent->client->ps.origin[2] > 1450 && (int) ent->client->ps.origin[2] < 2000)
							{
								gentity_t *npc_ent = NULL;
								
								if (ent->client->pers.hunter_quest_messages == 0)
								{
									int effect_iterator = 0;
									gentity_t *this_ent = NULL;

									// zyk: cleaning the crystals and effects if they are already spawned
									for (effect_iterator = (MAX_CLIENTS + BODY_QUEUE_SIZE); effect_iterator < level.num_entities; effect_iterator++)
									{
										this_ent = &g_entities[effect_iterator];

										if (this_ent && (this_ent->NPC || Q_stricmp(this_ent->targetname, "zyk_quest_models") == 0 || 
											Q_stricmp(this_ent->classname, "npc_spawner") == 0 || Q_stricmp(this_ent->classname, "npc_vehicle") == 0 || 
											Q_stricmp(this_ent->classname, "npc_human_merc") == 0))
										{ // zyk: cleaning quest models, npcs and vehicles
											G_FreeEntity(this_ent);
										}
									}

									if (ent->client->pers.universe_quest_counter & (1 << 3))
									{
										npc_ent = Zyk_NPC_SpawnType("guardian_of_time",12834,8500,1700,179);
										zyk_text_message(ent, "universe/mission_14/mission_14_time", qtrue, qfalse);
									}
									else
										npc_ent = Zyk_NPC_SpawnType("guardian_of_time",10894,8521,1700,0);

									if (npc_ent)
										npc_ent->client->pers.universe_quest_messages = -2000;
								}
								else if (ent->client->pers.hunter_quest_messages == 1)
								{
									if (ent->client->pers.universe_quest_counter & (1 << 2))
									{
										npc_ent = Zyk_NPC_SpawnType("master_of_evil",12834,8500,1700,179);
										zyk_text_message(ent, "universe/mission_14/mission_14_thor", qtrue, qfalse);
									}
									else
										npc_ent = Zyk_NPC_SpawnType("master_of_evil",11433,8499,1700,179);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 2)
								{
									if (ent->client->pers.universe_quest_counter & (1 << 1))
									{
										npc_ent = Zyk_NPC_SpawnType("guardian_of_universe",12834,8500,1700,179);
										zyk_text_message(ent, "universe/mission_14/mission_14_guardians", qtrue, qfalse);
									}
									else
										npc_ent = Zyk_NPC_SpawnType("guardian_of_universe",10840,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity",10956,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9",11085,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness",11234,8627,1700,-90);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 6)
								{
									if (ent->client->pers.universe_quest_counter & (1 << 0))
									{
										npc_ent = Zyk_NPC_SpawnType("sage_of_universe",12834,8500,1700,179);
										zyk_text_message(ent, "universe/mission_14/mission_14_sages", qtrue, qfalse);
									}
									else
										npc_ent = Zyk_NPC_SpawnType("sage_of_universe",10840,8416,1700,90);

									if (npc_ent)
										npc_ent->client->pers.universe_quest_messages = -2000;
								}
								else if (ent->client->pers.hunter_quest_messages == 7)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light",10956,8416,1700,90);
									if (npc_ent)
										npc_ent->client->pers.universe_quest_messages = -2000;
								}
								else if (ent->client->pers.hunter_quest_messages == 8)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness",11085,8416,1700,90);
									if (npc_ent)
										npc_ent->client->pers.universe_quest_messages = -2000;
								}
								else if (ent->client->pers.hunter_quest_messages == 9)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity",11234,8416,1700,90);
									ent->client->pers.universe_quest_messages = 1;
									if (npc_ent)
										npc_ent->client->pers.universe_quest_messages = -2000;
								}

								if (ent->client->pers.hunter_quest_messages < 10)
								{
									ent->client->pers.hunter_quest_messages++;
									ent->client->pers.hunter_quest_timer = level.time + 1200;
								}
							}
						}
					}
					else if (level.quest_map == 18)
					{ 
						zyk_try_get_dark_quest_note(ent, 11);

						// zyk: Universe Quest artifact
						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 3 && !(ent->client->pers.universe_quest_counter & (1 << 4)) && ent->client->pers.universe_quest_timer < level.time)
						{
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);
								npc_ent = Zyk_NPC_SpawnType("quest_reborn",-3703,-3575,1121,90);
							}
							else if (ent->client->pers.universe_quest_messages == 1)
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_blue",-1796,-802,1353,-90);
							else if (ent->client->pers.universe_quest_messages == 2)
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_red",-730,-1618,729,179);
							else if (ent->client->pers.universe_quest_messages == 3)
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_red",-735,-1380,729,179);
							else if (ent->client->pers.universe_quest_messages == 4)
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_boss",-792,-1504,729,179);
							else if (ent->client->pers.universe_quest_messages == 5)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_mage",-120,-1630,857,179);
								if (npc_ent)
								{
									npc_ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 5500;

									npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;
									ent->client->pers.universe_quest_artifact_holder_id = 4;
								}
							}

							if (ent->client->pers.universe_quest_messages < 6)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 1200;
							}
						}
					}
					else if (level.quest_map == 20)
					{
						// zyk: Guardian of Agility
						if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(ent->client->pers.defeated_guardians & (1 << 8)) && ent->client->pers.can_play_quest == 1 && ent->client->pers.guardian_mode == 0 && (int) ent->client->ps.origin[0] > 8374 && (int) ent->client->ps.origin[0] < 8574 && (int) ent->client->ps.origin[1] > -1422 && (int) ent->client->ps.origin[1] < -1222 && (int) ent->client->ps.origin[2] > -165 && (int) ent->client->ps.origin[2] < -160)
						{
							if (ent->client->pers.light_quest_timer < level.time)
							{
								if (ent->client->pers.light_quest_messages == 0)
								{
									zyk_text_message(ent, "light/guardian_of_agility", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.light_quest_messages == 1)
								{
									spawn_boss(ent,9773,-1779,-162,90,"guardian_boss_5",9773,-1199,-162,90,5);
								}
								ent->client->pers.light_quest_messages++;
								ent->client->pers.light_quest_timer = level.time + 3000;
							}
						}

						// zyk: Universe Quest artifact
						if (ent->client->pers.universe_quest_progress == 2 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control == 3 && !(ent->client->pers.universe_quest_counter & (1 << 7)) && ent->client->pers.universe_quest_timer < level.time)
						{
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_text_message(ent, "universe/mission_2/mission_2_artifact_presence", qtrue, qfalse, ent->client->pers.netname);
								npc_ent = Zyk_NPC_SpawnType("quest_mage",8480,-1084,-90,90);
								if (npc_ent)
								{
									npc_ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 5500;

									npc_ent->client->pers.universe_quest_artifact_holder_id = ent-g_entities;
									ent->client->pers.universe_quest_artifact_holder_id = 7;
								}
							}

							if (ent->client->pers.universe_quest_messages < 1)
							{
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
						}
					}
					else if (level.quest_map == 24)
					{
						if (ent->client->pers.universe_quest_progress == 5 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_objective_control != -1 && ent->client->pers.universe_quest_timer < level.time)
						{ // zyk: amulets objective of Universe Quest in mp/siege_desert
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",9102,2508,-358,-179);
							else if (ent->client->pers.universe_quest_messages == 1)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",9290,2236,-486,-84);
							else if (ent->client->pers.universe_quest_messages == 2)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",10520,1236,-486,-174);
							else if (ent->client->pers.universe_quest_messages == 3)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",11673,751,-486,175);
							else if (ent->client->pers.universe_quest_messages == 4)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",12570,-860,-486,177);
							else if (ent->client->pers.universe_quest_messages == 5)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",11540,-1677,-486,179);
							else if (ent->client->pers.universe_quest_messages == 6)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",11277,-2915,-486,179);
							else if (ent->client->pers.universe_quest_messages == 7)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",10386,-3408,-486,2);
							else if (ent->client->pers.universe_quest_messages == 8)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",9906,-2373,-487,2);
							else if (ent->client->pers.universe_quest_messages == 9)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",9097,-919,-486,-176);
							else if (ent->client->pers.universe_quest_messages == 10)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",6732,-1208,-486,-174);
							else if (ent->client->pers.universe_quest_messages == 11)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",6802,-654,-486,-60);
							else if (ent->client->pers.universe_quest_messages == 12)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",5734,-2395,-486,92);
							else if (ent->client->pers.universe_quest_messages == 13)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",4594,-1727,-486,173);
							else if (ent->client->pers.universe_quest_messages == 14)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",2505,-1616,-486,170);
							else if (ent->client->pers.universe_quest_messages == 15)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",3298,-564,-486,-86);
							else if (ent->client->pers.universe_quest_messages == 16)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",3532,231,-486,-8);
							else if (ent->client->pers.universe_quest_messages == 17)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",1832,-1103,-486,6);
							else if (ent->client->pers.universe_quest_messages == 18)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",1727,-480,-486,7);
							else if (ent->client->pers.universe_quest_messages == 19)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",2653,1014,-486,0);
							else if (ent->client->pers.universe_quest_messages == 20)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",4346,-1209,-486,-177);
							else if (ent->client->pers.universe_quest_messages == 21)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",2372,-2413,-486,90);
							else if (ent->client->pers.universe_quest_messages == 22)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-5549,-841,57,178);
							else if (ent->client->pers.universe_quest_messages == 23)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-6035,-2285,-486,-179);
							else if (ent->client->pers.universe_quest_messages == 24)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-7149,-2482,-486,176);
							else if (ent->client->pers.universe_quest_messages == 25)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-7304,-1155,-486,-177);
							else if (ent->client->pers.universe_quest_messages == 26)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-8071,-381,-486,-1);
							else if (ent->client->pers.universe_quest_messages == 27)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-9596,-1116,-486,1);
							else if (ent->client->pers.universe_quest_messages == 28)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-9762,-191,-486,5);
							else if (ent->client->pers.universe_quest_messages == 29)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-11311,-638,9,-1);
							else if (ent->client->pers.universe_quest_messages == 30)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-11437,-662,-486,179);
							else if (ent->client->pers.universe_quest_messages == 31)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-9344,837,-66,90);
							else if (ent->client->pers.universe_quest_messages == 32)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-7710,-1665,-358,178);
							else if (ent->client->pers.universe_quest_messages == 33)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-8724,-1275,-486,176);
							else if (ent->client->pers.universe_quest_messages == 34)
								npc_ent = Zyk_NPC_SpawnType("quest_protocol_imp",-12810,325,-422,-90);
							else if (ent->client->pers.universe_quest_messages == 35)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",10350,-357,-486,179);
							else if (ent->client->pers.universe_quest_messages == 36)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",5935,-1304,-486,125);
							else if (ent->client->pers.universe_quest_messages == 37)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",4516,-679,-486,-144);
							else if (ent->client->pers.universe_quest_messages == 38)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-6327,-1071,-486,-179);
							else if (ent->client->pers.universe_quest_messages == 39)
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-8120,781,-486,-96);
							else if (ent->client->pers.universe_quest_messages == 60)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_sand_raider_green",12173,-304,-486,-179);
								zyk_text_message(ent, "universe/mission_5/mission_5_raider_0", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 61)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_sand_raider_brown",12173,-225,-486,-179);
								zyk_text_message(ent, "universe/mission_5/mission_5_raider_1", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 62)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_sand_raider_blue",12173,-137,-486,-179);
								zyk_text_message(ent, "universe/mission_5/mission_5_raider_2", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 63)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_sand_raider_red",12173,-41,-486,-179);
								zyk_text_message(ent, "universe/mission_5/mission_5_raider_3", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 199)
							{
								npc_ent = Zyk_NPC_SpawnType("quest_jawa",-7710,-1665,-358,178);
								if (npc_ent)
									npc_ent->client->pers.universe_quest_objective_control = -330;

								npc_ent = NULL;
							}
							else if (ent->client->pers.universe_quest_messages == 200)
							{
								npc_ent = Zyk_NPC_SpawnType("sage_of_light",-7867,-1484,-358,-90);
								zyk_text_message(ent, "universe/mission_5/mission_5_sage_0", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 201)
							{
								npc_ent = Zyk_NPC_SpawnType("sage_of_darkness",-7867,-1759,-358,90);
								zyk_text_message(ent, "universe/mission_5/mission_5_sage_1", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 202)
							{
								npc_ent = Zyk_NPC_SpawnType("sage_of_eternity",-7746,-1782,-358,90);
								zyk_text_message(ent, "universe/mission_5/mission_5_sage_2", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 203)
							{
								npc_ent = Zyk_NPC_SpawnType("sage_of_universe",-7775,-1492,-358,-90);
							}
							else if (ent->client->pers.universe_quest_messages >= 205 && ent->client->pers.universe_quest_messages <= 222)
							{
								if (ent->client->pers.universe_quest_messages == 205 || ent->client->pers.universe_quest_messages == 206 || ent->client->pers.universe_quest_messages == 209 || 
									ent->client->pers.universe_quest_messages == 210 || ent->client->pers.universe_quest_messages == 213 || ent->client->pers.universe_quest_messages == 215 || 
									ent->client->pers.universe_quest_messages == 219 || ent->client->pers.universe_quest_messages == 222)
								{
									zyk_text_message(ent, va("universe/mission_5/mission_5_sage_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
								}
								else
								{
									zyk_text_message(ent, va("universe/mission_5/mission_5_sage_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 223)
							{
								zyk_text_message(ent, "universe/mission_5/mission_5_sage_223", qtrue, qfalse);

								ent->client->pers.universe_quest_progress = 6;
								if (ent->client->pers.universe_quest_counter & (1 << 29))
								{ // zyk: if player is in Challenge Mode, do not remove this bit value
									ent->client->pers.universe_quest_counter = 0;
									ent->client->pers.universe_quest_counter |= (1 << 29);
								}
								else
									ent->client->pers.universe_quest_counter = 0;
								ent->client->pers.universe_quest_objective_control = -1;

								save_account(ent, qtrue);

								quest_get_new_player(ent);
							}

							if (ent->client->pers.universe_quest_messages < 40 && npc_ent)
							{ // zyk: tests npc_ent so if for some reason the npc dont get spawned, the server tries to spawn it again
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 200;

								// zyk: sets the universe_quest_objective_control based in universe_quest_messages value so each npc can say a different message. If its 35 (protocol npc), sets the player id
								if (ent->client->pers.universe_quest_messages == 35)
									npc_ent->client->pers.universe_quest_objective_control = ent-g_entities;
								else
									npc_ent->client->pers.universe_quest_objective_control = ent->client->pers.universe_quest_messages * (-10);
							}
							else if (ent->client->pers.universe_quest_messages >= 60 && ent->client->pers.universe_quest_messages <= 63 && npc_ent)
							{ // zyk: invoking the sand raiders
								npc_ent->client->pers.universe_quest_objective_control = ent-g_entities;
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 2000;
							}
							else if (ent->client->pers.universe_quest_messages >= 199 && ent->client->pers.universe_quest_messages <= 203)
							{ // zyk: invoking the sages
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 3000;

								if (npc_ent)
									npc_ent->client->pers.universe_quest_objective_control = -205; // zyk: flag to set this npc as a sage in this map
							}
							else if (ent->client->pers.universe_quest_messages >= 205 && ent->client->pers.universe_quest_messages <= 223)
							{ // zyk: talking to the sages
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
							else
							{
								ent->client->pers.universe_quest_timer = level.time + 1000;
							}
						}

						if (ent->client->pers.universe_quest_progress == 16 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: Save the City mission in Sages Sequel
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								if (ent->client->pers.universe_quest_messages == 0)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 9102, 2508, -358, -179);
								else if (ent->client->pers.universe_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 9290, 2236, -486, -84);
								else if (ent->client->pers.universe_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 10520, 1236, -486, -174);
								else if (ent->client->pers.universe_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 11673, 751, -486, 175);
								else if (ent->client->pers.universe_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 12570, -860, -486, 177);
								else if (ent->client->pers.universe_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 11540, -1677, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 11277, -2915, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 10386, -3408, -486, 2);
								else if (ent->client->pers.universe_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 9906, -2373, -487, 2);
								else if (ent->client->pers.universe_quest_messages == 9)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 9097, -919, -486, -176);
								else if (ent->client->pers.universe_quest_messages == 10)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 6732, -1208, -486, -174);
								else if (ent->client->pers.universe_quest_messages == 11)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 6802, -654, -486, -60);
								else if (ent->client->pers.universe_quest_messages == 12)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 5734, -2395, -486, 92);
								else if (ent->client->pers.universe_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 4594, -1727, -486, 173);
								else if (ent->client->pers.universe_quest_messages == 14)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2505, -1616, -486, 170);
								else if (ent->client->pers.universe_quest_messages == 15)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 3298, -564, -486, -86);
								else if (ent->client->pers.universe_quest_messages == 16)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 3532, 231, -486, -8);
								else if (ent->client->pers.universe_quest_messages == 17)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 1832, -1103, -486, 6);
								else if (ent->client->pers.universe_quest_messages == 18)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 1727, -480, -486, 7);
								else if (ent->client->pers.universe_quest_messages == 19)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2653, 1014, -486, 0);
								else if (ent->client->pers.universe_quest_messages == 20)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 4346, -1209, -486, -177);
								else if (ent->client->pers.universe_quest_messages == 21)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 2372, -2413, -486, 90);
								else if (ent->client->pers.universe_quest_messages == 22)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -5549, -841, 57, 178);
								else if (ent->client->pers.universe_quest_messages == 23)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -6035, -2285, -486, -179);
								else if (ent->client->pers.universe_quest_messages == 24)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -7149, -2482, -486, 176);
								else if (ent->client->pers.universe_quest_messages == 25)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -7304, -1155, -486, -177);
								else if (ent->client->pers.universe_quest_messages == 26)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -8071, -381, -486, -1);
								else if (ent->client->pers.universe_quest_messages == 27)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -9596, -1116, -486, 1);
								else if (ent->client->pers.universe_quest_messages == 28)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -9762, -191, -486, 5);
								else if (ent->client->pers.universe_quest_messages == 29)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -11311, -638, 9, -1);
								else if (ent->client->pers.universe_quest_messages == 30)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -11437, -662, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 31)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -9344, 837, -66, 90);
								else if (ent->client->pers.universe_quest_messages == 32)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -7710, -1665, -358, 178);
								else if (ent->client->pers.universe_quest_messages == 33)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -8724, -1275, -486, 176);
								else if (ent->client->pers.universe_quest_messages == 34)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -12810, 325, -422, -90);
								else if (ent->client->pers.universe_quest_messages == 35)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 10350, -357, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 36)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 5935, -1304, -486, 125);
								else if (ent->client->pers.universe_quest_messages == 37)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 4516, -679, -486, -144);
								else if (ent->client->pers.universe_quest_messages == 38)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -6327, -1071, -486, -179);
								else if (ent->client->pers.universe_quest_messages == 39)
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -8120, 781, -486, -96);
								else if (ent->client->pers.universe_quest_messages == 40)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_mage", -8171, -381, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 41)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 12173, -225, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 42)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_mage", 12173, -137, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 43)
								{
									zyk_text_message(ent, "universe/mission_16_sages/mission_16_sages_citizen", qtrue, qfalse);
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12173, -41, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 44)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", -7710, -1665, -358, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 45)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", -11361, -638, 29, 50);
								}
								else if (ent->client->pers.universe_quest_messages == 46)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 2555, -1616, -476, 170);
								}
								else if (ent->client->pers.universe_quest_messages == 47)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12173, -600, -348, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 48)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12173, -500, -456, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 49)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", -7760, -1665, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 50)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12023, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 51)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12073, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 52)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12223, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 53)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", 12373, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 54)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", -7760, -1725, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 55)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", -7304, -1155, -486, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 100)
								{
									ent->client->pers.universe_quest_progress = 17;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages < 56 && npc_ent)
								{ // zyk: tests npc_ent so if for some reason the npc dont get spawned, the server tries to spawn it again
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;

									if (npc_ent)
										npc_ent->client->pers.universe_quest_objective_control = ent->s.number; // zyk: flag to set this npc as a mage or sage in this map

									if (npc_ent && ent->client->pers.universe_quest_messages > 42)
									{ // zyk: giving guns to quest_jawa citizens
										npc_ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
									}
								}
								else if (ent->client->pers.universe_quest_messages < 56)
								{
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 17 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 0))
						{ // zyk: third Sages Sequel mission
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 6)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 1)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", -7867, -1484, -358, -90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 2)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -7746, -1782, -358, 90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 3)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", -7867, -1759, -358, 90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 4)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_jawa", -7710, -1665, -358, 179);
									if (npc_ent)
									{ // zyk: Samir, the mayor
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}
								else if (ent->client->pers.hunter_quest_messages == 5)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe", -7775, -1492, -358, -90);
									if (npc_ent)
									{
										npc_ent->client->pers.universe_quest_messages = -2000;
									}
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -7710, -1665, -358);

								if (ent->client->pers.universe_quest_messages == 0 && ent->client->pers.hunter_quest_messages == 6 && Distance(ent->client->ps.origin, zyk_quest_point) < 200)
								{
									ent->client->pers.universe_quest_messages++;
								}
								
								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 28)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 2 || ent->client->pers.universe_quest_messages == 4 || 
										ent->client->pers.universe_quest_messages == 7 || ent->client->pers.universe_quest_messages == 19 || ent->client->pers.universe_quest_messages == 24 || 
										ent->client->pers.universe_quest_messages == 28)
									{
										zyk_text_message(ent, va("universe/mission_17_sages/mission_17_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_17_sages/mission_17_sages_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 29)
								{
									ent->client->pers.universe_quest_progress = 18;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages > 0)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 18 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: War at the City mission in Thor Sequel
							gentity_t *npc_ent = NULL;

							if (ent->client->pers.hunter_quest_timer < level.time)
							{ // zyk: calls mages to help the player
								if (ent->client->pers.hunter_quest_messages > 0)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_mage", (int)ent->client->ps.origin[0], (int)ent->client->ps.origin[1], (int)ent->client->ps.origin[2], (int)ent->client->ps.viewangles[1]);
									if (npc_ent)
									{
										npc_ent->client->playerTeam = NPCTEAM_PLAYER;
										npc_ent->client->enemyTeam = NPCTEAM_ENEMY;
									}

									ent->client->pers.hunter_quest_messages = 0;

									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_mage", qtrue, qfalse);
								}

								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								if (ent->client->pers.universe_quest_messages == 0)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 9102, 2508, -358, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 9290, 2236, -486, -84);
								else if (ent->client->pers.universe_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 10520, 1236, -486, -174);
								else if (ent->client->pers.universe_quest_messages == 3)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 11673, 751, -486, 175);
								else if (ent->client->pers.universe_quest_messages == 4)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 12570, -860, -486, 177);
								else if (ent->client->pers.universe_quest_messages == 5)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 11540, -1677, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 6)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 11277, -2915, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 7)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 10386, -3408, -486, 2);
								else if (ent->client->pers.universe_quest_messages == 8)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 9906, -2373, -487, 2);
								else if (ent->client->pers.universe_quest_messages == 9)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 9097, -919, -486, -176);
								else if (ent->client->pers.universe_quest_messages == 10)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 6732, -1208, -486, -174);
								else if (ent->client->pers.universe_quest_messages == 11)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 6802, -654, -486, -60);
								else if (ent->client->pers.universe_quest_messages == 12)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 5734, -2395, -486, 92);
								else if (ent->client->pers.universe_quest_messages == 13)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 4594, -1727, -486, 173);
								else if (ent->client->pers.universe_quest_messages == 14)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 2505, -1616, -486, 170);
								else if (ent->client->pers.universe_quest_messages == 15)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 3298, -564, -486, -86);
								else if (ent->client->pers.universe_quest_messages == 16)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 3532, 231, -486, -8);
								else if (ent->client->pers.universe_quest_messages == 17)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 1832, -1103, -486, 6);
								else if (ent->client->pers.universe_quest_messages == 18)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 1727, -480, -486, 7);
								else if (ent->client->pers.universe_quest_messages == 19)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 2653, 1014, -486, 0);
								else if (ent->client->pers.universe_quest_messages == 20)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 4346, -1209, -486, -177);
								else if (ent->client->pers.universe_quest_messages == 21)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_universe", 2372, -2413, -486, 90);
								else if (ent->client->pers.universe_quest_messages == 22)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -5549, -841, 57, 178);
								else if (ent->client->pers.universe_quest_messages == 23)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -6035, -2285, -486, -179);
								else if (ent->client->pers.universe_quest_messages == 24)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -7149, -2482, -486, 176);
								else if (ent->client->pers.universe_quest_messages == 25)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -7304, -1155, -486, -177);
								else if (ent->client->pers.universe_quest_messages == 26)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -8071, -381, -486, -1);
								else if (ent->client->pers.universe_quest_messages == 27)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -9596, -1116, -486, 1);
								else if (ent->client->pers.universe_quest_messages == 28)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -9762, -191, -486, 5);
								else if (ent->client->pers.universe_quest_messages == 29)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -11311, -638, 9, -1);
								else if (ent->client->pers.universe_quest_messages == 30)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -11437, -662, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 31)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -9344, 837, -66, 90);
								else if (ent->client->pers.universe_quest_messages == 32)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -7710, -1665, -358, 178);
								else if (ent->client->pers.universe_quest_messages == 33)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -8724, -1275, -486, 176);
								else if (ent->client->pers.universe_quest_messages == 34)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -12810, 325, -422, -90);
								else if (ent->client->pers.universe_quest_messages == 35)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 10350, -357, -486, 179);
								else if (ent->client->pers.universe_quest_messages == 36)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 5935, -1304, -486, 125);
								else if (ent->client->pers.universe_quest_messages == 37)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 4516, -679, -486, -144);
								else if (ent->client->pers.universe_quest_messages == 38)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -6327, -1071, -486, -179);
								else if (ent->client->pers.universe_quest_messages == 39)
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -8120, 781, -486, -96);
								else if (ent->client->pers.universe_quest_messages == 40)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_eternity", -8171, -381, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 41)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 12173, -225, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 42)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 12173, -137, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 43)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 12173, -41, -486, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 44)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -7710, -1665, -358, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 45)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -11361, -638, 29, 50);
								}
								else if (ent->client->pers.universe_quest_messages == 46)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_light", 2555, -1616, -476, 170);
								}
								else if (ent->client->pers.universe_quest_messages == 47)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_boss_9", 12173, -600, -348, -179);
								}
								else if (ent->client->pers.universe_quest_messages == 48)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_darkness", 12173, -500, -456, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 49)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_eternity", -7760, -1665, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 50)
								{
									npc_ent = Zyk_NPC_SpawnType("sage_of_universe", 12023, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 51)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 12073, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 52)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", 12223, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 53)
								{
									npc_ent = Zyk_NPC_SpawnType("guardian_of_darkness", 12373, -600, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 54)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -7760, -1725, -348, 178);
								}
								else if (ent->client->pers.universe_quest_messages == 55)
								{
									npc_ent = Zyk_NPC_SpawnType("quest_citizen_warrior", -7304, -1155, -486, 178);
									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_citizen", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 56)
								{
									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_56", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 57)
								{
									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_57", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 58)
								{
									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_58", qtrue, qfalse, ent->client->pers.netname);
								}
								else if (ent->client->pers.universe_quest_messages == 100)
								{
									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_100", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 101)
								{
									zyk_text_message(ent, "universe/mission_18_thor/mission_18_thor_101", qtrue, qfalse);
								}
								else if (ent->client->pers.universe_quest_messages == 102)
								{
									ent->client->pers.universe_quest_progress = 19;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}

								if (ent->client->pers.universe_quest_messages < 56 && npc_ent)
								{ // zyk: tests npc_ent so if for some reason the npc dont get spawned, the server tries to spawn it again
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 100;

									npc_ent->client->pers.universe_quest_objective_control = ent->s.number; // zyk: flag to set this npc as a citizen in this map

									npc_ent->client->playerTeam = NPCTEAM_ENEMY;
									npc_ent->client->enemyTeam = NPCTEAM_PLAYER;
								}
								else if (ent->client->pers.universe_quest_messages < 56)
								{
									ent->client->pers.universe_quest_timer = level.time + 500;
								}
								else if (ent->client->pers.universe_quest_messages > 55 && ent->client->pers.universe_quest_messages < 59)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
								else if (ent->client->pers.universe_quest_messages >= 100 && ent->client->pers.universe_quest_messages < 102)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 19 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: Universe Quest, The Path of Evil mission of Thor Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 3)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									zyk_NPC_Kill_f("all");
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("guardian_of_time", -2000, -800, -470, 0);
								else if (ent->client->pers.hunter_quest_messages == 2)
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -1900, -800, -470, 179);

								if (npc_ent)
								{
									if (ent->client->pers.hunter_quest_messages == 2)
										npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -2000, -800, -470);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 3 && Distance(ent->client->ps.origin, zyk_quest_point) < 300))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 14)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 4 || ent->client->pers.universe_quest_messages == 6 ||
										ent->client->pers.universe_quest_messages == 9 || ent->client->pers.universe_quest_messages == 11)
									{
										zyk_text_message(ent, va("universe/mission_19_thor/mission_19_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_19_thor/mission_19_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 15)
								{
									ent->client->pers.universe_quest_progress = 20;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 20 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: Universe Quest Thor Sequel, Guardian of Time boss battle
							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -2000, -800, -470);

								if (ent->client->pers.universe_quest_messages == 0 && Distance(ent->client->ps.origin, zyk_quest_point) < 300)
								{
									ent->client->pers.universe_quest_messages++;
								}

								if (ent->client->pers.universe_quest_messages == 1)
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 3000;

									spawn_boss(ent, -1900, -800, -470, 179, "guardian_of_time_boss", -2000, -800, -470, 0, 20);
								}
								else if (ent->client->pers.universe_quest_messages == 3)
								{
									ent->client->pers.universe_quest_progress = 21;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
						else if (ent->client->pers.universe_quest_progress == 21 && ent->client->pers.can_play_quest == 1 &&
							ent->client->pers.universe_quest_counter & (1 << 2))
						{ // zyk: Universe Quest, final mission of Thor Sequel
							if (ent->client->pers.hunter_quest_timer < level.time && ent->client->pers.hunter_quest_messages < 2)
							{
								gentity_t *npc_ent = NULL;

								if (ent->client->pers.hunter_quest_messages == 0)
									zyk_NPC_Kill_f("all");
								else if (ent->client->pers.hunter_quest_messages == 1)
									npc_ent = Zyk_NPC_SpawnType("thor_boss", -1900, -800, -470, 179);

								if (npc_ent)
								{
									npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

									npc_ent->client->playerTeam = NPCTEAM_PLAYER;
									npc_ent->client->enemyTeam = NPCTEAM_ENEMY;

									npc_ent->client->pers.universe_quest_messages = -2000;
								}

								ent->client->pers.hunter_quest_messages++;
								ent->client->pers.hunter_quest_timer = level.time + 1000;
							}

							if (ent->client->pers.universe_quest_timer < level.time)
							{
								vec3_t zyk_quest_point;

								VectorSet(zyk_quest_point, -1900, -800, -470);

								if (ent->client->pers.universe_quest_messages > 0 || (ent->client->pers.hunter_quest_messages == 2 && Distance(ent->client->ps.origin, zyk_quest_point) < 300))
								{
									ent->client->pers.universe_quest_messages++;
									ent->client->pers.universe_quest_timer = level.time + 5000;
								}

								if (ent->client->pers.universe_quest_messages >= 1 && ent->client->pers.universe_quest_messages <= 11)
								{
									if (ent->client->pers.universe_quest_messages == 1 || ent->client->pers.universe_quest_messages == 10)
									{
										zyk_text_message(ent, va("universe/mission_21_thor/mission_21_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
									}
									else
									{
										zyk_text_message(ent, va("universe/mission_21_thor/mission_21_thor_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
									}
								}
								else if (ent->client->pers.universe_quest_messages == 12)
								{
									ent->client->pers.universe_quest_progress = 22;

									save_account(ent, qtrue);

									quest_get_new_player(ent);
								}
							}
						}
					}
					else if (level.quest_map == 25)
					{ // zyk: seventh objective of Universe Quest
						if (ent->client->pers.universe_quest_progress == 6 && ent->client->pers.can_play_quest == 1 && ent->client->pers.universe_quest_timer < level.time)
						{
							gentity_t *npc_ent = NULL;
							if (ent->client->pers.universe_quest_messages == 0)
							{
								zyk_text_message(ent, "universe/mission_6/mission_6_arrival", qtrue, qfalse, ent->client->pers.netname);
								npc_ent = Zyk_NPC_SpawnType("quest_reborn_boss",1800,-2900,2785,90);
							}
							else if (ent->client->pers.universe_quest_messages >= 2 && ent->client->pers.universe_quest_messages <= 9)
							{
								if (ent->client->pers.universe_quest_messages == 5 || ent->client->pers.universe_quest_messages == 6)
								{
									zyk_text_message(ent, va("universe/mission_6/mission_6_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse);
								}
								else
								{
									zyk_text_message(ent, va("universe/mission_6/mission_6_%d", ent->client->pers.universe_quest_messages), qtrue, qfalse, ent->client->pers.netname);
								}
							}
							else if (ent->client->pers.universe_quest_messages == 10)
							{
								spawn_boss(ent,2135,-2857,2620,-90,"master_of_evil",0,0,0,0,12);

								npc_ent = NULL;
							}
							else if (ent->client->pers.universe_quest_messages == 12)
							{ // zyk: defeated Master of Evil
								zyk_NPC_Kill_f("all"); // zyk: killing the guardian spawns
								zyk_text_message(ent, "universe/mission_6/mission_6_12", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 13)
							{ // zyk: defeated Master of Evil
								zyk_text_message(ent, "universe/mission_6/mission_6_13", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 14)
							{ // zyk: defeated Master of Evil
								zyk_text_message(ent, "universe/mission_6/mission_6_14", qtrue, qfalse, ent->client->pers.netname);
							}
							else if (ent->client->pers.universe_quest_messages == 15)
							{ // zyk: defeated Master of Evil
								zyk_text_message(ent, "universe/mission_6/mission_6_15", qtrue, qfalse);
							}
							else if (ent->client->pers.universe_quest_messages == 16)
							{ // zyk: defeated Master of Evil
								zyk_text_message(ent, "universe/mission_6/mission_6_16", qtrue, qfalse, ent->client->pers.netname);

								ent->client->pers.universe_quest_progress = 7;
								if (ent->client->pers.universe_quest_counter & (1 << 29))
								{ // zyk: if player is in Challenge Mode, do not remove this bit value
									ent->client->pers.universe_quest_counter = 0;
									ent->client->pers.universe_quest_counter |= (1 << 29);
								}
								else
									ent->client->pers.universe_quest_counter = 0;

								save_account(ent, qtrue);

								quest_get_new_player(ent);
							}

							if (ent->client->pers.universe_quest_messages < 1)
							{ // zyk: spawning the reborn npcs
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 1500;
							}
							else if (ent->client->pers.universe_quest_messages > 1 && ent->client->pers.universe_quest_messages < 11)
							{ // zyk: talking to the Master of Evil
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}
							else if (ent->client->pers.universe_quest_messages == 11)
							{ // zyk: fighting the Master of Evil
								ent->client->pers.universe_quest_timer = level.time + 2000;
							}
							else if (ent->client->pers.universe_quest_messages > 11)
							{ // zyk: defeated the Master of Evil
								ent->client->pers.universe_quest_messages++;
								ent->client->pers.universe_quest_timer = level.time + 5000;
							}

							if (npc_ent)
							{ // zyk: setting the player who invoked this npc
								npc_ent->client->pers.universe_quest_objective_control = ent-g_entities;
							}
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
			poison_dart_hits(ent);
			fire_bolt_hits(ent);

			if (ent->client->pers.universe_quest_artifact_holder_id != -1 && ent->health > 0 && ent->client->ps.powerups[PW_FORCE_BOON] < (level.time + 1000))
			{ // zyk: artifact holder npcs. Keep their artifact (force boon) active
				ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 1000;
			}

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

			// zyk: abilities of custom quest npcs
			if (ent->client->pers.player_statuses & (1 << 28) && ent->health > 0)
			{
				// zyk: magic powers
				if (ent->client->pers.light_quest_timer < level.time)
				{
					int random_number = Q_irand(0, 29);

					if (ent->client->sess.selected_left_special_power & (1 << MAGIC_HEALING_WATER) && random_number == 0)
					{
						healing_water(ent, 120);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_WATER_SPLASH) && random_number == 1)
					{
						water_splash(ent, 400, 15);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_WATER_ATTACK) && random_number == 2)
					{
						water_attack(ent, 500, 40);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_EARTHQUAKE) && random_number == 3)
					{
						earthquake(ent, 2000, 300, 500);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_ROCKFALL) && random_number == 4)
					{
						rock_fall(ent, 500, 40);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_SHIFTING_SAND) && random_number == 5)
					{
						shifting_sand(ent, 1000);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_SLEEPING_FLOWERS) && random_number == 6)
					{
						sleeping_flowers(ent, 2500, 350);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_POISON_MUSHROOMS) && random_number == 7)
					{
						poison_mushrooms(ent, 100, 600);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_TREE_OF_LIFE) && random_number == 8)
					{
						tree_of_life(ent);
					}
					else if (ent->client->sess.selected_left_special_power & (1 << MAGIC_MAGIC_SHIELD) && random_number == 9)
					{
						magic_shield(ent, 6000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_DOME_OF_DAMAGE) && random_number == 10)
					{
						dome_of_damage(ent, 500, 25);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_MAGIC_DISABLE) && random_number == 11)
					{
						magic_disable(ent, 450);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ULTRA_SPEED) && random_number == 12)
					{
						ultra_speed(ent, 15000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_SLOW_MOTION) && random_number == 13)
					{
						slow_motion(ent, 400, 15000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_FAST_AND_SLOW) && random_number == 14)
					{
						fast_and_slow(ent, 400, 6000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_FLAME_BURST) && random_number == 15)
					{
						flame_burst(ent, 5000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ULTRA_FLAME) && random_number == 16)
					{
						ultra_flame(ent, 500, 35);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_FLAMING_AREA) && random_number == 17)
					{
						flaming_area(ent, 20);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_BLOWING_WIND) && random_number == 18)
					{
						blowing_wind(ent, 700, 5000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_HURRICANE) && random_number == 19)
					{
						hurricane(ent, 600, 5000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_REVERSE_WIND) && random_number == 20)
					{
						reverse_wind(ent, 700, 5000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ULTRA_RESISTANCE) && random_number == 21)
					{
						ultra_resistance(ent, 30000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ULTRA_STRENGTH) && random_number == 22)
					{
						ultra_strength(ent, 30000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ENEMY_WEAKENING) && random_number == 23)
					{
						enemy_nerf(ent, 450);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ICE_STALAGMITE) && random_number == 24)
					{
						ice_stalagmite(ent, 500, 130);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ICE_BOULDER) && random_number == 25)
					{
						ice_boulder(ent, 380, 40);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_ICE_BLOCK) && random_number == 26)
					{
						ice_block(ent, 3500);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_HEALING_AREA) && random_number == 27)
					{
						healing_area(ent, 2, 5000);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_MAGIC_EXPLOSION) && random_number == 28)
					{
						magic_explosion(ent, 320, 130, 900);
					}
					else if (ent->client->sess.selected_left_special_power  & (1 << MAGIC_LIGHTNING_DOME) && random_number == 29)
					{
						lightning_dome(ent, 70);
					}

					ent->client->pers.light_quest_timer = level.time + ent->client->pers.light_quest_messages;
				}

				// zyk: ultimate magic and quest powers
				if (ent->client->pers.hunter_quest_timer < level.time)
				{
					int random_number = Q_irand(0, 7);

					if (ent->client->sess.selected_right_special_power  & (1 << 0) && random_number == 0)
					{
						ultra_drain(ent, 450, 30, 8000);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 1) && random_number == 1)
					{
						immunity_power(ent, 20000);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 2) && random_number == 2)
					{
						chaos_power(ent, 400, 4600);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 3) && random_number == 3)
					{
						time_power(ent, 400, 4000);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 4) && random_number == 4)
					{ // zyk: Light Power
						ent->client->pers.quest_power_status |= (1 << 14);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 5) && random_number == 5)
					{ // zyk: Dark Power
						ent->client->pers.quest_power_status |= (1 << 15);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 6) && random_number == 6)
					{ // zyk: Eternity Power
						ent->client->pers.quest_power_status |= (1 << 16);
					}
					else if (ent->client->sess.selected_right_special_power  & (1 << 7) && random_number == 7)
					{ // zyk: Universe Power
						ent->client->pers.quest_power_status |= (1 << 13);
					}

					ent->client->pers.hunter_quest_timer = level.time + ent->client->pers.hunter_quest_messages;
				}

				// zyk: unique abilities
				if (ent->client->pers.universe_quest_timer < level.time)
				{
					int random_number = Q_irand(0, 4);

					if (ent->client->sess.selected_special_power & (1 << 0) && random_number == 0)
					{
						ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;

						ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
						ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
						ent->client->ps.forceHandExtendTime = level.time + 2000;

						zyk_super_beam(ent, ent->client->ps.viewangles[1]);
					}
					else if (ent->client->sess.selected_special_power & (1 << 1) && random_number == 1)
					{
						ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
						elemental_attack(ent);
					}
					else if (ent->client->sess.selected_special_power & (1 << 2) && random_number == 2)
					{
						ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
						zyk_no_attack(ent);
					}
					else if (ent->client->sess.selected_special_power & (1 << 3) && random_number == 3)
					{
						ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
						force_scream(ent);
					}
					else if (ent->client->sess.selected_special_power & (1 << 4) && random_number == 4)
					{
						ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
						zyk_force_storm(ent);
					}

					ent->client->pers.universe_quest_timer = level.time + ent->client->pers.universe_quest_messages;
				}
			}

			// zyk: quest guardians special abilities
			if (ent->client->pers.guardian_invoked_by_id != -1 && ent->health > 0)
			{
				if (ent->client->pers.guardian_mode == 1 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_1") == 0))
				{ // zyk: Guardian of Water
					if (ent->client->pers.guardian_timer < level.time)
					{
						gentity_t *player_ent = &g_entities[ent->client->pers.guardian_invoked_by_id];
						int distance = (int)Distance(ent->client->ps.origin,player_ent->client->ps.origin);

						if (distance > 400)
						{
							healing_water(ent, 120);
							trap->SendServerCommand( -1, "chat \"^4Guardian of Water: ^7Healing Water!\"");
						}
						else
						{
							water_splash(ent,400,15);
							trap->SendServerCommand( -1, "chat \"^4Guardian of Water: ^7Water Splash!\"");
						}

						ent->client->pers.guardian_timer = level.time + 14000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						water_attack(ent, 2500, 40);
						trap->SendServerCommand(-1, "chat \"^4Guardian of Water: ^7Water Attack!\"");
						ent->client->pers.light_quest_timer = level.time + 11000;
					}
				}
				else if (ent->client->pers.guardian_mode == 2 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_2") == 0))
				{ // zyk: Guardian of Earth
					if (ent->client->pers.guardian_timer < level.time)
					{ // zyk: uses earthquake ability
						earthquake(ent,2000,350,3000);
						ent->client->pers.guardian_timer = level.time + 3000 + ent->health;
						trap->SendServerCommand( -1, "chat \"^3Guardian of Earth: ^7Earthquake!\"");
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						rock_fall(ent, 2000, 40);
						ent->client->pers.light_quest_timer = level.time + 10000;
						trap->SendServerCommand( -1, "chat \"^3Guardian of Earth: ^7Rockfall!\"");
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{
						shifting_sand(ent, 5500);
						ent->client->pers.universe_quest_timer = level.time + 11000;
						trap->SendServerCommand(-1, "chat \"^3Guardian of Earth: ^7Shifting Sand!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 3 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_3") == 0))
				{ // zyk: Guardian of Forest
					if (ent->client->pers.guardian_timer < level.time)
					{ // zyk: uses sleeping flowers or poison mushrooms
						if (Q_irand(0,3) != 0)
						{
							sleeping_flowers(ent,2500,1500);
							trap->SendServerCommand( -1, "chat \"^2Guardian of Forest: ^7Sleeping Flowers!\"");
							ent->client->pers.guardian_timer = level.time + 12000;
						}
						else
						{
							poison_mushrooms(ent,100,3000);
							trap->SendServerCommand( -1, va("chat \"^2Guardian of Forest: ^7Poison Mushrooms!\""));
							ent->client->pers.guardian_timer = level.time + 12000;
						}
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						tree_of_life(ent);
						ent->client->pers.light_quest_timer = level.time + 15000;
						trap->SendServerCommand(-1, va("chat \"^2Guardian of Forest: ^7Tree of Life!\""));
					}
				}
				else if (ent->client->pers.guardian_mode == 4 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_4") == 0))
				{ // zyk: Guardian of Intelligence
					if (ent->client->pers.guardian_timer < level.time)
					{
						if (ent->client->pers.light_quest_messages == 0)
						{
							dome_of_damage(ent, 1700, 25);
							ent->client->pers.light_quest_messages = 1;
							trap->SendServerCommand( -1, "chat \"^5Guardian of Intelligence: ^7Dome of Damage!\"");
						}
						else
						{
							magic_shield(ent, 6000);
							ent->client->pers.light_quest_messages = 0;
							trap->SendServerCommand( -1, "chat \"^5Guardian of Intelligence: ^7Magic Shield!\"");
						}

						ent->client->pers.guardian_timer = level.time + ent->health + 8000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						magic_disable(ent, 500);
						ent->client->pers.light_quest_timer = level.time + 14000;
						trap->SendServerCommand(-1, "chat \"^5Guardian of Intelligence: ^7Magic Disable!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 5 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_5") == 0))
				{ // zyk: Guardian of Agility
					// zyk: adding jetpack to this boss
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

					if (ent->client->pers.light_quest_timer < level.time)
					{ // zyk: after losing half HP, uses his special ability
						ultra_speed(ent,10000);
						trap->SendServerCommand( -1, "chat \"^6Guardian of Agility: ^7Ultra Speed!\"");
						ent->client->pers.light_quest_timer = level.time + 12000;
					}

					if (ent->client->pers.guardian_timer < level.time)
					{
						slow_motion(ent,600,12000);
						trap->SendServerCommand( -1, "chat \"^6Guardian of Agility: ^7Slow Motion!\"");
						ent->client->pers.guardian_timer = level.time + 14000;
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{
						fast_and_slow(ent, 600, 6000);
						ent->client->pers.universe_quest_timer = level.time + 20000;
						trap->SendServerCommand(-1, "chat \"^6Guardian of Agility: ^7Fast and Slow!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 6 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_6") == 0))
				{ // zyk: Guardian of Fire
					// zyk: take him back if he falls
					if (ent->client->ps.origin[2] < -600)
					{
						vec3_t origin;
						vec3_t yaw;

						origin[0] = 0.0f;
						origin[1] = -269.0f;
						origin[2] = -374.0f;
						yaw[0] = 0.0f;
						yaw[1] = 90.0f;
						yaw[2] = 0.0f;
						zyk_TeleportPlayer( ent, origin, yaw );
					}

					if (ent->client->pers.guardian_timer < level.time)
					{ // zyk: fire ability
						flame_burst(ent, 5000);
						trap->SendServerCommand( -1, "chat \"^1Guardian of Fire: ^7Flame Burst!\"");
						ent->client->pers.guardian_timer = level.time + 18000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						ultra_flame(ent, 4000, 35);
						trap->SendServerCommand( -1, "chat \"^1Guardian of Fire: ^7Ultra Flame!\"");
						ent->client->pers.light_quest_timer = level.time + 16000;
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{
						flaming_area(ent, 20);
						ent->client->pers.universe_quest_timer = level.time + 19000;
						trap->SendServerCommand(-1, "chat \"^1Guardian of Fire: ^7Flaming Area!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 7 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_7") == 0))
				{ // zyk: Guardian of Wind
					if (ent->client->pers.guardian_timer < level.time)
					{
						if (!Q_irand(0,1))
						{ // zyk: randomly choose between Blowing Wind and Reverse Wind
							blowing_wind(ent, 2500, 5000);
							trap->SendServerCommand(-1, "chat \"^7Guardian of Wind: ^7Blowing Wind!\"");
						}
						else
						{
							reverse_wind(ent, 2500, 5000);
							trap->SendServerCommand(-1, "chat \"^7Guardian of Wind: ^7Reverse Wind!\"");
						}

						ent->client->pers.guardian_timer = level.time + 12000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						hurricane(ent,700,5000);
						trap->SendServerCommand( -1, "chat \"^7Guardian of Wind: ^7Hurricane!\"");
						ent->client->pers.light_quest_timer = level.time + 12000;
					}
				}
				else if (ent->client->pers.guardian_mode == 16 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_10") == 0))
				{ // zyk: Guardian of Ice
					if (ent->client->pers.guardian_timer < level.time)
					{
						ice_stalagmite(ent, 500, 130);
						ent->client->pers.guardian_timer = level.time + 16000;
						trap->SendServerCommand( -1, "chat \"^5Guardian of Ice: ^7Ice Stalagmite!\"");
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						ice_boulder(ent, 400, 40);
						trap->SendServerCommand( -1, "chat \"^5Guardian of Ice: ^7Ice Boulder!\"");
						ent->client->pers.light_quest_timer = level.time + 16000;
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{
						ice_block(ent, 3500);
						ent->client->pers.universe_quest_timer = level.time + 16000;
						trap->SendServerCommand(-1, "chat \"^5Guardian of Ice: ^7Ice Block!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 8 || (ent->client->pers.guardian_mode == 18 && Q_stricmp(ent->NPC_type, "guardian_boss_9") == 0))
				{ // zyk: Guardian of Light
					if (ent->client->pers.hunter_quest_messages == 0 && ent->health < (ent->client->ps.stats[STAT_MAX_HEALTH]))
					{ // zyk: after losing half HP, uses his special ability
						ent->client->pers.hunter_quest_messages = 1;
						ent->client->pers.quest_power_status |= (1 << 14);
						trap->SendServerCommand( -1, "chat \"^5Guardian of Light: ^7Light Power!\"");
					}

					if (ent->client->pers.guardian_timer < level.time)
					{
						lightning_dome(ent, 70);
						trap->SendServerCommand( -1, "chat \"^5Guardian of Light: ^7Lightning Dome!\"");
						ent->client->pers.guardian_timer = level.time + 15000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						water_attack(ent, 4000, 40);
						trap->SendServerCommand(-1, "chat \"^5Guardian of Light: ^7Water Attack!\"");
						ent->client->pers.light_quest_timer = level.time + 19000;
					}
				}
				else if (ent->client->pers.guardian_mode == 9 || (ent->client->pers.guardian_mode == 18 && Q_stricmp(ent->NPC_type, "guardian_of_darkness") == 0))
				{ // zyk: Guardian of Darkness
					if (ent->client->pers.hunter_quest_messages == 0 && ent->health < (ent->client->ps.stats[STAT_MAX_HEALTH]))
					{ // zyk: after losing half HP, uses his special ability
						ent->client->pers.hunter_quest_messages = 1;
						ent->client->pers.quest_power_status |= (1 << 15);
						trap->SendServerCommand( -1, "chat \"^1Guardian of Darkness: ^7Dark Power!\"");
					}

					if (ent->client->pers.guardian_timer < level.time)
					{
						magic_explosion(ent, 320, 130, 900);
						trap->SendServerCommand(-1, "chat \"^1Guardian of Darkness: ^7Magic Explosion!\"");
						ent->client->pers.guardian_timer = level.time + 17000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						earthquake(ent, 2000, 300, 1000);
						trap->SendServerCommand(-1, "chat \"^1Guardian of Darkness: ^7Earthquake!\"");
						ent->client->pers.light_quest_timer = level.time + 19000;
					}
				}
				else if (ent->client->pers.guardian_mode == 10 || (ent->client->pers.guardian_mode == 18 && Q_stricmp(ent->NPC_type, "guardian_of_eternity") == 0))
				{ // zyk: Guardian of Eternity
					if (ent->client->pers.hunter_quest_messages == 0 && ent->health < (ent->client->ps.stats[STAT_MAX_HEALTH]))
					{ // zyk: after losing half HP, uses his special ability
						ent->client->pers.hunter_quest_messages = 1;
						ent->client->pers.quest_power_status |= (1 << 16);
						trap->SendServerCommand( -1, "chat \"^3Guardian of Eternity: ^7Eternity Power!\"");
					}

					if (ent->client->pers.guardian_timer < level.time)
					{
						healing_area(ent,2,5000);
						trap->SendServerCommand( -1, "chat \"^3Guardian of Eternity: ^7Healing Area!\"");
						ent->client->pers.guardian_timer = level.time + 15000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						magic_shield(ent, 6000);
						trap->SendServerCommand(-1, "chat \"^3Guardian of Eternity: ^7Magic Shield!\"");
						ent->client->pers.light_quest_timer = level.time + 19000;
					}
				}
				else if (ent->client->pers.guardian_mode == 11 || (ent->client->pers.guardian_mode == 17 && Q_stricmp(ent->NPC_type, "guardian_boss_8") == 0))
				{ // zyk: Guardian of Resistance
					if (ent->client->pers.guardian_timer < level.time)
					{
						ultra_resistance(ent, 10000);
						ent->client->pers.guardian_timer = level.time + 14000;
						trap->SendServerCommand( -1, "chat \"^3Guardian of Resistance: ^7Ultra Resistance!\"");
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						ultra_strength(ent, 10000);
						ent->client->pers.light_quest_timer = level.time + 14000;
						trap->SendServerCommand( -1, "chat \"^3Guardian of Resistance: ^7Ultra Strength!\"");
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{
						enemy_nerf(ent, 1000);
						ent->client->pers.universe_quest_timer = level.time + 12000;
						trap->SendServerCommand(-1, "chat \"^3Guardian of Resistance: ^7Enemy Weakening!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 12)
				{ // zyk: Master of Evil
					// zyk: adding jetpack to this boss
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

					// zyk: take him back if he falls
					if (ent->client->ps.origin[0] < 400 || ent->client->ps.origin[0] > 3600 || ent->client->ps.origin[1] < -4450 || ent->client->ps.origin[1] > -1250 || ent->client->ps.origin[2] < 2500 || ent->client->ps.origin[2] > 4000)
					{
						vec3_t origin;
						vec3_t yaw;

						origin[0] = 2135.0f;
						origin[1] = -2857.0f;
						origin[2] = 2800.0f;
						yaw[0] = 0.0f;
						yaw[1] = -90.0f;
						yaw[2] = 0.0f;
						zyk_TeleportPlayer( ent, origin, yaw );
					}

					if (ent->client->pers.guardian_timer < level.time)
					{
						if (ent->client->NPC_class == CLASS_REBORN)
							ent->client->NPC_class = CLASS_BOBAFETT;
						else
							ent->client->NPC_class = CLASS_REBORN;
						
						if (ent->health < (ent->client->ps.stats[STAT_MAX_HEALTH] / 5))
							Zyk_NPC_SpawnType("quest_mage", 2135, -2857, 2800, 90);
						else if (ent->health < ((ent->client->ps.stats[STAT_MAX_HEALTH]/5) * 2))
							Zyk_NPC_SpawnType("quest_reborn_boss",2135,-2857,2800,90);
						else if (ent->health < ((ent->client->ps.stats[STAT_MAX_HEALTH]/5) * 3))
							Zyk_NPC_SpawnType("quest_reborn_red",2135,-2857,2800,90);
						else if (ent->health < ((ent->client->ps.stats[STAT_MAX_HEALTH]/5) * 4))
							Zyk_NPC_SpawnType("quest_reborn_blue",2135,-2857,2800,90);
						else if (ent->health < ent->client->ps.stats[STAT_MAX_HEALTH])
							Zyk_NPC_SpawnType("quest_reborn",2135,-2857,2800,-90);

						if (!ent->client->ps.powerups[PW_CLOAKED])
							Jedi_Cloak(ent);

						ultra_drain(ent, 450, 30, 8000);
						trap->SendServerCommand( -1, "chat \"^1Master of Evil: ^7Ultra Drain!\"");

						ent->client->pers.guardian_timer = level.time + 30000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						ultra_flame(ent, 4000, 35);
						trap->SendServerCommand(-1, "chat \"^1Master of Evil: ^7Ultra Flame!\"");
						ent->client->pers.light_quest_timer = level.time + 29000;
					}
				}
				else if (ent->client->pers.guardian_mode == 13 || (ent->client->pers.guardian_mode == 18 && Q_stricmp(ent->NPC_type, "guardian_of_universe") == 0))
				{ // zyk: Guardian of Universe
					if (ent->client->pers.guardian_timer < level.time)
					{
						gentity_t *player_ent = &g_entities[ent->client->pers.guardian_invoked_by_id];
						gentity_t *npc_ent = NULL;
						vec3_t origin;
						vec3_t yaw;

						origin[0] = player_ent->client->ps.origin[0];
						origin[1] = player_ent->client->ps.origin[1];
						origin[2] = player_ent->client->ps.origin[2] + 200;

						yaw[0] = 0.0f;
						yaw[1] = -179.0f;
						yaw[2] = 0.0f;

						zyk_TeleportPlayer( ent, origin, yaw );

						if (ent->health < ent->client->ps.stats[STAT_MAX_HEALTH] && ent->client->pers.hunter_quest_messages < 15)
						{ // zyk: she can spawn up to 15 clones
							ent->client->pers.hunter_quest_messages++;
							npc_ent = NPC_SpawnType(player_ent, "guardian_of_universe",NULL,qfalse);
						}

						if (npc_ent)
						{
							npc_ent->health = ent->client->ps.stats[STAT_MAX_HEALTH]/10;
							npc_ent->client->ps.stats[STAT_MAX_HEALTH] = npc_ent->health;
						}

						if (ent->client->pers.guardian_mode == 18 && Q_irand(0, 1) == 0)
						{
							ultra_drain(ent, 450, 30, 8000);
							trap->SendServerCommand(-1, "chat \"^2Guardian of Universe: ^7Ultra Drain!\"");
						}
						else
						{
							immunity_power(ent, 20000);
							trap->SendServerCommand(-1, "chat \"^2Guardian of Universe: ^7Immunity Power!\"");
						}

						ent->client->pers.guardian_timer = level.time + 35000;
					}

					if (ent->health < (ent->client->ps.stats[STAT_MAX_HEALTH]/2) && ent->client->pers.light_quest_messages == 0)
					{
						ent->client->pers.light_quest_messages = 1;
						ent->client->pers.quest_power_status |= (1 << 13);
						trap->SendServerCommand( -1, "chat \"^2Guardian of Universe: ^7Universe Power!\"");
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						if (ent->client->pers.guardian_mode == 18)
						{ // zyk: unique
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;

							ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
							ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
							ent->client->ps.forceHandExtendTime = level.time + 2000;

							zyk_super_beam(ent, ent->client->ps.viewangles[1]);
						}

						magic_explosion(ent, 320, 130, 900);
						trap->SendServerCommand( -1, "chat \"^2Guardian of Universe: ^7Magic Explosion!\"");
						ent->client->pers.light_quest_timer = level.time + 16000;
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{
						magic_shield(ent, 6000);
						ent->client->pers.universe_quest_timer = level.time + 29000;
						trap->SendServerCommand(-1, "chat \"^2Guardian of Universe: ^7Magic Shield!\"");
					}
				}
				else if (ent->client->pers.guardian_mode == 14)
				{ // zyk: Guardian of Chaos
					if (ent->client->pers.guardian_timer < level.time)
					{
						if (ent->client->pers.hunter_quest_messages == 0)
						{
							if (!ent->client->ps.powerups[PW_CLOAKED])
								Jedi_Cloak(ent);

							magic_shield(ent,6000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Magic Shield!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 1)
						{
							blowing_wind(ent,3000,5000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Blowing Wind!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 2)
						{
							ice_block(ent, 3500);
							trap->SendServerCommand(-1, "chat \"^1Guardian of Chaos: ^7Ice Block!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 3)
						{
							healing_water(ent,120);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Healing Water!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 4)
						{
							immunity_power(ent,20000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Immunity Power!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 5)
						{
							sleeping_flowers(ent, 2500, 1000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Sleeping Flowers!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 6)
						{
							ultra_strength(ent,12000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Ultra Strength!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 7)
						{
							ultra_drain(ent, 450, 30, 8000);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Ultra Drain!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 8)
						{
							ice_stalagmite(ent, 2000, 130);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Ice Stalagmite!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 9)
						{
							ice_boulder(ent, 1000, 40);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Ice Boulder!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 10)
						{
							water_attack(ent, 1600, 40);
							trap->SendServerCommand(-1, va("chat \"^1Guardian of Chaos: ^7Water Attack!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 11)
						{
							poison_mushrooms(ent,100,1800);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Poison Mushrooms!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 12)
						{
							ultra_speed(ent,15000);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Ultra Speed!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 13)
						{
							slow_motion(ent,1000,10000);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Slow Motion!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 14)
						{
							water_splash(ent,1400,15);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Water Splash!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 15)
						{
							rock_fall(ent, 1600, 40);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Rockfall!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 16)
						{
							ultra_flame(ent, 2200, 35);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Ultra Flame!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 17)
						{
							magic_disable(ent, 2200);
							trap->SendServerCommand(-1, va("chat \"^1Guardian of Chaos: ^7Magic Disable!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 18)
						{
							dome_of_damage(ent, 2000, 25);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Dome of Damage!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 19)
						{
							shifting_sand(ent, 2500);
							trap->SendServerCommand(-1, va("chat \"^1Guardian of Chaos: ^7Shifting Sand!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 20)
						{
							flame_burst(ent, 5000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Flame Burst!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 21)
						{
							fast_and_slow(ent, 1000, 6000);
							trap->SendServerCommand(-1, "chat \"^1Guardian of Chaos: ^7Fast and Slow!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 22)
						{
							flaming_area(ent, 20);
							trap->SendServerCommand(-1, "chat \"^1Guardian of Chaos: ^7Flaming Area!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 23)
						{
							hurricane(ent,1200,5000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Hurricane!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 24)
						{
							enemy_nerf(ent, 2000);
							trap->SendServerCommand(-1, "chat \"^1Guardian of Chaos: ^7Enemy Weakening!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 25)
						{
							lightning_dome(ent, 70);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Lightning Dome!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 26)
						{
							earthquake(ent,2000,300,3000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Earthquake!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 27)
						{
							time_power(ent,1600, 4000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Time Power!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 28)
						{
							healing_area(ent,2,5000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Healing Area!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 29)
						{
							tree_of_life(ent);
							trap->SendServerCommand(-1, "chat \"^1Guardian of Chaos: ^7Tree of Life!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 30)
						{
							vec3_t origin, angles;

							VectorSet(origin,-2836,-26946,500);
							VectorSet(angles,0,0,0);

							zyk_TeleportPlayer(ent,origin,angles);

							ultra_resistance(ent,12000);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Ultra Resistance!\"");
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 31)
						{
							reverse_wind(ent, 2500, 5000);
							trap->SendServerCommand(-1, va("chat \"^1Guardian of Chaos: ^7Reverse Wind!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 32)
						{
							magic_explosion(ent, 320, 130, 900);
							trap->SendServerCommand( -1, va("chat \"^1Guardian of Chaos: ^7Magic Explosion!\""));
							ent->client->pers.hunter_quest_messages++;
						}
						else if (ent->client->pers.hunter_quest_messages == 33)
						{
							chaos_power(ent,1600, 4600);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Chaos Power!\"");
							ent->client->pers.hunter_quest_messages = 0;
						}

						ent->client->pers.guardian_timer = level.time + (ent->health/2) + 5000;
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						if (ent->client->pers.light_quest_messages == 0)
						{
							ent->client->pers.light_quest_messages = 1;
						}
						else if (ent->client->pers.light_quest_messages == 1)
						{
							ent->client->pers.light_quest_messages = 2;
							ent->client->pers.quest_power_status |= (1 << 14);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Light Power!\"");
						}
						else if (ent->client->pers.light_quest_messages == 2)
						{
							ent->client->pers.light_quest_messages = 3;
							ent->client->pers.quest_power_status |= (1 << 15);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Dark Power!\"");
						}
						else if (ent->client->pers.light_quest_messages == 3)
						{
							ent->client->pers.light_quest_messages = 4;
							ent->client->pers.quest_power_status |= (1 << 16);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Eternity Power!\"");
						}
						else if (ent->client->pers.light_quest_messages == 4)
						{
							ent->client->pers.light_quest_messages = 5;
							ent->client->pers.quest_power_status |= (1 << 13);
							trap->SendServerCommand( -1, "chat \"^1Guardian of Chaos: ^7Universe Power!\"");
						}

						ent->client->pers.light_quest_timer = level.time + 27000;
					}
				}
				else if (ent->client->pers.guardian_mode == 15 && Q_stricmp(ent->NPC_type, "ymir_boss") == 0)
				{ // zyk: Ymir
				  // zyk: adding jetpack to this boss
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

					if (ent->client->pers.guardian_timer < level.time)
					{
						int random_magic = Q_irand(0, 25);

						if (random_magic == 0)
						{
							ultra_strength(ent, 30000);
						}
						else if (random_magic == 1)
						{
							poison_mushrooms(ent, 100, 600);
						}
						else if (random_magic == 2)
						{
							water_splash(ent, 400, 15);
						}
						else if (random_magic == 3)
						{
							ultra_flame(ent, 500, 35);
						}
						else if (random_magic == 4)
						{
							rock_fall(ent, 500, 40);
						}
						else if (random_magic == 5)
						{
							dome_of_damage(ent, 500, 25);
						}
						else if (random_magic == 6)
						{
							hurricane(ent, 600, 5000);
						}
						else if (random_magic == 7)
						{
							slow_motion(ent, 400, 15000);
						}
						else if (random_magic == 8)
						{
							ultra_resistance(ent, 30000);
						}
						else if (random_magic == 9)
						{
							sleeping_flowers(ent, 2500, 350);
						}
						else if (random_magic == 10)
						{
							healing_water(ent, 120);
						}
						else if (random_magic == 11)
						{
							flame_burst(ent, 5000);
						}
						else if (random_magic == 12)
						{
							earthquake(ent, 2000, 300, 500);
						}
						else if (random_magic == 13)
						{
							magic_shield(ent, 6000);
						}
						else if (random_magic == 14)
						{
							blowing_wind(ent, 700, 5000);
						}
						else if (random_magic == 15)
						{
							ultra_speed(ent, 15000);
						}
						else if (random_magic == 16)
						{
							ice_stalagmite(ent, 500, 130);
						}
						else if (random_magic == 17)
						{
							ice_boulder(ent, 380, 40);
						}
						else if (random_magic == 18)
						{
							water_attack(ent, 500, 40);
						}
						else if (random_magic == 19)
						{
							tree_of_life(ent);
						}
						else if (random_magic == 20)
						{
							magic_disable(ent, 450);
						}
						else if (random_magic == 21)
						{
							fast_and_slow(ent, 400, 6000);
						}
						else if (random_magic == 22)
						{
							flaming_area(ent, 20);
						}
						else if (random_magic == 23)
						{
							reverse_wind(ent, 700, 5000);
						}
						else if (random_magic == 24)
						{
							enemy_nerf(ent, 450);
						}
						else if (random_magic == 25)
						{
							ice_block(ent, 3500);
						}

						ent->client->pers.guardian_timer = level.time + Q_irand(7000, 10000);

						if (ent->spawnflags & 131072)
						{ // zyk: boss is stronger now
							ent->client->pers.guardian_timer -= 1000;
						}
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{ // zyk: using Crystal of Magic
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;

						ent->health += 100;
						ent->client->ps.stats[STAT_ARMOR] += 100;

						if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH])
							ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

						if (ent->client->ps.stats[STAT_ARMOR] > ent->client->ps.stats[STAT_MAX_HEALTH])
							ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];

						if (ent->client->NPC_class == CLASS_REBORN)
							ent->client->NPC_class = CLASS_BOBAFETT;
						else
							ent->client->NPC_class = CLASS_REBORN;

						ent->client->pers.light_quest_timer = level.time + Q_irand(11000, 14000);

						if (ent->spawnflags & 131072)
						{ // zyk: boss is stronger now
							int random_unique = Q_irand(0, 1);

							if (random_unique == 0)
							{
								ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

								elemental_attack(ent);
							}
							else
							{
								ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

								zyk_no_attack(ent);
							}
						}
					}
				}
				else if (ent->client->pers.guardian_mode == 15 && Q_stricmp(ent->NPC_type, "thor_boss") == 0)
				{ // zyk: Thor
				  // zyk: adding jetpack to this boss
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

					if (ent->client->pers.guardian_timer < level.time)
					{
						int random_magic = Q_irand(0, 6);

						if (random_magic == 0)
						{
							ultra_drain(ent, 450, 30, 8000);
						}
						else if (random_magic == 1)
						{
							immunity_power(ent, 20000);
						}
						else if (random_magic == 2)
						{
							chaos_power(ent, 400, 4600);
						}
						else if (random_magic == 3)
						{
							time_power(ent, 400, 4000);
						}
						else if (random_magic == 4)
						{
							healing_area(ent, 2, 5000);
						}
						else if (random_magic == 5)
						{
							magic_explosion(ent, 320, 130, 900);
						}
						else if (random_magic == 6)
						{
							lightning_dome(ent, 70);
						}

						ent->client->pers.guardian_timer = level.time + Q_irand(8000, 12000);

						if (ent->spawnflags & 131072)
						{ // zyk: boss is stronger now
							ent->client->pers.guardian_timer -= 1000;
						}
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{ // zyk: using Crystal of Magic
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;

						ent->health += 100;
						ent->client->ps.stats[STAT_ARMOR] += 100;

						if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH])
							ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

						if (ent->client->ps.stats[STAT_ARMOR] > ent->client->ps.stats[STAT_MAX_HEALTH])
							ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];

						if (ent->client->NPC_class == CLASS_REBORN)
							ent->client->NPC_class = CLASS_BOBAFETT;
						else
							ent->client->NPC_class = CLASS_REBORN;

						ent->client->pers.light_quest_timer = level.time + Q_irand(10000, 12000);

						if (ent->spawnflags & 131072)
						{ // zyk: boss is stronger now
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;

							ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
							ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
							ent->client->ps.forceHandExtendTime = level.time + 2000;

							zyk_super_beam(ent, ent->client->ps.viewangles[1]);

							ent->client->pers.light_quest_timer -= 1000;
						}
					}
				}
				else if (ent->client->pers.guardian_mode == 19)
				{ // zyk: Ymir
					// zyk: adding jetpack to this boss
					ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

					if (ent->client->pers.guardian_timer < level.time)
					{
						int random_magic = Q_irand(0, 25);

						if (random_magic == 0)
						{
							ultra_strength(ent, 30000);
						}
						else if (random_magic == 1)
						{
							poison_mushrooms(ent, 100, 600);
						}
						else if (random_magic == 2)
						{
							water_splash(ent, 400, 15);
						}
						else if (random_magic == 3)
						{
							ultra_flame(ent, 500, 35);
						}
						else if (random_magic == 4)
						{
							rock_fall(ent, 500, 40);
						}
						else if (random_magic == 5)
						{
							dome_of_damage(ent, 500, 25);
						}
						else if (random_magic == 6)
						{
							hurricane(ent, 600, 5000);
						}
						else if (random_magic == 7)
						{
							slow_motion(ent, 400, 15000);
						}
						else if (random_magic == 8)
						{
							ultra_resistance(ent, 30000);
						}
						else if (random_magic == 9)
						{
							sleeping_flowers(ent, 2500, 350);
						}
						else if (random_magic == 10)
						{
							healing_water(ent, 120);
						}
						else if (random_magic == 11)
						{
							flame_burst(ent, 5000);
						}
						else if (random_magic == 12)
						{
							earthquake(ent, 2000, 300, 500);
						}
						else if (random_magic == 13)
						{
							magic_shield(ent, 6000);
						}
						else if (random_magic == 14)
						{
							blowing_wind(ent, 700, 5000);
						}
						else if (random_magic == 15)
						{
							ultra_speed(ent, 15000);
						}
						else if (random_magic == 16)
						{
							ice_stalagmite(ent, 500, 130);
						}
						else if (random_magic == 17)
						{
							ice_boulder(ent, 380, 40);
						}
						else if (random_magic == 18)
						{
							water_attack(ent, 500, 40);
						}
						else if (random_magic == 19)
						{
							tree_of_life(ent);
						}
						else if (random_magic == 20)
						{
							magic_disable(ent, 450);
						}
						else if (random_magic == 21)
						{
							fast_and_slow(ent, 400, 6000);
						}
						else if (random_magic == 22)
						{
							flaming_area(ent, 20);
						}
						else if (random_magic == 23)
						{
							reverse_wind(ent, 700, 5000);
						}
						else if (random_magic == 24)
						{
							enemy_nerf(ent, 450);
						}
						else if (random_magic == 25)
						{
							ice_block(ent, 3500);
						}

						ent->client->pers.guardian_timer = level.time + Q_irand(7000, 10000);
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{ // zyk: spawning mages to help
						int random_unique = Q_irand(0, 1);

						if (ent->client->NPC_class == CLASS_REBORN)
							ent->client->NPC_class = CLASS_BOBAFETT;
						else
							ent->client->NPC_class = CLASS_REBORN;

						if (random_unique == 0)
						{
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

							elemental_attack(ent);
						}
						else
						{
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

							zyk_no_attack(ent);
						}

						Zyk_NPC_SpawnType("quest_mage", -6049, 1438, 57, 0);

						ent->client->pers.light_quest_timer = level.time + Q_irand(12000, 14000);
					}
				}
				else if (ent->client->pers.guardian_mode == 20)
				{ // zyk: Guardian of Time
					if (ent->client->pers.guardian_timer < level.time)
					{
						int random_magic = Q_irand(0, 4);

						if (random_magic == 0)
						{
							dome_of_damage(ent, 20000, 25);
						}
						else if (random_magic == 1)
						{
							shifting_sand(ent, 20000);
						}
						else if (random_magic == 2)
						{
							time_power(ent, 20000, 4000);
						}
						else if (random_magic == 3)
						{
							fast_and_slow(ent, 20000, 6000);
						}
						else if (random_magic == 4)
						{
							magic_disable(ent, 20000);
						}

						ent->client->pers.guardian_timer = level.time + Q_irand(8000, 11000);
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{ // zyk: using Crystal of Magic
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;

						// zyk: Universe Power
						ent->client->pers.quest_power_status |= (1 << 13);

						ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;

						ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
						ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
						ent->client->ps.forceHandExtendTime = level.time + 2000;

						zyk_super_beam(ent, ent->client->ps.viewangles[1]);

						ent->client->pers.light_quest_timer = level.time + ((ent->health + ent->client->ps.stats[STAT_ARMOR]) / 2) + 7000;
					}
				}
				else if (ent->client->pers.guardian_mode == 21)
				{ // zyk: Soul of Sorrow
					if ((int)ent->client->ps.origin[2] < -10200)
					{ // zyk: bring him back if he falls
						vec3_t origin;
						vec3_t yaw;

						origin[0] = 2336.0f;
						origin[1] = 3425.0f;
						origin[2] = -9800.0f;
						yaw[0] = 0.0f;
						yaw[1] = 179.0f;
						yaw[2] = 0.0f;
						zyk_TeleportPlayer(ent, origin, yaw);
					}

					if (ent->client->pers.guardian_timer < level.time)
					{
						int random_magic = Q_irand(0, 25);
						gentity_t *player_ent = &g_entities[ent->client->pers.guardian_invoked_by_id];
						int distance = (int)Distance(ent->client->ps.origin, player_ent->client->ps.origin);

						// zyk: using powers in a smarter way
						if (distance < 300)
						{ // zyk: close range makes boss use close range powers more frequently
							random_magic = Q_irand(0, 12);
						}
						else
						{ // zyk: long range makes boss use long range powers more frequently
							random_magic = Q_irand(13, 25);
						}

						if (random_magic == 0)
						{
							immunity_power(ent, 20000);
						}
						else if (random_magic == 1)
						{
							lightning_dome(ent, 70);
						}
						else if (random_magic == 2)
						{
							ultra_drain(ent, 450, 30, 8000);
						}
						else if (random_magic == 3)
						{
							magic_explosion(ent, 320, 130, 900);
						}
						else if (random_magic == 4)
						{
							flaming_area(ent, 20);
						}
						else if (random_magic == 5)
						{
							slow_motion(ent, 5000, 15000);
						}
						else if (random_magic == 6)
						{
							sleeping_flowers(ent, 2500, 5000);
						}
						else if (random_magic == 7)
						{
							blowing_wind(ent, 5000, 1800);
						}
						else if (random_magic == 8)
						{
							flame_burst(ent, 5000);
						}
						else if (random_magic == 9)
						{
							magic_shield(ent, 6000);
						}
						else if (random_magic == 10)
						{
							healing_water(ent, 120);
						}
						else if (random_magic == 11)
						{
							healing_area(ent, 2, 5000);
						}
						else if (random_magic == 12)
						{
							enemy_nerf(ent, 5000);
						}
						else if (random_magic == 13)
						{
							water_attack(ent, 5000, 40);
						}
						else if (random_magic == 14)
						{
							shifting_sand(ent, 5000);
						}
						else if (random_magic == 15)
						{
							magic_disable(ent, 5000);
						}
						else if (random_magic == 16)
						{
							chaos_power(ent, 5000, 4600);
						}
						else if (random_magic == 17)
						{
							dome_of_damage(ent, 5000, 25);
						}
						else if (random_magic == 18)
						{
							reverse_wind(ent, 5000, 1800);
						}
						else if (random_magic == 19)
						{
							ice_boulder(ent, 5000, 40);
						}
						else if (random_magic == 20)
						{
							time_power(ent, 5000, 4000);
						}
						else if (random_magic == 21)
						{
							ice_stalagmite(ent, 5000, 130);
						}
						else if (random_magic == 22)
						{
							rock_fall(ent, 5000, 40);
						}
						else if (random_magic == 23)
						{
							poison_mushrooms(ent, 100, 5000);
						}
						else if (random_magic == 24)
						{
							ultra_flame(ent, 5000, 35);
						}
						else if (random_magic == 25)
						{
							ice_block(ent, 3500);
						}

						ent->client->pers.guardian_timer = level.time + Q_irand(8000, 10000);
					}

					if (ent->client->pers.light_quest_timer < level.time)
					{
						int random_unique = Q_irand(0, 2);

						ent->client->pers.quest_power_status |= (1 << 13);

						if (random_unique == 0)
						{
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;

							ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
							ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
							ent->client->ps.forceHandExtendTime = level.time + 2000;

							zyk_super_beam(ent, ent->client->ps.viewangles[1]);
						}
						else if (random_unique == 1)
						{
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

							zyk_no_attack(ent);
						}
						else if (random_unique == 2)
						{
							ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

							force_scream(ent);
						}

						ent->client->pers.light_quest_timer = level.time + ((ent->health + ent->client->ps.stats[STAT_ARMOR]) / 2) + 3000;
					}

					if (ent->client->pers.universe_quest_timer < level.time)
					{ // zyk: destroys tiles from the background
						int k = 0;
						int tile_count = 0;
						int zyk_chosen_tile = Q_irand(0, (47 - ent->client->pers.hunter_quest_messages));
						vec3_t origin;
						vec3_t yaw;

						for (k = (MAX_CLIENTS + BODY_QUEUE_SIZE); k < level.num_entities; k++)
						{
							gentity_t *zyk_tile_ent = &g_entities[k];

							if (zyk_tile_ent && Q_stricmp(zyk_tile_ent->targetname, "zyk_quest_models") == 0 && Q_stricmp(zyk_tile_ent->classname, "misc_model_breakable") == 0 && 
								((int)zyk_tile_ent->s.origin[0] != 2336 || (int)zyk_tile_ent->s.origin[1] != 3425))
							{ // zyk: do not drop the central tile
								if (tile_count == zyk_chosen_tile)
								{
									gentity_t *effect_ent = zyk_quest_item("env/lbolt1", zyk_tile_ent->s.origin[0], zyk_tile_ent->s.origin[1], -9960, "", "");

									if (effect_ent)
									{
										level.special_power_effects[effect_ent->s.number] = ent->s.number;
										level.special_power_effects_timer[effect_ent->s.number] = level.time + 1000;

										G_Sound(effect_ent, CHAN_AUTO, G_SoundIndex("sound/effects/tram_boost.mp3"));

										G_FreeEntity(zyk_tile_ent);

										ent->client->pers.hunter_quest_messages++;
									}

									break;
								}

								tile_count++;
							}
						}

						// zyk: send him back to his original catwalk
						origin[0] = 2336.0f;
						origin[1] = 3425.0f;
						origin[2] = -9800.0f;
						yaw[0] = 0.0f;
						yaw[1] = 179.0f;
						yaw[2] = 0.0f;
						zyk_TeleportPlayer(ent, origin, yaw);

						ent->client->pers.universe_quest_timer = level.time + ((ent->health + ent->client->ps.stats[STAT_ARMOR]) / 2) + 1000;
					}
				}
			}
			else if (ent->health > 0 && Q_stricmp(ent->NPC_type, "quest_mage") == 0 && ent->enemy && ent->client->pers.guardian_timer < level.time)
			{ // zyk: powers used by the quest_mage npc
				int random_magic = Q_irand(0, 26);

				if (random_magic == 0)
				{
					ultra_strength(ent, 30000);
				}
				else if (random_magic == 1)
				{
					poison_mushrooms(ent, 100, 600);
				}
				else if (random_magic == 2)
				{
					water_splash(ent, 400, 15);
				}
				else if (random_magic == 3)
				{
					ultra_flame(ent, 500, 35);
				}
				else if (random_magic == 4)
				{
					rock_fall(ent, 500, 40);
				}
				else if (random_magic == 5)
				{
					dome_of_damage(ent, 500, 25);
				}
				else if (random_magic == 6)
				{
					hurricane(ent, 600, 5000);
				}
				else if (random_magic == 7)
				{
					slow_motion(ent, 400, 15000);
				}
				else if (random_magic == 8)
				{
					ultra_resistance(ent, 30000);
				}
				else if (random_magic == 9)
				{
					sleeping_flowers(ent, 2500, 350);
				}
				else if (random_magic == 10)
				{
					healing_water(ent, 120);
				}
				else if (random_magic == 11)
				{
					flame_burst(ent, 5000);
				}
				else if (random_magic == 12)
				{
					earthquake(ent, 2000, 300, 500);
				}
				else if (random_magic == 13)
				{
					magic_shield(ent, 6000);
				}
				else if (random_magic == 14)
				{
					blowing_wind(ent, 700, 5000);
				}
				else if (random_magic == 15)
				{
					ultra_speed(ent, 15000);
				}
				else if (random_magic == 16)
				{
					ice_stalagmite(ent, 500, 130);
				}
				else if (random_magic == 17)
				{
					ice_boulder(ent, 380, 40);
				}
				else if (random_magic == 18)
				{
					water_attack(ent, 500, 40);
				}
				else if (random_magic == 19)
				{
					shifting_sand(ent, 1000);
				}
				else if (random_magic == 20)
				{
					tree_of_life(ent);
				}
				else if (random_magic == 21)
				{
					magic_disable(ent, 450);
				}
				else if (random_magic == 22)
				{
					fast_and_slow(ent, 400, 6000);
				}
				else if (random_magic == 23)
				{
					flaming_area(ent, 20);
				}
				else if (random_magic == 24)
				{
					reverse_wind(ent, 700, 5000);
				}
				else if (random_magic == 25)
				{
					enemy_nerf(ent, 450);
				}
				else if (random_magic == 26)
				{
					ice_block(ent, 3500);
				}

				ent->client->pers.guardian_timer = level.time + Q_irand(3000, 6000);
			}
			else if (ent->client->pers.universe_quest_messages == -10000 && ent->health > 0 && ent->enemy && Q_stricmp(ent->NPC_type, "ymir_boss") == 0)
			{ // zyk: Ymir
				if (ent->client->pers.guardian_timer < level.time)
				{
					int random_magic = Q_irand(0, 26);

					if (random_magic == 0)
					{
						ultra_strength(ent, 30000);
					}
					else if (random_magic == 1)
					{
						poison_mushrooms(ent, 100, 600);
					}
					else if (random_magic == 2)
					{
						water_splash(ent, 400, 15);
					}
					else if (random_magic == 3)
					{
						ultra_flame(ent, 500, 35);
					}
					else if (random_magic == 4)
					{
						rock_fall(ent, 500, 40);
					}
					else if (random_magic == 5)
					{
						dome_of_damage(ent, 500, 25);
					}
					else if (random_magic == 6)
					{
						hurricane(ent, 600, 5000);
					}
					else if (random_magic == 7)
					{
						slow_motion(ent, 400, 15000);
					}
					else if (random_magic == 8)
					{
						ultra_resistance(ent, 30000);
					}
					else if (random_magic == 9)
					{
						sleeping_flowers(ent, 2500, 350);
					}
					else if (random_magic == 10)
					{
						healing_water(ent, 120);
					}
					else if (random_magic == 11)
					{
						flame_burst(ent, 5000);
					}
					else if (random_magic == 12)
					{
						earthquake(ent, 2000, 300, 500);
					}
					else if (random_magic == 13)
					{
						magic_shield(ent, 6000);
					}
					else if (random_magic == 14)
					{
						blowing_wind(ent, 700, 5000);
					}
					else if (random_magic == 15)
					{
						ultra_speed(ent, 15000);
					}
					else if (random_magic == 16)
					{
						ice_stalagmite(ent, 500, 130);
					}
					else if (random_magic == 17)
					{
						ice_boulder(ent, 380, 40);
					}
					else if (random_magic == 18)
					{
						water_attack(ent, 500, 40);
					}
					else if (random_magic == 19)
					{
						shifting_sand(ent, 1000);
					}
					else if (random_magic == 20)
					{
						tree_of_life(ent);
					}
					else if (random_magic == 21)
					{
						magic_disable(ent, 450);
					}
					else if (random_magic == 22)
					{
						fast_and_slow(ent, 400, 6000);
					}
					else if (random_magic == 23)
					{
						flaming_area(ent, 20);
					}
					else if (random_magic == 24)
					{
						reverse_wind(ent, 700, 5000);
					}
					else if (random_magic == 25)
					{
						enemy_nerf(ent, 450);
					}
					else if (random_magic == 26)
					{
						ice_block(ent, 3500);
					}

					ent->client->pers.guardian_timer = level.time + Q_irand(6000, 10000);
				}

				if (ent->client->pers.light_quest_timer < level.time)
				{ // zyk: unique
					ent->client->pers.light_quest_timer = level.time + Q_irand(8000, 10000);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

					zyk_no_attack(ent);
				}
			}
			else if (ent->client->pers.universe_quest_messages == -10000 && ent->health > 0 && ent->enemy && Q_stricmp(ent->NPC_type, "thor_boss") == 0)
			{ // zyk: Thor
				if (ent->client->pers.guardian_timer < level.time)
				{
					int random_magic = Q_irand(0, 6);

					if (random_magic == 0)
					{
						ultra_drain(ent, 450, 30, 8000);
					}
					else if (random_magic == 1)
					{
						immunity_power(ent, 20000);
					}
					else if (random_magic == 2)
					{
						chaos_power(ent, 400, 4600);
					}
					else if (random_magic == 3)
					{
						time_power(ent, 400, 4000);
					}
					else if (random_magic == 4)
					{
						healing_area(ent, 2, 5000);
					}
					else if (random_magic == 5)
					{
						magic_explosion(ent, 320, 130, 900);
					}
					else if (random_magic == 6)
					{
						lightning_dome(ent, 70);
					}

					ent->client->pers.guardian_timer = level.time + Q_irand(7000, 12000);
				}

				if (ent->client->pers.light_quest_timer < level.time)
				{ // zyk: unique
					ent->client->pers.light_quest_timer = level.time + Q_irand(8000, 10000);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
					ent->client->ps.forceHandExtendTime = level.time + 2000;

					zyk_super_beam(ent, ent->client->ps.viewangles[1]);
				}
			}
		}

		// zyk: added check for mind control on npcs here. NPCs being mind controlled cant think
		if (!(ent && ent->client && ent->client->pers.being_mind_controlled != -1))
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
