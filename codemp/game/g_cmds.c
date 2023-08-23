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
#include "bg_saga.h"

#include "ui/menudef.h"			// for the voice chats

//rww - for getting bot commands...
int AcceptBotCommand(char *cmd, gentity_t *pl);
//end rww

void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );

void Cmd_NPC_f( gentity_t *ent );
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);

void password_encrypt(char password[], int key)
{
	int i = 0;

	for (i = 0; i < strlen(password); i++)
	{
		password[i] = password[i] - key;
	}
}

// zyk: returns the max level of a RPG skill
int zyk_max_skill_level(int skill_index)
{
	int max_skill_levels[NUMBER_OF_SKILLS];

	max_skill_levels[SKILL_JUMP] = 4;
	max_skill_levels[SKILL_PUSH] = 3;
	max_skill_levels[SKILL_PULL] = 3;
	max_skill_levels[SKILL_SPEED] = 3;
	max_skill_levels[SKILL_SENSE] = 3;
	max_skill_levels[SKILL_SABER_ATTACK] = 5;
	max_skill_levels[SKILL_SABER_DEFENSE] = 3;
	max_skill_levels[SKILL_SABER_THROW] = 4;
	max_skill_levels[SKILL_ABSORB] = 4;
	max_skill_levels[SKILL_HEAL] = 4;
	max_skill_levels[SKILL_PROTECT] = 4;
	max_skill_levels[SKILL_MIND_TRICK] = 3;
	max_skill_levels[SKILL_TEAM_HEAL] = 3;
	max_skill_levels[SKILL_LIGHTNING] = 4;
	max_skill_levels[SKILL_GRIP] = 3;
	max_skill_levels[SKILL_DRAIN] = 4;
	max_skill_levels[SKILL_RAGE] = 4;
	max_skill_levels[SKILL_TEAM_ENERGIZE] = 3;
	max_skill_levels[SKILL_SENSE_HEALTH] = 3;
	max_skill_levels[SKILL_SHIELD_HEALING] = 3;
	max_skill_levels[SKILL_FASTER_FORCE_REGEN] = 2;
	max_skill_levels[SKILL_FORCE_POWER] = 10;

	max_skill_levels[SKILL_STUN_BATON] = 2;
	max_skill_levels[SKILL_SABER] = 1;
	max_skill_levels[SKILL_BLASTER_PISTOL] = 2;
	max_skill_levels[SKILL_E11_BLASTER_RIFLE] = 2;
	max_skill_levels[SKILL_DISRUPTOR] = 2;
	max_skill_levels[SKILL_BOWCASTER] = 2;
	max_skill_levels[SKILL_REPEATER] = 2;
	max_skill_levels[SKILL_DEMP2] = 2;
	max_skill_levels[SKILL_FLECHETTE] = 2;
	max_skill_levels[SKILL_ROCKET_LAUNCHER] = 2;
	max_skill_levels[SKILL_CONCUSSION_RIFLE] = 2;
	max_skill_levels[SKILL_BRYAR_PISTOL] = 2;
	max_skill_levels[SKILL_MELEE_SPEED] = 3;
	max_skill_levels[SKILL_MELEE] = 3;

	max_skill_levels[SKILL_MAX_HEALTH] = 10;
	max_skill_levels[SKILL_HEALTH_STRENGTH] = 5;
	max_skill_levels[SKILL_MAX_WEIGHT] = 10;
	max_skill_levels[SKILL_MAX_STAMINA] = 5;
	max_skill_levels[SKILL_UNDERWATER] = 2;
	max_skill_levels[SKILL_RUN_SPEED] = 3;

	max_skill_levels[SKILL_MAGIC_FIST] = 4;
	max_skill_levels[SKILL_MAX_MP] = 10;
	max_skill_levels[SKILL_MAGIC_MAGIC_SENSE] = 7;
	max_skill_levels[SKILL_MAGIC_HEALING_AREA] = 7;
	max_skill_levels[SKILL_MAGIC_ENEMY_WEAKENING] = 7;
	max_skill_levels[SKILL_MAGIC_DOME_OF_DAMAGE] = 7;
	max_skill_levels[SKILL_MAGIC_WATER_MAGIC] = 7;
	max_skill_levels[SKILL_MAGIC_EARTH_MAGIC] = 7;
	max_skill_levels[SKILL_MAGIC_FIRE_MAGIC] = 7;
	max_skill_levels[SKILL_MAGIC_AIR_MAGIC] = 7;
	max_skill_levels[SKILL_MAGIC_DARK_MAGIC] = 7;
	max_skill_levels[SKILL_MAGIC_LIGHT_MAGIC] = 7;

	if (skill_index >= 0 && skill_index < NUMBER_OF_SKILLS)
	{
		return max_skill_levels[skill_index];
	}

	return 0;
}

// zyk: returns the name of a RPG skill
char* zyk_skill_name(int skill_index)
{
	char *skill_names[NUMBER_OF_SKILLS];

	skill_names[SKILL_JUMP] = "Jump";
	skill_names[SKILL_PUSH] = "Push";
	skill_names[SKILL_PULL] = "Pull";
	skill_names[SKILL_SPEED] = "Speed";
	skill_names[SKILL_SENSE] = "Sense";
	skill_names[SKILL_SABER_ATTACK] = "Saber Attack";
	skill_names[SKILL_SABER_DEFENSE] = "Saber Defense";
	skill_names[SKILL_SABER_THROW] = "Saber Throw";
	skill_names[SKILL_ABSORB] = "Absorb";
	skill_names[SKILL_HEAL] = "Heal";
	skill_names[SKILL_PROTECT] = "Protect";
	skill_names[SKILL_MIND_TRICK] = "Mind Trick";
	skill_names[SKILL_TEAM_HEAL] = "Team Heal";
	skill_names[SKILL_LIGHTNING] = "Lightning";
	skill_names[SKILL_GRIP] = "Grip";
	skill_names[SKILL_DRAIN] = "Drain";
	skill_names[SKILL_RAGE] = "Rage";
	skill_names[SKILL_TEAM_ENERGIZE] = "Team Energize";
	skill_names[SKILL_SENSE_HEALTH] = "Sense Health";
	skill_names[SKILL_SHIELD_HEALING] = "Shield Healing";
	skill_names[SKILL_FASTER_FORCE_REGEN] = "Faster Force Regen";
	skill_names[SKILL_FORCE_POWER] = "Force Power";

	skill_names[SKILL_STUN_BATON] = "Stun Baton";
	skill_names[SKILL_SABER] = "Saber Damage";
	skill_names[SKILL_BLASTER_PISTOL] = "Blaster Pistol";
	skill_names[SKILL_E11_BLASTER_RIFLE] = "E11 Blaster Rifle";
	skill_names[SKILL_DISRUPTOR] = "Disruptor";
	skill_names[SKILL_BOWCASTER] = "Bowcaster";
	skill_names[SKILL_REPEATER] = "Repeater";
	skill_names[SKILL_DEMP2] = "DEMP2";
	skill_names[SKILL_FLECHETTE] = "Flechette";
	skill_names[SKILL_ROCKET_LAUNCHER] = "Rocket Launcher";
	skill_names[SKILL_CONCUSSION_RIFLE] = "Concussion Rifle";
	skill_names[SKILL_BRYAR_PISTOL] = "Bryar Pistol";
	skill_names[SKILL_MELEE_SPEED] = "Melee Punch Speed";
	skill_names[SKILL_MELEE] = "Melee";

	skill_names[SKILL_MAX_HEALTH] = "Max Health";
	skill_names[SKILL_HEALTH_STRENGTH] = "Health Strength";
	skill_names[SKILL_MAX_WEIGHT] = "Max Weight";
	skill_names[SKILL_MAX_STAMINA] = "Max Stamina";
	skill_names[SKILL_UNDERWATER] = "Underwater";
	skill_names[SKILL_RUN_SPEED] = "Run Speed";

	skill_names[SKILL_MAGIC_FIST] = "Magic Fist";
	skill_names[SKILL_MAX_MP] = "Max Magic Points";
	skill_names[SKILL_MAGIC_MAGIC_SENSE] = "Magic Sense";
	skill_names[SKILL_MAGIC_HEALING_AREA] = "Healing Area";
	skill_names[SKILL_MAGIC_ENEMY_WEAKENING] = "Enemy Weakening";
	skill_names[SKILL_MAGIC_DOME_OF_DAMAGE] = "Dome of Damage";
	skill_names[SKILL_MAGIC_WATER_MAGIC] = "Water Magic";
	skill_names[SKILL_MAGIC_EARTH_MAGIC] = "Earth Magic";
	skill_names[SKILL_MAGIC_FIRE_MAGIC] = "Fire Magic";
	skill_names[SKILL_MAGIC_AIR_MAGIC] = "Air Magic";
	skill_names[SKILL_MAGIC_DARK_MAGIC] = "Dark Magic";
	skill_names[SKILL_MAGIC_LIGHT_MAGIC] = "Light Magic";

	if (skill_index >= 0 && skill_index < NUMBER_OF_SKILLS)
	{
		return G_NewString(skill_names[skill_index]);
	}

	return "";
}

// zyk: return the Elemental Spirit name
char* zyk_get_spirit_name(zyk_main_quest_t spirit_value)
{
	char spirit_names[NUMBER_OF_MAGIC_SPIRITS][32] = {
		"^4Water Spirit",
		"^3Earth Spirit",
		"^1Fire Spirit",
		"^2Air Spirit",
		"^6Dark Spirit",
		"^5Light Spirit"
	};

	return G_NewString(spirit_names[spirit_value - (MAX_QUEST_MISSIONS - 7)]);
}

// zyk: returns the description of a RPG skill
char* zyk_skill_description(int skill_index)
{
	if (skill_index == SKILL_JUMP)
		return "increases force Jump. Max Level has no height limit, and it lets you jump out of water";
	if (skill_index == SKILL_PUSH)
		return "pushes the opponent forward";
	if (skill_index == SKILL_PULL)
		return "pulls the opponent towards you";
	if (skill_index == SKILL_SPEED)
		return "increases your speed by 1.7 times at level 1 and +0.4 at other levels";
	if (skill_index == SKILL_SENSE)
		return "See people through walls, invisible people or cloaked people. Dodge disruptor shots";
	if (skill_index == SKILL_SABER_ATTACK)
		return va("gives you the saber. If you are using Single Saber, gives you the saber styles. If using duals or staff, increases saber damage, which is increased by 20 per cent for each level. Saber damage scale is %.2f", g_saberDamageScale.value);
	if (skill_index == SKILL_SABER_DEFENSE)
		return "increases your ability to block, parry enemy saber attacks or enemy shots";
	if (skill_index == SKILL_SABER_THROW)
		return va("throws your saber at enemy and gets it back. Each level increases max distance and saber throw speed. Has %d damage", zyk_saber_throw_damage.integer);
	if (skill_index == SKILL_ABSORB)
		return "allows you to absorb force power attacks done to you";
	if (skill_index == SKILL_HEAL)
		return "recover some Health. Each level increases hp restored";
	if (skill_index == SKILL_PROTECT)
		return "decreases damage done to you by non-force power attacks. At level 4 decreases force consumption when receiving damage";
	if (skill_index == SKILL_MIND_TRICK)
		return "makes yourself invisible to the players affected by this force power. Level 1 has a duration of 20 seconds, level 2 is 25 seconds and level 3 is 30 seconds";
	if (skill_index == SKILL_TEAM_HEAL)
		return "restores some health to players near you";
	if (skill_index == SKILL_LIGHTNING)
		return "attacks with a powerful electric attack at players near you. At level 4, does more damage and pushes the enemy back";
	if (skill_index == SKILL_GRIP)
		return "attacks a player by holding and damaging him";
	if (skill_index == SKILL_DRAIN)
		return "drains force power from a player to restore your health. At level 4, with full health, restores some shield and also suck hp/shield from the enemy to restore your hp/shield";
	if (skill_index == SKILL_RAGE)
		return "makes you 1.3 times faster, increases your saber attack speed and damage and makes you get less damage";
	if (skill_index == SKILL_TEAM_ENERGIZE)
		return "restores some force power to players near you. Regens some blaster pack and power cell ammo of the target players";
	if (skill_index == SKILL_SENSE_HEALTH)
		return "allows you to see info about someone, including npcs. Level 1 shows current health. Level 2 shows name, health and shield. Level 3 shows name, health and max health, shield and max shield, force and max force, mp and max mp. To use it, when you are near a player or npc, use ^3Sense ^7force power";
	if (skill_index == SKILL_SHIELD_HEALING)
		return va("Makes ^3Heal ^7restore some shield if your health is full by spending %d force. Makes ^3Team Heal ^7restore some shield to other players if their health is full", zyk_max_force_power.integer / 2);
	if (skill_index == SKILL_FASTER_FORCE_REGEN)
		return "increases how fast your force is recovered";
	if (skill_index == SKILL_FORCE_POWER)
		return "increases the max force power you have. Necessary to allow you to use force powers and force-based skills";

	if (skill_index == SKILL_STUN_BATON)
		return va("attacks someone with a small electric charge. Has %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level. Can fire the flame thrower when using alternate fire. With Stun Baton Upgrade, it opens any door, even locked ones, and also decloaks enemies and decrease their moving speed for some seconds", zyk_stun_baton_damage.integer);
	if (skill_index == SKILL_SABER)
		return "increases Saber damage based on your current level";
	if (skill_index == SKILL_BLASTER_PISTOL)
		return va("the popular Star Wars pistol used by Han Solo in the movies. Normal fire is a single blaster shot, alternate fire allows you to fire a powerful charged shot. The pistol shot does %d damage. The charged shot causes a lot more damage depending on how much it was charged. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_blaster_pistol_damage.integer);
	if (skill_index == SKILL_E11_BLASTER_RIFLE)
		return va("the rifle used by the Storm Troopers. E11 shots do %d damage. Normal fire is a single shot, while the alternate fire is the rapid fire. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_e11_blaster_rifle_damage.integer);
	if (skill_index == SKILL_DISRUPTOR)
		return va("the sniper, used by the rodians ingame. Normal fire is a shot that causes %d damage, alternate fire allows zoom and a charged shot that when fully charged, causes %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_disruptor_damage.integer, zyk_disruptor_alt_damage.integer);
	if (skill_index == SKILL_BOWCASTER)
		return va("the famous weapon used by Chewbacca. Normal fire can be charged to fire up to 5 shots at once. It does %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_bowcaster_damage.integer);
	if (skill_index == SKILL_REPEATER)
		return va("a powerful weapon with a rapid fire and a plasma bomb. Normal fire shoots the rapid fire, and does %d damage. Alt fire fires the plasma bomb and does %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_repeater_damage.integer, zyk_repeater_alt_damage.integer);
	if (skill_index == SKILL_DEMP2)
		return va("a very powerful weapon against machine npc and some vehicles, causing more damage to them and stunning them. Normal fire does %d damage and alt fire can be charged and does %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_demp2_damage.integer, zyk_demp2_alt_damage.integer);
	if (skill_index == SKILL_FLECHETTE)
		return va("this weapon is similar to a shotgun. Normal fire causes %d damage. Alt fire shoots 2 bombs and causes %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_flechette_damage.integer, zyk_flechette_alt_damage.integer);
	if (skill_index == SKILL_ROCKET_LAUNCHER)
		return va("a powerful explosive weapon. Normal fire shoots a rocket causing %d damage. Alt fire shoots a homing missile. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_rocket_damage.integer);
	if (skill_index == SKILL_CONCUSSION_RIFLE)
		return va("it shoots a powerful shot that has a big damage area. Alt fire shoots a ray similar to disruptor shots, but it can go through force fields and can throw the enemy on the ground. Normal fire does %d damage and alt fire does %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_concussion_damage.integer, zyk_concussion_alt_damage.integer);
	if (skill_index == SKILL_BRYAR_PISTOL)
		return va("very similar to the blaster pistol, but this one has a better fire rate with normal shot. Does %d damage. Each level multiplies damage per amount of this weapon in inventory and per this skill level", zyk_blaster_pistol_damage.integer);
	if (skill_index == SKILL_MELEE_SPEED)
		return "Each level increases how fast you can punch with Melee";
	if (skill_index == SKILL_MELEE)
		return va("allows you to punch, kick or do a special melee attack by holding both Attack and Alt Attack buttons (usually the mouse buttons). At level 0, melee attacks cause only half normal damage. Right hand punch causes %d normal damage, left hand punch causes %d normal damage and kick causes %d damage at level 1", zyk_melee_right_hand_damage.integer, zyk_melee_left_hand_damage.integer, zyk_melee_kick_damage.integer);
	
	if (skill_index == SKILL_MAX_HEALTH)
		return "Each level increases your max health";
	if (skill_index == SKILL_HEALTH_STRENGTH)
		return "Each level increases your health resistance to damage";
	if (skill_index == SKILL_MAX_WEIGHT)
		return "Everything you carry has a weight. This skill increases the max weight you can carry. Use /list to see the currentweight/maxweight ratio";
	if (skill_index == SKILL_MAX_STAMINA)
		return "Each level increases your max stamina. Stamina is used by any action the player does. Low stamina makes run speed slower. If Stamina runs out you will pass out for some seconds. Each skill level decreases time you need to rest. Meditating recovers stamina faster, and the amount recovered increases with each skill level. Use bacta canister or big bacta items to regen stamina";
	if (skill_index == SKILL_UNDERWATER)
		return "Each level increases your air underwater";
	if (skill_index == SKILL_RUN_SPEED)
		return va("At level 0 your run speed is %f. Each level increases it by 50", g_speed.value);
	
	if (skill_index == SKILL_MAGIC_FIST)
		return "allows you to attack with magic bolts when using melee punches. Each level gives a new bolt type. To select a bolt type, get melee and press Saber Style Key";
	if (skill_index == SKILL_MAX_MP)
		return "increases the max amount of Magic Points the player can have";
	if (skill_index == SKILL_MAGIC_MAGIC_SENSE)
		return "similar to Sense at max level. Sense Health works with it";
	if (skill_index == SKILL_MAGIC_HEALING_AREA)
		return "creates an energy area that recovers health and shield to you and your allies. It also does a little damage to enemies";
	if (skill_index == SKILL_MAGIC_ENEMY_WEAKENING)
		return "decreases damage and resistance of enemies nearby. Also makes enemy Stamina decrease faster";
	if (skill_index == SKILL_MAGIC_DOME_OF_DAMAGE)
		return "an energy dome appears at your position each half second, damaging enemies inside it";
	if (skill_index == SKILL_MAGIC_WATER_MAGIC)
		return "creates an ice block that protects you and restores some health. Hits enemies around you with Water damage. Enemies hit by Water get their hp drained to restore your health";
	if (skill_index == SKILL_MAGIC_EARTH_MAGIC)
		return "creates a tree that protects you from damage. Creates earthquakes and hits enemies with Earth damage";
	if (skill_index == SKILL_MAGIC_FIRE_MAGIC)
		return "fires a flame burst and hits enemies around you with Fire damage. Enemies hit by it will catch fire for some seconds";
	if (skill_index == SKILL_MAGIC_AIR_MAGIC)
		return "blows people away with a strong wind, and does a little Air damage to them. Increases your run speed. Enemies hit by Air will have their run speed decreased";
	if (skill_index == SKILL_MAGIC_DARK_MAGIC)
		return "creates a black hole, sucking everyone nearby. The closer the enemies are, the more damage they receive. Enemies hit with Dark Magic will get their health drained";
	if (skill_index == SKILL_MAGIC_LIGHT_MAGIC)
		return "creates a lightning dome that damages enemies nearby. Protects you with a shield that reduces damage and protects from some force powers. Creates a big shining light around you. While inside the light, enemies will get confused/stunned and will have their MP drained to restore your MP. While inside the light, you slowly get health, take less damage and any attacker who hits you gets knocked down";

	return "";
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[256];
	char		string[MAX_STRING_CHARS-1];
	int			stringlength, prefix;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, scoreFlags, accuracy, perfect;

	// send the latest information on all clients
	string[0] = '\0';
	stringlength = 0;
	scoreFlags = 0;

	numSorted = level.numConnectedClients;

	// This is dumb that we are capping to 20 clients but support 32 in server
	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

	// estimate prefix length to avoid oversize of final string
	prefix = Com_sprintf( entry, sizeof(entry), "scores %i %i %i", level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE], level.numConnectedClients );

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}

		if( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		}
		else {
			accuracy = 0;
		}
		perfect = ( cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0 ) ? 1 : 0;

		j = Com_sprintf (entry, sizeof(entry),
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i", level.sortedClients[i],
			cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime)/60000,
			scoreFlags, g_entities[level.sortedClients[i]].s.powerups, accuracy,
			cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
			cl->ps.persistant[PERS_EXCELLENT_COUNT],
			cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT],
			cl->ps.persistant[PERS_DEFEND_COUNT],
			cl->ps.persistant[PERS_ASSIST_COUNT],
			perfect,
			cl->ps.persistant[PERS_CAPTURES]);

		if (stringlength + j + prefix >= sizeof(string))
			break;

		strcpy( string + stringlength, entry );
		stringlength += j;
	}

	trap->SendServerCommand( ent-g_entities, va("scores %i %i %i%s", level.numConnectedClients,
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
		string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}

/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap->Argc();
	for ( i = start ; i < c ; i++ ) {
		trap->Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
StringIsInteger
==================
*/
qboolean StringIsInteger( const char *s ) {
	int			i=0, len=0;
	qboolean	foundDigit=qfalse;

	for ( i=0, len=strlen( s ); i<len; i++ )
	{
		if ( !isdigit( s[i] ) )
			return qfalse;

		foundDigit = qtrue;
	}

	return foundDigit;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, const char *s, qboolean allowconnecting ) {
	gclient_t	*cl;
	int			idnum;
	char		cleanInput[MAX_NETNAME];

	if ( StringIsInteger( s ) )
	{// numeric values could be slot numbers
		idnum = atoi( s );
		if ( idnum >= 0 && idnum < level.maxclients )
		{
			cl = &level.clients[idnum];
			if ( cl->pers.connected == CON_CONNECTED )
				return idnum;
			else if ( allowconnecting && cl->pers.connected == CON_CONNECTING )
				return idnum;
		}
	}

	Q_strncpyz( cleanInput, s, sizeof(cleanInput) );
	Q_StripColor( cleanInput );

	for ( idnum=0,cl=level.clients; idnum < level.maxclients; idnum++,cl++ )
	{// check for a name match
		if ( cl->pers.connected != CON_CONNECTED )
			if ( !allowconnecting || cl->pers.connected < CON_CONNECTING )
				continue;

		// zyk: changed this so the player name must contain the search string
		if ( Q_stristr( cl->pers.netname_nocolor, cleanInput ) ) // if ( !Q_stricmp( cl->pers.netname_nocolor, cleanInput ) )
			return idnum;
	}

	if (to)
		trap->SendServerCommand( to-g_entities, va( "print \"User %s is not on the server\n\"", s ) );
	else
		trap->Print(va( "User %s is not on the server\n", s ));

	return -1;
}

// zyk: plays an animation from anims.h
void Cmd_Emote_f( gentity_t *ent )
{
	char arg[MAX_TOKEN_CHARS] = {0};
	int anim_id = -1;

	if (zyk_allow_emotes.integer < 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in this server\n\"" );
		return;
	}

	if (level.gametype == GT_SIEGE)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in Siege gametype\n\"" );
		return;
	}

	if (zyk_allow_emotes.integer != 1 && ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use emotes in private duel\n\"");
		return;
	}

	if ( trap->Argc () < 2 ) {
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: emote <anim id between 0 and %d>\n\"",MAX_ANIMATIONS-1) );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	anim_id = atoi(arg);

	if (anim_id < 0 || anim_id >= MAX_ANIMATIONS)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: anim ID must be between 0 and %d>\n\"",MAX_ANIMATIONS-1) );
		return;
	}

	if (ent->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot use emotes while knocked down\n\"");
		return;
	}

	ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
	ent->client->ps.forceDodgeAnim = anim_id;
	ent->client->ps.forceHandExtendTime = level.time + 1000;

	ent->client->pers.player_statuses |= (1 << 1);
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void zyk_remove_force_powers( gentity_t *ent )
{
	int i = 0;

	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
	}

	ent->client->ps.weapon = WP_MELEE;

	// zyk: reset the force powers of this player
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
}

void zyk_remove_guns( gentity_t *ent )
{
	int i = 0;

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
	}

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
	ent->client->ps.weapon = WP_MELEE;

	ent->client->ps.ammo[AMMO_BLASTER] = 0;
	ent->client->ps.ammo[AMMO_POWERCELL] = 0;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = 0;
	ent->client->ps.ammo[AMMO_ROCKETS] = 0;
	ent->client->ps.ammo[AMMO_THERMAL] = 0;
	ent->client->ps.ammo[AMMO_TRIPMINE] = 0;
	ent->client->ps.ammo[AMMO_DETPACK] = 0;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_NONE);

	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}

	// zyk: reset the force powers of this player
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
}

void zyk_add_force_powers( gentity_t *ent )
{
	int i = 0;

	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown |= (1 << i);
		if (i == FP_SABER_OFFENSE) // zyk: gives Desann and Tavion styles
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_5;
		else
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
	}
}

void zyk_add_guns( gentity_t *ent )
{
	int i = 0;

	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
	}

	ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		if (i != WP_SABER && i != WP_EMPLACED_GUN && i != WP_TURRET)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << i);
	}

	ent->client->ps.ammo[AMMO_BLASTER] = zyk_max_blaster_pack_ammo.integer;
	ent->client->ps.ammo[AMMO_POWERCELL] = zyk_max_power_cell_ammo.integer;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = zyk_max_metal_bolt_ammo.integer;
	ent->client->ps.ammo[AMMO_ROCKETS] = zyk_max_rocket_ammo.integer;
	ent->client->ps.ammo[AMMO_THERMAL] = zyk_max_thermal_ammo.integer;
	ent->client->ps.ammo[AMMO_TRIPMINE] = zyk_max_tripmine_ammo.integer;
	ent->client->ps.ammo[AMMO_DETPACK] = zyk_max_detpack_ammo.integer;

	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
}

void Cmd_Give_f( gentity_t *ent )
{
	char arg1[MAX_TOKEN_CHARS] = {0};
	char arg2[MAX_TOKEN_CHARS] = {0};
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_GIVE)))
	{ // zyk: give admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if (level.gametype != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Give command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: /give <player name or ID> <option>.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot give stuff to RPG players.\n\"" );
		return;
	}

	if (g_entities[client_id].client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot give stuff to players in private duels\n\"");
		return;
	}

	if (client_id < MAX_CLIENTS && level.sniper_players[client_id] != -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot give stuff to players in Sniper Battle\n\"");
		return;
	}

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT)))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
		return;
	}

	if (Q_stricmp(arg2, "force") == 0)
	{
		if (g_entities[client_id].client->pers.player_statuses & (1 << 12))
		{ // zyk: remove force powers
			zyk_remove_force_powers(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 12);
			trap->SendServerCommand( -1, va("print \"Removed force powers from %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
		else
		{ // zyk: add force powers
			zyk_remove_guns(&g_entities[client_id]);
			zyk_add_force_powers(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 13);
			g_entities[client_id].client->pers.player_statuses |= (1 << 12);
			trap->SendServerCommand( -1, va("print \"Added force powers to %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
	}
	else if (Q_stricmp(arg2, "guns") == 0)
	{
		if (g_entities[client_id].client->pers.player_statuses & (1 << 13))
		{ // zyk: remove guns
			zyk_remove_guns(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 13);
			trap->SendServerCommand( -1, va("print \"Removed guns from %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
		else
		{ // zyk: add guns
			zyk_remove_force_powers(&g_entities[client_id]);
			zyk_add_guns(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << 12);
			g_entities[client_id].client->pers.player_statuses |= (1 << 13);
			trap->SendServerCommand( -1, va("print \"Added guns to %s^7\n\"", g_entities[client_id].client->pers.netname) );
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid option. Must be ^3force ^7or ^3guns^7.\n\"" );
		return;
	}
}

/*
==================
Cmd_Scale_f

Scales player size
==================
*/
void do_scale(gentity_t *ent, int new_size)
{
	ent->client->ps.iModelScale = new_size;
	ent->client->pers.player_scale = new_size;

	if (new_size == 100) // zyk: default size
		ent->client->pers.player_statuses &= ~(1 << 4);
	else
		ent->client->pers.player_statuses |= (1 << 4);
}

void Cmd_Scale_f( gentity_t *ent ) {
	char arg1[MAX_TOKEN_CHARS] = {0};
	char arg2[MAX_TOKEN_CHARS] = {0};
	int client_id = -1;
	int new_size = 0;

	if (!(ent->client->pers.bitvalue & (1 << ADM_SCALE)))
	{ // zyk: scale admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if (level.gametype != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Scale command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: /scale <player name or ID> <size between 20 and 500>.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	new_size = atoi(arg2);

	if (new_size < 20 || new_size > 500)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Size must be between 20 and 500.\n\"" );
		return;
	}

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && 
		g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT)))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
		return;
	}

	do_scale(&g_entities[client_id], new_size);
	
	trap->SendServerCommand( -1, va("print \"Scaled player %s ^7to ^3%d^7\n\"", g_entities[client_id].client->pers.netname, new_size) );
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent ) {
	char *msg = NULL;

	ent->flags ^= FL_GODMODE;
	if ( !(ent->flags & FL_GODMODE) )
		msg = "godmode OFF";
	else
		msg = "godmode ON";

	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char *msg = NULL;

	ent->flags ^= FL_NOTARGET;
	if ( !(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF";
	else
		msg = "notarget ON";

	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char *msg = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_NOCLIP)))
	{ // zyk: noclip admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	// zyk: this command can only be used in Admin-Only Mode
	if (ent->client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot noclip in RPG Mode.\n\"" );
		return;
	}

	if (g_gametype.integer != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Noclip command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot noclip while being eaten by a rancor.\n\"" );
		return;
	}

	// zyk: deactivating saber
	if ( ent->client->ps.saberHolstered < 2 )
	{
		Cmd_ToggleSaber_f(ent);
	}

	ent->client->noclip = !ent->client->noclip;
	if ( !ent->client->noclip )
		msg = "noclip OFF";
	else
		msg = "noclip ON";

	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent )
{
	if ( !ent->client->pers.localClient )
	{
		trap->SendServerCommand(ent-g_entities, "print \"The levelshot command must be executed by a local client\n\"");
		return;
	}

	// doesn't work in single player
	if ( level.gametype == GT_SINGLE_PLAYER )
	{
		trap->SendServerCommand(ent-g_entities, "print \"Must not be in singleplayer mode for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap->SendServerCommand( ent-g_entities, "clientLevelShot" );
}

#if 0
/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap->Argc() != 2 ) {
		return;
	}
	trap->Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap->GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap->SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}
#endif

void G_Kill( gentity_t *ent ) {
	if ((level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) &&
		level.numPlayingClients > 1 && !level.warmupTime)
	{
		if (!g_allowDuelSuicide.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")) );
			return;
		}
	}

	// zyk: target has been paralyzed by an admin
	if (ent && ent->client && !ent->NPC && ent->client->pers.player_statuses & (1 << 6))
		return;

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

	if (ent->client->ps.duelInProgress == qtrue)
	{ // zyk: if player is in a private duel, gives kill to the other duelist
		gentity_t *other = &g_entities[ent->client->ps.duelIndex];

		player_die(ent, other, other, 100000, MOD_SUICIDE);
	}
	else if (ent->client->ps.otherKillerTime > level.time && ent->client->ps.otherKiller != ENTITYNUM_NONE)
	{ // zyk: self killing while otherKiller is set gives kill to the otherKiller
		gentity_t *other = &g_entities[ent->client->ps.otherKiller];

		player_die(ent, other, other, 100000, MOD_SUICIDE);
	}
	else
	{
		player_die(ent, ent, ent, 100000, MOD_SUICIDE);
	}
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent ) {
	if (ent && ent->client)
	{
		ent->client->pers.player_statuses |= (1 << 24);
	}

	G_Kill( ent );
}

void Cmd_KillOther_f( gentity_t *ent )
{
	int			i;
	char		otherindex[MAX_TOKEN_CHARS];
	gentity_t	*otherEnt = NULL;

	if ( trap->Argc () < 2 ) {
		trap->SendServerCommand( ent-g_entities, "print \"Usage: killother <player id>\n\"" );
		return;
	}

	trap->Argv( 1, otherindex, sizeof( otherindex ) );
	i = ClientNumberFromString( ent, otherindex, qfalse );
	if ( i == -1 ) {
		return;
	}

	otherEnt = &g_entities[i];
	if ( !otherEnt->inuse || !otherEnt->client ) {
		return;
	}

	if ( (otherEnt->health <= 0 || otherEnt->client->tempSpectate >= level.time || otherEnt->client->sess.sessionTeam == TEAM_SPECTATOR) )
	{
		// Intentionally displaying for the command user
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}

	G_Kill( otherEnt );
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	client->ps.fd.forceDoInit = 1; //every time we change teams make sure our force powers are set right

	if (level.gametype == GT_SIEGE)
	{ //don't announce these things in siege
		return;
	}

	if ( client->sess.sessionTeam == TEAM_RED ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")) );
	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")));
	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")));
	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")));
	}

	G_LogPrintf( "ChangeTeam: %i [%s] (%s) \"%s^7\" %s -> %s\n", (int)(client - level.clients), client->sess.IP, client->pers.guid, client->pers.netname, TeamName( oldTeam ), TeamName( client->sess.sessionTeam ) );
}

qboolean G_PowerDuelCheckFail(gentity_t *ent)
{
	int			loners = 0;
	int			doubles = 0;

	if (!ent->client || ent->client->sess.duelTeam == DUELTEAM_FREE)
	{
		return qtrue;
	}

	G_PowerDuelCount(&loners, &doubles, qfalse);

	if (ent->client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
	{
		return qtrue;
	}

	if (ent->client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
SetTeam
=================
*/
qboolean g_dontPenalizeTeam = qfalse;
qboolean g_preventTeamBegin = qfalse;
void SetTeam( gentity_t *ent, char *s ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	// fix: this prevents rare creation of invalid players
	if (!ent->inuse)
	{
		return;
	}

	//
	// see what change is requested
	//
	client = ent->client;

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE; // SPECTATOR_SCOREBOARD disabling this for now since it is totally broken on client side
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( level.gametype >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			//For now, don't do this. The legalize function will set powers properly now.
			/*
			if (g_forceBasedTeams.integer)
			{
				if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					team = TEAM_BLUE;
				}
				else
				{
					team = TEAM_RED;
				}
			}
			else
			{
			*/
				team = PickTeam( clientNum );
			//}
		}

		if ( g_teamForceBalance.integer && !g_jediVmerc.integer ) {
			int		counts[TEAM_NUM_TEAMS];

			//JAC: Invalid clientNum was being used
			counts[TEAM_BLUE] = TeamCount( ent-g_entities, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( ent-g_entities, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
				{
					trap->SendServerCommand( ent->client->ps.clientNum,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED_SWITCH")) );
				}
				else
				*/
				{
					//JAC: Invalid clientNum was being used
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
				}
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					trap->SendServerCommand( ent->client->ps.clientNum,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE_SWITCH")) );
				}
				else
				*/
				{
					//JAC: Invalid clientNum was being used
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
				}
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}

		//For now, don't do this. The legalize function will set powers properly now.
		/*
		if (g_forceBasedTeams.integer)
		{
			if (team == TEAM_BLUE && ent->client->ps.fd.forceSide != FORCE_LIGHTSIDE)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBELIGHT")) );
				return;
			}
			if (team == TEAM_RED && ent->client->ps.fd.forceSide != FORCE_DARKSIDE)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEDARK")) );
				return;
			}
		}
		*/

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	oldTeam = client->sess.sessionTeam;

	if (level.gametype == GT_SIEGE)
	{
		if (client->tempSpectate >= level.time &&
			team == TEAM_SPECTATOR)
		{ //sorry, can't do that.
			return;
		}

		if ( team == oldTeam && team != TEAM_SPECTATOR )
			return;

		client->sess.siegeDesiredTeam = team;
		//oh well, just let them go.
		/*
		if (team != TEAM_SPECTATOR)
		{ //can't switch to anything in siege unless you want to switch to being a fulltime spectator
			//fill them in on their objectives for this team now
			trap->SendServerCommand(ent-g_entities, va("sb %i", client->sess.siegeDesiredTeam));

			trap->SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time the round begins.\n\"") );
			return;
		}
		*/
		if (client->sess.sessionTeam != TEAM_SPECTATOR &&
			team != TEAM_SPECTATOR)
		{ //not a spectator now, and not switching to spec, so you have to wait til you die.
			//trap->SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time you respawn.\n\"") );
			qboolean doBegin;
			if (ent->client->tempSpectate >= level.time)
			{
				doBegin = qfalse;
			}
			else
			{
				doBegin = qtrue;
			}

			if (doBegin)
			{
				// Kill them so they automatically respawn in the team they wanted.
				if (ent->health > 0)
				{
					ent->flags &= ~FL_GODMODE;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
					player_die( ent, ent, ent, 100000, MOD_TEAM_CHANGE );
				}
			}

			if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
			{
				SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qfalse);
			}

			return;
		}
	}

	// override decision if limiting the players
	if ( (level.gametype == GT_DUEL)
		&& level.numNonSpectatorClients >= 2 )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( (level.gametype == GT_POWERDUEL)
		&& (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent)) )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( g_maxGameClients.integer > 0 &&
		level.numNonSpectatorClients >= g_maxGameClients.integer )
	{
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	//
	// execute the team change
	//

	//If it's siege then show the mission briefing for the team you just joined.
//	if (level.gametype == GT_SIEGE && team != TEAM_SPECTATOR)
//	{
//		trap->SendServerCommand(clientNum, va("sb %i", team));
//	}

	// if the player was dead leave the body
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		MaintainBodyQueue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		g_dontPenalizeTeam = qtrue;
		player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		g_dontPenalizeTeam = qfalse;

	}
	// they go to the end of the line for tournaments
	if ( team == TEAM_SPECTATOR && oldTeam != team )
		AddTournamentQueue( client );

	// clear votes if going to spectator (specs can't vote)
	if ( team == TEAM_SPECTATOR )
		G_ClearVote( ent );
	// also clear team votes if switching red/blue or going to spec
	G_ClearTeamVote( ent, oldTeam );

	client->sess.sessionTeam = (team_t)team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			//SetLeader( team, clientNum );
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	BroadcastTeamChange( client, oldTeam );

	//make a disappearing effect where they were before teleporting them to the appropriate spawn point,
	//if we were not on the spec team
	if (oldTeam != TEAM_SPECTATOR)
	{
		gentity_t *tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = clientNum;
	}

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( clientNum ) )
		return;

	if (!g_preventTeamBegin && level.load_entities_timer == 0)
	{ // zyk: do not call this while entities are being placed in map
		ClientBegin( clientNum, qfalse );
	}
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
extern void G_LeaveVehicle( gentity_t *ent, qboolean ConCheck );
void StopFollowing( gentity_t *ent ) {
	int i=0;
	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;
	ent->client->ps.weapon = WP_NONE;
	G_LeaveVehicle( ent, qfalse ); // clears m_iVehicleNum as well
	ent->client->ps.emplacedIndex = 0;
	//ent->client->ps.m_iVehicleNum = 0;
	ent->client->ps.viewangles[ROLL] = 0.0f;
	ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
	ent->client->ps.forceHandExtendTime = 0;
	ent->client->ps.zoomMode = 0;
	ent->client->ps.zoomLocked = qfalse;
	ent->client->ps.zoomLockTime = 0;
	ent->client->ps.saberMove = LS_NONE;
	ent->client->ps.legsAnim = 0;
	ent->client->ps.legsTimer = 0;
	ent->client->ps.torsoAnim = 0;
	ent->client->ps.torsoTimer = 0;
	ent->client->ps.isJediMaster = qfalse; // major exploit if you are spectating somebody and they are JM and you reconnect
	ent->client->ps.cloakFuel = 100; // so that fuel goes away after stop following them
	ent->client->ps.jetpackFuel = 100; // so that fuel goes away after stop following them
	ent->health = ent->client->ps.stats[STAT_HEALTH] = 100; // so that you don't keep dead angles if you were spectating a dead person
	ent->client->ps.bobCycle = 0;
	ent->client->ps.pm_type = PM_SPECTATOR;
	ent->client->ps.eFlags &= ~EF_DISINTEGRATION;
	for ( i=0; i<PW_NUM_POWERUPS; i++ )
		ent->client->ps.powerups[i] = 0;
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent ) {
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	oldTeam = ent->client->sess.sessionTeam;

	if ( trap->Argc() != 2 ) {
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")) );
			break;
		case TEAM_RED:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")) );
			break;
		case TEAM_FREE:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
			break;
		case TEAM_SPECTATOR:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
			break;
		}
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if (gEscaping)
	{
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( level.gametype == GT_DUEL
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {//in a tournament game
		//disallow changing teams
		trap->SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Duel\n\"" );
		return;
		//FIXME: why should this be a loss???
		//ent->client->sess.losses++;
	}

	if (level.gametype == GT_POWERDUEL)
	{ //don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
		trap->SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Power Duel\n\"" );
		return;
	}

	trap->Argv( 1, s, sizeof( s ) );

	SetTeam( ent, s );

	// fix: update team switch time only if team change really happend
	if (oldTeam != ent->client->sess.sessionTeam)
		ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t *ent)
{
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if (level.gametype != GT_POWERDUEL)
	{ //don't bother doing anything if this is not power duel
		return;
	}

	/*
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You cannot change your duel team unless you are a spectator.\n\""));
		return;
	}
	*/

	if ( trap->Argc() != 2 )
	{ //No arg so tell what team we're currently on.
		oldTeam = ent->client->sess.duelTeam;
		switch ( oldTeam )
		{
		case DUELTEAM_FREE:
			trap->SendServerCommand( ent-g_entities, va("print \"None\n\"") );
			break;
		case DUELTEAM_LONE:
			trap->SendServerCommand( ent-g_entities, va("print \"Single\n\"") );
			break;
		case DUELTEAM_DOUBLE:
			trap->SendServerCommand( ent-g_entities, va("print \"Double\n\"") );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchDuelTeamTime > level.time )
	{ //debounce for changing
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	trap->Argv( 1, s, sizeof( s ) );

	oldTeam = ent->client->sess.duelTeam;

	if (!Q_stricmp(s, "free"))
	{
		ent->client->sess.duelTeam = DUELTEAM_FREE;
	}
	else if (!Q_stricmp(s, "single"))
	{
		ent->client->sess.duelTeam = DUELTEAM_LONE;
	}
	else if (!Q_stricmp(s, "double"))
	{
		ent->client->sess.duelTeam = DUELTEAM_DOUBLE;
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"'%s' not a valid duel team.\n\"", s) );
	}

	if (oldTeam == ent->client->sess.duelTeam)
	{ //didn't actually change, so don't care.
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //ok..die
		int curTeam = ent->client->sess.duelTeam;
		ent->client->sess.duelTeam = oldTeam;
		G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		ent->client->sess.duelTeam = curTeam;
	}
	//reset wins and losses
	ent->client->sess.wins = 0;
	ent->client->sess.losses = 0;

	//get and distribute relevent paramters
	if ( ClientUserinfoChanged( ent->s.number ) )
		return;

	ent->client->switchDuelTeamTime = level.time + 5000;
}

int G_TeamForSiegeClass(const char *clName)
{
	int i = 0;
	int team = SIEGETEAM_TEAM1;
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);
	siegeClass_t *scl;

	if (!stm)
	{
		return 0;
	}

	while (team <= SIEGETEAM_TEAM2)
	{
		scl = stm->classes[i];

		if (scl && scl->name[0])
		{
			if (!Q_stricmp(clName, scl->name))
			{
				return team;
			}
		}

		i++;
		if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses)
		{
			if (team == SIEGETEAM_TEAM2)
			{
				break;
			}
			team = SIEGETEAM_TEAM2;
			stm = BG_SiegeFindThemeForTeam(team);
			i = 0;
		}
	}

	return 0;
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f( gentity_t *ent )
{
	char className[64];
	int team = 0;
	int preScore;
	qboolean startedAsSpec = qfalse;

	if (level.gametype != GT_SIEGE)
	{ //classes are only valid for this gametype
		return;
	}

	if (!ent->client)
	{
		return;
	}

	if (trap->Argc() < 1)
	{
		return;
	}

	if ( ent->client->switchClassTime > level.time )
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSSWITCH")) );
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		startedAsSpec = qtrue;
	}

	trap->Argv( 1, className, sizeof( className ) );

	team = G_TeamForSiegeClass(className);

	if (!team)
	{ //not a valid class name
		return;
	}

	if (ent->client->sess.sessionTeam != team)
	{ //try changing it then
		g_preventTeamBegin = qtrue;
		if (team == TEAM_RED)
		{
			SetTeam(ent, "red");
		}
		else if (team == TEAM_BLUE)
		{
			SetTeam(ent, "blue");
		}
		g_preventTeamBegin = qfalse;

		if (ent->client->sess.sessionTeam != team)
		{ //failed, oh well
			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR ||
				ent->client->sess.siegeDesiredTeam != team)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSTEAM")) );
				return;
			}
		}
	}

	//preserve 'is score
	preScore = ent->client->ps.persistant[PERS_SCORE];

	//Make sure the class is valid for the team
	BG_SiegeCheckClassLegality(team, className);

	//Set the session data
	strcpy(ent->client->sess.siegeClass, className);

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( ent->s.number ) )
		return;

	if (ent->client->tempSpectate < level.time)
	{
		// Kill him (makes sure he loses flags, etc)
		if (ent->health > 0 && !startedAsSpec)
		{
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		}

		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || startedAsSpec)
		{ //respawn them instantly.
			ClientBegin( ent->s.number, qfalse );
		}
	}
	//set it back after we do all the stuff
	ent->client->ps.persistant[PERS_SCORE] = preScore;

	ent->client->switchClassTime = level.time + 5000;
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f( gentity_t *ent )
{
	char fpChStr[1024];
	const char *buf;
//	Cmd_Kill_f(ent);
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //if it's a spec, just make the changes now
		//trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "FORCEAPPLIED")) );
		//No longer print it, as the UI calls this a lot.
		WP_InitForcePowers( ent );
		goto argCheck;
	}

	buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

	strcpy(fpChStr, buf);

	trap->SendServerCommand( ent-g_entities, va("print \"%s%s\n\"", S_COLOR_GREEN, fpChStr) );

	ent->client->ps.fd.forceDoInit = 1;
argCheck:
	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{ //If this is duel, don't even bother changing team in relation to this.
		return;
	}

	if (trap->Argc() > 1)
	{
		char	arg[MAX_TOKEN_CHARS];

		trap->Argv( 1, arg, sizeof( arg ) );

		if ( arg[0] )
		{ //if there's an arg, assume it's a combo team command from the UI.
			Cmd_Team_f(ent);
		}
	}
}

extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride)
{
	char truncSaberName[MAX_QPATH] = {0};

	if ( !siegeOverride && level.gametype == GT_SIEGE && ent->client->siegeClass != -1 &&
		(bgSiegeClasses[ent->client->siegeClass].saberStance || bgSiegeClasses[ent->client->siegeClass].saber1[0] || bgSiegeClasses[ent->client->siegeClass].saber2[0]) )
	{ //don't let it be changed if the siege class has forced any saber-related things
		return qfalse;
	}

	Q_strncpyz( truncSaberName, saberName, sizeof( truncSaberName ) );

	if ( saberNum == 0 && (!Q_stricmp( "none", truncSaberName ) || !Q_stricmp( "remove", truncSaberName )) )
	{ //can't remove saber 0 like this
		Q_strncpyz( truncSaberName, DEFAULT_SABER, sizeof( truncSaberName ) );
	}

	//Set the saber with the arg given. If the arg is
	//not a valid sabername defaults will be used.
	WP_SetSaber( ent->s.number, ent->client->saber, saberNum, truncSaberName );

	if ( !ent->client->saber[0].model[0] )
	{
		assert(0); //should never happen!
		Q_strncpyz( ent->client->pers.saber1, DEFAULT_SABER, sizeof( ent->client->pers.saber1 ) );
	}
	else
		Q_strncpyz( ent->client->pers.saber1, ent->client->saber[0].name, sizeof( ent->client->pers.saber1 ) );

	if ( !ent->client->saber[1].model[0] )
		Q_strncpyz( ent->client->pers.saber2, "none", sizeof( ent->client->pers.saber2 ) );
	else
		Q_strncpyz( ent->client->pers.saber2, ent->client->saber[1].name, sizeof( ent->client->pers.saber2 ) );

	if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
	{
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
	}

	return qtrue;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( ent->client->sess.spectatorState == SPECTATOR_NOT && ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if ( trap->Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg, qfalse );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if ( level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	if ( level.clients[ i ].tempSpectate >= level.time ) {
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		//WTF???
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
		// fix: update team switch time only if team change really happend
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			ent->client->switchTeamTime = level.time + 5000;
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;
	qboolean	looped = qfalse;

	if ( ent->client->sess.spectatorState == SPECTATOR_NOT && ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {\
		//WTF???
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
		// fix: update team switch time only if team change really happend
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			ent->client->switchTeamTime = level.time + 5000;
	}

	if ( dir != 1 && dir != -1 ) {
		trap->Error( ERR_DROP, "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = ent->client->sess.spectatorClient;
	original = clientnum;

	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients )
		{
			// Avoid /team follow1 crash
			if ( looped )
			{
				clientnum = original;
				break;
			}
			else
			{
				clientnum = 0;
				looped = qtrue;
			}
		}
		if ( clientnum < 0 ) {
			if ( looped )
			{
				clientnum = original;
				break;
			}
			else
			{
				clientnum = level.maxclients - 1;
				looped = qtrue;
			}
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].tempSpectate >= level.time ) {
			return;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}

void Cmd_FollowNext_f( gentity_t *ent ) {
	Cmd_FollowCycle_f( ent, 1 );
}

void Cmd_FollowPrev_f( gentity_t *ent ) {
	Cmd_FollowCycle_f( ent, -1 );
}

// zyk: gives or removes jetpack from player
void zyk_jetpack(gentity_t* ent)
{
	// zyk: player starts with jetpack if it is enabled in player settings, is not in Siege Mode, and does not have all force powers through /give command
	if ((ent->client->sess.amrpgmode == 1 && 
		!(ent->client->pers.player_settings & (1 << SETTINGS_JETPACK))) && zyk_allow_jetpack_command.integer &&
		(level.gametype != GT_SIEGE || zyk_allow_jetpack_in_siege.integer) && 
		level.gametype != GT_JEDIMASTER && !(ent->client->pers.player_statuses & (1 << 12)))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
	}
	else if (ent->client->sess.amrpgmode < 2)
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK);
		if (ent->client->jetPackOn)
		{
			Jetpack_Off(ent);
		}
	}
}

// zyk: loads settings valid both to Admin-Only Mode and to RPG Mode
void zyk_load_common_settings(gentity_t* ent)
{
	// zyk: loading the starting weapon based in player settings
	if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER) && !(ent->client->pers.player_settings & (1 << SETTINGS_SABER_START)))
	{
		ent->client->ps.weapon = WP_SABER;
	}
	else
	{
		ent->client->ps.weapon = WP_MELEE;
	}

	zyk_jetpack(ent);
}

// zyk: remove white spaces from a string
char* zyk_string_with_no_whitespaces(char *old_string)
{
	int i = 0, j = 0;
	char new_string[MAX_STRING_CHARS];

	while (old_string[i] != '\0')
	{
		if (old_string[i] != ' ')
		{
			new_string[j] = old_string[i];
			j++;
		}

		i++;
	}

	new_string[j] = '\0';

	return G_NewString(new_string);
}

// zyk: loads the player account
void load_account(gentity_t* ent)
{
	FILE* account_file;
	char content[128];

	strcpy(content, "");
	account_file = fopen(va("zykmod/accounts/%s.txt", ent->client->sess.filename), "r");
	if (account_file != NULL)
	{
		int i = 0;
		// zyk: this variable will validate the skillpoints this player has
		// if he has more than the max skillpoints defined, then server must remove the exceeding ones
		int validate_skillpoints = 0;
		int max_skillpoints = 0;
		int j = 0;

		// zyk: loading the account password
		fscanf(account_file, "%s", content);
		strcpy(ent->client->pers.password, content);

		// zyk: loading the amrpgmode value
		fscanf(account_file, "%s", content);
		ent->client->sess.amrpgmode = atoi(content);

		if ((zyk_allow_rpg_mode.integer == 0 || (zyk_allow_rpg_in_other_gametypes.integer == 0 && level.gametype != GT_FFA)) && ent->client->sess.amrpgmode == 2)
		{ // zyk: RPG Mode not allowed. Change his account to Admin-Only Mode
			ent->client->sess.amrpgmode = 1;
		}
		else if (level.gametype == GT_SIEGE || level.gametype == GT_JEDIMASTER)
		{ // zyk: Siege and Jedi Master will never allow RPG Mode
			ent->client->sess.amrpgmode = 1;
		}

		// zyk: loading player_settings value
		fscanf(account_file, "%s", content);
		ent->client->pers.player_settings = atoi(content);

		// zyk: loading the admin command bit value
		fscanf(account_file, "%s", content);
		ent->client->pers.bitvalue = atoi(content);

		// zyk: loading the current char
		fscanf(account_file, "%s", content);
		strcpy(ent->client->sess.rpgchar, content);

		// zyk: reading the mod version of the last time this account was saved. Will be used in future versions for account validation
		fscanf(account_file, "%s", content);

		fclose(account_file);

		// zyk: loading the char file
		account_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, ent->client->sess.rpgchar), "r");
		if (account_file != NULL)
		{
			// zyk: loading level up score value
			fscanf(account_file, "%s", content);
			ent->client->pers.level_up_score = atoi(content);

			// zyk: loading Level value
			fscanf(account_file, "%s", content);
			ent->client->pers.level = atoi(content);

			// zyk: loading Skillpoints value
			fscanf(account_file, "%s", content);
			ent->client->pers.skillpoints = atoi(content);

			if (ent->client->pers.level > zyk_rpg_max_level.integer)
			{ // zyk: validating level
				ent->client->pers.level = zyk_rpg_max_level.integer;
			}
			else if (ent->client->pers.level < 1)
			{
				ent->client->pers.level = 1;
			}

			for (j = 1; j <= ent->client->pers.level; j++)
			{
				if ((j % 5) == 0)
				{ // zyk: level divisible by 5 has more skillpoints
					max_skillpoints += 3;
				}
				else
				{
					max_skillpoints++;
				}
			}

			validate_skillpoints = ent->client->pers.skillpoints;
			// zyk: loading skill levels
			for (i = 0; i < NUMBER_OF_SKILLS; i++)
			{
				fscanf(account_file, "%s", content);
				ent->client->pers.skill_levels[i] = atoi(content);
				validate_skillpoints += ent->client->pers.skill_levels[i];
			}

			// zyk: validating skillpoints
			if (validate_skillpoints != max_skillpoints)
			{
				// zyk: if not valid, reset all skills and set the max skillpoints he can have in this level
				for (i = 0; i < NUMBER_OF_SKILLS; i++)
				{
					ent->client->pers.skill_levels[i] = 0;
				}

				ent->client->pers.skillpoints = max_skillpoints;
			}

			// zyk: loading RPG inventory
			for (i = 0; i < MAX_RPG_INVENTORY_ITEMS; i++)
			{
				fscanf(account_file, "%s", content);
				ent->client->pers.rpg_inventory[i] = atoi(content);
			}

			// zyk: loading credits value
			fscanf(account_file, "%s", content);
			ent->client->pers.credits = atoi(content);

			// zyk: validating credits
			if (ent->client->pers.credits > zyk_max_rpg_credits.integer)
			{
				ent->client->pers.credits = zyk_max_rpg_credits.integer;
			}
			else if (ent->client->pers.credits < 0)
			{
				ent->client->pers.credits = 0;
			}

			// zyk: loading Magic Fist selection and selected powers
			fscanf(account_file, "%s", content);
			ent->client->sess.magic_fist_selection = atoi(content);

			// zyk: Main Quest progress
			fscanf(account_file, "%s", content);
			ent->client->pers.main_quest_progress = atoi(content);

			// zyk: Side Quest progress
			fscanf(account_file, "%s", content);
			ent->client->pers.side_quest_progress = atoi(content);

			// zyk: last health
			fscanf(account_file, "%s", content);
			ent->client->pers.last_health = atoi(content);

			// zyk: last shield
			fscanf(account_file, "%s", content);
			ent->client->pers.last_shield = atoi(content);

			// zyk: last mp
			fscanf(account_file, "%s", content);
			ent->client->pers.last_mp = atoi(content);

			// zyk: last stamina
			fscanf(account_file, "%s", content);
			ent->client->pers.last_stamina = atoi(content);

			if (ent->client->sess.amrpgmode == 1)
			{
				ent->client->ps.fd.forcePowerMax = zyk_max_force_power.integer;

				// zyk: setting default max hp and shield
				ent->client->ps.stats[STAT_MAX_HEALTH] = 100;

				if (ent->health > 100)
					ent->health = 100;

				if (ent->client->ps.stats[STAT_ARMOR] > 100)
					ent->client->ps.stats[STAT_ARMOR] = 100;

				// zyk: reset the force powers of this player
				WP_InitForcePowers(ent);

				if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0 &&
					level.gametype != GT_JEDIMASTER && level.gametype != GT_SIEGE
					)
					ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

				if (level.gametype != GT_JEDIMASTER && level.gametype != GT_SIEGE)
					ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

				zyk_load_common_settings(ent);
			}

			fclose(account_file);
		}
		else
		{ // zyk: char file could not be loaded
			trap->SendServerCommand(ent->s.number, "print \"Char file could not be loaded!\n\"");
		}
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no account with this login or password.\n\"");
	}
}

// zyk: saves info into the player account file. If save_char_file is qtrue, this function must save the char file
void save_account(gentity_t* ent, qboolean save_char_file)
{
	// zyk: used to prevent account save in map change time or before loading account after changing map
	if (level.voteExecuteTime < level.time && ent->client->pers.connected == CON_CONNECTED && ent->client->sess.amrpgmode > 0)
	{ // zyk: players can only save things if server is not at RP Mode or if it is allowed in config
		if (save_char_file == qtrue)
		{  // zyk: save the RPG char
			FILE* account_file;
			gclient_t* client;
			char content[MAX_STRING_CHARS];
			int i = 0;

			client = ent->client;
			strcpy(content, "");

			for (i = 0; i < NUMBER_OF_SKILLS; i++)
			{
				strcpy(content, va("%s%d\n", content, client->pers.skill_levels[i]));
			}

			for (i = 0; i < MAX_RPG_INVENTORY_ITEMS; i++)
			{
				strcpy(content, va("%s%d\n", content, client->pers.rpg_inventory[i]));
			}

			account_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, ent->client->sess.rpgchar), "w");

			fprintf(account_file, "%d\n%d\n%d\n%s%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
				client->pers.level_up_score, client->pers.level, client->pers.skillpoints, content, client->pers.credits,
				client->sess.magic_fist_selection, client->pers.main_quest_progress, client->pers.side_quest_progress,
				client->pers.last_health, client->pers.last_shield, client->pers.last_mp, client->pers.last_stamina);

			fclose(account_file);
		}
		else
		{ // zyk: save the main account file
			FILE* account_file;
			gclient_t* client;

			client = ent->client;
			account_file = fopen(va("zykmod/accounts/%s.txt", ent->client->sess.filename), "w");
			fprintf(account_file, "%s\n%d\n%d\n%d\n%s\n%s\n",
				client->pers.password, client->sess.amrpgmode, client->pers.player_settings, client->pers.bitvalue, client->sess.rpgchar, zyk_string_with_no_whitespaces(GAMEVERSION));
			fclose(account_file);
		}
	}
}

/*
==================
G_Say
==================
*/

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg )
{
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	if ( mode == SAY_ALLY && ent != other && zyk_is_ally(ent, other) == qfalse) { // zyk: allychat. Send it only to allies and to the player himself
		return;
	}
	/*
	// no chatting to players in tournaments
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& other->client->sess.sessionTeam == TEAM_FREE
		&& ent->client->sess.sessionTeam != TEAM_FREE ) {
		//Hmm, maybe some option to do so if allowed?  Or at least in developer mode...
		return;
	}
	*/
	//They've requested I take this out.

	if (level.gametype == GT_SIEGE &&
		ent->client && (ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == TEAM_SPECTATOR) &&
		other->client->sess.sessionTeam != TEAM_SPECTATOR &&
		other->client->tempSpectate < level.time)
	{ //siege temp spectators should not communicate to ingame players
		return;
	}

	// zyk: if player is ignored, then he cant say anything to the target player
	if ((ent->s.number < 31 && level.ignored_players[other->s.number][0] & (1 << ent->s.number)) || 
		(ent->s.number >= 31 && level.ignored_players[other->s.number][1] & (1 << (ent->s.number - 31))))
	{
		return;
	}

	if (locMsg)
	{
		trap->SendServerCommand( other-g_entities, va("%s \"%s\" \"%s\" \"%c\" \"%s\" %i",
			mode == SAY_TEAM ? "ltchat" : "lchat",
			name, locMsg, color, message, ent->s.number));
	}
	else
	{
		trap->SendServerCommand( other-g_entities, va("%s \"%s%c%c%s\" %i",
			mode == SAY_TEAM ? "tchat" : "chat",
			name, Q_COLOR_ESCAPE, color, message, ent->s.number));
	}
}

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons
	char		text[MAX_SAY_TEXT];
	char		location[64];
	char		*locMsg = NULL;

	if ( level.gametype < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	Q_strncpyz( text, chatText, sizeof(text) );

	Q_strstrip( text, "\n\r", "  " );

	switch ( mode ) {
	default:
	case SAY_ALL:
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << 0))
			return;

		G_LogPrintf( "say: %s: %s\n", ent->client->pers.netname, text );
		Com_sprintf (name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		break;
	case SAY_TEAM:
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << 0))
			return;

		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, text );
		if (Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ",
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ",
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
		if (target && target->inuse && target->client && level.gametype >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_MAGENTA;
		break;
	case SAY_ALLY: // zyk: say to allies
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << 0))
			return;

		G_LogPrintf( "sayally: %s: %s\n", ent->client->pers.netname, text );
		Com_sprintf (name, sizeof(name), EC"{%s%c%c"EC"}"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );

		color = COLOR_WHITE;
		break;
	}

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text, locMsg );
		return;
	}

	// echo the text to the console
	if ( dedicated.integer ) {
		trap->Print( "%s%s\n", name, text);
	}

	// send it to all the appropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_SayTo( ent, other, mode, color, name, text, locMsg );
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_Say_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_ALL, p );
}

/*
==================
Cmd_SayTeam_f
==================
*/
static void Cmd_SayTeam_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_SayTeam_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	// zyk: if not in TEAM gametypes and player has allies, use allychat (SAY_ALLY) instead of SAY_ALL
	if (zyk_number_of_allies(ent,qfalse) > 0)
		G_Say( ent, NULL, (level.gametype>=GT_TEAM) ? SAY_TEAM : SAY_ALLY, p );
	else
		G_Say( ent, NULL, (level.gametype>=GT_TEAM) ? SAY_TEAM : SAY_ALL, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	char		*p;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap->Argc () < 3 ) {
		trap->SendServerCommand( ent-g_entities, "print \"Usage: tell <player id or name> <message>\n\"" );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qfalse ); // zyk: changed this. Now it will use new function
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];

	p = ConcatArgs( 2 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_Tell_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}

//siege voice command
static void Cmd_VoiceCommand_f(gentity_t *ent)
{
	gentity_t *te;
	char arg[MAX_TOKEN_CHARS];
	char *s;
	char content[MAX_TOKEN_CHARS];
	int i = 0;

	/* zyk: now the voice commands will be allowed in all gametypes
	if (level.gametype < GT_TEAM)
	{
		return;
	}
	*/

	strcpy(content,"");

	if (trap->Argc() < 2)
	{
		// zyk: other gamemodes will show info of how to use the voice_cmd
		if (level.gametype < GT_TEAM)
		{
			for (i = 0; i < MAX_CUSTOM_SIEGE_SOUNDS; i++)
			{
				strcpy(content,va("%s%s\n",content,bg_customSiegeSoundNames[i]));
			}
			trap->SendServerCommand( ent-g_entities, va("print \"Usage: /voice_cmd <arg> [f]\nThe f argument is optional, it will make the command use female voice.\nThe arg may be one of the following:\n %s\"",content) );
		}
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		ent->client->tempSpectate >= level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")) );
		return;
	}

	trap->Argv(1, arg, sizeof(arg));

	if (arg[0] == '*')
	{ //hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
		return;
	}

	s = va("*%s", arg);

	//now, make sure it's a valid sound to be playing like this.. so people can't go around
	//screaming out death sounds or whatever.
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (!bg_customSiegeSoundNames[i])
		{
			break;
		}
		if (!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //it matches this one, so it's ok
			break;
		}
		i++;
	}

	if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i])
	{ //didn't find it in the list
		return;
	}

	if (level.gametype >= GT_TEAM)
	{
		te = G_TempEntity(vec3_origin, EV_VOICECMD_SOUND);
		te->s.groundEntityNum = ent->s.number;
		te->s.eventParm = G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
		te->r.svFlags |= SVF_BROADCAST;
	}
	else
	{ // zyk: in other gamemodes that are not Team ones, just do a G_Sound call to each allied player
		char arg2[MAX_TOKEN_CHARS];
		char voice_dir[32];

		strcpy(voice_dir,"mp_generic_male");

		if (trap->Argc() == 3)
		{
			trap->Argv(2, arg2, sizeof(arg2));
			if (Q_stricmp(arg2, "f") == 0)
				strcpy(voice_dir,"mp_generic_female");
		}

		G_Sound(ent,CHAN_VOICE,G_SoundIndex(va("sound/chars/%s/misc/%s.mp3",voice_dir,arg)));

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (zyk_is_ally(ent,&g_entities[i]) == qtrue)
			{
				trap->SendServerCommand(i, va("chat \"%s: ^3%s\"",ent->client->pers.netname,arg));
				G_Sound(&g_entities[i],CHAN_VOICE,G_SoundIndex(va("sound/chars/%s/misc/%s.mp3",voice_dir,arg)));
			}
		}
	}
}


static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};
static size_t numgc_orders = ARRAY_LEN( gc_orders );

void Cmd_GameCommand_f( gentity_t *ent ) {
	int				targetNum;
	unsigned int	order;
	gentity_t		*target;
	char			arg[MAX_TOKEN_CHARS] = {0};

	if ( trap->Argc() != 3 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1 ) );
		return;
	}

	trap->Argv( 2, arg, sizeof( arg ) );
	order = atoi( arg );

	if ( order >= numgc_orders ) {
		trap->SendServerCommand( ent-g_entities, va("print \"Bad order: %i\n\"", order));
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qfalse );
	if ( targetNum == -1 )
		return;

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client )
		return;

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order] );
	G_Say( ent, target, SAY_TELL, gc_orders[order] );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT) )
		G_Say( ent, ent, SAY_TELL, gc_orders[order] );
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	// zyk: changed code, so it will always use ps.origin and the ps.viewangles
	if(ent->client)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"origin: %s angles: %s\n\"", vtos(ent->client->ps.origin), vtos(ent->client->ps.viewangles)));
	}
}

static const char *gameNames[GT_MAX_GAME_TYPE] = {
	"Free For All",
	"Holocron FFA",
	"Jedi Master",
	"Duel",
	"Power Duel",
	"Single Player",
	"Team FFA",
	"Siege",
	"Capture the Flag",
	"Capture the Ysalamiri"
};

/*
==================
Cmd_CallVote_f
==================
*/
extern void SiegeClearSwitchData(void); //g_saga.c

qboolean G_VoteCapturelimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 0x7FFFFFFF, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteClientkick( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = atoi ( arg2 );

	if ( n < 0 || n >= level.maxclients ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"invalid client number %d.\n\"", n ) );
		return qfalse;
	}

	if ( g_entities[n].client->pers.connected == CON_DISCONNECTED ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"there is no client with the client number %d.\n\"", n ) );
		return qfalse;
	}

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, g_entities[n].client->pers.netname );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteFraglimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 0x7FFFFFFF, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteGametype( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int gt = atoi( arg2 );

	// ffa, ctf, tdm, etc
	if ( arg2[0] && isalpha( arg2[0] ) ) {
		gt = BG_GetGametypeForString( arg2 );
		if ( gt == -1 )
		{
			trap->SendServerCommand( ent-g_entities, va( "print \"Gametype (%s) unrecognised, defaulting to FFA/Deathmatch\n\"", arg2 ) );
			gt = GT_FFA;
		}
	}
	// numeric but out of range
	else if ( gt < 0 || gt >= GT_MAX_GAME_TYPE ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Gametype (%i) is out of range, defaulting to FFA/Deathmatch\n\"", gt ) );
		gt = GT_FFA;
	}

	// logically invalid gametypes, or gametypes not fully implemented in MP
	if ( gt == GT_SINGLE_PLAYER ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"This gametype is not supported (%s).\n\"", arg2 ) );
		return qfalse;
	}

	level.votingGametype = qtrue;
	level.votingGametypeTo = gt;

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, gt );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[gt] );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteKick( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int clientid = ClientNumberFromString( ent, arg2, qtrue );
	gentity_t *target = NULL;

	if ( clientid == -1 )
		return qfalse;

	target = &g_entities[clientid];
	if ( !target || !target->inuse || !target->client )
		return qfalse;

	Com_sprintf( level.voteString, sizeof( level.voteString ), "clientkick %d", clientid );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "kick %s", target->client->pers.netname );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

const char *G_GetArenaInfoByMap( const char *map );

void Cmd_MapList_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];

	if ( trap->Argc() < 2 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Use ^3/maplist <page number> ^7to see map list. Use ^3/maplist bsp ^7to show bsp files, which can be used in /callvote map <bsp file>\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));

	if (Q_stricmp(arg1, "bsp") == 0)
	{
		int i, toggle=0;
		char map[24] = "--", buf[512] = {0};

		Q_strcat( buf, sizeof( buf ), "Map list:" );

		for ( i=0; i<level.arenas.num; i++ ) {
			Q_strncpyz( map, Info_ValueForKey( level.arenas.infos[i], "map" ), sizeof( map ) );
			Q_StripColor( map );

			if ( G_DoesMapSupportGametype( map, level.gametype ) ) {
				char *tmpMsg = va( " ^%c%s", (++toggle&1) ? COLOR_GREEN : COLOR_YELLOW, map );
				if ( strlen( buf ) + strlen( tmpMsg ) >= sizeof( buf ) ) {
					trap->SendServerCommand( ent-g_entities, va( "print \"%s\"", buf ) );
					buf[0] = '\0';
				}
				Q_strcat( buf, sizeof( buf ), tmpMsg );
			}
		}

		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", buf ) );
	}
	else
	{
		int page = 1; // zyk: page the user wants to see
		char file_content[MAX_STRING_CHARS];
		char content[512];
		int i = 0;
		int results_per_page = zyk_list_cmds_results_per_page.integer; // zyk: number of results per page
		FILE *map_list_file;
		strcpy(file_content,"");
		strcpy(content,"");

		page = atoi(arg1);

		if (page == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid page number\n\"" );
			return;
		}

		map_list_file = fopen("zykmod/maplist.txt","r");
		if (map_list_file != NULL)
		{
			while(i < (results_per_page * (page-1)) && fgets(content, sizeof(content), map_list_file) != NULL)
			{ // zyk: reads the file until it reaches the position corresponding to the page number
				i++;
			}

			while(i < (results_per_page * page) && fgets(content, sizeof(content), map_list_file) != NULL)
			{ // zyk: fgets returns NULL at EOF
				strcpy(file_content,va("%s%s",file_content,content));
				i++;
			}

			fclose(map_list_file);
			trap->SendServerCommand(ent-g_entities, va("print \"\n%s\n\"",file_content));
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"The maplist file does not exist\n\"" );
			return;
		}
	}
}

qboolean G_VotePoll( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	// zyk: did not put message
	if ( numArgs < 3 ) {
		return qfalse;
	}

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "poll %s", arg2 );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );

	// zyk: now the vote poll will appear in chat
	trap->SendServerCommand( -1, va("chat \"^3Poll System: ^7%s ^2Yes^3^1/No^7\"",arg2));

	return qtrue;
}

qboolean G_VoteMap( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	char s[MAX_CVAR_VALUE_STRING] = {0}, bspName[MAX_QPATH] = {0}, *mapName = NULL, *mapName2 = NULL;
	fileHandle_t fp = NULL_FILE;
	const char *arenaInfo;

	// didn't specify a map, show available maps
	if ( numArgs < 3 ) {
		Cmd_MapList_f( ent );
		return qfalse;
	}

	if ( strchr( arg2, '\\' ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Can't have mapnames with a \\\n\"" );
		return qfalse;
	}

	Com_sprintf( bspName, sizeof(bspName), "maps/%s.bsp", arg2 );
	if ( trap->FS_Open( bspName, &fp, FS_READ ) <= 0 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Can't find map %s on server\n\"", bspName ) );
		if( fp != NULL_FILE )
			trap->FS_Close( fp );
		return qfalse;
	}
	trap->FS_Close( fp );

	if ( !G_DoesMapSupportGametype( arg2, level.gametype ) ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOVOTE_MAPNOTSUPPORTEDBYGAME" ) ) );
		return qfalse;
	}

	// preserve the map rotation
	trap->Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
	if ( *s )
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", arg1, arg2, s );
	else
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );

	arenaInfo = G_GetArenaInfoByMap(arg2);
	if ( arenaInfo ) {
		mapName = Info_ValueForKey( arenaInfo, "longname" );
		mapName2 = Info_ValueForKey( arenaInfo, "map" );
	}

	if ( !mapName || !mapName[0] )
		mapName = "ERROR";

	if ( !mapName2 || !mapName2[0] )
		mapName2 = "ERROR";

	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "map %s (%s)", mapName, mapName2 );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteMapRestart( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 60, atoi( arg2 ) );
	if ( numArgs < 3 )
		n = 5;
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteNextmap( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	char s[MAX_CVAR_VALUE_STRING];

	trap->Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
	if ( !*s ) {
		trap->SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
		return qfalse;
	}
	SiegeClearSwitchData();
	Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteTimelimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	float tl = Com_Clamp( 0.0f, 35790.0f, atof( arg2 ) );
	if ( Q_isintegral( tl ) )
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, (int)tl );
	else
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %.3f", arg1, tl );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteWarmup( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 1, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

typedef struct voteString_s {
	const char	*string;
	const char	*aliases;	// space delimited list of aliases, will always show the real vote string
	qboolean	(*func)(gentity_t *ent, int numArgs, const char *arg1, const char *arg2);
	int			numArgs;	// number of REQUIRED arguments, not total/optional arguments
	uint32_t	validGT;	// bit-flag of valid gametypes
	qboolean	voteDelay;	// if true, will delay executing the vote string after it's accepted by g_voteDelay
	const char	*shortHelp;	// NULL if no arguments needed
} voteString_t;

static voteString_t validVoteStrings[] = {
	//	vote string				aliases										# args	valid gametypes							exec delay		short help
	{	"capturelimit",			"caps",				G_VoteCapturelimit,		1,		GTB_CTF|GTB_CTY,						qtrue,			"<num>" },
	{	"clientkick",			NULL,				G_VoteClientkick,		1,		GTB_ALL,								qfalse,			"<clientnum>" },
	{	"fraglimit",			"frags",			G_VoteFraglimit,		1,		GTB_ALL & ~(GTB_SIEGE|GTB_CTF|GTB_CTY),	qtrue,			"<num>" },
	{	"g_doWarmup",			"dowarmup warmup",	G_VoteWarmup,			1,		GTB_ALL,								qtrue,			"<0-1>" },
	{	"g_gametype",			"gametype gt mode",	G_VoteGametype,			1,		GTB_ALL,								qtrue,			"<num or name>" },
	{	"kick",					NULL,				G_VoteKick,				1,		GTB_ALL,								qfalse,			"<client name>" },
	{	"map",					NULL,				G_VoteMap,				0,		GTB_ALL,								qtrue,			"<name>" },
	{	"map_restart",			"restart",			G_VoteMapRestart,		0,		GTB_ALL,								qtrue,			"<optional delay>" },
	{	"nextmap",				NULL,				G_VoteNextmap,			0,		GTB_ALL,								qtrue,			NULL },
	{	"poll",					NULL,				G_VotePoll,				0,		GTB_ALL,								qtrue,			"<message>" },
	{	"timelimit",			"time",				G_VoteTimelimit,		1,		GTB_ALL &~GTB_SIEGE,					qtrue,			"<num>" },
};
static const int validVoteStringsSize = ARRAY_LEN( validVoteStrings );

void Svcmd_ToggleAllowVote_f( void ) {
	if ( trap->Argc() == 1 ) {
		int i = 0;
		for ( i = 0; i<validVoteStringsSize; i++ ) {
			if ( (g_allowVote.integer & (1 << i)) )	trap->Print( "%2d [X] %s\n", i, validVoteStrings[i].string );
			else									trap->Print( "%2d [ ] %s\n", i, validVoteStrings[i].string );
		}
		return;
	}
	else {
		char arg[8] = { 0 };
		int index;

		trap->Argv( 1, arg, sizeof( arg ) );
		index = atoi( arg );

		if ( index < 0 || index >= validVoteStringsSize ) {
			Com_Printf( "ToggleAllowVote: Invalid range: %i [0, %i]\n", index, validVoteStringsSize - 1 );
			return;
		}

		trap->Cvar_Set( "g_allowVote", va( "%i", (1 << index) ^ (g_allowVote.integer & ((1 << validVoteStringsSize) - 1)) ) );
		trap->Cvar_Update( &g_allowVote );

		Com_Printf( "%s %s^7\n", validVoteStrings[index].string, ((g_allowVote.integer & (1 << index)) ? "^2Enabled" : "^1Disabled") );
	}
}

void Cmd_CallVote_f( gentity_t *ent ) {
	int				i=0, numArgs=0;
	char			arg1[MAX_CVAR_VALUE_STRING] = {0};
	char			arg2[MAX_CVAR_VALUE_STRING] = {0};
	voteString_t	*vote = NULL;

	// not allowed to vote at all
	if ( !g_allowVote.integer ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOVOTE" ) ) );
		return;
	}

	// vote in progress
	else if ( level.voteTime ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "VOTEINPROGRESS" ) ) );
		return;
	}

	// can't vote as a spectator, except in (power)duel
	else if ( level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL && ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOSPECVOTE" ) ) );
		return;
	}

	// zyk: tests if this player can vote now
	if (zyk_vote_timer.integer > 0 && ent->client->sess.vote_timer > 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You cannot vote now, wait %d seconds and try again.\n\"", ent->client->sess.vote_timer));
		return;
	}

	level.voting_player = ent->s.number;

	// make sure it is a valid command to vote on
	numArgs = trap->Argc();
	trap->Argv( 1, arg1, sizeof( arg1 ) );
	if ( numArgs > 1 )
		Q_strncpyz( arg2, ConcatArgs( 2 ), sizeof( arg2 ) );

	// filter ; \n \r
	if ( Q_strchrs( arg1, ";\r\n" ) || Q_strchrs( arg2, ";\r\n" ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	// check for invalid votes
	for ( i=0; i<validVoteStringsSize; i++ ) {
		if ( !(g_allowVote.integer & (1<<i)) )
			continue;

		if ( !Q_stricmp( arg1, validVoteStrings[i].string ) )
			break;

		// see if they're using an alias, and set arg1 to the actual vote string
		if ( validVoteStrings[i].aliases ) {
			char tmp[MAX_TOKEN_CHARS] = {0}, *p = NULL;
			const char *delim = " ";
			Q_strncpyz( tmp, validVoteStrings[i].aliases, sizeof( tmp ) );
			p = strtok( tmp, delim );
			while ( p != NULL ) {
				if ( !Q_stricmp( arg1, p ) ) {
					Q_strncpyz( arg1, validVoteStrings[i].string, sizeof( arg1 ) );
					goto validVote;
				}
				p = strtok( NULL, delim );
			}
		}
	}
	// invalid vote string, abandon ship
	if ( i == validVoteStringsSize ) {
		char buf[1024] = {0};
		int toggle = 0;
		trap->SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap->SendServerCommand( ent-g_entities, "print \"Allowed vote strings are: \"" );
		for ( i=0; i<validVoteStringsSize; i++ ) {
			if ( !(g_allowVote.integer & (1<<i)) )
				continue;

			toggle = !toggle;
			if ( validVoteStrings[i].shortHelp ) {
				Q_strcat( buf, sizeof( buf ), va( "^%c%s %s ",
					toggle ? COLOR_GREEN : COLOR_YELLOW,
					validVoteStrings[i].string,
					validVoteStrings[i].shortHelp ) );
			}
			else {
				Q_strcat( buf, sizeof( buf ), va( "^%c%s ",
					toggle ? COLOR_GREEN : COLOR_YELLOW,
					validVoteStrings[i].string ) );
			}
		}

		//FIXME: buffer and send in multiple messages in case of overflow
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", buf ) );
		return;
	}

validVote:
	vote = &validVoteStrings[i];
	if ( !(vote->validGT & (1<<level.gametype)) ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s is not applicable in this gametype.\n\"", arg1 ) );
		return;
	}

	if ( numArgs < vote->numArgs+2 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s requires more arguments: %s\n\"", arg1, vote->shortHelp ) );
		return;
	}

	level.votingGametype = qfalse;

	level.voteExecuteDelay = vote->voteDelay ? g_voteDelay.integer : 0;

	// there is still a vote to be executed, execute it and store the new vote
	if ( level.voteExecuteTime ) {
		level.voteExecuteTime = 0;
		trap->SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );
	}

	// pass the args onto vote-specific handlers for parsing/filtering
	if ( vote->func ) {
		if ( !vote->func( ent, numArgs, arg1, arg2 ) )
			return;
	}
	// otherwise assume it's a command
	else {
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
		Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	}
	Q_strstrip( level.voteStringClean, "\"\n\r", NULL );

	trap->SendServerCommand( -1, va( "print \"%s^7 %s (%s)\n\"", ent->client->pers.netname, G_GetStringEdString( "MP_SVGAME", "PLCALLEDVOTE" ), level.voteStringClean ) );

	// start the voting, the caller automatically votes yes
	level.voteTime = level.time;
	level.voteYes = 0; // zyk: the caller no longer counts as yes, because it may be a poll or the caller may regret the vote
	level.voteNo = 0;

	for ( i=0; i<level.maxclients; i++ ) {
		level.clients[i].mGameFlags &= ~PSG_VOTED;
		level.clients[i].pers.vote = 0;
	}

	//ent->client->mGameFlags |= PSG_VOTED; // zyk: no longer count the caller
	//ent->client->pers.vote = 1; // zyk: no longer count the caller

	trap->SetConfigstring( CS_VOTE_TIME,	va( "%i", level.voteTime ) );
	trap->SetConfigstring( CS_VOTE_STRING,	level.voteDisplayString );
	trap->SetConfigstring( CS_VOTE_YES,		va( "%i", level.voteYes ) );
	trap->SetConfigstring( CS_VOTE_NO,		va( "%i", level.voteNo ) );
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent ) {
	char		msg[64] = {0};

	if ( !level.voteTime ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_VOTED ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY")) );
		return;
	}
	if (level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
			return;
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLVOTECAST")) );

	ent->client->mGameFlags |= PSG_VOTED;

	trap->Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.voteYes++;
		ent->client->pers.vote = 1;
		trap->SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	} else {
		level.voteNo++;
		ent->client->pers.vote = 2;
		trap->SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );
	}

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

qboolean G_TeamVoteLeader( gentity_t *ent, int cs_offset, team_t team, int numArgs, const char *arg1, const char *arg2 ) {
	int clientid = numArgs == 2 ? ent->s.number : ClientNumberFromString( ent, arg2, qfalse );
	gentity_t *target = NULL;

	if ( clientid == -1 )
		return qfalse;

	target = &g_entities[clientid];
	if ( !target || !target->inuse || !target->client )
		return qfalse;

	if ( target->client->sess.sessionTeam != team )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"User %s is not on your team\n\"", arg2 ) );
		return qfalse;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "leader %d", clientid );
	Q_strncpyz( level.teamVoteDisplayString[cs_offset], level.teamVoteString[cs_offset], sizeof( level.teamVoteDisplayString[cs_offset] ) );
	Q_strncpyz( level.teamVoteStringClean[cs_offset], level.teamVoteString[cs_offset], sizeof( level.teamVoteStringClean[cs_offset] ) );
	return qtrue;
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent ) {
	team_t	team = ent->client->sess.sessionTeam;
	int		i=0, cs_offset=0, numArgs=0;
	char	arg1[MAX_CVAR_VALUE_STRING] = {0};
	char	arg2[MAX_CVAR_VALUE_STRING] = {0};

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	// not allowed to vote at all
	if ( !g_allowTeamVote.integer ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	// vote in progress
	else if ( level.teamVoteTime[cs_offset] ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")) );
		return;
	}

	// can't vote as a spectator
	else if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
		return;
	}

	// make sure it is a valid command to vote on
	numArgs = trap->Argc();
	trap->Argv( 1, arg1, sizeof( arg1 ) );
	if ( numArgs > 1 )
		Q_strncpyz( arg2, ConcatArgs( 2 ), sizeof( arg2 ) );

	// filter ; \n \r
	if ( Q_strchrs( arg1, ";\r\n" ) || Q_strchrs( arg2, ";\r\n" ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid team vote string.\n\"" );
		return;
	}

	// pass the args onto vote-specific handlers for parsing/filtering
	if ( !Q_stricmp( arg1, "leader" ) ) {
		if ( !G_TeamVoteLeader( ent, cs_offset, team, numArgs, arg1, arg2 ) )
			return;
	}
	else {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid team vote string.\n\"" );
		trap->SendServerCommand( ent-g_entities, va("print \"Allowed team vote strings are: ^%c%s %s\n\"", COLOR_GREEN, "leader", "<optional client name or number>" ));
		return;
	}

	Q_strstrip( level.teamVoteStringClean[cs_offset], "\"\n\r", NULL );

	for ( i=0; i<level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team )
			trap->SendServerCommand( i, va("print \"%s^7 called a team vote (%s)\n\"", ent->client->pers.netname, level.teamVoteStringClean[cs_offset] ) );
	}

	// start the voting, the caller autoamtically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i=0; i<level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team ) {
			level.clients[i].mGameFlags &= ~PSG_TEAMVOTED;
			level.clients[i].pers.teamvote = 0;
		}
	}
	ent->client->mGameFlags |= PSG_TEAMVOTED;
	ent->client->pers.teamvote = 1;

	trap->SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
	trap->SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteDisplayString[cs_offset] );
	trap->SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	trap->SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	team_t		team = ent->client->sess.sessionTeam;
	int			cs_offset=0;
	char		msg[64] = {0};

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_TEAMVOTED ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")) );

	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap->Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.teamVoteYes[cs_offset]++;
		ent->client->pers.teamvote = 1;
		trap->SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else {
		level.teamVoteNo[cs_offset]++;
		ent->client->pers.teamvote = 2;
		trap->SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( trap->Argc() != 5 ) {
		trap->SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap->Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap->Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}

void G_LeaveVehicle( gentity_t* ent, qboolean ConCheck ) {

	if (ent->client->ps.m_iVehicleNum)
	{ //tell it I'm getting off
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			if ( ConCheck ) { // check connection
				clientConnected_t pCon = ent->client->pers.connected;
				ent->client->pers.connected = CON_DISCONNECTED;
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
				ent->client->pers.connected = pCon;
			} else { // or not.
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
			}
		}
	}

	ent->client->ps.m_iVehicleNum = 0;
}

int G_ItemUsable(playerState_t *ps, int forcedUse)
{
	vec3_t fwd, fwdorg, dest, pos;
	vec3_t yawonly;
	vec3_t mins, maxs;
	vec3_t trtest;
	trace_t tr;

	// fix: dead players shouldn't use items
	if (ps->stats[STAT_HEALTH] <= 0) {
		return 0;
	}

	if (ps->m_iVehicleNum)
	{
		return 0;
	}

	if (ps->pm_flags & PMF_USE_ITEM_HELD)
	{ //force to let go first
		return 0;
	}

	if (!forcedUse)
	{
		forcedUse = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	}

	if (!BG_IsItemSelectable(ps, forcedUse))
	{
		return 0;
	}

	switch (forcedUse)
	{
	case HI_MEDPAC:
	case HI_MEDPAC_BIG:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return 0;
		}

		if (ps->stats[STAT_HEALTH] <= 0)
		{
			return 0;
		}

		return 1;
	case HI_SEEKER:
		if (ps->eFlags & EF_SEEKERDRONE)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SEEKER_ALREADYDEPLOYED);
			return 0;
		}

		return 1;
	case HI_SENTRY_GUN:
		if (ps->fd.sentryDeployed)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_ALREADYPLACED);
			return 0;
		}

		yawonly[ROLL] = 0;
		yawonly[PITCH] = 0;
		yawonly[YAW] = ps->viewangles[YAW];

		VectorSet( mins, -8, -8, 0 );
		VectorSet( maxs, 8, 8, 24 );

		AngleVectors(yawonly, fwd, NULL, NULL);

		fwdorg[0] = ps->origin[0] + fwd[0]*64;
		fwdorg[1] = ps->origin[1] + fwd[1]*64;
		fwdorg[2] = ps->origin[2] + fwd[2]*64;

		trtest[0] = fwdorg[0] + fwd[0]*16;
		trtest[1] = fwdorg[1] + fwd[1]*16;
		trtest[2] = fwdorg[2] + fwd[2]*16;

		trap->Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID, qfalse, 0, 0);

		if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_NOROOM);
			return 0;
		}

		return 1;
	case HI_SHIELD:
		mins[0] = -8;
		mins[1] = -8;
		mins[2] = 0;

		maxs[0] = 8;
		maxs[1] = 8;
		maxs[2] = 8;

		AngleVectors (ps->viewangles, fwd, NULL, NULL);
		fwd[2] = 0;
		VectorMA(ps->origin, 64, fwd, dest);
		trap->Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT, qfalse, 0, 0 );
		if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid)
		{
			VectorCopy(tr.endpos, pos);
			VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
			trap->Trace( &tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID, qfalse, 0, 0 );
			if ( !tr.startsolid && !tr.allsolid )
			{
				return 1;
			}
		}
		G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SHIELD_NOROOM);
		return 0;
	case HI_JETPACK: //do something?
		return 1;
	case HI_HEALTHDISP:
		return 1;
	case HI_AMMODISP:
		return 1;
	case HI_EWEB:
		return 1;
	case HI_CLOAK:
		return 1;
	default:
		return 1;
	}
}

void saberKnockDown(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void Cmd_ToggleSaber_f(gentity_t *ent)
{
	if (ent->client->ps.fd.forceGripCripple)
	{ //if they are being gripped, don't let them unholster their saber
		if (ent->client->ps.saberHolstered)
		{
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		if (ent->client->ps.saberEntityNum)
		{ //turn it off in midair
			saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
		}
		return;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

//	if (ent->client->ps.duelInProgress && !ent->client->ps.saberHolstered)
//	{
//		return;
//	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}

	// zyk: noclip does not allow toggle saber
	if ( ent->client->noclip == qtrue )
	{
		return;
	}

	if (ent->client && ent->client->ps.weaponTime < 1)
	{
		if (ent->client->ps.saberHolstered == 2)
		{
			ent->client->ps.saberHolstered = 0;

			if (ent->client->saber[0].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			}
			if (ent->client->saber[1].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
			}
		}
		else
		{
			ent->client->ps.saberHolstered = 2;
			if (ent->client->saber[0].soundOff)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
			}
			if (ent->client->saber[1].soundOff &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
			}
			//prevent anything from being done for 400ms after holster
			ent->client->ps.weaponTime = 400;
		}
	}
}

extern vmCvar_t		d_saberStanceDebug;

extern qboolean WP_SaberCanTurnOffSomeBlades( saberInfo_t *saber );
void Cmd_SaberAttackCycle_f(gentity_t *ent)
{
	int selectLevel = 0;
	qboolean usingSiegeStyle = qfalse;

	if ( !ent || !ent->client )
	{
		return;
	}

	if ( level.intermissionQueued || level.intermissiontime )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s (saberAttackCycle)\n\"", G_GetStringEdString( "MP_SVGAME", "CANNOT_TASK_INTERMISSION" ) ) );
		return;
	}

	if ( ent->health <= 0
			|| ent->client->tempSpectate >= level.time
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}


	if ( ent->client->ps.weapon != WP_SABER )
	{
        return;
	}
	/*
	if (ent->client->ps.weaponTime > 0)
	{ //no switching attack level when busy
		return;
	}
	*/

	if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
	{ //no cycling for akimbo
		if ( WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//can turn second saber off
			if ( ent->client->ps.saberHolstered == 1 )
			{//have one holstered
				//unholster it
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
				//g_active should take care of this, but...
				ent->client->ps.fd.saberAnimLevel = SS_DUAL;
			}
			else if ( ent->client->ps.saberHolstered == 0 )
			{//have none holstered
				if ( (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
				{//can't turn it off manually
				}
				else if ( ent->client->saber[1].bladeStyle2Start > 0
					&& (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
				{//can't turn it off manually
				}
				else
				{
					//turn it off
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
					ent->client->ps.saberHolstered = 1;
					//g_active should take care of this, but...
					ent->client->ps.fd.saberAnimLevel = SS_FAST;
				}
			}

			if (d_saberStanceDebug.integer)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"") );
			}
			return;
		}
	}
	else if (ent->client->saber[0].numBlades > 1
		&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
	{ //use staff stance then.
		if ( ent->client->ps.saberHolstered == 1 )
		{//second blade off
			if ( ent->client->ps.saberInFlight )
			{//can't turn second blade back on if it's in the air, you naughty boy!
				if (d_saberStanceDebug.integer)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"") );
				}
				return;
			}
			//turn it on
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			ent->client->ps.saberHolstered = 0;
			//g_active should take care of this, but...
			if ( ent->client->saber[0].stylesForbidden )
			{//have a style we have to use
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
				if ( ent->client->ps.weaponTime <= 0 )
				{ //not busy, set it now
					ent->client->ps.fd.saberAnimLevel = selectLevel;
				}
				else
				{ //can't set it now or we might cause unexpected chaining, so queue it
					ent->client->saberCycleQueue = selectLevel;
				}
			}
		}
		else if ( ent->client->ps.saberHolstered == 0 )
		{//both blades on
			if ( (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
			{//can't turn it off manually
			}
			else if ( ent->client->saber[0].bladeStyle2Start > 0
				&& (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
			{//can't turn it off manually
			}
			else
			{
				//turn second one off
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
				//g_active should take care of this, but...
				if ( ent->client->saber[0].singleBladeStyle != SS_NONE )
				{
					if ( ent->client->ps.weaponTime <= 0 )
					{ //not busy, set it now
						ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
					}
					else
					{ //can't set it now or we might cause unexpected chaining, so queue it
						ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
					}
				}
			}
		}
		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"") );
		}
		return;
	}

	if (ent->client->saberCycleQueue)
	{ //resume off of the queue if we haven't gotten a chance to update it yet
		selectLevel = ent->client->saberCycleQueue;
	}
	else
	{
		selectLevel = ent->client->ps.fd.saberAnimLevel;
	}

	if (level.gametype == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		bgSiegeClasses[ent->client->siegeClass].saberStance)
	{ //we have a flag of useable stances so cycle through it instead
		int i = selectLevel+1;

		usingSiegeStyle = qtrue;

		while (i != selectLevel)
		{ //cycle around upward til we hit the next style or end up back on this one
			if (i >= SS_NUM_SABER_STYLES)
			{ //loop back around to the first valid
				i = SS_FAST;
			}

			if (bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << i))
			{ //we can use this one, select it and break out.
				selectLevel = i;
				break;
			}
			i++;
		}

		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"") );
		}
	}
	else
	{
		selectLevel++;
		if ( selectLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
		{
			selectLevel = FORCE_LEVEL_1;
		}
		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"") );
		}
	}
/*
#ifndef FINAL_BUILD
	switch ( selectLevel )
	{
	case FORCE_LEVEL_1:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sfast\n\"", S_COLOR_BLUE) );
		break;
	case FORCE_LEVEL_2:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %smedium\n\"", S_COLOR_YELLOW) );
		break;
	case FORCE_LEVEL_3:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sstrong\n\"", S_COLOR_RED) );
		break;
	}
#endif
*/
	if ( !usingSiegeStyle )
	{
		//make sure it's valid, change it if not
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
	}

	if (ent->client->ps.weaponTime <= 0)
	{ //not busy, set it now
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = selectLevel;
	}
	else
	{ //can't set it now or we might cause unexpected chaining, so queue it
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = selectLevel;
	}
}

qboolean G_OtherPlayersDueling(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.duelInProgress)
		{
			return qtrue;
		}
		i++;
	}

	return qfalse;
}

void Cmd_EngageDuel_f(gentity_t *ent)
{
	trace_t tr;
	vec3_t forward, fwdOrg;

	if (!g_privateDuel.integer)
	{
		return;
	}

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{ //rather pointless in this mode..
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	// zyk: dont engage if held by a rancor to prevent player being invisible after eaten
	if (ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
	{
		return;
	}

	/*
	if (!ent->client->ps.saberHolstered)
	{ //must have saber holstered at the start of the duel
		return;
	}
	*/
	//NOTE: No longer doing this..

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->ps.duelInProgress)
	{
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{ // zyk: cannot accept duel in RPG Mode
		return;
	}

	if (level.duel_tournament_mode > 1 && level.duel_players[ent->s.number] != -1)
	{ // zyk: during a Duel Tournament, players cannot private duel
		return;
	}

	//New: Don't let a player duel if he just did and hasn't waited 10 seconds yet (note: If someone challenges him, his duel timer will reset so he can accept)
	/*if (ent->client->ps.fd.privateDuelTime > level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_JUSTDID")) );
		return;
	}

	if (G_OtherPlayersDueling())
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_BUSY")) );
		return;
	}*/

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0]*256;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1]*256;
	fwdOrg[2] = (ent->client->ps.origin[2]+ent->client->ps.viewheight) + forward[2]*256;

	trap->Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 ||
			challenged->client->ps.weapon != WP_SABER || challenged->client->ps.duelInProgress ||
			challenged->client->ps.saberInFlight ||
			challenged->client->ps.eFlags2 & EF2_HELD_BY_MONSTER) // zyk: added this condition to prevent player being invisible after eaten by a rancor
		{
			return;
		}

		if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time)
		{
			trap->SendServerCommand( /*challenged-g_entities*/-1, va("print \"%s ^7%s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );

			ent->client->ps.duelInProgress = qtrue;
			challenged->client->ps.duelInProgress = qtrue;

			// zyk: reset hp and shield of both players
			ent->health = 100;
			ent->client->ps.stats[STAT_ARMOR] = 100;

			challenged->health = 100;
			challenged->client->ps.stats[STAT_ARMOR] = 100;

			// zyk: disable jetpack of both players
			Jetpack_Off(ent);
			Jetpack_Off(challenged);

			ent->client->ps.duelTime = level.time + 2000;
			challenged->client->ps.duelTime = level.time + 2000;

			G_AddEvent(ent, EV_PRIVATE_DUEL, 1);
			G_AddEvent(challenged, EV_PRIVATE_DUEL, 1);

			//Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

			if (!ent->client->ps.saberHolstered)
			{
				if (ent->client->saber[0].soundOff)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				}
				if (ent->client->saber[1].soundOff &&
					ent->client->saber[1].model[0])
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				}
				ent->client->ps.weaponTime = 400;
				ent->client->ps.saberHolstered = 2;
			}
			if (!challenged->client->ps.saberHolstered)
			{
				if (challenged->client->saber[0].soundOff)
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[0].soundOff);
				}
				if (challenged->client->saber[1].soundOff &&
					challenged->client->saber[1].model[0])
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[1].soundOff);
				}
				challenged->client->ps.weaponTime = 400;
				challenged->client->ps.saberHolstered = 2;
			}
		}
		else
		{
			//Print the message that a player has been challenged in private, only announce the actual duel initiation in private
			trap->SendServerCommand( challenged-g_entities, va("cp \"%s ^7%s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
			trap->SendServerCommand( ent-g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
		}

		challenged->client->ps.fd.privateDuelTime = 0; //reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->ps.duelIndex = challenged->s.number;
		ent->client->ps.duelTime = level.time + 5000;
	}
}

#ifndef FINAL_BUILD
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

void Cmd_DebugSetSaberMove_f(gentity_t *self)
{
	int argNum = trap->Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	self->client->ps.saberMove = atoi(arg);
	self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;

	if (self->client->ps.saberMove >= LS_MOVE_MAX)
	{
		self->client->ps.saberMove = LS_MOVE_MAX-1;
	}

	Com_Printf("Anim for move: %s\n", animTable[saberMoveData[self->client->ps.saberMove].animToUse].name);
}

void Cmd_DebugSetBodyAnim_f(gentity_t *self)
{
	int argNum = trap->Argc();
	char arg[MAX_STRING_CHARS];
	int i = 0;

	if (argNum < 2)
	{
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(arg, animTable[i].name))
		{
			break;
		}
		i++;
	}

	if (i == MAX_ANIMATIONS)
	{
		Com_Printf("Animation '%s' does not exist\n", arg);
		return;
	}

	G_SetAnim(self, NULL, SETANIM_BOTH, i, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

	Com_Printf("Set body anim to %s\n", arg);
}
#endif

void StandardSetBodyAnim(gentity_t *self, int anim, int flags)
{
	G_SetAnim(self, NULL, SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t *self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t *self, int num);
extern void G_SetVehDamageFlags( gentity_t *veh, int shipSurf, int damageLevel );
#endif

// zyk: displays the yellow bar that shows the cooldown time between magic powers
void display_yellow_bar(gentity_t *ent, int duration)
{
	gentity_t *te = NULL;

	te = G_TempEntity( ent->client->ps.origin, EV_LOCALTIMER );
	te->s.time = level.time;
	te->s.time2 = duration;
	te->s.owner = ent->client->ps.clientNum;
}

// zyk: returns the max amount of Magic Power this player can have
int zyk_max_magic_power(gentity_t *ent)
{
	int max_mp = ent->client->pers.skill_levels[SKILL_MAX_MP] * 100;

	return max_mp;
}

// zyk: tests if this skill is one of the magic powers
qboolean zyk_is_magic_power_skill(int skill_index)
{
	if (skill_index >= SKILL_MAGIC_MAGIC_SENSE && skill_index <= SKILL_MAGIC_LIGHT_MAGIC)
	{
		return qtrue;
	}

	return qfalse;
}

// zyk: get the index of the Magic Power of this magic skill
int zyk_get_magic_index(int skill_index)
{
	if (zyk_is_magic_power_skill(skill_index) == qtrue)
	{
		return (skill_index - SKILL_MAGIC_MAGIC_SENSE);
	}

	return -1;
}

qboolean TryGrapple(gentity_t *ent)
{
	if (ent->client->ps.weaponTime > 0)
	{ //weapon busy
		return qfalse;
	}
	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //force power or knockdown or something
		return qfalse;
	}
	if (ent->client->grappleState)
	{ //already grappling? but weapontime should be > 0 then..
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER && ent->client->ps.weapon != WP_MELEE)
	{
		return qfalse;
	}

	if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
	{
		Cmd_ToggleSaber_f(ent);
		if (!ent->client->ps.saberHolstered)
		{ //must have saber holstered
			return qfalse;
		}
	}

	//G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_PA_1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	if (ent->client->ps.torsoAnim == BOTH_KYLE_GRAB)
	{ //providing the anim set succeeded..
		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		ent->client->dangerTime = level.time;
		
		return qtrue;
	}

	return qfalse;
}

void Cmd_TargetUse_f( gentity_t *ent )
{
	if ( trap->Argc() > 1 )
	{
		char sArg[MAX_STRING_CHARS] = {0};
		gentity_t *targ;

		trap->Argv( 1, sArg, sizeof( sArg ) );
		targ = G_Find( NULL, FOFS( targetname ), sArg );

		while ( targ )
		{
			if ( targ->use )
				targ->use( targ, ent, ent );
			targ = G_Find( targ, FOFS( targetname ), sArg );
		}
	}
}

void Cmd_TheDestroyer_f( gentity_t *ent ) {
	if ( !ent->client->ps.saberHolstered || ent->client->ps.weapon != WP_SABER )
		return;

	Cmd_ToggleSaber_f( ent );
}

void Cmd_BotMoveForward_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, arg, -1, -1 );
}

void Cmd_BotMoveBack_f( gentity_t *ent ) {
	int arg = -4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, arg, -1, -1 );
}

void Cmd_BotMoveRight_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, arg, -1 );
}

void Cmd_BotMoveLeft_f( gentity_t *ent ) {
	int arg = -4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, arg, -1 );
}

void Cmd_BotMoveUp_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, -1, arg );
}

void Cmd_AddBot_f( gentity_t *ent ) {
	//because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
	trap->SendServerCommand( ent-g_entities, va( "print \"%s.\n\"", G_GetStringEdString( "MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER" ) ) );
}

// zyk: new functions

// zyk: send the rpg events to the client-side game to all players so players who connect later than one already in the map
//      will receive the events of the one in the map
void send_rpg_events(int send_event_timer)
{
	int i = 0;
	gentity_t *player_ent = NULL;

	for (i = 0; i < level.maxclients; i++)
	{
		player_ent = &g_entities[i];

		if (player_ent && player_ent->client && player_ent->client->pers.connected == CON_CONNECTED && 
			player_ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{
			player_ent->client->pers.send_event_timer = level.time + send_event_timer;
			player_ent->client->pers.send_event_interval = level.time + 100;
			player_ent->client->pers.player_statuses &= ~(1 << 2);
			player_ent->client->pers.player_statuses &= ~(1 << 3);
		}
	}
}

// zyk: sets the Max HP a player can have in RPG Mode
void set_max_health(gentity_t *ent)
{
	ent->client->pers.max_rpg_health = 100 + (ent->client->pers.skill_levels[SKILL_MAX_HEALTH] * RPG_MAX_HEALTH_INCREASE);
}

// zyk: sets the Max Shield a player can have in RPG Mode
void set_max_shield(gentity_t *ent)
{
	if (ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR] > 0)
	{ // zyk: Shield Generator Upgrade. With it, player can have shield. Set it to the max shield possible
		ent->client->pers.max_rpg_shield = 100 + (zyk_max_skill_level(SKILL_MAX_HEALTH) * RPG_MAX_HEALTH_INCREASE);
	}
	else
	{
		ent->client->pers.max_rpg_shield = 0;
	}
}

void set_max_force(gentity_t* ent)
{
	ent->client->pers.max_force_power = (int)ceil((zyk_max_force_power.value / 4.0) * ent->client->pers.skill_levels[SKILL_FORCE_POWER]);
	ent->client->ps.fd.forcePowerMax = ent->client->pers.max_force_power;
}

// zyk: sets the Max Weight of stuff the player can carry
void set_max_weight(gentity_t* ent)
{
	ent->client->pers.max_weight = 200 + (ent->client->pers.skill_levels[SKILL_MAX_WEIGHT] * 120);
}

// zyk: set the Max Stamina of this player
void set_max_stamina(gentity_t* ent)
{
	ent->client->pers.max_stamina = RPG_DEFAULT_STAMINA + (ent->client->pers.skill_levels[SKILL_MAX_STAMINA] * RPG_DEFAULT_STAMINA);
}

// zyk: increases or decreases RPG player stamina
void zyk_set_stamina(gentity_t* ent, int amount, qboolean add)
{
	if (add == qtrue)
	{
		ent->client->pers.current_stamina += amount;

		if (ent->client->pers.current_stamina > ent->client->pers.max_stamina)
		{
			ent->client->pers.current_stamina = ent->client->pers.max_stamina;
		}
	}
	else
	{
		if (ent->client->pers.quest_power_status & (1 << MAGIC_HIT_BY_ENEMY_WEAKENING))
		{ // zyk: Enemy Weakening decreases Stamina faster
			amount *= 2;
		}

		if (ent->client->pers.stamina_out_timer <= level.time)
		{
			ent->client->pers.current_stamina -= amount;
		}
	}
}

// zyk: gives credits to the player
void add_credits(gentity_t *ent, int credits)
{
	ent->client->pers.credits += credits;
	if (ent->client->pers.credits > zyk_max_rpg_credits.integer)
		ent->client->pers.credits = zyk_max_rpg_credits.integer;
}

// zyk: removes credits from the player
void remove_credits(gentity_t *ent, int credits)
{
	ent->client->pers.credits -= credits;
	if (ent->client->pers.credits < 0)
		ent->client->pers.credits = 0;
}

// zyk: validates user input to avoid malicious input
qboolean zyk_check_user_input(char *user_input, int user_input_size)
{
	int i = 0;

	char allowed_chars[63] = {
		'A',
		'B',
		'C',
		'D',
		'E',
		'F',
		'G',
		'H',
		'I',
		'J',
		'K',
		'L',
		'M',
		'N',
		'O',
		'P',
		'Q',
		'R',
		'S',
		'T',
		'U',
		'V',
		'W',
		'X',
		'Y',
		'Z',
		'a',
		'b',
		'c',
		'd',
		'e',
		'f',
		'g',
		'h',
		'i',
		'j',
		'k',
		'l',
		'm',
		'n',
		'o',
		'p',
		'q',
		'r',
		's',
		't',
		'u',
		'v',
		'w',
		'x',
		'y',
		'z',
		'0',
		'1',
		'2',
		'3',
		'4',
		'5',
		'6',
		'7',
		'8',
		'9',
		'\0'
	};

	if (user_input_size > MAX_STRING_CHARS)
	{ // zyk: somehow the string is bigger than the max
		return qfalse;
	}

	if (user_input[0] == '\0')
	{ // zyk: empty string
		return qfalse;
	}

	while (user_input[i] != '\0' && i < user_input_size)
	{
		int j = 0;

		while (j < 63)
		{
			if (allowed_chars[j] == user_input[i])
			{ // zyk: char is an allowed one. Go to the next char
				break;
			}
			else if (allowed_chars[j] == '\0')
			{ // zyk: reached the NULL. It means the user input did not have any allowed char
				return qfalse;
			}

			j++;
		}

		i++;
	}

	if (user_input[i] != '\0' && i == user_input_size)
	{ // zyk: string did not terminate with NULL
		return qfalse;
	}

	return qtrue;
}

// zyk: initialize RPG skills of this player
void initialize_rpg_skills(gentity_t* ent, qboolean init_all)
{
	if (ent->client->sess.amrpgmode == 2)
	{
		int i = 0;

		// zyk: validating max skill levels. If for some reason a skill is above max, get the skillpoint back
		for (i = 0; i < NUMBER_OF_SKILLS; i++)
		{
			if (ent->client->pers.skill_levels[i] > zyk_max_skill_level(i))
			{
				ent->client->pers.skill_levels[i]--;
				ent->client->pers.skillpoints++;
			}
		}

		if (init_all == qtrue)
		{
			ent->client->pers.rpg_inventory_modified = qfalse;
		}

		// zyk: loading initial inventory
		ent->client->ps.stats[STAT_WEAPONS] = 0;
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_NONE);
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
		ent->client->ps.ammo[AMMO_BLASTER] = 0;
		ent->client->ps.ammo[AMMO_POWERCELL] = 0;
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = 0;
		ent->client->ps.ammo[AMMO_ROCKETS] = 0;
		ent->client->ps.ammo[AMMO_THERMAL] = 0;
		ent->client->ps.ammo[AMMO_TRIPMINE] = 0;
		ent->client->ps.ammo[AMMO_DETPACK] = 0;
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = 0;
		ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

		// zyk: settings the correct amount of stuff to the player based on the RPG inventory

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

		// zyk: flame thrower fuel and jetpack fuel
		ent->client->ps.cloakFuel = ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_FLAME_THROWER_FUEL];
		ent->client->ps.jetpackFuel = ent->client->pers.rpg_inventory[RPG_INVENTORY_MISC_JETPACK_FUEL];
		ent->client->pers.jetpack_fuel = ent->client->ps.jetpackFuel * JETPACK_SCALE;

		if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_THERMALS] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);

		if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_TRIPMINES] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);

		if (ent->client->pers.rpg_inventory[RPG_INVENTORY_AMMO_DETPACKS] > 0)
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

		// zyk: loading Jump value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)) && ent->client->pers.skill_levels[SKILL_JUMP] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LEVITATION);
		if (ent->client->pers.skill_levels[SKILL_JUMP] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LEVITATION);
		ent->client->ps.fd.forcePowerLevel[FP_LEVITATION] = ent->client->pers.skill_levels[SKILL_JUMP];

		// zyk: loading Push value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PUSH)) && ent->client->pers.skill_levels[SKILL_PUSH] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PUSH);
		if (ent->client->pers.skill_levels[SKILL_PUSH] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PUSH);
		ent->client->ps.fd.forcePowerLevel[FP_PUSH] = ent->client->pers.skill_levels[SKILL_PUSH];

		// zyk: loading Pull value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PULL)) && ent->client->pers.skill_levels[SKILL_PULL] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PULL);
		if (ent->client->pers.skill_levels[SKILL_PULL] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PULL);
		ent->client->ps.fd.forcePowerLevel[FP_PULL] = ent->client->pers.skill_levels[SKILL_PULL];

		// zyk: loading Speed value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SPEED)) && ent->client->pers.skill_levels[SKILL_SPEED] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SPEED);
		if (ent->client->pers.skill_levels[SKILL_SPEED] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SPEED);
		ent->client->ps.fd.forcePowerLevel[FP_SPEED] = ent->client->pers.skill_levels[SKILL_SPEED];

		// zyk: loading Sense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SEE)) && ent->client->pers.skill_levels[SKILL_SENSE] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
		if (ent->client->pers.skill_levels[SKILL_SENSE] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
		ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[SKILL_SENSE];

		// zyk: loading Saber Offense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_OFFENSE)) && ent->client->pers.skill_levels[SKILL_SABER_ATTACK] > 0)
		{
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_OFFENSE);
		}
		if (ent->client->pers.skill_levels[SKILL_SABER_ATTACK] == 0)
		{
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_OFFENSE);
		}
		ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = ent->client->pers.skill_levels[SKILL_SABER_ATTACK];

		// zyk: loading Saber Defense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_DEFENSE)) && ent->client->pers.skill_levels[SKILL_SABER_DEFENSE] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_DEFENSE);
		if (ent->client->pers.skill_levels[SKILL_SABER_DEFENSE] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_DEFENSE);
		ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = ent->client->pers.skill_levels[SKILL_SABER_DEFENSE];

		// zyk: loading Saber Throw value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABERTHROW)) && ent->client->pers.skill_levels[SKILL_SABER_THROW] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABERTHROW);
		if (ent->client->pers.skill_levels[SKILL_SABER_THROW] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABERTHROW);
		ent->client->ps.fd.forcePowerLevel[FP_SABERTHROW] = ent->client->pers.skill_levels[SKILL_SABER_THROW];

		// zyk: loading Absorb value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_ABSORB)) && ent->client->pers.skill_levels[SKILL_ABSORB] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_ABSORB);
		if (ent->client->pers.skill_levels[SKILL_ABSORB] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_ABSORB);

		if (ent->client->pers.skill_levels[SKILL_ABSORB] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = ent->client->pers.skill_levels[SKILL_ABSORB];
		else
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = FORCE_LEVEL_3;

		// zyk: loading Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_HEAL)) && ent->client->pers.skill_levels[SKILL_HEAL] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_HEAL);
		if (ent->client->pers.skill_levels[SKILL_HEAL] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_HEAL] = ent->client->pers.skill_levels[SKILL_HEAL];

		// zyk: loading Protect value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PROTECT)) && ent->client->pers.skill_levels[SKILL_PROTECT] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PROTECT);
		if (ent->client->pers.skill_levels[SKILL_PROTECT] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PROTECT);

		if (ent->client->pers.skill_levels[SKILL_PROTECT] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = ent->client->pers.skill_levels[SKILL_PROTECT];
		else
			ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = FORCE_LEVEL_3;

		// zyk: loading Mind Trick value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TELEPATHY)) && ent->client->pers.skill_levels[SKILL_MIND_TRICK] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TELEPATHY);
		if (ent->client->pers.skill_levels[SKILL_MIND_TRICK] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TELEPATHY);
		ent->client->ps.fd.forcePowerLevel[FP_TELEPATHY] = ent->client->pers.skill_levels[SKILL_MIND_TRICK];

		// zyk: loading Team Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) && ent->client->pers.skill_levels[SKILL_TEAM_HEAL] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_HEAL);
		if (ent->client->pers.skill_levels[SKILL_TEAM_HEAL] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] = ent->client->pers.skill_levels[SKILL_TEAM_HEAL];

		// zyk: loading Lightning value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LIGHTNING)) && ent->client->pers.skill_levels[SKILL_LIGHTNING] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		if (ent->client->pers.skill_levels[SKILL_LIGHTNING] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LIGHTNING);

		if (ent->client->pers.skill_levels[SKILL_LIGHTNING] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = ent->client->pers.skill_levels[SKILL_LIGHTNING];
		else
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = FORCE_LEVEL_3;

		// zyk: loading Grip value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_GRIP)) && ent->client->pers.skill_levels[SKILL_GRIP] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_GRIP);
		if (ent->client->pers.skill_levels[SKILL_GRIP] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_GRIP);
		ent->client->ps.fd.forcePowerLevel[FP_GRIP] = ent->client->pers.skill_levels[SKILL_GRIP];

		// zyk: loading Drain value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_DRAIN)) && ent->client->pers.skill_levels[SKILL_DRAIN] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_DRAIN);
		if (ent->client->pers.skill_levels[SKILL_DRAIN] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_DRAIN);

		if (ent->client->pers.skill_levels[SKILL_DRAIN] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_DRAIN] = ent->client->pers.skill_levels[SKILL_DRAIN];
		else
			ent->client->ps.fd.forcePowerLevel[FP_DRAIN] = FORCE_LEVEL_3;

		// zyk: loading Rage value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_RAGE)) && ent->client->pers.skill_levels[SKILL_RAGE] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_RAGE);
		if (ent->client->pers.skill_levels[SKILL_RAGE] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_RAGE);

		if (ent->client->pers.skill_levels[SKILL_RAGE] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_RAGE] = ent->client->pers.skill_levels[SKILL_RAGE];
		else
			ent->client->ps.fd.forcePowerLevel[FP_RAGE] = FORCE_LEVEL_3;

		// zyk: loading Team Energize value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) && ent->client->pers.skill_levels[SKILL_TEAM_ENERGIZE] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_FORCE);
		if (ent->client->pers.skill_levels[SKILL_TEAM_ENERGIZE] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_FORCE);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] = ent->client->pers.skill_levels[SKILL_TEAM_ENERGIZE];

		set_max_health(ent);
		set_max_shield(ent);
		set_max_force(ent);
		set_max_weight(ent);
		set_max_stamina(ent);

		// zyk: setting rpg control attributes

		if (ent->client->sess.magic_fist_selection > ent->client->pers.skill_levels[SKILL_MAGIC_FIST])
		{ // zyk: reset magic fist selected if player downgrades Magic Fist skill
			ent->client->sess.magic_fist_selection = 0;
		}

		if (init_all == qtrue)
		{
			zyk_load_common_settings(ent);

			ent->client->pers.sense_health_timer = 0;

			ent->client->pers.thermal_vision = qfalse;
			ent->client->pers.thermal_vision_cooldown_time = 0;

			ent->client->pers.quest_power_status = 0;

			// zyk: used to add a cooldown between each flame
			ent->client->cloakDebReduce = 0;

			ent->client->pers.energy_modulator_mode = 0;

			ent->client->pers.credits_modifier = 0;
			ent->client->pers.score_modifier = 0;

			ent->client->pers.buy_sell_timer = 0;

			ent->client->pers.current_quest_event = 0;
			ent->client->pers.quest_event_timer = 0;

			ent->client->pers.stamina_timer = 0;
			ent->client->pers.stamina_out_timer = 0;

			// zyk: loading initial force
			ent->client->ps.fd.forcePower = ent->client->pers.max_force_power;

			// zyk: loading last MP
			ent->client->pers.magic_power = ent->client->pers.last_mp;

			// zyk: loading last shield
			ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.last_shield;

			if (!(ent->client->pers.player_statuses & (1 << 24)) && 
				ent->client->pers.last_health <= 0)
			{ // zyk: reload player stats if he died and he did not use /kill command
				// zyk: loading initial health
				ent->health = ent->client->pers.max_rpg_health;
				ent->client->ps.stats[STAT_HEALTH] = ent->health;

				// zyk: loading initial Stamina as the last value plus the minimum Stamina
				ent->client->pers.current_stamina = ent->client->pers.last_stamina + RPG_MIN_STAMINA;
			}
			else
			{ // zyk: reload the last stats
				ent->health = ent->client->pers.last_health;
				ent->client->ps.stats[STAT_HEALTH] = ent->health;

				ent->client->pers.current_stamina = ent->client->pers.last_stamina;
			}
		}
		else
		{ // it is possible to have above max of these if player downgrades these skills
			if (ent->health > ent->client->pers.max_rpg_health)
			{
				ent->health = ent->client->pers.max_rpg_health;
				ent->client->ps.stats[STAT_HEALTH] = ent->health;
			}

			if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield)
			{
				ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
			}

			if (ent->client->ps.fd.forcePower > ent->client->pers.max_force_power)
			{
				ent->client->ps.fd.forcePower = ent->client->pers.max_force_power;
			}

			if (ent->client->pers.magic_power > zyk_max_magic_power(ent))
			{
				ent->client->pers.magic_power = zyk_max_magic_power(ent);
			}

			if (ent->client->pers.current_stamina > ent->client->pers.max_stamina)
			{
				ent->client->pers.current_stamina = ent->client->pers.max_stamina;
			}
		}

		// zyk: update the rpg stuff info at the client-side game
		send_rpg_events(10000);
	}
}

// zyk: gives rpg score to the player
void rpg_score(gentity_t *ent)
{
	int send_message = 0; // zyk: if its 1, sends the message in player console
	char message[128];

	strcpy(message,"");

	add_credits(ent, (10 + ent->client->pers.credits_modifier));

	if (ent->client->pers.level < zyk_rpg_max_level.integer)
	{
		ent->client->pers.level_up_score += (1 + ent->client->pers.score_modifier); // zyk: add score to the RPG mode score

		if (ent->client->pers.level_up_score >= (ent->client->pers.level * zyk_level_up_score_factor.integer))
		{ // zyk: player got a new level
			ent->client->pers.level_up_score -= (ent->client->pers.level * zyk_level_up_score_factor.integer);
			ent->client->pers.level++;

			if (ent->client->pers.level % 5 == 0) // zyk: every level divisible by 5 gives bonus skillpoints
				ent->client->pers.skillpoints += 3;
			else
				ent->client->pers.skillpoints++;

			initialize_rpg_skills(ent, qfalse);

			strcpy(message,va("^3New Level: ^7%d^3, Skillpoints: ^7%d\n", ent->client->pers.level, ent->client->pers.skillpoints));

			send_message = 1;
		}
	}

	save_account(ent, qtrue); // zyk: saves new score and credits in the account file

	// zyk: cleaning the modifiers after they are applied
	ent->client->pers.credits_modifier = 0;
	ent->client->pers.score_modifier = 0;

	if (send_message == 1)
	{
		trap->SendServerCommand( ent->s.number, va("chat \"%s\"", message));
	}
}

// zyk: increases the RPG skill counter by this amount
void rpg_skill_counter(gentity_t *ent, int amount)
{
	if (ent && ent->client && ent->client->sess.amrpgmode == 2)
	{ // zyk: now RPG mode increases level up score after a certain amount of attacks
		// zyk: when player does things, it will decrease Stamina
		zyk_set_stamina(ent, amount, qfalse);

		if (ent->client->pers.level < zyk_rpg_max_level.integer)
		{
			ent->client->pers.skill_counter += amount;

			if (ent->client->pers.skill_counter >= zyk_max_skill_counter.integer)
			{
				ent->client->pers.skill_counter = 0;

				// zyk: skill counter does not give credits, only Level Up Score
				ent->client->pers.credits_modifier = -10;

				rpg_score(ent);
			}
		}
	}
}

void zyk_update_inventory_quantity(gentity_t* ent, qboolean add_item, zyk_inventory_t item)
{
	if (add_item == qtrue)
	{
		ent->client->pers.rpg_inventory[item]++;
	}
	else
	{
		ent->client->pers.rpg_inventory[item]--;

		if (ent->client->pers.rpg_inventory[item] > 0)
		{ // zyk: set the weapon or item if player still has it
			if (item == RPG_INVENTORY_WP_STUN_BATON)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);

			if (item == RPG_INVENTORY_WP_SABER)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

			if (item == RPG_INVENTORY_WP_BLASTER_PISTOL)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

			if (item == RPG_INVENTORY_WP_E11_BLASTER_RIFLE)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);

			if (item == RPG_INVENTORY_WP_DISRUPTOR)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);

			if (item == RPG_INVENTORY_WP_BOWCASTER)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);

			if (item == RPG_INVENTORY_WP_REPEATER)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);

			if (item == RPG_INVENTORY_WP_DEMP2)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);

			if (item == RPG_INVENTORY_WP_FLECHETTE)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);

			if (item == RPG_INVENTORY_WP_ROCKET_LAUNCHER)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);

			if (item == RPG_INVENTORY_WP_CONCUSSION)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);

			if (item == RPG_INVENTORY_WP_BRYAR_PISTOL)
				ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);

			if (item == RPG_INVENTORY_ITEM_BINOCULARS)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);

			if (item == RPG_INVENTORY_ITEM_BACTA_CANISTER)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);

			if (item == RPG_INVENTORY_ITEM_SENTRY_GUN)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

			if (item == RPG_INVENTORY_ITEM_SEEKER_DRONE)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);

			if (item == RPG_INVENTORY_ITEM_EWEB)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);

			if (item == RPG_INVENTORY_ITEM_BIG_BACTA)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);

			if (item == RPG_INVENTORY_ITEM_FORCE_FIELD)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);

			if (item == RPG_INVENTORY_ITEM_CLOAK)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);

			if (item == RPG_INVENTORY_ITEM_JETPACK)
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

		}
	}

	ent->client->pers.rpg_inventory_modified = qtrue;
}

/*
==================
Cmd_DateTime_f
==================
*/
void Cmd_DateTime_f( gentity_t *ent ) {
	time_t current_time;

	time(&current_time);
	// zyk: shows current server date and time
	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", ctime(&current_time)) ); 
}

// zyk: adds a new RPG char with default values
void add_new_char(gentity_t *ent)
{
	int i = 0;

	ent->client->pers.level_up_score = 0;
	ent->client->pers.level = 1;
	ent->client->pers.skillpoints = 1;

	for (i = 0; i < NUMBER_OF_SKILLS; i++)
	{
		ent->client->pers.skill_levels[i] = 0;
	}

	// zyk: initializing RPG inventory
	for (i = 0; i < MAX_RPG_INVENTORY_ITEMS; i++)
	{
		ent->client->pers.rpg_inventory[i] = 0;
	}

	ent->client->pers.credits = RPG_INITIAL_CREDITS;
	ent->client->sess.magic_fist_selection = 0;

	// zyk: in RPG Mode, player must actually buy these
	ent->client->ps.jetpackFuel = 0;
	ent->client->ps.cloakFuel = 0;
	ent->client->pers.jetpack_fuel = 0;

	// zyk: so the char starts with the original health
	ent->client->pers.last_health = 100;
	ent->client->pers.current_stamina = RPG_DEFAULT_STAMINA;
	ent->client->pers.last_stamina = RPG_DEFAULT_STAMINA;
}

// zyk: creates the directory correctly depending on the OS
void zyk_create_dir(char *file_path)
{
#if defined(__linux__)
	system(va("mkdir -p zykmod/%s", file_path));
#else
	system(va("mkdir \"zykmod/%s\"", file_path));
#endif
}

/*
==================
Cmd_NewAccount_f
==================
*/
void Cmd_NewAccount_f( gentity_t *ent ) {
	FILE *logins_file;
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	char content[1024];
	int i = 0;

	strcpy(content,"");

	if ( trap->Argc() != 3) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must write a login and a password of your choice. Example: ^3/new yourlogin yourpass^7.\n\"" ); 
		return;
	}
	trap->Argv(1, arg1, sizeof( arg1 ));
	trap->Argv(2, arg2, sizeof( arg2 ));

	// zyk: creates the account if player is not logged in
	if (ent->client->sess.amrpgmode != 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You are already logged in.\n\"" ); 
		return;
	}

	if (strlen(arg1) > MAX_ACC_NAME_SIZE)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Login has a maximum of %d characters.\n\"", MAX_ACC_NAME_SIZE) );
		return;
	}
	if (strlen(arg2) > MAX_ACC_NAME_SIZE)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Password has a maximum of %d characters.\n\"", MAX_ACC_NAME_SIZE) );
		return;
	}

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid login. Only letters and numbers allowed.\n\"");
		return;
	}

	// zyk: validating if this login already exists
	zyk_create_dir("accounts");

#if defined(__linux__)
	system("ls zykmod/accounts > zykmod/accounts/accounts.txt");
#else
	system("dir /B \"zykmod/accounts\" > zykmod/accounts/accounts.txt");
#endif

	logins_file = fopen("zykmod/accounts/accounts.txt","r");
	if (logins_file != NULL)
	{
		i = fscanf(logins_file, "%s", content);
		while (i != -1)
		{
			if (Q_stricmp(content,"accounts.txt") != 0)
			{ // zyk: validating login, which is the file name
				if (Q_stricmp( content, va("%s.txt",arg1) ) == 0)
				{ // zyk: if this login is the same as the one passed in arg1, then it already exists
					fclose(logins_file);
					trap->SendServerCommand( ent-g_entities, "print \"Login is used by another player.\n\"" );
					return;
				}
			}
			i = fscanf(logins_file, "%s", content);
		}
		fclose(logins_file);
	}

	strcpy(ent->client->sess.filename, arg1);

	password_encrypt(arg2, 0xFACE);
	strcpy(ent->client->pers.password, arg2);

	// zyk: setting the values to be saved in the account file
	if (zyk_allow_rpg_mode.integer == 0)
	{
		ent->client->sess.amrpgmode = 1;
	}
	else
	{
		ent->client->sess.amrpgmode = 2;
	}

	ent->client->pers.player_settings = 0;
	ent->client->pers.bitvalue = 0;

	add_new_char(ent);

	// zyk: saving the default char
	strcpy(ent->client->sess.rpgchar, arg1);

	save_account(ent, qfalse);
	save_account(ent, qtrue);

	if (ent->client->sess.amrpgmode == 2)
	{
		initialize_rpg_skills(ent, qtrue);
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Account created successfully in ^2Admin-Only ^7Mode\n\"");
	}

	// zyk: starting the tutorial, to help players use the mod features
	ent->client->pers.tutorial_step = 0;
	ent->client->pers.tutorial_timer = level.time + 1000;
	ent->client->pers.player_statuses |= (1 << 25);
}

/*
==================
Cmd_LoginAccount_f
==================
*/
void Cmd_LoginAccount_f( gentity_t *ent ) {
	if (ent->client->sess.amrpgmode == 0)
	{
		char arg1[MAX_STRING_CHARS];
		char arg2[MAX_STRING_CHARS];
		char password[32];
		int i = 0;
		FILE *account_file;
		gentity_t *player_ent = NULL;

		strcpy(password,"");

		if ( trap->Argc() != 3)
		{ 
			trap->SendServerCommand( ent->s.number, "print \"You must write your login and password.\n\"" );
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		trap->Argv(2, arg2, sizeof( arg2 ));

		if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
		{
			trap->SendServerCommand(ent->s.number, "print \"Invalid login. Only letters and numbers allowed.\n\"");
			return;
		}

		for (i = 0; i < level.maxclients; i++)
		{
			player_ent = &g_entities[i];
			if (player_ent && player_ent->client && player_ent->client->sess.amrpgmode > 0 && Q_stricmp(player_ent->client->sess.filename,arg1) == 0)
			{
				trap->SendServerCommand( ent->s.number, "print \"There is already someone logged in this account.\n\"" );
				return;
			}
		}

		// zyk: cannot login if player is in Duel Tournament or Sniper Battle
		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{
			trap->SendServerCommand(ent->s.number, "print \"Cannot login while in a Duel Tournament\n\"");
			return;
		}

		if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
		{
			trap->SendServerCommand(ent->s.number, "print \"Cannot login while in a Sniper Battle\n\"");
			return;
		}

		// zyk: validating login
		account_file = fopen(va("zykmod/accounts/%s.txt",arg1),"r");
		if (account_file == NULL)
		{
			trap->SendServerCommand( ent->s.number, "print \"Login does not exist.\n\"" );
			return;
		}

		// zyk: validating password
		fscanf(account_file,"%s",password);
		fclose(account_file);

		password_encrypt(arg2, 0xFACE);

		if (strlen(password) != strlen(arg2) || Q_strncmp(password, arg2, strlen(password)) != 0)
		{
			trap->SendServerCommand( ent->s.number, "print \"The password is incorrect.\n\"" );
			return;
		}

		// zyk: valid login and password
		strcpy(ent->client->sess.filename, arg1);
		strcpy(ent->client->pers.password, arg2);

		load_account(ent);

		if (ent->client->sess.amrpgmode == 1)
		{
			trap->SendServerCommand(ent->s.number, "print \"^7Account loaded succesfully in ^2Admin-Only Mode^7. Use command ^3/list^7.\n\"");
		}
		else if (ent->client->sess.amrpgmode == 2)
		{
			initialize_rpg_skills(ent, qtrue);
			trap->SendServerCommand( ent->s.number, "print \"^7Account loaded succesfully in ^2RPG Mode^7. Use command ^3/list^7.\n\"" );

			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
				if (ent && ent->client)
				{
					ent->client->pers.player_statuses |= (1 << 24);
				}

				G_Kill(ent);
			}
		}
	}
	else
	{
		trap->SendServerCommand( ent->s.number, "print \"You are already logged in.\n\"" );
	}
}

extern void zyk_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_spawn_entity(gentity_t *ent);
extern void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_main_spawn_entity(gentity_t *ent);

// zyk: tests if the race must be finished
void try_finishing_race()
{
	int j = 0, has_someone_racing = 0;
	gentity_t *this_ent = NULL;

	if (level.race_mode != 0)
	{
		for (j = 0; j < level.maxclients; j++)
		{ 
			this_ent = &g_entities[j];
			if (this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->pers.race_position > 0)
			{ // zyk: searches for the players who are still racing to see if we must finish the race
				has_someone_racing = 1;
			}
		}

		if (has_someone_racing == 0)
		{ // zyk: no one is racing, so finish the race
			level.race_mode = 0;

			for (j = MAX_CLIENTS; j < level.num_entities; j++)
			{
				this_ent = &g_entities[j];
				if (this_ent && Q_stricmp(this_ent->targetname, "zyk_race_line") == 0)
				{ // zyk: removes this start or finish line
					G_FreeEntity(this_ent);
				}
			}

			trap->SendServerCommand( -1, va("chat \"^3Race System: ^7The race is over!\""));
		}
	}
}

/*
==================
Cmd_LogoutAccount_f
==================
*/
void Cmd_LogoutAccount_f( gentity_t *ent ) {
	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a Duel Tournament\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a Sniper Battle\n\"");
		return;
	}

	// zyk: saving the not logged player mode in session
	ent->client->sess.amrpgmode = 0;

	ent->client->pers.bitvalue = 0;

	// zyk: resetting the forcePowerMax to the cvar value
	ent->client->ps.fd.forcePowerMax = zyk_max_force_power.integer;

	// zyk: resetting max hp and shield to 100
	ent->client->ps.stats[STAT_MAX_HEALTH] = 100;

	if (ent->health > 100)
		ent->health = 100;

	if (ent->client->ps.stats[STAT_ARMOR] > 100)
		ent->client->ps.stats[STAT_ARMOR] = 100;

	// zyk: resetting force powers
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0 &&
		level.gametype != GT_JEDIMASTER && level.gametype != GT_SIEGE
		)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	if (level.gametype != GT_JEDIMASTER && level.gametype != GT_SIEGE)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

	// zyk: update the rpg stuff info at the client-side game
	send_rpg_events(10000);
			
	trap->SendServerCommand( ent-g_entities, "print \"Account logout finished succesfully.\n\"" );
}

qboolean rpg_upgrade_skill(gentity_t *ent, int upgrade_value, qboolean dont_show_message)
{
	if (ent->client->pers.skill_levels[upgrade_value - 1] < zyk_max_skill_level(upgrade_value - 1))
	{
		ent->client->pers.skill_levels[upgrade_value - 1]++;
		ent->client->pers.skillpoints--;
	}
	else
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, va("print \"You reached the maximum level of ^3%s ^7skill.\n\"", zyk_skill_name(upgrade_value - 1)));
		return qfalse;
	}

	return qtrue;
}

char *zyk_get_settings_values(gentity_t *ent)
{
	int i = 0;
	char content[1024];

	strcpy(content,"");

	for (i = 0; i < MAX_PLAYER_SETTINGS; i++)
	{ // zyk: settings values
		if (!(ent->client->pers.player_settings & (1 << i)))
		{
			strcpy(content,va("%sON-",content));
		}
		else
		{
			strcpy(content,va("%sOFF-",content));
		}
	}

	return G_NewString(content);
}

/*
==================
Cmd_ZykMod_f
==================
*/
// zyk: sends info to the client-side menu if player has the client-side plugin
void Cmd_ZykMod_f( gentity_t *ent ) {

	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{
		int i = 0;
		char content[1024];

		strcpy(content,"");

		for (i = 0; i < NUMBER_OF_SKILLS; i++)
		{
			strcpy(content, va("%s%d/%d-", content, ent->client->pers.skill_levels[i], zyk_max_skill_level(i)));
		}

		strcpy(content, va("%s%s", content, zyk_get_settings_values(ent)));

		strcpy(content, va("%s%d-%d-%d-%d-", 
			content, 0, ent->client->pers.main_quest_progress, ent->client->pers.side_quest_progress, MAX_QUEST_MISSIONS));

		trap->SendServerCommand(ent->s.number, va("zykmod \"%d/%d-%d/%d-%d-%d/%d-%d/%d-%d-NOCLASS-%s\"",ent->client->pers.level, zyk_rpg_max_level.integer,ent->client->pers.level_up_score,(ent->client->pers.level * zyk_level_up_score_factor.integer),ent->client->pers.skillpoints,ent->client->pers.skill_counter,zyk_max_skill_counter.integer,ent->client->pers.magic_power,zyk_max_magic_power(ent),ent->client->pers.credits,content));
	}
	else if (ent->client->sess.amrpgmode == 1)
	{ // zyk: just sends the player settings
		int i = 0;
		char content[1024];

		strcpy(content,"");

		for (i = 0; i < 103; i++)
		{
			if (i == 97)
			{
				strcpy(content, va("%s%s", content, zyk_get_settings_values(ent)));
			}
			else
			{
				strcpy(content, va("%s0-", content));
			}
				
		}

		trap->SendServerCommand(ent->s.number, va("zykmod \"%s\"", content));
	}
}

char *zyk_get_rpg_chars(gentity_t *ent, char *separator)
{
	FILE *chars_file;
	char content[64];
	char chars[MAX_STRING_CHARS];
	int i = 0;

	strcpy(content, "");
	strcpy(chars, "");

#if defined(__linux__)
	system(va("cd zykmod/accounts ; ls %s_* > chars_%d.txt", ent->client->sess.filename, ent->s.number));
#else
	system(va("cd \"zykmod/accounts\" & dir /B %s_* > chars_%d.txt", ent->client->sess.filename, ent->s.number));
#endif

	chars_file = fopen(va("zykmod/accounts/chars_%d.txt", ent->s.number), "r");
	if (chars_file != NULL)
	{
		i = fscanf(chars_file, "%s", content);
		while (i != EOF)
		{
			if (Q_stricmp(content, va("chars_%d.txt", ent->s.number)) != 0)
			{ // zyk: getting the char names
				int j = strlen(ent->client->sess.filename) + 1, k = 0;

				while (j < 64)
				{
					if (content[j] == '.' && content[j + 1] == 't' && content[j + 2] == 'x' && content[j + 3] == 't')
					{
						content[k] = '\0';
						break;
					}
					else
					{
						content[k] = content[j];
					}

					j++;
					k++;
				}

				strcpy(chars, va("%s^7%s%s", chars, content, separator));
			}
			i = fscanf(chars_file, "%s", content);
		}
		fclose(chars_file);
	}

	return G_NewString(chars);
}

/*
==================
Cmd_ZykChars_f
==================
*/
void Cmd_ZykChars_f(gentity_t *ent) {
	// zyk: sends info to the client-side menu if player has the client-side plugin
	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	trap->SendServerCommand(ent->s.number, va("zykchars \"%s^7%s<zykc>\"", zyk_get_rpg_chars(ent, "<zyk>"), ent->client->sess.rpgchar));
}

extern zyk_magic_element_t zyk_get_magic_element(int magic_number);
extern zyk_main_quest_t zyk_get_magic_spirit(zyk_magic_element_t magic_element);
qboolean validate_upgrade_skill(gentity_t *ent, int upgrade_value, qboolean dont_show_message)
{
	// zyk: validation on the upgrade level, which must be in the range of valid skills.
	if (upgrade_value < 1 || upgrade_value > NUMBER_OF_SKILLS)
	{
		trap->SendServerCommand( ent->s.number, "print \"Invalid skill number.\n\"" );
		return qfalse;
	}

	// zyk: the user must have skillpoints to get a new skill level
	if (ent->client->pers.skillpoints == 0)
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"Not enough skillpoints.\n\"" );
		return qfalse;
	}

	/*
	// zyk: each magic requires a certain Elemental Spirit to be upgraded to a level > 1
	if (upgrade_value > (NUMBER_OF_SKILLS - MAX_MAGIC_POWERS))
	{
		int magic_skill_index = (upgrade_value - 1) - (NUMBER_OF_SKILLS - MAX_MAGIC_POWERS);
		zyk_magic_element_t skill_element = zyk_get_magic_element(magic_skill_index);
		zyk_main_quest_t magic_spirit = zyk_get_magic_spirit(skill_element);

		if (ent->client->pers.skill_levels[upgrade_value - 1] > 0 && !(ent->client->pers.main_quest_progress & (1 << magic_spirit)))
		{ // zyk: if player is trying to upgrade the Magic skill again, he must have the corresponding Elemental Spirit
			if (dont_show_message == qfalse)
				trap->SendServerCommand(ent->s.number, va("print \"You don't have the %s.\n\"", zyk_get_spirit_name(magic_spirit)));
			return qfalse;
		}
	}
	*/

	return qtrue;
}

void do_upgrade_skill(gentity_t *ent, int upgrade_value, qboolean update_all)
{
	if (update_all == qfalse)
	{ // zyk: update a single skill
		qboolean is_upgraded = qfalse;

		if (validate_upgrade_skill(ent, upgrade_value, qfalse) == qfalse)
		{
			return;
		}

		// zyk: the upgrade is done if it doesnt go above the maximum level of the skill
		is_upgraded = rpg_upgrade_skill(ent, upgrade_value, qfalse);

		if (is_upgraded == qfalse)
			return;

		// zyk: saving the account file with the upgraded skill
		save_account(ent, qtrue);

		trap->SendServerCommand( ent-g_entities, "print \"Skill upgraded successfully.\n\"" );

		Cmd_ZykMod_f(ent);
	}
	else
	{ // zyk: update all skills
		int i = 0;

		for (i = 1; i <= NUMBER_OF_SKILLS; i++)
		{
			int j = 0;

			for (j = 0; j < 5; j++)
			{
				if (validate_upgrade_skill(ent, i, qtrue) == qtrue)
				{
					// zyk: the upgrade is done if it doesnt go above the maximum level of the skill
					rpg_upgrade_skill(ent, i, qtrue);
				}
			}
		}

		// zyk: saving the account file with the upgraded skill
		save_account(ent, qtrue);

		trap->SendServerCommand( ent-g_entities, "print \"Skills upgraded successfully.\n\"" );
		Cmd_ZykMod_f(ent);
	}

	initialize_rpg_skills(ent, qfalse);
}

/*
==================
Cmd_UpSkill_f
==================
*/
void Cmd_UpSkill_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS]; // zyk: value the user sends as an arg which is the skill to be upgraded
	int upgrade_value; // zyk: the integer value of arg1
		    
	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent->s.number, "print \"You must specify the number of the skill to be upgraded.\n\"" ); 
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	upgrade_value = atoi(arg1);

	if (Q_stricmp(arg1, "all") == 0)
	{ // zyk: upgrade all skills of this class
		do_upgrade_skill(ent, 0, qtrue);
	}
	else
	{
		do_upgrade_skill(ent, upgrade_value, qfalse);
	}
}

void do_downgrade_skill(gentity_t *ent, int downgrade_value)
{
	// zyk: validation on the downgrade level, which must be in the range of valid skills.
	if (downgrade_value < 1 || downgrade_value > NUMBER_OF_SKILLS)
	{
		trap->SendServerCommand( ent->s.number, "print \"Invalid skill number.\n\"" );
		return;
	}

	if (ent->client->pers.skill_levels[downgrade_value - 1] > 0)
	{
		ent->client->pers.skill_levels[downgrade_value - 1]--;
		ent->client->pers.skillpoints++;
	}
	else
	{
		trap->SendServerCommand( ent->s.number, va("print \"You reached the minimum level of ^3%s ^7skill.\n\"", zyk_skill_name(downgrade_value - 1)));
		return;
	}

	// zyk: saving the account file with the downgraded skill
	save_account(ent, qtrue);

	trap->SendServerCommand( ent->s.number, "print \"Skill downgraded successfully.\n\"" );

	initialize_rpg_skills(ent, qfalse);

	Cmd_ZykMod_f(ent);
}

/*
==================
Cmd_DownSkill_f
==================
*/
void Cmd_DownSkill_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS]; // zyk: value the user sends as an arg which is the skill to be downgraded
	int downgrade_value; // zyk: the integer value of arg1

	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent->s.number, "print \"You must specify the number of the skill to be downgraded.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	downgrade_value = atoi(arg1);

	do_downgrade_skill(ent, downgrade_value);
}

// zyk: used to format text when player wants to list skills
char* zyk_add_whitespaces(int skill_index, int biggest_skill_name_length)
{
	char result_text[MAX_STRING_CHARS];
	int skill_name_length = strlen(zyk_skill_name(skill_index));
	int i = 0;

	strcpy(result_text, "");

	for (i = 0; i < (biggest_skill_name_length - skill_name_length); i++)
	{
		// zyk: adds a whitespace at the end of the current string
		strcpy(result_text, va("%s ", result_text));
	}

	return G_NewString(result_text);
}

// zyk: lists skills from a specific category
void zyk_list_category_skills(gentity_t* ent, gentity_t* target_ent, char *skill_color, int lowest_skill_index, int highest_skill_index, int number_of_whitespaces)
{
	char message[1024];
	char final_chars[32];
	int i = 0;
	int current_skill_index = lowest_skill_index;
	int skill_count = (highest_skill_index - lowest_skill_index) + 1;

	strcpy(message, "");
	strcpy(final_chars, "");

	while (i < skill_count)
	{
		if ((i % 2) == 0)
		{ // zyk: adds whitespaces in first column
			strcpy(final_chars, va("%s ", zyk_add_whitespaces(current_skill_index, number_of_whitespaces)));
		}
		else
		{
			strcpy(final_chars, "\n");
		}

		strcpy(message, va("%s%s%d - %s: %d/%d%s", message,
			skill_color, (current_skill_index + 1), zyk_skill_name(current_skill_index),
			ent->client->pers.skill_levels[current_skill_index], zyk_max_skill_level(current_skill_index), final_chars));

		current_skill_index++;
		i++;
	}

	if ((i % 2) != 0)
	{ // zyk: if this category has an odd number of skills, add a final line break
		strcpy(message, va("%s\n", message));
	}

	trap->SendServerCommand(target_ent->s.number, va("print \"%s\"", message));
}

void zyk_list_player_skills(gentity_t *ent, gentity_t *target_ent, char *arg1)
{
	if (Q_stricmp( arg1, "force" ) == 0)
	{
		zyk_list_category_skills(ent, target_ent, "^5", SKILL_JUMP, SKILL_FORCE_POWER, 18);
	}
	else if (Q_stricmp( arg1, "weapons" ) == 0)
	{
		zyk_list_category_skills(ent, target_ent, "^3", SKILL_STUN_BATON, SKILL_MELEE, 17);
	}
	else if (Q_stricmp( arg1, "other" ) == 0)
	{
		zyk_list_category_skills(ent, target_ent, "^7", SKILL_MAX_HEALTH, SKILL_RUN_SPEED, 15);
	}
	else if (Q_stricmp(arg1, "magic") == 0)
	{
		zyk_list_category_skills(ent, target_ent, "^2", SKILL_MAGIC_FIST, SKILL_MAGIC_LIGHT_MAGIC, 18);
	}
}

void list_rpg_info(gentity_t *ent, gentity_t *target_ent)
{ // zyk: lists general RPG info of this player
	trap->SendServerCommand(target_ent->s.number, va("print \"\n^2Account: ^7%s\n^2Char: ^7%s\n\n^3Level: ^7%d/%d\n^3Level Up Score: ^7%d/%d\n^3Skill Points: ^7%d\n^3Action Counter: ^7%d/%d\n^3Magic Points: ^7%d/%d\n^3Weight: ^7%d/%d\n^3Stamina: ^7%d/%d\n^3Credits: ^7%d\n\n^7Use ^2/list rpg ^7to see console commands\n\n\"", 
		ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->pers.level, zyk_rpg_max_level.integer, 
		ent->client->pers.level_up_score, (ent->client->pers.level * zyk_level_up_score_factor.integer), ent->client->pers.skillpoints, 
		ent->client->pers.skill_counter, zyk_max_skill_counter.integer, ent->client->pers.magic_power, zyk_max_magic_power(ent), 
		ent->client->pers.current_weight, ent->client->pers.max_weight, ent->client->pers.current_stamina, ent->client->pers.max_stamina, ent->client->pers.credits));
}

char* zyk_get_inventory_item_name(int inventory_index)
{
	char* inventory_item_names[MAX_RPG_INVENTORY_ITEMS];

	inventory_item_names[RPG_INVENTORY_WP_STUN_BATON] = "Stun Baton";
	inventory_item_names[RPG_INVENTORY_WP_SABER] = "Saber";
	inventory_item_names[RPG_INVENTORY_WP_BLASTER_PISTOL] = "Blaster Pistol";
	inventory_item_names[RPG_INVENTORY_WP_E11_BLASTER_RIFLE] = "E11 Blaster Rifle";
	inventory_item_names[RPG_INVENTORY_WP_DISRUPTOR] = "Disruptor";
	inventory_item_names[RPG_INVENTORY_WP_BOWCASTER] = "Bowcaster";
	inventory_item_names[RPG_INVENTORY_WP_REPEATER] = "Repeater";
	inventory_item_names[RPG_INVENTORY_WP_DEMP2] = "DEMP2";
	inventory_item_names[RPG_INVENTORY_WP_FLECHETTE] = "Flechette";
	inventory_item_names[RPG_INVENTORY_WP_ROCKET_LAUNCHER] = "Rocket Launcher";
	inventory_item_names[RPG_INVENTORY_WP_CONCUSSION] = "Concussion";
	inventory_item_names[RPG_INVENTORY_WP_BRYAR_PISTOL] = "Bryar Pistol";

	inventory_item_names[RPG_INVENTORY_AMMO_BLASTER_PACK] = "Blaster Pack Ammo";
	inventory_item_names[RPG_INVENTORY_AMMO_POWERCELL] = "Powercell Ammo";
	inventory_item_names[RPG_INVENTORY_AMMO_METAL_BOLTS] = "Metal Bolts Ammo";
	inventory_item_names[RPG_INVENTORY_AMMO_ROCKETS] = "Rockets Ammo";
	inventory_item_names[RPG_INVENTORY_AMMO_THERMALS] = "Thermals";
	inventory_item_names[RPG_INVENTORY_AMMO_TRIPMINES] = "Trip Mines";
	inventory_item_names[RPG_INVENTORY_AMMO_DETPACKS] = "Detpacks";

	inventory_item_names[RPG_INVENTORY_ITEM_BINOCULARS] = "Binoculars";
	inventory_item_names[RPG_INVENTORY_ITEM_BACTA_CANISTER] = "Bacta Canister";
	inventory_item_names[RPG_INVENTORY_ITEM_SENTRY_GUN] = "Sentry Gun";
	inventory_item_names[RPG_INVENTORY_ITEM_SEEKER_DRONE] = "Seeker Drone";
	inventory_item_names[RPG_INVENTORY_ITEM_EWEB] = "E-Web";
	inventory_item_names[RPG_INVENTORY_ITEM_BIG_BACTA] = "Big Bacta";
	inventory_item_names[RPG_INVENTORY_ITEM_FORCE_FIELD] = "Force Field";
	inventory_item_names[RPG_INVENTORY_ITEM_CLOAK] = "Cloak Item";
	inventory_item_names[RPG_INVENTORY_ITEM_JETPACK] = "Jetpack";

	inventory_item_names[RPG_INVENTORY_MISC_JETPACK_FUEL] = "Jetpack Fuel";
	inventory_item_names[RPG_INVENTORY_MISC_FLAME_THROWER_FUEL] = "Flame Thrower Fuel";

	inventory_item_names[RPG_INVENTORY_UPGRADE_BACTA] = "Bacta Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_FORCE_FIELD] = "Force Field Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_CLOAK] = "Cloak Item Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR] = "Shield Generator";
	inventory_item_names[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] = "Impact Reducer Armor";
	inventory_item_names[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] = "Deflective Armor";
	inventory_item_names[RPG_INVENTORY_UPGRADE_SABER_ARMOR] = "Saber Armor";
	inventory_item_names[RPG_INVENTORY_UPGRADE_FLAME_THROWER] = "Flame Thrower";
	inventory_item_names[RPG_INVENTORY_UPGRADE_STUN_BATON] = "Stun Baton Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] = "Blaster Pistol Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] = "Bryar Pistol Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE] = "E11 Blaster Rifle Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_DISRUPTOR] = "Disruptor Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_BOWCASTER] = "Bowcaster Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_DEMP2] = "DEMP2 Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_REPEATER] = "Repeater Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_FLECHETTE] = "Flechette Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_CONCUSSION] = "Concussion Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER] = "Rocket Launcher Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_DETPACKS] = "Detpacks Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_JETPACK] = "Jetpack Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_RADAR] = "Radar";
	inventory_item_names[RPG_INVENTORY_UPGRADE_THERMAL_VISION] = "Thermal Vision";
	inventory_item_names[RPG_INVENTORY_UPGRADE_SENTRY_GUN] = "Sentry Gun Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_SEEKER_DRONE] = "Seeker Drone Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_EWEB] = "E-Web Upgrade";
	inventory_item_names[RPG_INVENTORY_UPGRADE_ENERGY_MODULATOR] = "Energy Modulator";

	if (inventory_index >= 0 && inventory_index < MAX_RPG_INVENTORY_ITEMS)
	{
		return G_NewString(inventory_item_names[inventory_index]);
	}

	return "";
}

char* zyk_add_inventory_whitespaces(int inventory_index, int biggest_inventory_item_name_length)
{
	char result_text[MAX_STRING_CHARS];
	int inventory_item_name_length = strlen(zyk_get_inventory_item_name(inventory_index));
	int i = 0;

	strcpy(result_text, "");

	for (i = 0; i < (biggest_inventory_item_name_length - inventory_item_name_length); i++)
	{
		// zyk: adds a whitespace at the end of the current string
		strcpy(result_text, va("%s ", result_text));
	}

	return G_NewString(result_text);
}

void zyk_list_inventory(gentity_t* ent, int page)
{
	int inventory_it = 0;
	int results_per_page = 20; // zyk: number of results per page

	char message[MAX_STRING_CHARS];
	char final_chars[32];

	strcpy(message, "");
	strcpy(final_chars, "");

	for (inventory_it = 0; inventory_it < MAX_RPG_INVENTORY_ITEMS; inventory_it++)
	{
		if (inventory_it >= ((page - 1) * results_per_page) && inventory_it < (page * results_per_page))
		{
			if ((inventory_it % 2) == 0)
			{ // zyk: adds whitespaces in first column
				strcpy(final_chars, va("%s ", zyk_add_inventory_whitespaces(inventory_it, 30)));
			}
			else
			{
				strcpy(final_chars, "\n");
			}

			strcpy(message, va("%s^3%s: ^7%d%s", message, zyk_get_inventory_item_name(inventory_it),
				ent->client->pers.rpg_inventory[inventory_it], final_chars));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", message));
}

/*
==================
Cmd_ListAccount_f
==================
*/
void Cmd_ListAccount_f( gentity_t *ent ) {
	if (ent->client->sess.amrpgmode == 2)
	{
		if (trap->Argc() == 1)
		{ // zyk: if player didnt pass arguments, lists general info
			list_rpg_info(ent, ent);
		}
		else
		{
			char arg1[MAX_STRING_CHARS];
			int i = 0;

			trap->Argv(1, arg1, sizeof( arg1 ));

			if (Q_stricmp( arg1, "rpg" ) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"\n^2/list force: ^7lists force power skills\n^2/list weapons: ^7lists weapon skills\n^2/list other: ^7lists miscellaneous skills\n^2/list magic: ^7lists magic skills\n^2/list [skill number]: ^7lists info about a skill\n^2/list inventory: ^7shows player inventory\n^2/list quests: ^7lists the quests\n^2/list commands: ^7lists the RPG Mode console commands\n\n\"");
			}
			else if (Q_stricmp( arg1, "force" ) == 0 || Q_stricmp( arg1, "weapons" ) == 0 || 
					Q_stricmp( arg1, "other" ) == 0 || Q_stricmp(arg1, "magic") == 0)
			{
				zyk_list_player_skills(ent, ent, G_NewString(arg1));
			}
			else if (Q_stricmp(arg1, "inventory") == 0)
			{
				if (trap->Argc() == 2)
				{
					trap->SendServerCommand(ent->s.number, "print \"Must pass a page number. Example: ^3/list inventory 1^7\n\"");
				}
				else
				{
					int page = 1; // zyk: page the user wants to see
					char arg2[MAX_STRING_CHARS];

					strcpy(arg2, "");

					trap->Argv(2, arg2, sizeof(arg2));

					page = atoi(arg2);

					if (page <= 0)
						page = 1;

					zyk_list_inventory(ent, page);
				}
			}
			else if (Q_stricmp( arg1, "quests" ) == 0)
			{
				if (zyk_allow_quests.integer == 1)
				{
					char quest_player[512];
					char target_player[512];
					int j = 0;

					strcpy(quest_player, "");
					strcpy(target_player, "");

					for (j = 0; j < MAX_CLIENTS; j++)
					{
						gentity_t *player = &g_entities[j];

						if (player && player->client && player->client->sess.amrpgmode == 2 && level.bounty_quest_choose_target == qfalse && player->s.number == level.bounty_quest_target_id)
						{
							strcpy(target_player, va("%s", player->client->pers.netname));
						}
					}

					trap->SendServerCommand(ent->s.number, va("print \"\n^3RPG Mode Quests\n\n^3/list bounty: ^7The Bounty Hunter quest\n^3/list custom: ^7lists custom quests\n\n^3Quest Player: ^7%s\n^3Bounty Quest Target: ^7%s^7\n\n\"", quest_player, target_player));
				}
				else
					trap->SendServerCommand(ent->s.number, "print \"\n^3RPG Mode Quests\n\n^1Quests are not allowed in this server^7\n\n\"");
			}
			else if (Q_stricmp( arg1, "bounty" ) == 0)
			{
				trap->SendServerCommand( ent->s.number, va("print \"\n^3Bounty Quest\n^7Use ^3/bountyquest ^7so the server chooses a player to be the target. If the target defeats a RPG player, he receives 200 bonus credits. If a bounty hunter kills the target, he receives bonus credits based in the target player level.\n\n\"") );
			}
			else if (Q_stricmp(arg1, "custom") == 0)
			{
				int j = 0;
				char content[MAX_STRING_CHARS];

				strcpy(content, "");

				if (trap->Argc() == 2)
				{
					for (j = 0; j < MAX_CUSTOM_QUESTS; j++)
					{
						if (level.zyk_custom_quest_mission_count[j] != -1 && Q_stricmp(level.zyk_custom_quest_main_fields[j][1], "on") == 0)
						{ // zyk: an active custom quest
							strcpy(content, va("%s%d - %s\n", content, j, level.zyk_custom_quest_main_fields[j][0]));
						}
					}

					if (Q_stricmp(content, "") != 0)
					{
						trap->SendServerCommand(ent->s.number, va("print \"\n^3Custom Quests\n\n^7%s\n\n^7Use ^2/list custom <number> ^7to see the current mission of the quest\n\n\"", content));
					}
					else
					{
						trap->SendServerCommand(ent->s.number, "print \"\nThere are no active custom quests\n\n\"");
					}
				}
				else
				{
					char arg2[MAX_STRING_CHARS];
					int quest_number = -1;

					trap->Argv(2, arg2, sizeof(arg2));

					quest_number = atoi(arg2);

					if (quest_number >= 0 && quest_number < MAX_CUSTOM_QUESTS && level.zyk_custom_quest_mission_count[quest_number] != -1 && Q_stricmp(level.zyk_custom_quest_main_fields[quest_number][1], "on") == 0)
					{
						char *mission_title = zyk_get_mission_value(quest_number, atoi(level.zyk_custom_quest_main_fields[quest_number][2]), "title");
						char *mission_description = zyk_get_mission_value(quest_number, atoi(level.zyk_custom_quest_main_fields[quest_number][2]), "description");

						trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\n^3%d. %s\n\n^7%s\n\n\"", level.zyk_custom_quest_main_fields[quest_number][0], atoi(level.zyk_custom_quest_main_fields[quest_number][2]), mission_title, mission_description));
					}
					else if (quest_number >= 0 && quest_number < MAX_CUSTOM_QUESTS)
					{
						trap->SendServerCommand(ent->s.number, "print \"\nNo info for this quest\n\n\"");
					}
					else
					{
						trap->SendServerCommand(ent->s.number, "print \"\nInvalid quest number\n\n\"");
					}
				}
			}
			else if (Q_stricmp( arg1, "commands" ) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"\n^2RPG Mode commands\n\n^3/<new or zyknew> [login] [password]: ^7creates a new account.\n^3/<login or zyklogin> [login] [password]: ^7loads the account.\n^3/playermode: ^7switches between ^2Admin-Only Mode ^7and ^2RPG Mode^7.\n^3/up [skill number]: ^7upgrades a skill. Passing ^3all ^7as parameter upgrades all skills.\n^3/down [skill number]: ^7downgrades a skill.\n^3/adminlist: ^7lists admin commands.\n^3/adminup [player id or name] [command number]: ^7gives the player an admin command.\n^3/admindown [player id or name] [command number]: ^7removes an admin command from a player.\n^3/settings: ^7turn on or off player settings.\n^3/callseller: ^7calls the jawa seller.\n^3/creditgive [player id or name] [amount]: ^7gives credits to a player.\n^3/changepassword <new_password>: ^7changes the account password.\n^3/tutorial: ^7shows all info about the mod.\n^3/<logout or zyklogout>: ^7logout the account.\n\n\"" );
			}
			else
			{ // zyk: the player can also list the specific info of a skill passing the skill number as argument
				i = atoi(arg1);

				if (i >= 1 && i <= NUMBER_OF_SKILLS)
				{
					trap->SendServerCommand( ent->s.number, va("print \"^3%s: ^7%s\n\"", zyk_skill_name(i - 1), zyk_skill_description(i - 1)));
				}
				else
				{
					trap->SendServerCommand( ent->s.number, "print \"Invalid skill number.\n\"" );
				}
			}
		}
	}
	else if (ent->client->sess.amrpgmode == 1)
	{
		trap->SendServerCommand( ent->s.number, "print \"\n^2Admin-Only Mode commands\n\n^3/<new or zyknew> [login] [password]: ^7creates a new account.\n^3/<login or zyklogin> [login] [password]: ^7loads the account of the player.\n^3/playermode: ^7switches between the ^2Admin-Only Mode ^7and the ^2RPG Mode^7.\n^3/adminlist: ^7lists admin commands.\n^3/adminup [player id or name] [admin command number]: ^7gives the player a new admin command.\n^3/admindown [player id or name] [admin command number]: ^7removes an admin command from the player.\n^3/settings: ^7turn on or off player settings.\n^3/changepassword <new_password>: ^7changes the account password.\n^3/tutorial: ^7shows all info about the mod.\n^3/<logout or zyklogout>: ^7logout the account.\n\n\"" );
	}
	else
	{
		trap->SendServerCommand( ent->s.number, "print \"\n^1Account System\n\n^7The account system has 2 modes:\n^2Admin-Only Mode: ^7allows you to use admin commands\n^2RPG Mode: ^7allows you to use admin commands and to play the Level System\n\n^7Create a new account with ^3/<new or zyknew> <login> <password>\n^7where login and password are of your choice.\n\n\"" );
	}
}

/*
==================
Cmd_CallSeller_f
==================
*/
extern gentity_t *NPC_SpawnType( gentity_t *ent, char *npc_type, char *targetname, qboolean isVehicle );
void Cmd_CallSeller_f( gentity_t *ent ) {
	gentity_t *npc_ent = NULL;
	int i = 0;
	int seller_id = -1;

	for (i = MAX_CLIENTS; i < level.num_entities; i++)
	{
		npc_ent = &g_entities[i];

		if (npc_ent && npc_ent->client && npc_ent->NPC && Q_stricmp(npc_ent->NPC_type, "jawa_seller") == 0 && 
			npc_ent->health > 0 && npc_ent->client->pers.seller_invoked_by_id == ent->s.number)
		{ // zyk: found the seller of this player
			seller_id = npc_ent->s.number;
			break;
		}
	}

	if (seller_id == -1)
	{
		npc_ent = NPC_SpawnType(ent,"jawa_seller",NULL,qfalse);
		if (npc_ent)
		{
			npc_ent->client->pers.seller_invoked_by_id = ent->s.number;
			npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

			trap->SendServerCommand( ent->s.number, "chat \"^3Jawa Seller: ^7Hello, friend! I have some products to sell.\"");
		}
		else
		{
			trap->SendServerCommand( ent->s.number, va("chat \"%s: ^7The seller couldn't come...\"", ent->client->pers.netname));
		}
	}
	else
	{ // zyk: seller of this player is already in the map, remove him
		npc_ent = &g_entities[seller_id];

		G_FreeEntity(npc_ent);

		trap->SendServerCommand( ent->s.number, "chat \"^3Jawa Seller: ^7See you later, friend!\"");
	}
}

int zyk_get_seller_item_cost(zyk_seller_item_t item_number, qboolean buy_item)
{
	// zyk: costs to buy or sell for each seller item
	int seller_items_cost[MAX_SELLER_ITEMS][2];

	seller_items_cost[SELLER_BLASTER_PACK][0] = 5;
	seller_items_cost[SELLER_BLASTER_PACK][1] = 2;

	seller_items_cost[SELLER_POWERCELL][0] = 8;
	seller_items_cost[SELLER_POWERCELL][1] = 4;

	seller_items_cost[SELLER_METAL_BOLTS][0] = 7;
	seller_items_cost[SELLER_METAL_BOLTS][1] = 4;

	seller_items_cost[SELLER_ROCKETS][0] = 10;
	seller_items_cost[SELLER_ROCKETS][1] = 5;

	seller_items_cost[SELLER_THERMALS][0] = 12;
	seller_items_cost[SELLER_THERMALS][1] = 5;

	seller_items_cost[SELLER_TRIPMINES][0] = 14;
	seller_items_cost[SELLER_TRIPMINES][1] = 9;

	seller_items_cost[SELLER_DETPACKS][0] = 20;
	seller_items_cost[SELLER_DETPACKS][1] = 15;

	seller_items_cost[SELLER_AMMO_ALL][0] = 200;
	seller_items_cost[SELLER_AMMO_ALL][1] = 0;

	seller_items_cost[SELLER_FLAME_FUEL][0] = 20;
	seller_items_cost[SELLER_FLAME_FUEL][1] = 0;

	seller_items_cost[SELLER_SHIELD_BOOSTER][0] = 50;
	seller_items_cost[SELLER_SHIELD_BOOSTER][1] = 0;

	seller_items_cost[SELLER_SENTRY_GUN][0] = 120;
	seller_items_cost[SELLER_SENTRY_GUN][1] = 50;

	seller_items_cost[SELLER_SEEKER_DRONE][0] = 140;
	seller_items_cost[SELLER_SEEKER_DRONE][1] = 60;

	seller_items_cost[SELLER_BACTA_CANISTER][0] = 120;
	seller_items_cost[SELLER_BACTA_CANISTER][1] = 50;

	seller_items_cost[SELLER_FORCE_FIELD][0] = 200;
	seller_items_cost[SELLER_FORCE_FIELD][1] = 100;

	seller_items_cost[SELLER_BIG_BACTA][0] = 220;
	seller_items_cost[SELLER_BIG_BACTA][1] = 90;

	seller_items_cost[SELLER_EWEB][0] = 140;
	seller_items_cost[SELLER_EWEB][1] = 40;

	seller_items_cost[SELLER_BINOCULARS][0] = 10;
	seller_items_cost[SELLER_BINOCULARS][1] = 5;

	seller_items_cost[SELLER_JETPACK][0] = 500;
	seller_items_cost[SELLER_JETPACK][1] = 0;

	seller_items_cost[SELLER_CLOAK_ITEM][0] = 150;
	seller_items_cost[SELLER_CLOAK_ITEM][1] = 90;

	seller_items_cost[SELLER_BLASTER_PISTOL][0] = 90;
	seller_items_cost[SELLER_BLASTER_PISTOL][1] = 45;

	seller_items_cost[SELLER_BRYAR_PISTOL][0] = 90;
	seller_items_cost[SELLER_BRYAR_PISTOL][1] = 45;

	seller_items_cost[SELLER_E11_BLASTER][0] = 100;
	seller_items_cost[SELLER_E11_BLASTER][1] = 50;

	seller_items_cost[SELLER_DISRUPTOR][0] = 120;
	seller_items_cost[SELLER_DISRUPTOR][1] = 60;

	seller_items_cost[SELLER_BOWCASTER][0] = 110;
	seller_items_cost[SELLER_BOWCASTER][1] = 55;

	seller_items_cost[SELLER_DEMP2][0] = 150;
	seller_items_cost[SELLER_DEMP2][1] = 80;

	seller_items_cost[SELLER_REPEATER][0] = 150;
	seller_items_cost[SELLER_REPEATER][1] = 80;

	seller_items_cost[SELLER_FLECHETTE][0] = 170;
	seller_items_cost[SELLER_FLECHETTE][1] = 90;

	seller_items_cost[SELLER_CONCUSSION][0] = 300;
	seller_items_cost[SELLER_CONCUSSION][1] = 180;

	seller_items_cost[SELLER_ROCKET_LAUNCHER][0] = 250;
	seller_items_cost[SELLER_ROCKET_LAUNCHER][1] = 120;

	seller_items_cost[SELLER_LIGHTSABER][0] = 200;
	seller_items_cost[SELLER_LIGHTSABER][1] = 100;

	seller_items_cost[SELLER_STUN_BATON][0] = 20;
	seller_items_cost[SELLER_STUN_BATON][1] = 10;

	seller_items_cost[SELLER_YSALAMIRI][0] = 200;
	seller_items_cost[SELLER_YSALAMIRI][1] = 100;

	seller_items_cost[SELLER_JETPACK_FUEL][0] = 20;
	seller_items_cost[SELLER_JETPACK_FUEL][1] = 0;

	seller_items_cost[SELLER_FORCE_BOON][0] = 200;
	seller_items_cost[SELLER_FORCE_BOON][1] = 100;

	seller_items_cost[SELLER_MAGIC_POTION][0] = 100;
	seller_items_cost[SELLER_MAGIC_POTION][1] = 0;

	seller_items_cost[SELLER_BACTA_UPGRADE][0] = 1500;
	seller_items_cost[SELLER_BACTA_UPGRADE][1] = 900;

	seller_items_cost[SELLER_FORCE_FIELD_UPGRADE][0] = 1400;
	seller_items_cost[SELLER_FORCE_FIELD_UPGRADE][1] = 800;

	seller_items_cost[SELLER_CLOAK_UPGRADE][0] = 1100;
	seller_items_cost[SELLER_CLOAK_UPGRADE][1] = 700;

	seller_items_cost[SELLER_SHIELD_GENERATOR_UPGRADE][0] = 500;
	seller_items_cost[SELLER_SHIELD_GENERATOR_UPGRADE][1] = 300;

	seller_items_cost[SELLER_IMPACT_REDUCER_ARMOR][0] = 2000;
	seller_items_cost[SELLER_IMPACT_REDUCER_ARMOR][1] = 1000;

	seller_items_cost[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR][0] = 2000;
	seller_items_cost[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR][1] = 1000;

	seller_items_cost[RPG_INVENTORY_UPGRADE_SABER_ARMOR][0] = 2000;
	seller_items_cost[RPG_INVENTORY_UPGRADE_SABER_ARMOR][1] = 1000;

	seller_items_cost[SELLER_FLAME_THROWER][0] = 1000;
	seller_items_cost[SELLER_FLAME_THROWER][1] = 700;

	seller_items_cost[SELLER_STUN_BATON_UPGRADE][0] = 1500;
	seller_items_cost[SELLER_STUN_BATON_UPGRADE][1] = 900;

	seller_items_cost[SELLER_BLASTER_PISTOL_UPGRADE][0] = 800;
	seller_items_cost[SELLER_BLASTER_PISTOL_UPGRADE][1] = 300;

	seller_items_cost[SELLER_BRYAR_PISTOL_UPGRADE][0] = 800;
	seller_items_cost[SELLER_BRYAR_PISTOL_UPGRADE][1] = 300;

	seller_items_cost[SELLER_E11_BLASTER_RIFLE_UPGRADE][0] = 1200;
	seller_items_cost[SELLER_E11_BLASTER_RIFLE_UPGRADE][1] = 700;

	seller_items_cost[SELLER_DISRUPTOR_UPGRADE][0] = 1500;
	seller_items_cost[SELLER_DISRUPTOR_UPGRADE][1] = 900;

	seller_items_cost[SELLER_BOWCASTER_UPGRADE][0] = 1400;
	seller_items_cost[SELLER_BOWCASTER_UPGRADE][1] = 700;

	seller_items_cost[SELLER_DEMP2_UPGRADE][0] = 1000;
	seller_items_cost[SELLER_DEMP2_UPGRADE][1] = 500;

	seller_items_cost[SELLER_REPEATER_UPGRADE][0] = 1400;
	seller_items_cost[SELLER_REPEATER_UPGRADE][1] = 700;

	seller_items_cost[SELLER_FLECHETTE_UPGRADE][0] = 1100;
	seller_items_cost[SELLER_FLECHETTE_UPGRADE][1] = 600;

	seller_items_cost[SELLER_CONCUSSION_UPGRADE][0] = 1200;
	seller_items_cost[SELLER_CONCUSSION_UPGRADE][1] = 700;

	seller_items_cost[SELLER_ROCKETS_UPGRADE][0] = 1200;
	seller_items_cost[SELLER_ROCKETS_UPGRADE][1] = 500;

	seller_items_cost[SELLER_DETPACKS_UPGRADE][0] = 1200;
	seller_items_cost[SELLER_DETPACKS_UPGRADE][1] = 500;

	seller_items_cost[SELLER_JETPACK_UPGRADE][0] = 5000;
	seller_items_cost[SELLER_JETPACK_UPGRADE][1] = 3000;

	seller_items_cost[SELLER_GUNNER_RADAR][0] = 1000;
	seller_items_cost[SELLER_GUNNER_RADAR][1] = 500;

	seller_items_cost[SELLER_THERMAL_VISION][0] = 1000;
	seller_items_cost[SELLER_THERMAL_VISION][1] = 600;

	seller_items_cost[SELLER_SENTRY_GUN_UPGRADE][0] = 1800;
	seller_items_cost[SELLER_SENTRY_GUN_UPGRADE][1] = 1000;

	seller_items_cost[SELLER_SEEKER_DRONE_UPGRADE][0] = 16000;
	seller_items_cost[SELLER_SEEKER_DRONE_UPGRADE][1] = 900;

	seller_items_cost[SELLER_EWEB_UPGRADE][0] = 1200;
	seller_items_cost[SELLER_EWEB_UPGRADE][1] = 500;

	seller_items_cost[SELLER_ENERGY_MODULATOR][0] = 7000;
	seller_items_cost[SELLER_ENERGY_MODULATOR][1] = 5000;

	if (buy_item == qtrue)
	{
		return seller_items_cost[item_number][0];
	}

	return seller_items_cost[item_number][1];
}

// zyk: returns the seller item name
char* zyk_get_seller_item_name(zyk_seller_item_t item_number)
{
	char* seller_items_names[MAX_SELLER_ITEMS];

	seller_items_names[SELLER_BLASTER_PACK] = "Blaster Pack";
	seller_items_names[SELLER_POWERCELL] = "Power Cell";
	seller_items_names[SELLER_METAL_BOLTS] = "Metal Bolts";
	seller_items_names[SELLER_ROCKETS] = "Rockets";
	seller_items_names[SELLER_THERMALS] = "Thermals";
	seller_items_names[SELLER_TRIPMINES] = "Trip Mines";
	seller_items_names[SELLER_DETPACKS] = "Det Packs";
	seller_items_names[SELLER_AMMO_ALL] = "Ammo All";
	seller_items_names[SELLER_FLAME_FUEL] = "Flame Thrower Fuel";

	seller_items_names[SELLER_SHIELD_BOOSTER] = "Shield Booster";
	seller_items_names[SELLER_SENTRY_GUN] = "Sentry Gun";
	seller_items_names[SELLER_SEEKER_DRONE] = "Seeker Drone";
	seller_items_names[SELLER_BACTA_CANISTER] = "Bacta Canister";
	seller_items_names[SELLER_FORCE_FIELD] = "Force Field";
	seller_items_names[SELLER_BIG_BACTA] = "Big Bacta";
	seller_items_names[SELLER_EWEB] = "E-Web";
	seller_items_names[SELLER_BINOCULARS] = "Binoculars";
	seller_items_names[SELLER_JETPACK] = "Jetpack";
	seller_items_names[SELLER_CLOAK_ITEM] = "Cloak Item";

	seller_items_names[SELLER_BLASTER_PISTOL] = "Blaster Pistol";
	seller_items_names[SELLER_BRYAR_PISTOL] = "Bryar Pistol";
	seller_items_names[SELLER_E11_BLASTER] = "E11 Blaster Rifle";
	seller_items_names[SELLER_DISRUPTOR] = "Disruptor";
	seller_items_names[SELLER_BOWCASTER] = "Bowcaster";
	seller_items_names[SELLER_DEMP2] = "DEMP2";
	seller_items_names[SELLER_REPEATER] = "Repeater";
	seller_items_names[SELLER_FLECHETTE] = "Flechette";
	seller_items_names[SELLER_CONCUSSION] = "Concussion Rifle";
	seller_items_names[SELLER_ROCKET_LAUNCHER] = "Rocket Launcher";
	seller_items_names[SELLER_LIGHTSABER] = "Saber";
	seller_items_names[SELLER_STUN_BATON] = "Stun Baton";

	seller_items_names[SELLER_YSALAMIRI] = "Ysalamiri";
	seller_items_names[SELLER_JETPACK_FUEL] = "Jetpack Fuel";
	seller_items_names[SELLER_FORCE_BOON] = "Force Boon";
	seller_items_names[SELLER_MAGIC_POTION] = "Magic Potion";

	seller_items_names[SELLER_BACTA_UPGRADE] = "Bacta Upgrade";
	seller_items_names[SELLER_FORCE_FIELD_UPGRADE] = "Force Field Upgrade";
	seller_items_names[SELLER_CLOAK_UPGRADE] = "Cloak Item Upgrade";
	seller_items_names[SELLER_SHIELD_GENERATOR_UPGRADE] = "Shield Generator";
	seller_items_names[SELLER_IMPACT_REDUCER_ARMOR] = "Impact Reducer Armor";
	seller_items_names[SELLER_DEFLECTIVE_ARMOR] = "Deflective Armor";
	seller_items_names[SELLER_SABER_ARMOR] = "Saber Armor";
	seller_items_names[SELLER_FLAME_THROWER] = "Flame Thrower";
	seller_items_names[SELLER_STUN_BATON_UPGRADE] = "Stun Baton Upgrade";
	seller_items_names[SELLER_BLASTER_PISTOL_UPGRADE] = "Blaster Pistol Upgrade";
	seller_items_names[SELLER_BRYAR_PISTOL_UPGRADE] = "Bryar Pistol Upgrade";
	seller_items_names[SELLER_E11_BLASTER_RIFLE_UPGRADE] = "E11 Blaster Rifle Upgrade";
	seller_items_names[SELLER_DISRUPTOR_UPGRADE] = "Disruptor Upgrade";
	seller_items_names[SELLER_BOWCASTER_UPGRADE] = "Bowcaster Upgrade";
	seller_items_names[SELLER_DEMP2_UPGRADE] = "DEMP2 Upgrade";
	seller_items_names[SELLER_REPEATER_UPGRADE] = "Repeater Upgrade";
	seller_items_names[SELLER_FLECHETTE_UPGRADE] = "Flechette Upgrade";
	seller_items_names[SELLER_CONCUSSION_UPGRADE] = "Concussion Upgrade";
	seller_items_names[SELLER_ROCKETS_UPGRADE] = "Rocket Launcher Upgrade";
	seller_items_names[SELLER_DETPACKS_UPGRADE] = "Detpacks Upgrade";
	seller_items_names[SELLER_JETPACK_UPGRADE] = "Jetpack Upgrade";
	seller_items_names[SELLER_GUNNER_RADAR] = "Radar";
	seller_items_names[SELLER_THERMAL_VISION] = "Thermal Vision";
	seller_items_names[SELLER_SENTRY_GUN_UPGRADE] = "Sentry Gun Upgrade";
	seller_items_names[SELLER_SEEKER_DRONE_UPGRADE] = "Seeker Drone Upgrade";
	seller_items_names[SELLER_EWEB_UPGRADE] = "E-Web Upgrade";
	seller_items_names[SELLER_ENERGY_MODULATOR] = "Energy Modulator";

	if (item_number >= 0 && item_number < MAX_SELLER_ITEMS)
	{
		return G_NewString(seller_items_names[item_number]);
	}
	
	return "";
}

void zyk_show_stuff_category(gentity_t* ent, int min_item_index, int max_item_index)
{
	int i = 0;
	char content[MAX_STRING_CHARS];

	strcpy(content, "");

	for (i = min_item_index; i <= max_item_index; i++)
	{
		char sell_item_string[16];
		int sell_cost = zyk_get_seller_item_cost(i, qfalse);

		if (sell_cost > 0)
		{
			strcpy(sell_item_string, va("%d", sell_cost));
		}
		else
		{
			strcpy(sell_item_string, "^1no^7");
		}

		strcpy(content, va("%s\n^3%d - %s: ^7Buy: %d, Sell: %s", content, (i + 1), zyk_get_seller_item_name(i), zyk_get_seller_item_cost(i, qtrue), sell_item_string));
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\n\"", content));
}

/*
==================
Cmd_Stuff_f
==================
*/
void Cmd_Stuff_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{ // zyk: shows the categories of stuff
		trap->SendServerCommand(ent->s.number, "print \"\n^7Use ^2/stuff <category> ^7to buy or sell stuff\nThe Category may be ^3ammo^7, ^3items^7, ^3weapons^7, ^3misc ^7or ^3upgrades\n^7Use ^3/stuff <number> ^7to see info about the item\n\n^7Use ^2/buy <number> ^7to buy or ^2/sell <number> ^7to sell\nStuff bought from ^3upgrades ^7category are permanent\n\n\"");
		return;
	}
	else
	{
		char arg1[1024];
		int i = 0;
		int item_number = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		item_number = atoi(arg1);

		i = item_number - 1;

		if (Q_stricmp(arg1, "ammo" ) == 0)
		{
			zyk_show_stuff_category(ent, SELLER_BLASTER_PACK, SELLER_FLAME_FUEL);
		}
		else if (Q_stricmp(arg1, "items" ) == 0)
		{
			zyk_show_stuff_category(ent, SELLER_SHIELD_BOOSTER, SELLER_CLOAK_ITEM);
		}
		else if (Q_stricmp(arg1, "weapons" ) == 0)
		{
			zyk_show_stuff_category(ent, SELLER_BLASTER_PISTOL, SELLER_STUN_BATON);
		}
		else if (Q_stricmp(arg1, "misc") == 0)
		{
			zyk_show_stuff_category(ent, SELLER_YSALAMIRI, SELLER_MAGIC_POTION);
		}
		else if (Q_stricmp(arg1, "upgrades" ) == 0)
		{
			if (trap->Argc() == 2)
			{
				trap->SendServerCommand(ent->s.number, "print \"Must pass a page number. Example: ^3/stuff upgrades 1^7\n\"");
			}
			else
			{
				int page = 1; // zyk: page the user wants to see
				int last_item_offset = 9;
				int first_item_index = SELLER_BACTA_UPGRADE;
				int last_item_index = SELLER_BACTA_UPGRADE;
				char arg2[MAX_STRING_CHARS];

				strcpy(arg2, "");

				trap->Argv(2, arg2, sizeof(arg2));

				page = atoi(arg2);

				if (page <= 0)
					page = 1;

				first_item_index += (last_item_offset * (page - 1));
				last_item_index += (last_item_offset * page);

				if (first_item_index >= MAX_SELLER_ITEMS)
					return;

				if (last_item_index >= MAX_SELLER_ITEMS)
					last_item_index = MAX_SELLER_ITEMS - 1;

				zyk_show_stuff_category(ent, first_item_index, last_item_index);
			}
		}
		else if (i == SELLER_BLASTER_PACK)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of E11 Blaster Rifle, Blaster Pistol and Bryar Pistol weapons\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_POWERCELL)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of Disruptor, Bowcaster and DEMP2 weapons\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_METAL_BOLTS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of Repeater, Flechette and Concussion Rifle weapons\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_ROCKETS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of Rocket Launcher weapon\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_THERMALS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of thermals\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_TRIPMINES)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of trip mines\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DETPACKS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some ammo of det packs\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_AMMO_ALL)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers all ammo types, including flame thrower fuel\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FLAME_FUEL)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers all fuel of the flame thrower\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SHIELD_BOOSTER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers 50 shield\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SENTRY_GUN)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7portable gun which is placed in the ground and shoots nearby enemies\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SEEKER_DRONE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7portable remote drone that shoots enemies at sight\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BACTA_CANISTER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7item that recovers some health, mp and Stamina\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FORCE_FIELD)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7creates a force field wall in front of the player that can hold almost any attack, except the concussion rifle alternate fire, which can get through\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BIG_BACTA)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7item that recovers some health, mp and Stamina. Recovers double the amount of a Bacta Canister\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_EWEB)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7portable emplaced gun\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BINOCULARS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7allows the player to see far things better with the zoom feature\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_JETPACK)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7allows the player to fly. Jump and press Use Key to use it\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_CLOAK_ITEM)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7allows the player to cloak himself\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BLASTER_PISTOL)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7pistol that can shoot a charged shot with alt fire. Uses blaster pack ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BRYAR_PISTOL)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7similar to blaster pistol, but has a faster fire rate with normal fire. Uses blaster pack ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_E11_BLASTER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Rifle that is used by the stormtroopers. Uses blaster pack ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DISRUPTOR)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Sniper rifle which can desintegrate the enemy. Uses power cell ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BOWCASTER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7weapon that shoots green bolts, normal fire can be charged, and alt fire shoots a bouncing bolt. Uses power cell ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DEMP2)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7fires an electro magnetic pulse that causes bonus damage against droids. Uses power cell ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_REPEATER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7imperial weapon that shoots orbs and a plasma bomb with alt fire. Uses metal bolts ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FLECHETTE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7it is the shotgun of the game, and can shoot 2 bombs with alt fire. Uses metal bolts ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_CONCUSSION)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7powerful weapon, alt fire can shoot a beam that gets through force fields. Uses metal bolts ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_ROCKET_LAUNCHER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7weapon that shoots rockets and a homing missile with alternate fire. Uses rockets ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_LIGHTSABER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7the weapon of a Jedi/Sith. With single, each Saber Attack skill level gives new saber stances. With duals/staff the Saber Attack skill level gives more damage\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_STUN_BATON)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7weapon that fires a small electric charge\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_YSALAMIRI)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7disables the player force powers but also protects the player from enemy force powers\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_JETPACK_FUEL)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers all fuel of the jetpack\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FORCE_BOON)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7allows the player to regenerate force faster\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_MAGIC_POTION)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7recovers some MP\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BACTA_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Bacta Canister and Big Bacta recovers more HP, more MP and more Stamina\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FORCE_FIELD_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Force Field resists more damage. Allows getting them back pressing Use Key on them. Doing so requires some powercell ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_CLOAK_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Cloak Item will be able to cloak vehicles. Press Saber Stance key when riding a vehicle to cloak it\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SHIELD_GENERATOR_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7allows the player to restore his shield. The max shield will be the same as the max health\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_IMPACT_REDUCER_ARMOR)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7decreases damage to your health from any source by 15 per cent and reduces the knockback of some weapons attacks by 80 per cent\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DEFLECTIVE_ARMOR)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7absorbs 35 per cent weapon/melee damage to your health (only 5 per cent from saber) and deflects some weapon shots\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SABER_ARMOR)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7absorbs 5 per cent weapon/melee damage to your health (40 per cent from saber)\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FLAME_THROWER)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7gives you the flame thrower. To use it, get stun baton and use alternate fire\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_STUN_BATON_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7allows stun baton to open any door, including locked ones. Also makes stun baton decloak enemies and decrease their running speed for some seconds\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BLASTER_PISTOL_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Increases Blaster Pistol firerate\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BRYAR_PISTOL_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Increases Bryar Pistol firerate\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_E11_BLASTER_RIFLE_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7E11 Blaster Rifle altfire has less spread\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DISRUPTOR_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7increases firerate of Disruptor\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_BOWCASTER_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Bowcaster main fire can shoot more shots when charged. Bowcaster altfire bolt can bounce more times\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DEMP2_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7increases firerate of DEMP2 main fire\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_REPEATER_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7increases firerate of Repeater altfire\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_FLECHETTE_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7increases number of bolts shot by Flechette primary fire\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_CONCUSSION_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7Concussion rifle can break saber-only damage objects and can move pushable/pullable objects\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_ROCKETS_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7makes rockets able to damage saber-only damage objects and move pushable/pullable objects\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_DETPACKS_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7makes detpacks able to damage saber-only damage objects and move pushable/pullable objects\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_JETPACK_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7decreases jetpack fuel consumption a bit and makes the jetpack more stable and faster\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_GUNNER_RADAR)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7if you have the client plugin, a radar will be displayed in the screen showing positions of other players and npcs\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_THERMAL_VISION)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7makes binoculars detect enemies through a thermal vision system\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SENTRY_GUN_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7makes Sentry Gun stronger. Allows getting sentry guns back by pressing Use Key near them. Doing this requires some power cell ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_SEEKER_DRONE_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7makes seeker drone stronger\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_EWEB_UPGRADE)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7makes E-Web stronger\n\n\"", zyk_get_seller_item_name(i)));
		}
		else if (i == SELLER_ENERGY_MODULATOR)
		{
			trap->SendServerCommand(ent->s.number, va("print \"\n^3%s: ^7a device that converts powercell ammo and blaster pack ammo into attack power or an extra shield protection. It has two modes. First Mode increases damage of all attacks by 20 per cent and reduces flame thrower fuel usage. Second Mode increases resistance to damage from any source by 20 per cent. Activate it by getting melee and pressing Saber Style key. It uses blaster pack ammo, and it if runs out, uses powercell ammo\n\n\"", zyk_get_seller_item_name(i)));
		}
	}
}

// zyk: adds some MP to the RPG player
void zyk_add_mp(gentity_t* ent, int mp_amount)
{
	if ((ent->client->pers.magic_power + mp_amount) < zyk_max_magic_power(ent))
	{
		ent->client->pers.magic_power += mp_amount;
	}
	else
	{
		ent->client->pers.magic_power = zyk_max_magic_power(ent);
	}

	send_rpg_events(2000);
}

/*
==================
Cmd_Buy_f
==================
*/
void Cmd_Buy_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int value = 0;
	int found = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent->s.number, "print \"You must specify a product number.\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));
	value = atoi(arg1);

	// zyk: tests the cooldown time to buy or sell
	if (ent->client->pers.buy_sell_timer > level.time)
	{
		trap->SendServerCommand(ent->s.number, "print \"In Buy/Sell cooldown time.\n\"");
		return;
	}

	if (value < 1 || value > MAX_SELLER_ITEMS)
	{
		trap->SendServerCommand( ent->s.number, "print \"Invalid product number.\n\"" );
		return;
	}
	else
	{ // zyk: searches for the jawa to see if we are near him to buy or sell to him
		gentity_t *jawa_ent = NULL;
		int j = 0;

		for (j = MAX_CLIENTS; j < level.num_entities; j++)
		{
			jawa_ent = &g_entities[j];

			if (jawa_ent && jawa_ent->client && jawa_ent->NPC && jawa_ent->health > 0 && Q_stricmp( jawa_ent->NPC_type, "jawa_seller" ) == 0 && (int)Distance(ent->client->ps.origin, jawa_ent->client->ps.origin) < 90)
			{
				found = 1;
				break;
			}
		}

		if (found == 0)
		{
			trap->SendServerCommand(ent->s.number, "print \"You must be near the jawa seller to buy from him.\n\"" );
			return;
		}
	}

	// zyk: general validations. Some items require certain conditions to be bought
	if (value == (SELLER_LIGHTSABER + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_WP_SABER] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_LIGHTSABER)));
		return;
	}
	else if (value == (SELLER_BACTA_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BACTA] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_BACTA_UPGRADE)));
		return;
	}
	else if (value == (SELLER_FORCE_FIELD_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_FORCE_FIELD] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_FORCE_FIELD_UPGRADE)));
		return;
	}
	else if (value == (SELLER_CLOAK_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_CLOAK] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_CLOAK_UPGRADE)));
		return;
	}
	else if (value == (SELLER_SHIELD_GENERATOR_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_SHIELD_GENERATOR_UPGRADE)));
		return;
	}
	else if (value == (SELLER_IMPACT_REDUCER_ARMOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_IMPACT_REDUCER_ARMOR)));
		return;
	}
	else if (value == (SELLER_DEFLECTIVE_ARMOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_DEFLECTIVE_ARMOR)));
		return;
	}
	else if (value == (SELLER_SABER_ARMOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SABER_ARMOR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_SABER_ARMOR)));
		return;
	}
	else if (value == (SELLER_FLAME_THROWER + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_FLAME_THROWER] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_FLAME_THROWER)));
		return;
	}
	else if (value == (SELLER_STUN_BATON_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_STUN_BATON] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_STUN_BATON_UPGRADE)));
		return;
	}
	else if (value == (SELLER_BLASTER_PISTOL_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_BLASTER_PISTOL_UPGRADE)));
		return;
	}
	else if (value == (SELLER_BRYAR_PISTOL_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_BRYAR_PISTOL_UPGRADE)));
		return;
	}
	else if (value == (SELLER_E11_BLASTER_RIFLE_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_E11_BLASTER_RIFLE_UPGRADE)));
		return;
	}
	else if (value == (SELLER_DISRUPTOR_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DISRUPTOR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_DISRUPTOR_UPGRADE)));
		return;
	}
	else if (value == (SELLER_BOWCASTER_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BOWCASTER] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_BOWCASTER_UPGRADE)));
		return;
	}
	else if (value == (SELLER_DEMP2_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEMP2] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_DEMP2_UPGRADE)));
		return;
	}
	else if (value == (SELLER_REPEATER_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_REPEATER] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_REPEATER_UPGRADE)));
		return;
	}
	else if (value == (SELLER_FLECHETTE_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_FLECHETTE] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_FLECHETTE_UPGRADE)));
		return;
	}
	else if (value == (SELLER_CONCUSSION_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_CONCUSSION] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_CONCUSSION_UPGRADE)));
		return;
	}
	else if (value == (SELLER_ROCKETS_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_ROCKETS_UPGRADE)));
		return;
	}
	else if (value == (SELLER_DETPACKS_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DETPACKS] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_DETPACKS_UPGRADE)));
		return;
	}
	else if (value == (SELLER_JETPACK_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_JETPACK] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_JETPACK_UPGRADE)));
		return;
	}
	else if (value == (SELLER_GUNNER_RADAR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_RADAR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_GUNNER_RADAR)));
		return;
	}
	else if (value == (SELLER_THERMAL_VISION + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_THERMAL_VISION] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_THERMAL_VISION)));
		return;
	}
	else if (value == (SELLER_SENTRY_GUN_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SENTRY_GUN] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_SENTRY_GUN_UPGRADE)));
		return;
	}
	else if (value == (SELLER_SEEKER_DRONE_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SEEKER_DRONE] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_SEEKER_DRONE_UPGRADE)));
		return;
	}
	else if (value == (SELLER_EWEB_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_EWEB] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_EWEB_UPGRADE)));
		return;
	}
	else if (value == (SELLER_ENERGY_MODULATOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_ENERGY_MODULATOR] > 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You already have the %s.\n\"", zyk_get_seller_item_name(SELLER_ENERGY_MODULATOR)));
		return;
	}

	// zyk: buying the item if player has enough credits
	if (ent->client->pers.credits >= zyk_get_seller_item_cost((value - 1), qtrue))
	{
		if (value == (SELLER_BLASTER_PACK + 1))
		{
			Add_Ammo(ent, AMMO_BLASTER, 50);
		}
		else if (value == (SELLER_POWERCELL + 1))
		{
			Add_Ammo(ent, AMMO_POWERCELL, 50);
		}
		else if (value == (SELLER_METAL_BOLTS + 1))
		{
			Add_Ammo(ent, AMMO_METAL_BOLTS, 50);
		}
		else if (value == (SELLER_ROCKETS + 1))
		{
			Add_Ammo(ent, AMMO_ROCKETS, 2);
		}
		else if (value == (SELLER_THERMALS + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent, AMMO_THERMAL, 4);
		}
		else if (value == (SELLER_TRIPMINES + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent, AMMO_TRIPMINE, 3);
		}
		else if (value == (SELLER_DETPACKS + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent, AMMO_DETPACK, 2);
		}
		else if (value == (SELLER_SHIELD_BOOSTER + 1))
		{
			if (ent->client->ps.stats[STAT_ARMOR] < ent->client->pers.max_rpg_shield)
			{ // zyk: RPG Mode has the Max Shield skill that doesnt allow someone to heal shields above this value
				ent->client->ps.stats[STAT_ARMOR] += 50;
			}

			if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield)
				ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
		}
		else if (value == (SELLER_BACTA_CANISTER + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_BACTA_CANISTER);
		}
		else if (value == (SELLER_BIG_BACTA + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_BIG_BACTA);
		}
		else if (value == (SELLER_SENTRY_GUN + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_SENTRY_GUN);
		}
		else if (value == (SELLER_SEEKER_DRONE + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_SEEKER_DRONE);
		}
		else if (value == (SELLER_FORCE_FIELD + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_FORCE_FIELD);
		}
		else if (value == (SELLER_YSALAMIRI + 1))
		{
			if (ent->client->ps.powerups[PW_YSALAMIRI] < level.time)
				ent->client->ps.powerups[PW_YSALAMIRI] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_YSALAMIRI] += 60000;
		}
		else if (value == (SELLER_E11_BLASTER + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_E11_BLASTER_RIFLE);
		}
		else if (value == (SELLER_DISRUPTOR + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_DISRUPTOR);
		}
		else if (value == (SELLER_REPEATER + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_REPEATER);
		}
		else if (value == (SELLER_ROCKET_LAUNCHER + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_ROCKET_LAUNCHER);
		}
		else if (value == (SELLER_BOWCASTER + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_BOWCASTER);
		}
		else if (value == (SELLER_BLASTER_PISTOL + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_BLASTER_PISTOL);
		}
		else if (value == (SELLER_FLECHETTE + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_FLECHETTE);
		}
		else if (value == (SELLER_CONCUSSION + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_CONCUSSION);
		}
		else if (value == (SELLER_FLAME_FUEL + 1))
		{
			ent->client->ps.cloakFuel += 10;
		}
		else if (value == (SELLER_JETPACK_FUEL + 1))
		{
			ent->client->pers.jetpack_fuel += (10 * JETPACK_SCALE);
			ent->client->ps.jetpackFuel = ent->client->pers.jetpack_fuel / JETPACK_SCALE;
		}
		else if (value == (SELLER_LIGHTSABER + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_SABER);
		}
		else if (value == (SELLER_STUN_BATON + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_STUN_BATON);
		}
		else if (value == (SELLER_EWEB + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_EWEB);
		}
		else if (value == (SELLER_DEMP2 + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_DEMP2);
		}
		else if (value == (SELLER_BRYAR_PISTOL + 1))
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_WP_BRYAR_PISTOL);
		}
		else if (value == (SELLER_BINOCULARS + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_BINOCULARS);
		}
		else if (value == (SELLER_JETPACK + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_JETPACK);
		}
		else if (value == (SELLER_CLOAK_ITEM + 1))
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);

			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_ITEM_CLOAK);
		}
		else if (value == (SELLER_FORCE_BOON + 1))
		{
			if (ent->client->ps.powerups[PW_FORCE_BOON] < level.time)
				ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_FORCE_BOON] += 60000;
		}
		else if (value == (SELLER_MAGIC_POTION + 1))
		{
			zyk_add_mp(ent, 200);
		}
		else if (value == (SELLER_AMMO_ALL + 1))
		{
			Add_Ammo(ent,AMMO_BLASTER,100);

			Add_Ammo(ent,AMMO_POWERCELL,100);

			Add_Ammo(ent,AMMO_METAL_BOLTS,100);

			Add_Ammo(ent,AMMO_ROCKETS,10);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent,AMMO_THERMAL,4);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent,AMMO_TRIPMINE,3);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent,AMMO_DETPACK,2);

			ent->client->ps.cloakFuel = 20;
		}
		else if (value == (SELLER_BACTA_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_BACTA);
		}
		else if (value == (SELLER_FORCE_FIELD_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_FORCE_FIELD);
		}
		else if (value == (SELLER_CLOAK_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_CLOAK);
		}
		else if (value == (SELLER_SHIELD_GENERATOR_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR);

			set_max_shield(ent);
		}
		else if (value == (SELLER_IMPACT_REDUCER_ARMOR + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR);
		}
		else if (value == (SELLER_DEFLECTIVE_ARMOR + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR);
		}
		else if (value == (SELLER_SABER_ARMOR + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_SABER_ARMOR);
		}
		else if (value == (SELLER_FLAME_THROWER + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_FLAME_THROWER);
		}
		else if (value == (SELLER_STUN_BATON_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_STUN_BATON);
		}
		else if (value == (SELLER_BLASTER_PISTOL_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_BLASTER_PISTOL);
		}
		else if (value == (SELLER_BRYAR_PISTOL_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_BRYAR_PISTOL);
		}
		else if (value == (SELLER_E11_BLASTER_RIFLE_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE);
		}
		else if (value == (SELLER_DISRUPTOR_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_DISRUPTOR);
		}
		else if (value == (SELLER_BOWCASTER_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_BOWCASTER);
		}
		else if (value == (SELLER_DEMP2_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_DEMP2);
		}
		else if (value == (SELLER_REPEATER_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_REPEATER);
		}
		else if (value == (SELLER_FLECHETTE_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_FLECHETTE);
		}
		else if (value == (SELLER_CONCUSSION_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_CONCUSSION);
		}
		else if (value == (SELLER_ROCKETS_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER);
		}
		else if (value == (SELLER_DETPACKS_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_DETPACKS);
		}
		else if (value == (SELLER_JETPACK_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_JETPACK);

			// zyk: update the rpg stuff info at the client-side game
			send_rpg_events(10000);
		}
		else if (value == (SELLER_GUNNER_RADAR + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_RADAR);

			// zyk: update the rpg stuff info at the client-side game
			send_rpg_events(10000);
		}
		else if (value == (SELLER_THERMAL_VISION + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_THERMAL_VISION);
		}
		else if (value == (SELLER_SENTRY_GUN_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_SENTRY_GUN);
		}
		else if (value == (SELLER_SEEKER_DRONE_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_SEEKER_DRONE);
		}
		else if (value == (SELLER_EWEB_UPGRADE + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_EWEB);
		}
		else if (value == (SELLER_ENERGY_MODULATOR + 1))
		{
			zyk_update_inventory_quantity(ent, qtrue, RPG_INVENTORY_UPGRADE_ENERGY_MODULATOR);
		}

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		ent->client->pers.credits -= zyk_get_seller_item_cost((value - 1), qtrue);
		save_account(ent, qtrue);

		ent->client->pers.buy_sell_timer = level.time + zyk_buying_selling_cooldown.integer;

		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7Thanks %s^7!\n\"",ent->client->pers.netname) );

		Cmd_ZykMod_f(ent);
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7%s^7, my products are not free! Give me the money!\n\"",ent->client->pers.netname) );
		return;
	}
}

// zyk: if an item left the inventory, makes some adjustments on the player
extern void Jedi_Decloak(gentity_t *self);
void zyk_adjust_holdable_items(gentity_t *ent)
{
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)))
	{ // zyk: if player has no items left, deselect the held item
		ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	// zyk: if player no longer has Cloak Item and is cloaked, decloaks
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) && ent->client->ps.powerups[PW_CLOAKED])
		Jedi_Decloak(ent);
}

	/*
==================
Cmd_Sell_f
==================
*/
void Cmd_Sell_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int value = 0;
	int found = 0;
	int sold = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must specify a product number.\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));
	value = atoi(arg1);

	// zyk: tests the cooldown time to buy or sell
	if (ent->client->pers.buy_sell_timer > level.time)
	{
		trap->SendServerCommand(ent->s.number, "print \"In Buy/Sell cooldown time.\n\"");
		return;
	}

	if (value < 1 || value > MAX_SELLER_ITEMS || zyk_get_seller_item_cost((value - 1), qfalse) == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid product number.\n\"" );
		return;
	}
	else
	{ // zyk: searches for the jawa to see if we are near him to buy or sell to him
		gentity_t *jawa_ent = NULL;
		int j = 0;

		for (j = MAX_CLIENTS; j < level.num_entities; j++)
		{
			jawa_ent = &g_entities[j];

			if (jawa_ent && jawa_ent->client && jawa_ent->NPC && jawa_ent->health > 0 && Q_stricmp( jawa_ent->NPC_type, "jawa_seller" ) == 0 && (int)Distance(ent->client->ps.origin, jawa_ent->client->ps.origin) < 90)
			{
				found = 1;
				break;
			}
		}

		if (found == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You must be near the jawa seller to sell to him.\n\"" );
			return;
		}
	}

	if (value == (SELLER_BLASTER_PACK + 1) && ent->client->ps.ammo[AMMO_BLASTER] >= 100)
	{
		ent->client->ps.ammo[AMMO_BLASTER] -= 100;
		sold = 1;
	}
	else if (value == (SELLER_POWERCELL + 1) && ent->client->ps.ammo[AMMO_POWERCELL] >= 100)
	{
		ent->client->ps.ammo[AMMO_POWERCELL] -= 100;
		sold = 1;
	}
	else if (value == (SELLER_METAL_BOLTS + 1) && ent->client->ps.ammo[AMMO_METAL_BOLTS] >= 100)
	{
		ent->client->ps.ammo[AMMO_METAL_BOLTS] -= 100;
		sold = 1;
	}
	else if (value == (SELLER_ROCKETS + 1) && ent->client->ps.ammo[AMMO_ROCKETS] >= 10)
	{
		ent->client->ps.ammo[AMMO_ROCKETS] -= 10;
		sold = 1;
	}
	else if (value == (SELLER_THERMALS + 1) && ent->client->ps.ammo[AMMO_THERMAL] >= 4)
	{
		ent->client->ps.ammo[AMMO_THERMAL] -= 4;
		sold = 1;
	}
	else if (value == (SELLER_TRIPMINES + 1) && ent->client->ps.ammo[AMMO_TRIPMINE] >= 3)
	{
		ent->client->ps.ammo[AMMO_TRIPMINE] -= 3;
		sold = 1;
	}
	else if (value == (SELLER_DETPACKS + 1) && ent->client->ps.ammo[AMMO_DETPACK] >= 2)
	{
		ent->client->ps.ammo[AMMO_DETPACK] -= 2;
		sold = 1;
	}
	else if (value == (SELLER_BACTA_CANISTER + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);

		sold = 1;
	}
	else if (value == (SELLER_BIG_BACTA + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);

		sold = 1;
	}
	else if (value == (SELLER_SENTRY_GUN + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SENTRY_GUN);

		sold = 1;
	}
	else if (value == (SELLER_SEEKER_DRONE + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER);

		sold = 1;
	}
	else if (value == (SELLER_FORCE_FIELD + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SHIELD);

		sold = 1;
	}
	else if (value == (SELLER_YSALAMIRI + 1) && ent->client->ps.powerups[PW_YSALAMIRI])
	{
		ent->client->ps.powerups[PW_YSALAMIRI] = 0;
		sold = 1;
	}
	else if (value == (SELLER_E11_BLASTER + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BLASTER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BLASTER);
		if (ent->client->ps.weapon == WP_BLASTER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_DISRUPTOR + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DISRUPTOR)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DISRUPTOR);
		if (ent->client->ps.weapon == WP_DISRUPTOR)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_REPEATER + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_REPEATER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_REPEATER);
		if (ent->client->ps.weapon == WP_REPEATER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_ROCKET_LAUNCHER + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_ROCKET_LAUNCHER);
		if (ent->client->ps.weapon == WP_ROCKET_LAUNCHER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_BOWCASTER + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BOWCASTER);
		if (ent->client->ps.weapon == WP_BOWCASTER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_BLASTER_PISTOL + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);
		if (ent->client->ps.weapon == WP_BRYAR_PISTOL)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_FLECHETTE + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_FLECHETTE)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_FLECHETTE);
		if (ent->client->ps.weapon == WP_FLECHETTE)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_CONCUSSION + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_CONCUSSION)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_CONCUSSION);
		if (ent->client->ps.weapon == WP_CONCUSSION)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_LIGHTSABER + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
		if (ent->client->ps.weapon == WP_SABER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_STUN_BATON + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_STUN_BATON)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON);
		if (ent->client->ps.weapon == WP_STUN_BATON)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_EWEB + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_EWEB);
		sold = 1;
	}
	else if (value == (SELLER_DEMP2 + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DEMP2)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DEMP2);
		if (ent->client->ps.weapon == WP_DEMP2)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_BRYAR_PISTOL + 1) && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_OLD)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_OLD);
		if (ent->client->ps.weapon == WP_BRYAR_OLD)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == (SELLER_BINOCULARS + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_BINOCULARS);
		sold = 1;
	}
	else if (value == (SELLER_CLOAK_ITEM + 1) && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_CLOAK);
		sold = 1;
	}
	else if (value == (SELLER_FORCE_BOON + 1) && ent->client->ps.powerups[PW_FORCE_BOON])
	{
		ent->client->ps.powerups[PW_FORCE_BOON] = 0;
		sold = 1;
	}
	else if (value == (SELLER_BACTA_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BACTA] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_BACTA);
		sold = 1;
	}
	else if (value == (SELLER_FORCE_FIELD_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_FORCE_FIELD] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_FORCE_FIELD);
		sold = 1;
	}
	else if (value == (SELLER_CLOAK_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_CLOAK] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_CLOAK);
		sold = 1;
	}
	else if (value == (SELLER_SHIELD_GENERATOR_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_SHIELD_GENERATOR);
		set_max_shield(ent);
		sold = 1;
	}
	else if (value == (SELLER_IMPACT_REDUCER_ARMOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_IMPACT_REDUCER_ARMOR);
		sold = 1;
	}
	else if (value == (SELLER_DEFLECTIVE_ARMOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR);
		sold = 1;
	}
	else if (value == (SELLER_SABER_ARMOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SABER_ARMOR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_SABER_ARMOR);
		sold = 1;
	}
	else if (value == (SELLER_FLAME_THROWER + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_FLAME_THROWER] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_FLAME_THROWER);
		sold = 1;
	}
	else if (value == (SELLER_STUN_BATON_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_STUN_BATON] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_STUN_BATON);
		sold = 1;
	}
	else if (value == (SELLER_BLASTER_PISTOL_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BLASTER_PISTOL] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_BLASTER_PISTOL);
		sold = 1;
	}
	else if (value == (SELLER_BRYAR_PISTOL_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BRYAR_PISTOL] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_BRYAR_PISTOL);
		sold = 1;
	}
	else if (value == (SELLER_E11_BLASTER_RIFLE_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_E11_BLASTER_RIFLE);
		sold = 1;
	}
	else if (value == (SELLER_DISRUPTOR_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DISRUPTOR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_DISRUPTOR);
		sold = 1;
	}
	else if (value == (SELLER_BOWCASTER_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_BOWCASTER] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_BOWCASTER);
		sold = 1;
	}
	else if (value == (SELLER_DEMP2_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEMP2] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_DEMP2);
		sold = 1;
	}
	else if (value == (SELLER_REPEATER_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_REPEATER] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_REPEATER);
		sold = 1;
	}
	else if (value == (SELLER_FLECHETTE_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_FLECHETTE] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_FLECHETTE);
		sold = 1;
	}
	else if (value == (SELLER_CONCUSSION_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_CONCUSSION] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_CONCUSSION);
		sold = 1;
	}
	else if (value == (SELLER_ROCKETS_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_ROCKET_LAUNCHER);
		sold = 1;
	}
	else if (value == (SELLER_DETPACKS_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DETPACKS] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_DETPACKS);
		sold = 1;
	}
	else if (value == (SELLER_JETPACK_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_JETPACK] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_JETPACK);
		sold = 1;
	}
	else if (value == (SELLER_GUNNER_RADAR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_RADAR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_RADAR);
		sold = 1;
	}
	else if (value == (SELLER_THERMAL_VISION + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_THERMAL_VISION] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_THERMAL_VISION);
		sold = 1;
	}
	else if (value == (SELLER_SENTRY_GUN_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SENTRY_GUN] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_SENTRY_GUN);
		sold = 1;
	}
	else if (value == (SELLER_SEEKER_DRONE_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_SEEKER_DRONE] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_SEEKER_DRONE);
		sold = 1;
	}
	else if (value == (SELLER_EWEB_UPGRADE + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_EWEB] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_EWEB);
		sold = 1;
	}
	else if (value == (SELLER_ENERGY_MODULATOR + 1) && ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_ENERGY_MODULATOR] > 0)
	{
		zyk_update_inventory_quantity(ent, qfalse, RPG_INVENTORY_UPGRADE_ENERGY_MODULATOR);
		sold = 1;
	}

	zyk_adjust_holdable_items(ent);
	
	if (sold == 1)
	{
		add_credits(ent, zyk_get_seller_item_cost((value - 1), qfalse));
		save_account(ent, qtrue);

		ent->client->pers.buy_sell_timer = level.time + zyk_buying_selling_cooldown.integer;

		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7Thanks %s^7!\n\"",ent->client->pers.netname) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7You don't have this item.\n\"") );
	}
}

/*
==================
Cmd_ChangePassword_f
==================
*/
void Cmd_ChangePassword_f( gentity_t *ent ) {
	char arg1[1024];

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand(ent->s.number, "print \"You must be at Spectator Mode to change password.\n\"" );
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Use ^3/changepassword <new password> ^7to change it.\n\"" );
		return;
	}

	// zyk: gets the new password
	trap->Argv(1, arg1, sizeof( arg1 ));

	if (strlen(arg1) > MAX_ACC_NAME_SIZE)
	{
		trap->SendServerCommand(ent->s.number, va("print \"The password must have a maximum of %d characters.\n\"", MAX_ACC_NAME_SIZE) );
		return;
	}

	password_encrypt(arg1, 0xFACE);
	strcpy(ent->client->pers.password,arg1);
	save_account(ent, qfalse);

	trap->SendServerCommand(ent->s.number, "print \"Your password was changed successfully.\n\"" );
}

extern void zyk_TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );

/*
==================
Cmd_Teleport_f
==================
*/
void Cmd_Teleport_f( gentity_t *ent )
{
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	char arg3[MAX_STRING_CHARS];
	char arg4[MAX_STRING_CHARS];

	if (!(ent->client->pers.bitvalue & (1 << ADM_TELE)))
	{ // zyk: teleport admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if (g_gametype.integer != GT_FFA && zyk_allow_adm_in_other_gametypes.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Teleport command not allowed in gametypes other than FFA.\n\"" );
		return;
	}

	if (trap->Argc() == 1)
	{
		zyk_TeleportPlayer(ent,ent->client->pers.teleport_point,ent->client->pers.teleport_angles);
	}
	else if (trap->Argc() == 2)
	{
		int client_id = -1;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		if (Q_stricmp(arg1, "point") == 0)
		{
			VectorCopy(ent->client->ps.origin,ent->client->pers.teleport_point);
			VectorCopy(ent->client->ps.viewangles,ent->client->pers.teleport_angles);
			trap->SendServerCommand( ent-g_entities, va("print \"Marked point %s with angles %s\n\"", vtos(ent->client->pers.teleport_point), vtos(ent->client->pers.teleport_angles)) );
			return;
		}
		else
		{
			vec3_t target_origin;

			client_id = ClientNumberFromString( ent, arg1, qfalse );

			if (client_id == -1)
			{
				return;
			}

			if (g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT)))
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
				return;
			}

			VectorCopy(g_entities[client_id].client->ps.origin,target_origin);
			target_origin[2] = target_origin[2] + 100;

			zyk_TeleportPlayer(ent,target_origin,g_entities[client_id].client->ps.viewangles);
		}
	}
	else if (trap->Argc() == 3)
	{
		// zyk: teleporting a player to another
		int client1_id;
		int client2_id;

		vec3_t target_origin;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );

		client1_id = ClientNumberFromString( ent, arg1, qfalse );
		client2_id = ClientNumberFromString( ent, arg2, qfalse );

		if (client1_id == -1)
		{
			return;
		}

		if (g_entities[client1_id].client->sess.amrpgmode > 0 && g_entities[client1_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client1_id].client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		if (client2_id == -1)
		{
			return;
		}

		if (g_entities[client2_id].client->sess.amrpgmode > 0 && g_entities[client2_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client2_id].client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		VectorCopy(g_entities[client2_id].client->ps.origin,target_origin);
		target_origin[2] = target_origin[2] + 100;

		zyk_TeleportPlayer(&g_entities[client1_id],target_origin,g_entities[client2_id].client->ps.viewangles);
	}
	else if (trap->Argc() == 4)
	{
		// zyk: teleporting to coordinates
		vec3_t target_origin;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		trap->Argv( 3,  arg3, sizeof( arg3 ) );

		VectorSet(target_origin,atoi(arg1),atoi(arg2),atoi(arg3));

		zyk_TeleportPlayer(ent,target_origin,ent->client->ps.viewangles);
	}
	else if (trap->Argc() == 5)
	{
		// zyk: teleporting a player to coordinates
		vec3_t target_origin;
		int client_id;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		if (g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		trap->Argv( 3,  arg3, sizeof( arg3 ) );
		trap->Argv( 4,  arg4, sizeof( arg4 ) );

		VectorSet(target_origin,atoi(arg2),atoi(arg3),atoi(arg4));

		zyk_TeleportPlayer(&g_entities[client_id],target_origin,g_entities[client_id].client->ps.viewangles);
	}
}

/*
==================
Cmd_CreditGive_f
==================
*/
void Cmd_CreditGive_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = 0, value = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must specify a player.\n\"" );
		return;
	}

	if (trap->Argc() == 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must specify the amount of credits.\n\"" );
		return;
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse ); 
	value = atoi(arg2);

	if (client_id == -1)
	{
		return;
	}

	if (value < 1)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Can only use positive values.\n\"") );
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"The player is not in RPG Mode\n\"") );
		return;
	}

	if ((ent->client->pers.credits - value) < 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You don't have this amount of credits\n\"") );
		return;
	}

	add_credits(&g_entities[client_id], value);
	save_account(&g_entities[client_id], qtrue);

	remove_credits(ent, value);
	save_account(ent, qtrue);

	trap->SendServerCommand( client_id, va("chat \"^3Credit System: ^7You got %d credits from %s\n\"", value, ent->client->pers.netname) );

	trap->SendServerCommand( ent-g_entities, "print \"Done.\n\"" );
}

/*
==================
Cmd_AllyList_f
==================
*/
void Cmd_AllyList_f( gentity_t *ent ) {
	char message[1024];
	int i = 0;

	strcpy(message,"");

	for (i = 0; i < level.maxclients; i++)
	{
		int shown = 0;
		gentity_t *this_ent = &g_entities[i];

		if (zyk_is_ally(ent,this_ent) == qtrue)
		{
			strcpy(message,va("%s^7%s ^3(ally)",message,this_ent->client->pers.netname));
			shown = 1;
		}
		if (this_ent && this_ent->client && this_ent->client->pers.connected == CON_CONNECTED && zyk_is_ally(this_ent,ent) == qtrue)
		{
			if (shown == 1)
				strcpy(message,va("%s ^3(added you)",message));
			else
				strcpy(message,va("%s^7%s ^3(added you)",message,this_ent->client->pers.netname));

			shown = 1;
		}

		if (shown == 1)
		{
			strcpy(message,va("%s\n",message));
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", message) );
}

void zyk_add_ally(gentity_t *ent, int client_id)
{
	if (client_id > 15)
	{
		ent->client->sess.ally2 |= (1 << (client_id - 16));
	}
	else
	{
		ent->client->sess.ally1 |= (1 << client_id);
	}
}

void zyk_remove_ally(gentity_t *ent, int client_id)
{
	if (client_id > 15)
	{
		ent->client->sess.ally2 &= ~(1 << (client_id - 16));
	}
	else
	{
		ent->client->sess.ally1 &= ~(1 << client_id);
	}
}

/*
==================
Cmd_AllyAdd_f
==================
*/
void Cmd_AllyAdd_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent->s.number, va("print \"Use ^3/allyadd <player name or id> ^7to add an ally.\n\"") );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		int client_id = -1;

		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{ // zyk: cant add allies while in Duel Tournament
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add allies while in a Duel Tournament.\n\""));
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 

		if (client_id == -1)
		{
			return;
		}

		if (client_id == (ent-g_entities))
		{ // zyk: player cant add himself as ally
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add yourself as ally\n\"") );
			return; 
		}

		if (zyk_is_ally(ent,&g_entities[client_id]) == qtrue)
		{
			trap->SendServerCommand(ent->s.number, va("print \"You already have this ally.\n\"") );
			return;
		}

		// zyk: add this player as an ally
		zyk_add_ally(ent, client_id);

		// zyk: sending event to update radar at client-side
		G_AddEvent(ent, EV_USE_ITEM14, client_id);

		trap->SendServerCommand(ent->s.number, va("print \"Added ally %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"%s^7 added you as ally\n\"", ent->client->pers.netname) );
	}
}

/*
==================
Cmd_AllyChat_f
==================
*/
void Cmd_AllyChat_f( gentity_t *ent ) { // zyk: allows chatting with allies
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_AllyChat_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_ALLY, p );
}

/*
==================
Cmd_AllyRemove_f
==================
*/
void Cmd_AllyRemove_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Use ^3/allyremove <player name or id> ^7to remove an ally.\n\"") );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		int client_id = -1;

		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{ // zyk: cant remove allies while in Duel Tournament
			trap->SendServerCommand(ent->s.number, va("print \"You cannot remove allies while in a Duel Tournament.\n\""));
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 

		if (client_id == -1)
		{
			return;
		}

		if (zyk_is_ally(ent,&g_entities[client_id]) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"You do not have this ally.\n\"") );
			return;
		}

		// zyk: removes this ally
		zyk_remove_ally(ent, client_id);

		// zyk: sending event to update radar at client-side
		G_AddEvent(ent, EV_USE_ITEM14, (client_id + MAX_CLIENTS));

		trap->SendServerCommand( ent-g_entities, va("print \"Removed ally %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"%s^7 removed you as ally\n\"", ent->client->pers.netname) );
	}
}

/*
==================
Cmd_Settings_f
==================
*/
void Cmd_Settings_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		char message[1024];
		strcpy(message,"");

		if (ent->client->pers.player_settings & (1 << SETTINGS_RPG_QUESTS))
		{
			strcpy(message, va("\n^3 %d - Quests - ^1OFF", SETTINGS_RPG_QUESTS));
		}
		else
		{
			strcpy(message, va("\n^3 %d - Quests - ^2ON", SETTINGS_RPG_QUESTS));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_FORCE_FROM_ALLIES))
		{
			strcpy(message, va("%s\n^3 %d - Allow Force Powers from allies - ^1OFF", message, SETTINGS_FORCE_FROM_ALLIES));
		}
		else
		{
			strcpy(message, va("%s\n^3 %d - Allow Force Powers from allies - ^2ON", message, SETTINGS_FORCE_FROM_ALLIES));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_SCREEN_MESSAGE))
		{
			strcpy(message, va("%s\n^3 %d - Allow Screen Message - ^1OFF", message, SETTINGS_SCREEN_MESSAGE));
		}
		else
		{
			strcpy(message, va("%s\n^3 %d - Allow Screen Message - ^2ON", message, SETTINGS_SCREEN_MESSAGE));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_HEAL_ALLY))
		{
			strcpy(message, va("%s\n^3%d - Use healing force only at allied players - ^1OFF", message, SETTINGS_HEAL_ALLY));
		}
		else
		{
			strcpy(message, va("%s\n^3%d - Use healing force only at allied players - ^2ON", message, SETTINGS_HEAL_ALLY));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_SABER_START))
		{
			strcpy(message, va("%s\n^3%d - Start With Saber ^1OFF", message, SETTINGS_SABER_START));
		}
		else
		{
			strcpy(message, va("%s\n^3%d - Start With Saber ^2ON", message, SETTINGS_SABER_START));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_JETPACK))
		{
			strcpy(message, va("%s\n^3%d - Jetpack ^1OFF", message, SETTINGS_JETPACK));
		}
		else
		{
			strcpy(message, va("%s\n^3%d - Jetpack ^2ON", message, SETTINGS_JETPACK));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_ADMIN_PROTECT))
		{
			strcpy(message, va("%s\n^3%d - Admin Protect ^1OFF", message, SETTINGS_ADMIN_PROTECT));
		}
		else
		{
			strcpy(message, va("%s\n^3%d - Admin Protect ^2ON", message, SETTINGS_ADMIN_PROTECT));
		}

		if (ent->client->pers.player_settings & (1 << SETTINGS_BOSS_MUSIC))
		{
			strcpy(message, va("%s\n^3%d - Custom Music ^1OFF", message, SETTINGS_BOSS_MUSIC));
		}
		else
		{
			strcpy(message, va("%s\n^3%d - Custom Music ^2ON", message, SETTINGS_BOSS_MUSIC));
		}

		trap->SendServerCommand( ent->s.number, va("print \"%s\n\n^7Choose a setting above and use ^3/settings <number> ^7to turn it ^2ON ^7or ^1OFF^7\n\"", message) );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char new_status[32];
		int value = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		value = atoi(arg1);

		if (value < 0 || value > MAX_PLAYER_SETTINGS)
		{
			trap->SendServerCommand( ent->s.number, "print \"Invalid settings value.\n\"" );
			return;
		}

		if (ent->client->pers.player_settings & (1 << value))
		{
			ent->client->pers.player_settings &= ~(1 << value);
			strcpy(new_status,"^2ON^7");
		}
		else
		{
			ent->client->pers.player_settings |= (1 << value);
			strcpy(new_status,"^1OFF^7");
		}

		save_account(ent, qfalse);

		if (value == SETTINGS_RPG_QUESTS)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Quests %s\n\"", new_status) );
		}
		else if (value == SETTINGS_FORCE_FROM_ALLIES)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Allow Force Powers from allies %s\n\"", new_status) );
		}
		else if (value == SETTINGS_SCREEN_MESSAGE)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Allow Screen Message %s\n\"", new_status) );
		}
		else if (value == SETTINGS_HEAL_ALLY)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Use healing force only at allied players %s\n\"", new_status) );
		}
		else if (value == SETTINGS_SABER_START)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Start With Saber %s\n\"", new_status) );
		}
		else if (value == SETTINGS_JETPACK)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Jetpack %s\n\"", new_status) );
		}
		else if (value == SETTINGS_ADMIN_PROTECT)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Admin Protect %s\n\"", new_status) );
		}
		else if (value == SETTINGS_BOSS_MUSIC)
		{
			trap->SendServerCommand( ent->s.number, va("print \"Custom Music %s\n\"", new_status) );
		}

		if (value == SETTINGS_RPG_QUESTS && ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->sess.amrpgmode == 2)
		{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
			G_Kill(ent);
		}

		Cmd_ZykMod_f(ent);
	}
}

/*
==================
Cmd_BountyQuest_f
==================
*/
void Cmd_BountyQuest_f( gentity_t *ent ) {
	gentity_t *this_ent = NULL;

	if (zyk_allow_bounty_quest.integer != 1)
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Bounty Quest: ^7this quest is not allowed in this server\n\"") );
		return;
	}

	// zyk: reached MAX_CLIENTS, reset it to 0
	if (level.bounty_quest_target_id == level.maxclients)
		level.bounty_quest_target_id = 0;

	if (level.bounty_quest_choose_target == qtrue)
	{ // zyk: no one is the target, so choose one player to be the target
		while (level.bounty_quest_target_id < level.maxclients)
		{
			this_ent = &g_entities[level.bounty_quest_target_id];

			if (this_ent && this_ent->client && this_ent->client->sess.amrpgmode == 2 && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && 
				!(this_ent->client->pers.player_statuses & (1 << 26)))
			{
				level.bounty_quest_choose_target = qfalse;
				trap->SendServerCommand( -1, va("chat \"^3Bounty Quest: ^7A reward of ^3%d ^7credits will be given to who kills %s^7\n\"", (this_ent->client->pers.level*15), this_ent->client->pers.netname) );
				return;
			}

			level.bounty_quest_target_id++;
		}
		trap->SendServerCommand( -1, va("chat \"^3Bounty Quest: ^7No one was chosen as the target\n\"") );
	}
	else
	{ // zyk: there is already a target player
		this_ent = &g_entities[level.bounty_quest_target_id];
		if (this_ent && this_ent->client)
			trap->SendServerCommand( -1, va("chat \"^3Bounty Quest: ^7%s ^7is already the target\n\"", this_ent->client->pers.netname) );
	}
}

/*
==================
Cmd_PlayerMode_f
==================
*/
void Cmd_PlayerMode_f( gentity_t *ent ) {
	// zyk: cannot change mode if player is in Duel Tournament or Sniper Battle
	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot change account mode while in a Duel Tournament\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot change account mode while in a Sniper Battle\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot change account mode while in a Melee Battle\n\"");
		return;
	}

	if (level.rpg_lms_mode > 0 && level.rpg_lms_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot change account mode while in a RPG LMS Battle\n\"");
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{
		ent->client->sess.amrpgmode = 1;
	}
	else if (zyk_allow_rpg_mode.integer > 0)
	{
		ent->client->sess.amrpgmode = 2;

		// zyk: removing the /give stuff, which is not allowed to RPG players
		ent->client->pers.player_statuses &= ~(1 << 12);
		ent->client->pers.player_statuses &= ~(1 << 13);
	}

	save_account(ent, qfalse);

	if (ent->client->sess.amrpgmode == 1)
	{
		ent->client->ps.fd.forcePowerMax = zyk_max_force_power.integer;

		// zyk: setting default max hp and shield
		ent->client->ps.stats[STAT_MAX_HEALTH] = 100;

		if (ent->health > 100)
			ent->health = 100;

		if (ent->client->ps.stats[STAT_ARMOR] > 100)
			ent->client->ps.stats[STAT_ARMOR] = 100;

		// zyk: reset the force powers of this player
		WP_InitForcePowers(ent);

		if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0 &&
			level.gametype != GT_JEDIMASTER && level.gametype != GT_SIEGE
			)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

		if (level.gametype != GT_JEDIMASTER && level.gametype != GT_SIEGE)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

		zyk_load_common_settings(ent);

		if (level.bounty_quest_choose_target == qfalse && level.bounty_quest_target_id == ent->s.number)
		{ // zyk: if this player was the target, remove it so the bounty quest can get a new target
			level.bounty_quest_choose_target = qtrue;
			level.bounty_quest_target_id++;
		}

		trap->SendServerCommand( ent-g_entities, "print \"^7You are now in ^2Admin-Only mode^7.\n\"" );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"^7You are now in ^2RPG mode^7.\n\"" );
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->sess.amrpgmode == 2)
	{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
		if (ent && ent->client)
		{
			ent->client->pers.player_statuses |= (1 << 24);
		}

		G_Kill(ent);
	}
}

/*
==================
Cmd_ZykFile_f
==================
*/
void Cmd_ZykFile_f(gentity_t *ent) {
	int page = 1; // zyk: page the user wants to see
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	char file_content[MAX_STRING_CHARS * 4];
	char content[MAX_STRING_CHARS];
	int i = 0;
	int results_per_page = zyk_list_cmds_results_per_page.integer; // zyk: number of results per page
	FILE *server_file = NULL;
	strcpy(file_content, "");
	strcpy(content, "");

	if (trap->Argc() < 3)
	{
		trap->SendServerCommand(ent->s.number, "print \"Use ^3/zykfile <filename> <page number> ^7to see the results of this page or a search string. Example: ^3/zykfile npclist 1 ^7or to do a search ^3/zykfile npclist reborn^7\n\"");
		return;
	}

	// zyk: filename
	trap->Argv(1, arg1, sizeof(arg1));

	// zyk: page number or search string
	trap->Argv(2, arg2, sizeof(arg2));
	page = atoi(arg2);

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	server_file = fopen(va("zykmod/%s.txt", arg1), "r");
	if (server_file != NULL)
	{
		if (page > 0)
		{ // zyk: show results of this page
			while (i < (results_per_page * (page - 1)) && fgets(content, sizeof(content), server_file) != NULL)
			{ // zyk: reads the file until it reaches the position corresponding to the page number
				i++;
			}

			while (i < (results_per_page * page) && fgets(content, sizeof(content), server_file) != NULL)
			{ // zyk: fgets returns NULL at EOF
				strcpy(file_content, va("%s%s", file_content, content));
				i++;
			}
		}
		else
		{ // zyk: search for the string
			while (i < results_per_page && fgets(content, MAX_STRING_CHARS, server_file) != NULL)
			{ // zyk: fgets returns NULL at EOF
				if (strstr(content, arg2))
				{
					strcpy(file_content, va("%s%s", file_content, content));
					i++;
				}
			}
		}

		fclose(server_file);
		trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\"", file_content));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"This file does not exist\n\"");
	}
}

void zyk_spawn_race_line(int x, int y, int z, int yaw)
{
	gentity_t *new_ent_line = G_Spawn();

	// zyk: starting line
	zyk_set_entity_field(new_ent_line, "classname", "fx_runner");
	zyk_set_entity_field(new_ent_line, "targetname", "zyk_race_line");
	new_ent_line->s.modelindex = G_EffectIndex("mp/crystalbeamred");
	zyk_set_entity_field(new_ent_line, "origin", va("%d %d %d", x, y, z));
	zyk_set_entity_field(new_ent_line, "angles", va("0 %d 0", yaw));

	zyk_spawn_entity(new_ent_line);
}

/*
==================
Cmd_RaceMode_f
==================
*/
void Cmd_RaceMode_f( gentity_t *ent ) {
	if (zyk_allow_race_mode.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Race System: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join race while being in nofight mode\n\"");
		return;
	}

	if (ent->client->pers.race_position == 0)
	{
		int j = 0, swoop_number = -1;
		int occupied_positions[MAX_CLIENTS]; // zyk: sets 1 to each race position already occupied by a player
		gentity_t *this_ent = NULL;
		vec3_t origin, yaw;
		char zyk_info[MAX_INFO_STRING] = {0};
		char zyk_mapname[128] = {0};

		if (level.gametype == GT_CTF)
		{
			trap->SendServerCommand(ent->s.number, "print \"Races are not allowed in CTF.\n\"");
			return;
		}

		if (level.race_mode > 1)
		{
			trap->SendServerCommand(ent->s.number, "print \"Race has already started. Try again at the next race!\n\"");
			return;
		}

		// zyk: getting the map name
		trap->GetServerinfo(zyk_info, sizeof(zyk_info));
		Q_strncpyz(zyk_mapname, Info_ValueForKey( zyk_info, "mapname" ), sizeof(zyk_mapname));

		if (Q_stricmp(zyk_mapname, "t2_trip") == 0)
		{
			level.race_map = 1;

			// zyk: initializing array of occupied_positions
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				occupied_positions[j] = 0;
			}

			// zyk: calculates which position the swoop of this player must be spawned
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				this_ent = &g_entities[j];
				if (this_ent && ent != this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->pers.race_position > 0)
					occupied_positions[this_ent->client->pers.race_position - 1] = 1;
			}

			for (j = 0; j < MAX_RACERS; j++)
			{
				if (occupied_positions[j] == 0)
				{ // zyk: an empty race position, use this one
					swoop_number = j;
					break;
				}
			}

			if (swoop_number == -1)
			{ // zyk: exceeded the MAX_RACERS
				trap->SendServerCommand( ent-g_entities, "print \"The race is already full of racers! Try again later!\n\"" );
				return;
			}
			
			origin[0] = -3930;
			origin[1] = (-20683 + (swoop_number * 80));
			origin[2] = 1509;

			yaw[0] = 0.0f;
			yaw[1] = -179.0f;
			yaw[2] = 0.0f;

			if (level.race_mode == 0)
			{ // zyk: if this is the first player entering the race, clean the old race swoops left in the map and place start and finish lines
				int k = 0;

				// zyk: starting line
				zyk_spawn_race_line(-4568, -20820, 1494, 90);
				zyk_spawn_race_line(-4568, -18720, 1494, -90);

				// zyk: finish line
				zyk_spawn_race_line(4750, -9989, 1520, 179);
				zyk_spawn_race_line(3225, -9962, 1520, -1);

				for (k = 0; k < MAX_RACERS; k++)
				{
					if (level.race_mode_vehicle[k] != -1)
					{
						gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[k]];
						if (vehicle_ent)
						{
							G_FreeEntity(vehicle_ent);
						}
						
						level.race_mode_vehicle[k] = -1;
					}
				}
			}

			if (swoop_number < MAX_RACERS)
			{
				// zyk: removing a possible swoop that was in the same position by a player who tried to race before in this position
				if (level.race_mode_vehicle[swoop_number] != -1)
				{
					gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[swoop_number]];

					if (vehicle_ent && vehicle_ent->NPC && Q_stricmp(vehicle_ent->NPC_type, "swoop") == 0)
					{
						G_FreeEntity(vehicle_ent);
					}
				}

				// zyk: teleporting player to the swoop area
				zyk_TeleportPlayer( ent, origin, yaw);

				ent->client->pers.race_position = swoop_number + 1;

				this_ent = NPC_SpawnType(ent,"swoop",NULL,qtrue);
				if (this_ent)
				{ // zyk: setting the vehicle hover height and hover strength
					this_ent->m_pVehicle->m_pVehicleInfo->hoverHeight = 40.0;
					this_ent->m_pVehicle->m_pVehicleInfo->hoverStrength = 40.0;

					level.race_mode_vehicle[swoop_number] = this_ent->s.number;
				}

				level.race_start_timer = level.time + zyk_start_race_timer.integer; // zyk: race will start some seconds after the last player who joined the race
				level.race_mode = 1;

				trap->SendServerCommand( -1, va("chat \"^3Race System: ^7%s ^7joined the race!\n\"",ent->client->pers.netname) );
			}
		}
		else if (Q_stricmp(zyk_mapname, "t3_stamp") == 0)
		{
			int i = 0;

			level.race_map = 2;

			// zyk: initializing array of occupied_positions
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				occupied_positions[j] = 0;
			}

			// zyk: calculates which position the swoop of this player must be spawned
			for (j = 0; j < MAX_CLIENTS; j++)
			{
				this_ent = &g_entities[j];
				if (this_ent && ent != this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.sessionTeam != TEAM_SPECTATOR && this_ent->client->pers.race_position > 0)
					occupied_positions[this_ent->client->pers.race_position - 1] = 1;
			}

			for (j = 0; j < MAX_RACERS; j++)
			{
				if (occupied_positions[j] == 0)
				{ // zyk: an empty race position, use this one
					swoop_number = j;
					break;
				}
			}

			if (swoop_number == -1)
			{ // zyk: exceeded the MAX_RACERS
				trap->SendServerCommand(ent - g_entities, "print \"The race is already full of racers! Try again later!\n\"");
				return;
			}

			origin[0] = (1020 - ((swoop_number % 4) * 90));
			origin[1] = (1370 + ((swoop_number/4) * 90));
			origin[2] = 97;

			yaw[0] = 0.0f;
			yaw[1] = -90.0f;
			yaw[2] = 0.0f;

			if (level.race_mode == 0)
			{ // zyk: if this is the first player entering the race, clean the old race swoops left in the map
				int k = 0;

				for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
				{ // zyk: removing all entities except the spawnpoints
					gentity_t *removed_ent = &g_entities[i];

					if (removed_ent && Q_stricmp(removed_ent->classname, "func_breakable") == 0 && removed_ent->s.number >= 471 && removed_ent->s.number <= 472)
					{
						GlobalUse(removed_ent, removed_ent, removed_ent);
					}
					else if (removed_ent && Q_stricmp(removed_ent->classname, "info_player_deathmatch") != 0)
					{
						G_FreeEntity(removed_ent);
					}
				}

				for (k = 0; k < MAX_RACERS; k++)
				{
					if (level.race_mode_vehicle[k] != -1)
					{
						gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[k]];
						if (vehicle_ent)
						{
							G_FreeEntity(vehicle_ent);
						}

						level.race_mode_vehicle[k] = -1;
					}
				}

				// zyk: starting line
				zyk_spawn_race_line(660, 1198, 88, 1);
				zyk_spawn_race_line(1070, 1198, 88, 179);

				// zyk: finish line
				zyk_spawn_race_line(-6425, -168, -263, -1);
				zyk_spawn_race_line(-5700, -180, -263, 179);
			}

			if (swoop_number < MAX_RACERS)
			{
				// zyk: removing a possible swoop that was in the same position by a player who tried to race before in this position
				if (level.race_mode_vehicle[swoop_number] != -1)
				{
					gentity_t *vehicle_ent = &g_entities[level.race_mode_vehicle[swoop_number]];

					if (vehicle_ent && vehicle_ent->NPC && Q_stricmp(vehicle_ent->NPC_type, "tauntaun") == 0)
					{
						G_FreeEntity(vehicle_ent);
					}
				}

				// zyk: teleporting player to the swoop area
				zyk_TeleportPlayer(ent, origin, yaw);

				ent->client->pers.race_position = swoop_number + 1;

				this_ent = NPC_SpawnType(ent, "tauntaun", NULL, qtrue);
				if (this_ent)
				{ // zyk: setting the vehicle id and increasing tauntaun hp
					this_ent->health *= 5;
					level.race_mode_vehicle[swoop_number] = this_ent->s.number;
				}

				level.race_start_timer = level.time + zyk_start_race_timer.integer; // zyk: race will start some seconds after the last player who joined the race
				level.race_mode = 1;

				trap->SendServerCommand(-1, va("chat \"^3Race System: ^7%s ^7joined the race!\n\"", ent->client->pers.netname));
			}
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Races can only be done in ^3t2_trip ^7and ^3t3_stamp ^7maps.\n\"" );
		}
	}
	else
	{
		trap->SendServerCommand( -1, va("chat \"^3Race System: ^7%s ^7abandoned the race!\n\"",ent->client->pers.netname) );

		ent->client->pers.race_position = 0;
		try_finishing_race();
	}
}

/*
==================
Cmd_Drop_f
==================
*/
extern qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
void Cmd_Drop_f( gentity_t *ent ) {
	vec3_t vel;
	gitem_t *item = NULL;
	gentity_t *launched;
	int weapon = ent->client->ps.weapon;
	vec3_t uorg, vecnorm, thispush_org;
	int current_ammo = 0;
	int ammo_count = 0;

	if (weapon == WP_NONE || weapon == WP_EMPLACED_GUN || weapon == WP_TURRET)
	{ //can't have this
		return;
	}

	VectorCopy(ent->client->ps.origin, thispush_org);

	VectorCopy(ent->client->ps.origin, uorg);
	uorg[2] += 64;

	VectorSubtract(uorg, thispush_org, vecnorm);
	VectorNormalize(vecnorm);

	if (weapon == WP_SABER)
	{
		vel[0] = vecnorm[0]*100;
		vel[1] = vecnorm[1]*100;
		vel[2] = vecnorm[2]*100;
		saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum],ent,vel);
		return;
	}

	// zyk: velocity with which the item will be tossed
	vel[0] = vecnorm[0] * 500;
	vel[1] = vecnorm[1] * 500;
	vel[2] = vecnorm[2] * 500;

	// zyk: when using melee, drop items
	if (weapon == WP_MELEE && ent->client->ps.stats[STAT_HOLDABLE_ITEM] > 0 && 
		bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giType == IT_HOLDABLE)
	{
		item = BG_FindItemForHoldable(bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag);

		if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << item->giTag))
		{ // zyk: if player has the item in inventory, drop it
			launched = LaunchItem(item, ent->client->ps.origin, vel);

			// zyk: this player cannot get this item for 1 second
			launched->genericValue10 = level.time + 1000;
			launched->genericValue11 = ent->s.number;

			// zyk: remove item from inventory
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << item->giTag);

			zyk_adjust_holdable_items(ent);
		}
	}
	else if (weapon != WP_MELEE)
	{
		// find the item type for this weapon
		item = BG_FindItemForWeapon((weapon_t)weapon);

		launched = LaunchItem(item, ent->client->ps.origin, vel);

		launched->s.generic1 = ent->s.number;
		launched->s.powerups = level.time + 1500;

		launched->count = bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

		// zyk: setting amount of ammo in this dropped weapon
		current_ammo = ent->client->ps.ammo[weaponData[weapon].ammoIndex];
		ammo_count = (int)ceil(bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity * zyk_add_ammo_scale.value);

		if (current_ammo < ammo_count)
		{ // zyk: player does not have the default ammo to set in the weapon, so set the current_ammo of the player in the weapon
			ent->client->ps.ammo[weaponData[weapon].ammoIndex] -= current_ammo;
			if (zyk_add_ammo_scale.value > 0 && current_ammo > 0)
				launched->count = (current_ammo / zyk_add_ammo_scale.value);
			else
				launched->count = -1; // zyk: in this case, player has no ammo, so weapon should add no ammo to the player who picks up this weapon
		}
		else
		{
			ent->client->ps.ammo[weaponData[weapon].ammoIndex] -= ammo_count;
			if (zyk_add_ammo_scale.value > 0 && current_ammo > 0)
				launched->count = (ammo_count / zyk_add_ammo_scale.value);
			else
				launched->count = -1; // zyk: in this case, player has no ammo, so weapon should add no ammo to the player who picks up this weapon
		}

		if ((ent->client->ps.ammo[weaponData[weapon].ammoIndex] < 1 && weapon != WP_DET_PACK) ||
			(weapon != WP_THERMAL && weapon != WP_DET_PACK && weapon != WP_TRIP_MINE))
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << weapon);

			ent->s.weapon = WP_MELEE;
			ent->client->ps.weapon = WP_MELEE;
		}
	}
}

/*
==================
Cmd_Jetpack_f
==================
*/
void Cmd_Jetpack_f( gentity_t *ent ) {
	if (level.melee_mode > 1 && level.melee_players[ent->s.number] != -1)
	{ // zyk: cannot get jetpack in Melee Battle
		return;
	}

	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) && zyk_allow_jetpack_command.integer && 
		 ent->client->sess.amrpgmode < 2 &&
		(level.gametype != GT_SIEGE || zyk_allow_jetpack_in_siege.integer) && level.gametype != GT_JEDIMASTER && 
		!(ent->client->pers.player_statuses & (1 << 12)))
	{ // zyk: gets jetpack if player does not have it. RPG players need jetpack skill to get it
		// zyk: Jedi Master gametype will not allow jetpack
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
	}
	else
	{
		if (ent->client->jetPackOn)
			Jetpack_Off(ent);

		// zyk: player in RPG Mode drops his jetpack
		if (ent->client->sess.amrpgmode == 2 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK))
		{
			vec3_t vel;
			gitem_t* item = NULL;
			gentity_t* launched = NULL;
			vec3_t uorg, vecnorm, thispush_org;

			VectorCopy(ent->client->ps.origin, thispush_org);

			VectorCopy(ent->client->ps.origin, uorg);
			uorg[2] += 64;

			VectorSubtract(uorg, thispush_org, vecnorm);
			VectorNormalize(vecnorm);

			// zyk: velocity with which the item will be tossed
			vel[0] = vecnorm[0] * 500;
			vel[1] = vecnorm[1] * 500;
			vel[2] = vecnorm[2] * 500;

			item = BG_FindItemForHoldable(HI_JETPACK);

			launched = LaunchItem(item, ent->client->ps.origin, vel);

			// zyk: this player cannot get this item for 1 second
			launched->genericValue10 = level.time + 1000;
			launched->genericValue11 = ent->s.number;
		}

		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK);
	}
}

/*
==================
Cmd_Remap_f
==================
*/
extern shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];
void Cmd_Remap_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	float f = level.time * 0.001;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent->s.number, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 3)
	{
		trap->SendServerCommand( ent->s.number, va("print \"You must specify the old shader and new shader. Ex: ^3/remap models/weapons2/heavy_repeater/heavy_repeater_w.glm models/items/bacta^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	// zyk: validating the shader names size
	if (strlen(arg1) >= MAX_QPATH)
	{
		trap->SendServerCommand(ent->s.number, "print \"Old shader name is too big.\n\"");
		return;
	}

	if (strlen(arg2) >= MAX_QPATH)
	{
		trap->SendServerCommand(ent->s.number, "print \"New shader name is too big.\n\"");
		return;
	}

	AddRemap(G_NewString(arg1), G_NewString(arg2), f);
	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	trap->SendServerCommand( ent->s.number, "print \"Shader remapped\n\"" );
}

/*
==================
Cmd_RemapList_f
==================
*/
void Cmd_RemapList_f(gentity_t *ent) {
	int page = 1; // zyk: page the user wants to see
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char content[MAX_STRING_CHARS];
	int i = 0;
	int results_per_page = 8; // zyk: number of results per page

	strcpy(content, "");

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand(ent->s.number, "print \"You don't have this admin command.\n\"");
		return;
	}

	if (number_of_args < 2)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You must specify the page. Example: ^3/remaplist 1^7\n\""));
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	
	page = atoi(arg1);

	if (page == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	// zyk: makes i start from the first result of the correct page
	i = results_per_page * (page - 1);

	while (i < (results_per_page * page) && i < zyk_get_remap_count())
	{
		strcpy(content, va("%s%s - %s\n", content, remappedShaders[i].oldShader, remappedShaders[i].newShader));
		i++;
	}

	trap->SendServerCommand(ent->s.number, va("print \"\n^3Old Shader   -   New Shader\n\n^7%s\n\"", content));
}

/*
==================
Cmd_RemapDeleteFile_f
==================
*/
void Cmd_RemapDeleteFile_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	this_file = fopen(va("zykmod/remaps/%s/%s.txt",zyk_mapname,arg1),"r");
	if (this_file)
	{
		fclose(this_file);

		remove(va("zykmod/remaps/%s/%s.txt",zyk_mapname,arg1));

		trap->SendServerCommand( ent-g_entities, va("print \"File %s deleted from server\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_RemapSave_f
==================
*/
void Cmd_RemapSave_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	int i = 0;
	FILE *remap_file = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the file name\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	// zyk: saving remaps in the file
	remap_file = fopen(va("zykmod/remaps/%s/%s.txt",zyk_mapname,arg1),"w");
	for (i = 0; i < zyk_get_remap_count(); i++)
	{
		fprintf(remap_file,"%s\n%s\n%f\n",remappedShaders[i].oldShader,remappedShaders[i].newShader,remappedShaders[i].timeOffset);
	}
	fclose(remap_file);

	trap->SendServerCommand( ent-g_entities, va("print \"Remaps saved in %s file\n\"", arg1) );
}

/*
==================
Cmd_RemapLoad_f
==================
*/
void Cmd_RemapLoad_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	char old_shader[128];
	char new_shader[128];
	char time_offset[128];
	FILE *remap_file = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the file name\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	strcpy(old_shader,"");
	strcpy(new_shader,"");
	strcpy(time_offset,"");

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	// zyk: loading remaps from the file
	remap_file = fopen(va("zykmod/remaps/%s/%s.txt",zyk_mapname,arg1),"r");
	if (remap_file)
	{
		while(fscanf(remap_file,"%s",old_shader) != EOF)
		{
			fscanf(remap_file,"%s",new_shader);
			fscanf(remap_file,"%s",time_offset);

			AddRemap(G_NewString(old_shader), G_NewString(new_shader), atof(time_offset));
		}
		
		fclose(remap_file);

		trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

		trap->SendServerCommand( ent-g_entities, va("print \"Remaps loaded from %s file\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Remaps not loaded. File %s not found\n\"", arg1) );
	}
}

/*
==================
Cmd_EntUndo_f
==================
*/
void Cmd_EntUndo_f(gentity_t *ent) {
	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand(ent - g_entities, "print \"You don't have this admin command.\n\"");
		return;
	}

	if (level.last_spawned_entity)
	{ // zyk: removes the last entity spawned by /entadd command
		trap->SendServerCommand(ent->s.number, va("print \"Entity %d cleaned\n\"", level.last_spawned_entity->s.number));

		G_FreeEntity(level.last_spawned_entity);

		level.last_spawned_entity = NULL;
	}
}

/*
==================
Cmd_EntOrigin_f
==================
*/
void Cmd_EntOrigin_f(gentity_t *ent) {
	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand(ent - g_entities, "print \"You don't have this admin command.\n\"");
		return;
	}

	if (level.ent_origin_set == qfalse)
	{
		VectorCopy(ent->client->ps.origin, level.ent_origin);
		VectorCopy(ent->client->ps.viewangles, level.ent_angles);
		level.ent_origin_set = qtrue;

		trap->SendServerCommand(ent->s.number, va("print \"Entity origin: (%f %f %f) angles: (%f %f %f)\n\"", level.ent_origin[0], level.ent_origin[1], level.ent_origin[2], level.ent_angles[0], level.ent_angles[1], level.ent_angles[2]));
	}
	else
	{
		level.ent_origin_set = qfalse;
		trap->SendServerCommand(ent->s.number, "print \"Entity origin unset\n\"");
	}
}

/*
==================
Cmd_EntAdd_f
==================
*/
void Cmd_EntAdd_f( gentity_t *ent ) {
	gentity_t *new_ent = NULL;
	int number_of_args = trap->Argc();
	int i = 0;
	char key[64];
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	qboolean has_origin_set = qfalse; // zyk: if player do not pass an origin key, use the one set with /entorigin
	qboolean has_angles_set = qfalse; // zyk: if player do not pass an angles key, use the one set with /entorigin

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify at least the entity class. Ex: ^3/entadd info_player_deathmatch^7, which spawns a spawn point in the map\n\"") );
		return;
	}

	if ( number_of_args % 2 != 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify an even number of arguments after the spawnflags, because they are key/value pairs\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// zyk: spawns the new entity
	new_ent = G_Spawn();

	if (new_ent)
	{
		strcpy(key,"");

		// zyk: setting the entity classname
		zyk_main_set_entity_field(new_ent, "classname", G_NewString(arg1));

		for(i = 2; i < number_of_args; i++)
		{
			if (i % 2 == 0)
			{ // zyk: key
				trap->Argv( i, arg2, sizeof( arg2 ) );
				strcpy(key, G_NewString(arg2));

				if (Q_stricmp(key, "origin") == 0)
				{ // zyk: if origin was passed
					has_origin_set = qtrue;
				}

				if (Q_stricmp(key, "angles") == 0)
				{ // zyk: if angles was passed
					has_angles_set = qtrue;
				}
			}
			else
			{ // zyk: value
				trap->Argv( i, arg2, sizeof( arg2 ) );

				zyk_main_set_entity_field(new_ent, G_NewString(key), G_NewString(arg2));
			}
		}

		if (level.ent_origin_set == qtrue && (has_origin_set == qfalse || has_angles_set == qfalse))
		{ // zyk: if origin or angles were not passed, use the origin or angles set with /entorigin
			if (has_origin_set == qfalse)
			{
				zyk_main_set_entity_field(new_ent, "origin", G_NewString(va("%f %f %f", level.ent_origin[0], level.ent_origin[1], level.ent_origin[2])));
			}

			if (has_angles_set == qfalse)
			{
				zyk_main_set_entity_field(new_ent, "angles", G_NewString(va("%f %f %f", level.ent_angles[0], level.ent_angles[1], level.ent_angles[2])));
			}
		}
		else if (has_origin_set == qfalse)
		{ // zyk: origin field was not passed, so spawn entity where player is aiming at
			trace_t		tr;
			vec3_t		tfrom, tto, fwd;
			vec3_t		shot_mins, shot_maxs;
			int radius = 32768;

			VectorSet(tfrom, ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2] + 35);

			AngleVectors(ent->client->ps.viewangles, fwd, NULL, NULL);
			tto[0] = tfrom[0] + fwd[0] * radius;
			tto[1] = tfrom[1] + fwd[1] * radius;
			tto[2] = tfrom[2] + fwd[2] * radius;

			VectorSet(shot_mins, -5, -5, -5);
			VectorSet(shot_maxs, 5, 5, 5);

			trap->Trace(&tr, tfrom, shot_mins, shot_maxs, tto, ent->s.number, CONTENTS_SOLID, qfalse, 0, 0);

			if (tr.fraction != 1.0)
			{ // zyk: hit something
				zyk_main_set_entity_field(new_ent, "origin", G_NewString(va("%f %f %f", tr.endpos[0], tr.endpos[1], tr.endpos[2])));
			}
		}

		zyk_main_spawn_entity(new_ent);

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Entity %d spawned\n\"", new_ent->s.number) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Error in entity spawn\n\"") );
		return;
	}
}

/*
==================
Cmd_EntEdit_f
==================
*/
void Cmd_EntEdit_f( gentity_t *ent ) {
	gentity_t *this_ent = NULL;
	int number_of_args = trap->Argc();
	int entity_id = -1;
	int i = 0;
	char key[64];
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify at least the entity ID.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	entity_id = atoi(arg1);

	if (entity_id < 0 || entity_id >= level.num_entities)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Invalid Entity ID.\n\"") );
		return;
	}

	this_ent = &g_entities[entity_id];

	if (number_of_args == 2)
	{
		// zyk: players have their origin and yaw set in ps struct
		if (entity_id < (MAX_CLIENTS + BODY_QUEUE_SIZE))
			trap->SendServerCommand( ent-g_entities, va("print \"\n^3classname: ^7%s\n^3origin: ^7%f %f %f\n\n\"", this_ent->classname, this_ent->r.currentOrigin[0], this_ent->r.currentOrigin[1], this_ent->r.currentOrigin[2]) );
		else
		{
			char content[1024];

			strcpy(content, "");

			if (this_ent->inuse)
			{ 
				while (i < level.zyk_spawn_strings_values_count[entity_id])
				{
					strcpy(content, va("%s^3%s: ^7%s\n", content, level.zyk_spawn_strings[this_ent->s.number][i], level.zyk_spawn_strings[this_ent->s.number][i + 1]));

					i += 2;
				}

				trap->SendServerCommand(ent - g_entities, va("print \"\n%s\n\"", content));
			}
			else
			{
				trap->SendServerCommand(ent - g_entities, va("print \"Entity %d is not in use\n\"", entity_id));
			}
		}
	}
	else
	{
		if ( number_of_args % 2 != 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"You must specify an even number of arguments, because they are key/value pairs.\n\"") );
			return;
		}

		strcpy(key,"");

		for(i = 2; i < number_of_args; i++)
		{
			if (i % 2 == 0)
			{ // zyk: key
				trap->Argv(i, arg2, sizeof(arg2));
				strcpy(key, G_NewString(arg2));
			}
			else
			{ // zyk: value
				trap->Argv(i, arg2, sizeof(arg2));

				zyk_main_set_entity_field(this_ent, G_NewString(key), G_NewString(arg2));
			}
		}

		zyk_main_spawn_entity(this_ent);

		trap->SendServerCommand(ent-g_entities, va("print \"Entity %d edited\n\"", this_ent->s.number) );
	}
}

/*
==================
Cmd_EntSave_f
==================
*/
void Cmd_EntSave_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	int i = 0;
	int j = 0;
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent->s.number, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent->s.number, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	// zyk: saving the entities into the file
	this_file = fopen(va("zykmod/entities/%s/%s.txt",zyk_mapname,arg1),"w");

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		gentity_t *this_ent = &g_entities[i];

		if (this_ent && this_ent->inuse)
		{ // zyk: freed entities will not be saved
			j = 0;

			while (j < level.zyk_spawn_strings_values_count[this_ent->s.number])
			{
				fprintf(this_file, "%s;%s;", level.zyk_spawn_strings[this_ent->s.number][j], level.zyk_spawn_strings[this_ent->s.number][j + 1]);

				j += 2;
			}

			if (j > 0)
			{ // zyk: break line only if the entity had keys and values to save
				fprintf(this_file, "\n");
			}
		}
	}

	fclose(this_file);

	trap->SendServerCommand( ent->s.number, va("print \"Entities saved in %s file\n\"", arg1) );
}

/*
==================
Cmd_EntLoad_f
==================
*/
void Cmd_EntLoad_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	int i = 0;
	FILE *this_file = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	strcpy(level.load_entities_file, va("zykmod/entities/%s/%s.txt",zyk_mapname,arg1));

	this_file = fopen(level.load_entities_file,"r");
	if (this_file)
	{ // zyk: loads entities from the file if it exists
		fclose(this_file);

		// zyk: cleaning entities. Only the ones from the file will be in the map
		for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
		{
			gentity_t *target_ent = &g_entities[i];

			if (target_ent)
				G_FreeEntity( target_ent );
		}

		level.load_entities_timer = level.time + 1050;

		trap->SendServerCommand( ent-g_entities, va("print \"Loading entities from %s file\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_EntDeleteFile_f
==================
*/
void Cmd_EntDeleteFile_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid filename. Only letters and numbers allowed.\n\"");
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	this_file = fopen(va("zykmod/entities/%s/%s.txt",zyk_mapname,arg1),"r");
	if (this_file)
	{
		fclose(this_file);

		remove(va("zykmod/entities/%s/%s.txt",zyk_mapname,arg1));

		trap->SendServerCommand( ent-g_entities, va("print \"File %s deleted from server\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_EntNear_f
==================
*/
void Cmd_EntNear_f( gentity_t *ent ) {
	int i = 0;
	int distance = 200;
	int numListedEntities = 0;
	int entityList[MAX_GENTITIES];
	vec3_t mins, maxs, center;
	char message[MAX_STRING_CHARS * 2];
	char arg1[MAX_STRING_CHARS];
	gentity_t *this_ent = NULL;

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent->s.number, "print \"You don't have this admin command.\n\"" );
		return;
	}

	strcpy(message,"");

	if (trap->Argc() > 1)
	{
		trap->Argv(1, arg1, sizeof(arg1));

		distance = atoi(arg1);
	}

	VectorCopy(ent->client->ps.origin, center);

	for (i = 0; i < 3; i++)
	{
		mins[i] = center[i] - distance;
		maxs[i] = center[i] + distance;
	}

	numListedEntities = trap->EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for (i = 0; i < numListedEntities; i++)
	{
		this_ent = &g_entities[entityList[i]];

		if (this_ent && ent != this_ent && this_ent->s.number >= (MAX_CLIENTS + BODY_QUEUE_SIZE) && this_ent->inuse == qtrue)
		{
			strcpy(message,va("%s\n%d - %s",message, this_ent->s.number,this_ent->classname));
		}

		if (strlen(message) > (MAX_STRING_CHARS - 11))
		{
			trap->SendServerCommand(ent->s.number, "print \"Too much info. Decrease the distance argument\n\"");
			return;
		}
	}

	// zyk: if there are still enough room to list, use old method to get some entities not listed with EntitiesInBox
	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		int j = 0;
		qboolean already_found = qfalse;

		this_ent = &g_entities[i];

		for (j = 0; j < numListedEntities; j++)
		{
			if (entityList[j] == i)
			{ // zyk: this entity was already listed
				already_found = qtrue;
				break;
			}
		}

		if (this_ent && ent != this_ent && already_found == qfalse && this_ent->inuse == qtrue && (int)Distance(ent->client->ps.origin, this_ent->r.currentOrigin) < distance && this_ent->s.eType != ET_MOVER)
		{ // zyk: do not list mover entities in this old method, they are listed with EntitiesInBox
			strcpy(message, va("%s\n%d - %s", message, this_ent->s.number, this_ent->classname));
		}

		if (strlen(message) > (MAX_STRING_CHARS - 11))
		{
			trap->SendServerCommand(ent->s.number, "print \"Too much info. Decrease the distance argument\n\"");
			return;
		}
	}

	if (Q_stricmp(message, "") == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"No entities found\n\"");
		return;
	}

	trap->SendServerCommand( ent->s.number, va("print \"%s\n\"", message) );
}

/*
==================
Cmd_EntList_f
==================
*/
void Cmd_EntList_f( gentity_t *ent ) {
	int i = 0;
	int page_number = 0;
	gentity_t *target_ent;
	char arg1[MAX_STRING_CHARS];
	char message[1024];

	strcpy(message,"");

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a page number greater than 0. Example: ^3/entlist 5^7. You can also search for classname, targetname and target that matches at least part of it. Example: ^3/entlist info_player_deathmatch^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	page_number = atoi(arg1);

	if (page_number > 0)
	{
		for (i = 0; i < level.num_entities; i++)
		{
			if (i >= ((page_number - 1) * 10) && i < (page_number * 10))
			{ // zyk: this command lists 10 entities per page
				target_ent = &g_entities[i];
				strcpy(message, va("%s\n%d - %s - %s - %s",message,i,target_ent->classname,target_ent->targetname,target_ent->target));
			}
		}
	}
	else
	{ // zyk: search by classname, targetname or target
		int found_entities = 0;

		for (i = 0; i < level.num_entities; i++)
		{
			target_ent = &g_entities[i];

			if (target_ent && 
				((target_ent->classname && strstr(target_ent->classname, G_NewString(arg1))) ||
				 (target_ent->targetname && strstr(target_ent->targetname, G_NewString(arg1))) ||
				 (target_ent->target && strstr(target_ent->target, G_NewString(arg1)))))
			{
				strcpy(message, va("%s\n%d - %s - %s - %s",message,i,target_ent->classname,target_ent->targetname,target_ent->target));
				found_entities++;
			}

			// zyk: max entities to list
			if (found_entities == 14)
				break;
		}
	}

	trap->SendServerCommand( ent->s.number, va("print \"^3\nID - classname - targetname - target\n^7%s\n\n\"",message) );
}

/*
==================
Cmd_EntRemove_f
==================
*/
void Cmd_EntRemove_f( gentity_t *ent ) {
	int i = 0;
	int entity_id = -1;
	int entity_id2 = -1;
	gentity_t *target_ent;
	char   arg1[MAX_STRING_CHARS];
	char   arg2[MAX_STRING_CHARS];

	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify an entity id.\n\"") );
		return;
	}

	if (trap->Argc() == 2)
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );
		entity_id = atoi(arg1);

		if (entity_id >= 0 && entity_id < MAX_CLIENTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity ID %d is a player slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		for (i = 0; i < level.num_entities; i++)
		{
			target_ent = &g_entities[i];
			if ((target_ent-g_entities) == entity_id)
			{
				G_FreeEntity( target_ent );
				trap->SendServerCommand( ent-g_entities, va("print \"Entity %d removed.\n\"",i) );
				return;
			}
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Entity %d not found.\n\"",entity_id) );
	}
	else
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );
		entity_id = atoi(arg1);

		if (entity_id >= 0 && entity_id < MAX_CLIENTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 1 ID %d is a player slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		trap->Argv( 2, arg2, sizeof( arg2 ) );
		entity_id2 = atoi(arg2);

		if (entity_id2 >= 0 && entity_id2 < MAX_CLIENTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 2 ID %d is a player slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		for (i = 0; i < level.num_entities; i++)
		{
			target_ent = &g_entities[i];
			if ((target_ent-g_entities) >= entity_id && (target_ent-g_entities) <= entity_id2)
			{
				G_FreeEntity( target_ent );
			}
		}

		trap->SendServerCommand( ent-g_entities, "print \"Entities removed.\n\"" );
		return;
	}
}

/*
==================
Cmd_ClientPrint_f
==================
*/
void Cmd_ClientPrint_f( gentity_t *ent ) {
	int client_id = -1;
	char   arg1[MAX_STRING_CHARS];
	char   arg2[MAX_STRING_CHARS];

	if (!(ent->client->pers.bitvalue & (1 << ADM_CLIENTPRINT)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() < 3)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: /clientprint <player name or ID, or -1 to show to all players> <message>\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (atoi(arg1) != -1)
	{ // zyk: -1 means all players will get the message
		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}
	}

	trap->Argv( 2, arg2, sizeof( arg2 ) );

	trap->SendServerCommand( client_id, va("cp \"%s\"", arg2) );
}

/*
==================
Cmd_Silence_f
==================
*/
void Cmd_Silence_f( gentity_t *ent ) {
	int client_id = -1;
	char   arg[MAX_STRING_CHARS];

	if (!(ent->client->pers.bitvalue & (1 << ADM_SILENCE)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a player name or ID.\n\"") );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	client_id = ClientNumberFromString( ent, arg, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->pers.player_statuses & (1 << 0))
	{
		g_entities[client_id].client->pers.player_statuses &= ~(1 << 0);
		trap->SendServerCommand( -1, va("chat \"^3Admin System: ^7player %s^7 is no longer silenced!\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		g_entities[client_id].client->pers.player_statuses |= (1 << 0);
		trap->SendServerCommand( -1, va("chat \"^3Admin System: ^7player %s^7 is silenced!\n\"", g_entities[client_id].client->pers.netname) );
	}
}

// zyk: shows admin commands of this player. Shows info to the target_ent player
void zyk_show_admin_commands(gentity_t *ent, gentity_t *target_ent)
{
	char message[1024];
	char message_content[ADM_NUM_CMDS + 1][80];
	int i = 0;
	strcpy(message,"");

	while (i < ADM_NUM_CMDS)
	{
		strcpy(message_content[i],"");
		i++;
	}
	message_content[ADM_NUM_CMDS][0] = '\0';

	if ((ent->client->pers.bitvalue & (1 << ADM_NPC))) 
	{
		strcpy(message_content[ADM_NPC],va("^3  %d ^7- NPC: ^2yes\n",ADM_NPC));
	}
	else
	{
		strcpy(message_content[ADM_NPC],va("^3  %d ^7- NPC: ^1no\n",ADM_NPC));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_NOCLIP))) 
	{
		strcpy(message_content[ADM_NOCLIP],va("^3  %d ^7- NoClip: ^2yes\n",ADM_NOCLIP));
	}
	else
	{
		strcpy(message_content[ADM_NOCLIP],va("^3  %d ^7- NoClip: ^1no\n",ADM_NOCLIP));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_GIVEADM))) 
	{
		strcpy(message_content[ADM_GIVEADM],va("^3  %d ^7- GiveAdmin: ^2yes\n",ADM_GIVEADM));
	}
	else
	{
		strcpy(message_content[ADM_GIVEADM],va("^3  %d ^7- GiveAdmin: ^1no\n",ADM_GIVEADM));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_TELE))) 
	{
		strcpy(message_content[ADM_TELE],va("^3  %d ^7- Teleport: ^2yes\n",ADM_TELE));
	}
	else
	{
		strcpy(message_content[ADM_TELE],va("^3  %d ^7- Teleport: ^1no\n",ADM_TELE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_ADMPROTECT))) 
	{
		strcpy(message_content[ADM_ADMPROTECT],va("^3  %d ^7- AdminProtect: ^2yes\n",ADM_ADMPROTECT));
	}
	else
	{
		strcpy(message_content[ADM_ADMPROTECT],va("^3  %d ^7- AdminProtect: ^1no\n",ADM_ADMPROTECT));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM))) 
	{
		strcpy(message_content[ADM_ENTITYSYSTEM],va("^3  %d ^7- EntitySystem: ^2yes\n",ADM_ENTITYSYSTEM));
	}
	else
	{
		strcpy(message_content[ADM_ENTITYSYSTEM],va("^3  %d ^7- EntitySystem: ^1no\n",ADM_ENTITYSYSTEM));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_SILENCE))) 
	{
		strcpy(message_content[ADM_SILENCE],va("^3  %d ^7- Silence: ^2yes\n",ADM_SILENCE));
	}
	else
	{
		strcpy(message_content[ADM_SILENCE],va("^3  %d ^7- Silence: ^1no\n",ADM_SILENCE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_CLIENTPRINT))) 
	{
		strcpy(message_content[ADM_CLIENTPRINT],va("^3  %d ^7- ClientPrint: ^2yes\n",ADM_CLIENTPRINT));
	}
	else
	{
		strcpy(message_content[ADM_CLIENTPRINT],va("^3  %d ^7- ClientPrint: ^1no\n",ADM_CLIENTPRINT));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_KICK))) 
	{
		strcpy(message_content[ADM_KICK],va("^3  %d ^7- Kick: ^2yes\n",ADM_KICK));
	}
	else
	{
		strcpy(message_content[ADM_KICK],va("^3  %d ^7- Kick: ^1no\n",ADM_KICK));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_PARALYZE))) 
	{
		strcpy(message_content[ADM_PARALYZE],va("^3 %d ^7- Paralyze: ^2yes\n",ADM_PARALYZE));
	}
	else
	{
		strcpy(message_content[ADM_PARALYZE],va("^3 %d ^7- Paralyze: ^1no\n",ADM_PARALYZE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_GIVE))) 
	{
		strcpy(message_content[ADM_GIVE],va("^3 %d ^7- Give: ^2yes\n",ADM_GIVE));
	}
	else
	{
		strcpy(message_content[ADM_GIVE],va("^3 %d ^7- Give: ^1no\n",ADM_GIVE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_SCALE))) 
	{
		strcpy(message_content[ADM_SCALE],va("^3 %d ^7- Scale: ^2yes\n",ADM_SCALE));
	}
	else
	{
		strcpy(message_content[ADM_SCALE],va("^3 %d ^7- Scale: ^1no\n",ADM_SCALE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_PLAYERS))) 
	{
		strcpy(message_content[ADM_PLAYERS],va("^3 %d ^7- Players: ^2yes\n",ADM_PLAYERS));
	}
	else
	{
		strcpy(message_content[ADM_PLAYERS],va("^3 %d ^7- Players: ^1no\n",ADM_PLAYERS));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_DUELARENA)))
	{
		strcpy(message_content[ADM_DUELARENA], va("^3 %d ^7- DuelArena: ^2yes\n", ADM_DUELARENA));
	}
	else
	{
		strcpy(message_content[ADM_DUELARENA], va("^3 %d ^7- DuelArena: ^1no\n", ADM_DUELARENA));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_CUSTOMQUEST)))
	{
		strcpy(message_content[ADM_CUSTOMQUEST], va("^3 %d ^7- Custom Quest: ^2yes\n", ADM_CUSTOMQUEST));
	}
	else
	{
		strcpy(message_content[ADM_CUSTOMQUEST], va("^3 %d ^7- Custom Quest: ^1no\n", ADM_CUSTOMQUEST));
	}

	for (i = 0; i < ADM_NUM_CMDS; i++)
	{
		strcpy(message,va("%s%s",message,message_content[i]));
	}

	trap->SendServerCommand( target_ent->s.number, va("print \"\n%s^7\n%s\n^7Use ^3/adminlist <number> ^7to see command info\n\n\"", ent->client->pers.netname, message) );
}

/*
==================
Cmd_AdminList_f
==================
*/
void Cmd_AdminList_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		zyk_show_admin_commands(ent, ent);
	}
	else if (trap->Argc() == 2)
	{ // zyk: display help info for an admin command
		char arg1[MAX_STRING_CHARS];
		int command_number = 0;
		
		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		command_number = atoi(arg1);

		if (command_number == ADM_NPC)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/npc spawn <name> ^7to spawn a npc. Use ^3/npc spawn vehicle <name> to spawn a vehicle. Use ^3/npc kill all ^7to kill all npcs\n\n\"" );
		}
		else if (command_number == ADM_NOCLIP)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/noclip ^7to toggle noclip\n\n\"" );
		}
		else if (command_number == ADM_GIVEADM)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis flag allows admins to give or remove admin commands from a player with ^3/adminup <name> <command number> ^7and ^3/admindown <name> <command number>^7. Use ^3/adminlist show <player name or ID> ^7to see admin commands of a player\n\n\"" );
		}
		else if (command_number == ADM_TELE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis command can be ^3/teleport^7 or ^3/tele^7. Use ^3/teleport point ^7to mark a spot in map, then use ^3/teleport ^7to go there. Use ^3/teleport <player name or ID> ^7to teleport to a player. Use ^3/teleport <player name or ID> <player name or ID> ^7to teleport a player to another. Use ^3/teleport <x> <y> <z> ^7to teleport to coordinates. Use ^3/teleport <player name or ID> <x> <y> <z> ^7to teleport a player to coordinates\n\n\"" );
		}
		else if (command_number == ADM_ADMPROTECT)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nWith this flag, a player can use Admin Protect option in /settings to protect himself from admin commands\n\n\"" );
		}
		else if (command_number == ADM_ENTITYSYSTEM)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/entitysystem ^7to see the Entity System commands\n\n\"" );
		}
		else if (command_number == ADM_SILENCE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/silence <player name or ID> ^7to silence that player\n\n\"" );
		}
		else if (command_number == ADM_CLIENTPRINT)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/clientprint <player name or ID, or -1 to show to all players> <message> ^7to print a message in the screen\n\n\"" );
		}
		else if (command_number == ADM_KICK)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/admkick <player name or ID> ^7to kick a player from the server\n\n\"" );
		}
		else if (command_number == ADM_PARALYZE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/paralyze <player name or ID> ^7to paralyze a player. Use it again so the target player will no longer be paralyzed\n\n\"" );
		}
		else if (command_number == ADM_GIVE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/give <player name or ID> <option> ^7to give stuff to a player. Option may be ^3guns ^7or ^3force ^7\n\n\"" );
		}
		else if (command_number == ADM_SCALE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/scale <player name or ID> <size between 20 and 500> ^7to change the player model size\n\n\"" );
		}
		else if (command_number == ADM_PLAYERS)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/players ^7to see info about the players. Use ^3/players <player name or ID> ^7to see RPG info of a player. Use ^3/players <player name or ID> ^7and a third argument (^3force,weapons,other,ammo,items^7) to see skill levels of the player\n\n\"" );
		}
		else if (command_number == ADM_DUELARENA)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/duelarena ^7to set or unset the Duel Tournament arena in this map. The arena is saved automatically. Also, use ^3/duelpause ^7to pause the tournament and use it again to resume it\n\n\"");
		}
		else if (command_number == ADM_CUSTOMQUEST)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/customquest ^7to see commands to manage Custom Quests\n\n\"");
		}
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char arg2[MAX_STRING_CHARS];
		int client_id = -1;
		
		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		if (Q_stricmp(arg1, "show") == 0)
		{
			gentity_t *player_ent = NULL;

			if (!(ent->client->pers.bitvalue & (1 << ADM_GIVEADM)))
			{ // zyk: admin command
				trap->SendServerCommand( ent-g_entities, "print \"You must have GiveAdmin to use this admin command.\n\"" );
				return;
			}

			trap->Argv( 2,  arg2, sizeof( arg2 ) );

			client_id = ClientNumberFromString( ent, arg2, qfalse );
			if (client_id == -1)
			{
				return;
			}

			player_ent = &g_entities[client_id];

			if (player_ent->client->sess.amrpgmode == 0)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Player %s ^7is not logged in.\n\"", player_ent->client->pers.netname) );
				return;
			}

			// zyk: player is logged in. Show his admin commands
			zyk_show_admin_commands(player_ent, ent);
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid option.\n\"" );
		}
	}
}

/*
==================
Cmd_AdminUp_f
==================
*/
void Cmd_AdminUp_f( gentity_t *ent ) {
	if (ent->client->pers.bitvalue & (1 << ADM_GIVEADM))
	{
		char	arg1[MAX_STRING_CHARS];
		char	arg2[MAX_STRING_CHARS];
		int client_id = -1;
		int i = 0;
		int bitvaluecommand = 0;

		if ( trap->Argc() != 3 )
		{ 
			trap->SendServerCommand( ent-g_entities, "print \"You must write the player name and the admin command number.\n\"" ); 
			return; 
		}
		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		if (g_entities[client_id].client->sess.amrpgmode == 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player is not logged in\n\"") );
			return;
		}
		if (Q_stricmp (arg2, "all") == 0)
		{ // zyk: if player wrote all, give all commands to the target player
			for (i = 0; i < ADM_NUM_CMDS; i++)
				g_entities[client_id].client->pers.bitvalue |= (1 << i);
		}
		else
		{
			bitvaluecommand = atoi(arg2);
			if (bitvaluecommand < 0 || bitvaluecommand >= ADM_NUM_CMDS)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
				return; 
			}
			g_entities[client_id].client->pers.bitvalue |= (1 << bitvaluecommand);
		}

		save_account(&g_entities[client_id], qfalse);

		trap->SendServerCommand( ent-g_entities, "print \"Admin commands upgraded successfully.\n\"" );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"You can't use this command.\n\"" );
	}
}

/*
==================
Cmd_AdminDown_f
==================
*/
void Cmd_AdminDown_f( gentity_t *ent ) {
	if (ent->client->pers.bitvalue & (1 << ADM_GIVEADM))
	{
		char	arg1[MAX_STRING_CHARS];
		char	arg2[MAX_STRING_CHARS];
		int client_id = -1;
		int bitvaluecommand = 0;

		if ( trap->Argc() != 3 )
		{ 
			trap->SendServerCommand( ent-g_entities, "print \"You must write a player name and the admin command number.\n\"" ); 
			return; 
		}
		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 
				
		if (client_id == -1)
		{
			return;
		}

		if (g_entities[client_id].client->sess.amrpgmode == 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player is not logged in\n\"") );
			return;
		}

		if (Q_stricmp (arg2, "all") == 0)
		{ // zyk: if player wrote all, take away all admin commands from target player
			g_entities[client_id].client->pers.bitvalue = 0;
		}
		else
		{
			bitvaluecommand = atoi(arg2);
			if (bitvaluecommand < 0 || bitvaluecommand >= ADM_NUM_CMDS)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
				return; 
			}
			g_entities[client_id].client->pers.bitvalue &= ~(1 << bitvaluecommand);
		}

		save_account(&g_entities[client_id], qfalse);

		trap->SendServerCommand( ent-g_entities, "print \"Admin commands upgraded successfully.\n\"" );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"You can't use this command.\n\"" );
	}
}

/*
==================
Cmd_EntitySystem_f
==================
*/
void Cmd_EntitySystem_f( gentity_t *ent ) {
	if (!(ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	trap->SendServerCommand( ent-g_entities, va("print \"\n^3/entadd <classname> <spawnflags> <key value key value ...>: ^7adds a new entity in the map\n^3/entedit <entity id> [key value key value ... etc]: ^7shows entity info or edits fields\n^3/entnear: ^7lists entities in less than 200 map units or distance passed as argument\n^3/entlist <page number>: ^7lists all entities\n^3/entorigin: ^7sets this point as origin for new entities\n^3/entundo: ^7removes last added entity\n^3/entsave <filename>: ^7saves entities into a file. Use ^3default ^7name to make it load with the map\n^3/entload <filename>: ^7loads entities from a file\n^3/entremove <entity id>: ^7removes the entity from the map\n^3/entdeletefile <filename>: ^7removes a file created by /entsave\n^3/remap <shader> <new shader>: ^7remaps shaders in map\n^3/remaplist: ^7lists remapped shaders\n^3/remapsave <file name>: ^7saves remaps in a file. Use ^3default ^7name to make file load with the map\n^3/remapload <file name>: ^7loads remaps from file\n^3/remapdeletefile <file name>: ^7deletes a remap file\n\n\"") );
}

/*
==================
Cmd_AdmKick_f
==================
*/
void Cmd_AdmKick_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_KICK)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}
   
	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must specify the player name or ID.\n\"" ); 
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qtrue );

	if (client_id == -1)
	{
		return;
	}

	trap->SendConsoleCommand( EXEC_APPEND, va( "kick %d\n", client_id) );
}

/*
==================
Cmd_Order_f
==================
*/
void Cmd_Order_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int i = 0;
   
	if ( trap->Argc() == 1) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"^3/order follow: ^7npc will follow the leader\n^3/order guard: ^7npc will stand still shooting at everyone except the leader and his allies\n^3/order cover: ^7npc will follow the leader shooting at everyone except the leader and his allies\n\"" );
	}
	else
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );

		if (Q_stricmp(arg1, "follow") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->NPC_class != CLASS_VEHICLE && 
					this_ent->client->leader == ent)
				{
					this_ent->client->pers.player_statuses &= ~(1 << 18);
					this_ent->client->pers.player_statuses &= ~(1 << 19);
					this_ent->NPC->tempBehavior = BS_FOLLOW_LEADER;
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else if (Q_stricmp(arg1, "guard") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->NPC_class != CLASS_VEHICLE && 
					this_ent->client->leader == ent)
				{
					this_ent->NPC->tempBehavior = BS_STAND_GUARD;
					this_ent->client->pers.player_statuses &= ~(1 << 19);
					this_ent->client->pers.player_statuses |= (1 << 18);
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else if (Q_stricmp(arg1, "cover") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->NPC_class != CLASS_VEHICLE && 
					this_ent->client->leader == ent)
				{
					this_ent->NPC->tempBehavior = BS_FOLLOW_LEADER;
					this_ent->client->pers.player_statuses &= ~(1 << 18);
					this_ent->client->pers.player_statuses |= (1 << 19);
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid npc order.\n\"" );
		}
	}
}

/*
==================
Cmd_Paralyze_f
==================
*/
void Cmd_Paralyze_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_PARALYZE)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}
   
	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must specify the player name or ID.\n\"" ); 
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->pers.player_statuses & (1 << 6))
	{ // zyk: if paralyzed, remove it from the target player
		g_entities[client_id].client->pers.player_statuses &= ~(1 << 6);

		// zyk: kill the target player to prevent exploits with RPG Mode commands
		G_Kill(&g_entities[client_id]);

		trap->SendServerCommand( ent-g_entities, va("print \"Target player %s ^7is no longer paralyzed\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"You are no longer paralyzed\n\"") );
	}
	else
	{ // zyk: paralyze the target player
		g_entities[client_id].client->pers.player_statuses |= (1 << 6);

		g_entities[client_id].client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
		g_entities[client_id].client->ps.forceHandExtendTime = level.time + 500;
		g_entities[client_id].client->ps.velocity[2] += 150;
		g_entities[client_id].client->ps.forceDodgeAnim = 0;
		g_entities[client_id].client->ps.quickerGetup = qtrue;

		trap->SendServerCommand( ent-g_entities, va("print \"Paralyzed the target player %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"You were paralyzed by an admin\n\"") );
	}
}

/*
==================
Cmd_Players_f
==================
*/
void Cmd_Players_f( gentity_t *ent ) {
	char content[MAX_STRING_CHARS];
	int i = 0;
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_args = trap->Argc();

	strcpy(content,"ID - Name - IP - Type\n");

	if (!(ent->client->pers.bitvalue & (1 << ADM_PLAYERS)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if (number_of_args == 1)
	{
		for (i = 0; i < level.maxclients; i++)
		{
			gentity_t *player = &g_entities[i];

			if (player && player->client && player->client->pers.connected != CON_DISCONNECTED)
			{
				strcpy(content, va("%s%d - %s ^7- %s - ",content,player->s.number,player->client->pers.netname,player->client->sess.IP));

				if (player->client->sess.amrpgmode > 0)
				{
					if (player->client->pers.bitvalue > 0)
						strcpy(content, va("%s^3(admin)",content));
					else
						strcpy(content, va("%s^3(logged)",content));
				}

				if (player->client->sess.amrpgmode == 2)
				{
					strcpy(content, va("%s ^3(rpg)",content));
				}

				strcpy(content, va("%s^7\n",content));
			}
		}

		trap->SendServerCommand( ent-g_entities, va("print \"%s\"", content) );
	}
	else
	{
		gentity_t *player_ent = NULL;

		trap->Argv( 1, arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		player_ent = &g_entities[client_id];

		if (player_ent->client->sess.amrpgmode != 2)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player %s ^7is not in RPG Mode.\n\"", player_ent->client->pers.netname) );
			return;
		}

		if (number_of_args == 2)
		{
			list_rpg_info(player_ent, ent);
		}
		else
		{
			char arg2[MAX_STRING_CHARS];

			trap->Argv( 2, arg2, sizeof( arg2 ) );

			if (Q_stricmp(arg2, "force") == 0 || Q_stricmp(arg2, "weapons") == 0 || 
				Q_stricmp(arg2, "other") == 0 || Q_stricmp(arg2, "magic") == 0)
			{ // zyk: show skills of the player
				zyk_list_player_skills(player_ent, ent, G_NewString(arg2));
			}
			else
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid option.\n\"" );
			}
		}
	}
}

/*
==================
Cmd_Ignore_f
==================
*/
void Cmd_Ignore_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	gentity_t *player = NULL;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must pass a player name or ID as argument.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	player = &g_entities[client_id];

	if (client_id < 31 && !(level.ignored_players[ent->s.number][0] & (1 << client_id)))
	{
		level.ignored_players[ent->s.number][0] |= (1 << client_id);
		trap->SendServerCommand( ent-g_entities, va("print \"Ignored player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id >= 31 && !(level.ignored_players[ent->s.number][1] & (1 << (client_id - 31))))
	{
		level.ignored_players[ent->s.number][1] |= (1 << (client_id - 31));
		trap->SendServerCommand( ent-g_entities, va("print \"Ignored player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id < 31 && level.ignored_players[ent->s.number][0] & (1 << client_id))
	{
		level.ignored_players[ent->s.number][0] &= ~(1 << client_id);
		trap->SendServerCommand( ent-g_entities, va("print \"No longer ignore player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id >= 31 && level.ignored_players[ent->s.number][1] & (1 << (client_id - 31)))
	{
		level.ignored_players[ent->s.number][1] &= ~(1 << (client_id - 31));
		trap->SendServerCommand( ent-g_entities, va("print \"No longer ignore player %s^7\n\"", player->client->pers.netname) );
	}
}

/*
==================
Cmd_IgnoreList_f
==================
*/
void Cmd_IgnoreList_f(gentity_t *ent) {
	int i = 0;
	char ignored_players[MAX_STRING_CHARS];
	gentity_t *player = NULL;

	strcpy(ignored_players, "");

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (i < 31 && level.ignored_players[ent->s.number][0] & (1 << i))
		{
			player = &g_entities[i];
			strcpy(ignored_players, va("%s%s^7\n", ignored_players, player->client->pers.netname));
		}
		else if (i >= 31 && level.ignored_players[ent->s.number][1] & (1 << (i - 31)))
		{
			player = &g_entities[i];
			strcpy(ignored_players, va("%s%s^7\n", ignored_players, player->client->pers.netname));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s^7\n\"", ignored_players));
}

/*
==================
Cmd_Saber_f
==================
*/
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
extern qboolean G_SaberModelSetup(gentity_t *ent);
void Cmd_Saber_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int number_of_args = trap->Argc(), i = 0;
	qboolean changedSaber = qfalse;
	char userinfo[MAX_INFO_STRING] = {0}, *saber = NULL, *key = NULL, *value = NULL;

	if (zyk_allow_saber_command.integer < 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"This command is not allowed in this server.\n\"" );
		return;
	}

	if (zyk_allow_saber_command.integer > 1 && ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use this command in private duels.\n\"" );
		return;
	}

	if (zyk_allow_saber_command.integer > 1 && level.duel_tournament_mode == 4 && duel_tournament_is_duelist(ent) == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while duelling in Duel Tournament.\n\"");
		return;
	}

	if (number_of_args == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: /saber <saber1> <saber2>. Examples: /saber single_1, /saber single_1 single_1, /saber dual_1\n\"" );
		return;
	}

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );

	// zyk: setting sabers for this player
	trap->Argv( 1, arg1, sizeof( arg1 ) );

	saber = ent->client->pers.saber1;
	value = G_NewString(arg1);

	if ( Q_stricmp( value, saber ) )
	{
		Info_SetValueForKey( userinfo, "saber1", value );
	}

	saber = ent->client->pers.saber2;

	if (number_of_args == 2)
	{
		value = "none";
	}
	else
	{
		trap->Argv(2, arg2, sizeof(arg2));
		value = G_NewString(arg2);
	}

	if ( Q_stricmp( value, saber ) )
	{
		Info_SetValueForKey( userinfo, "saber2", value );
	}

	trap->SetUserinfo( ent->s.number, userinfo );

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );

	for ( i=0; i<MAX_SABERS; i++ )
	{
		saber = (i&1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
		value = Info_ValueForKey( userinfo, va( "saber%i", i+1 ) );
		if ( saber && value &&
			(Q_stricmp( value, saber ) || !saber[0] || !ent->client->saber[0].model[0]) )
		{ //doesn't match up (or our saber is BS), we want to try setting it
			if ( G_SetSaber( ent, i, value, qfalse ) )
				changedSaber = qtrue;

			//Well, we still want to say they changed then (it means this is siege and we have some overrides)
			else if ( !saber[0] || !ent->client->saber[0].model[0] )
				changedSaber = qtrue;
		}
	}

	if ( changedSaber )
	{ //make sure our new info is sent out to all the other clients, and give us a valid stance
		if ( !ClientUserinfoChanged( ent->s.number ) )
			return;

		//make sure the saber models are updated
		G_SaberModelSetup( ent );

		for ( i=0; i<MAX_SABERS; i++ )
		{
			saber = (i&1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
			key = va( "saber%d", i+1 );
			value = Info_ValueForKey( userinfo, key );
			if ( Q_stricmp( value, saber ) )
			{// they don't match up, force the user info
				Info_SetValueForKey( userinfo, key, saber );
				trap->SetUserinfo( ent->s.number, userinfo );
			}
		}

		if ( ent->client->saber[0].model[0] && ent->client->saber[1].model[0] )
		{ //dual
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_DUAL;
		}
		else if ( (ent->client->saber[0].saberFlags&SFL_TWO_HANDED) )
		{ //staff
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_STAFF;
		}
		else
		{
			ent->client->sess.saberLevel = Com_Clampi( SS_FAST, SS_STRONG, ent->client->sess.saberLevel );
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;

			// limit our saber style to our force points allocated to saber offense
			if ( level.gametype != GT_SIEGE && ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
				ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		}
		if ( level.gametype != GT_SIEGE )
		{// let's just make sure the styles we chose are cool
			if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
			{
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
				ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			}
		}
	}
}

// zyk: tests if some weapon shots will be deflected when hitting this player
qboolean zyk_can_deflect_shots(gentity_t *ent)
{
	if (ent->client && ent->client->sess.amrpgmode == 2)
	{
		// zyk: Deflective Armor
		if (ent->client->pers.rpg_inventory[RPG_INVENTORY_UPGRADE_DEFLECTIVE_ARMOR] > 0)
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
==================
Cmd_Magic_f
==================
*/
int zyk_get_magic_cost(int magic_number)
{
	int magic_costs[MAX_MAGIC_POWERS] = {
		5, // Magic Sense
		20, // Healing Area
		20, // Enemy Weakening
		25, // Dome of Damage
		30, // Water Magic
		30, // Earth Magic
		30, // Fire Magic
		30, // Air Magic
		30, // Dark Magic
		30, // Light Magic
		0,
		0,
		0
	};

	return magic_costs[magic_number];
}

extern void zyk_spawn_magic_element_effect(gentity_t* ent, int magic_number);
extern void magic_sense(gentity_t* ent);
extern void healing_area(gentity_t* ent);
extern void enemy_weakening(gentity_t* ent);
extern void dome_of_damage(gentity_t* ent);
extern void water_magic(gentity_t* ent);
extern void earth_magic(gentity_t* ent);
extern void fire_magic(gentity_t* ent);
extern void air_magic(gentity_t* ent);
extern void dark_magic(gentity_t* ent);
extern void light_magic(gentity_t* ent);
void zyk_cast_magic(gentity_t* ent, int skill_index)
{
	int magic_number = zyk_get_magic_index(skill_index);

	if (ent->client->pers.skill_levels[skill_index] < 1)
	{
		if (ent->s.number < MAX_CLIENTS)
			trap->SendServerCommand(ent->s.number, va("print \"You don't have %s.\n\"", zyk_skill_name(skill_index)));
		
		return;
	}

	if (ent->client->pers.quest_power_usage_timer < level.time)
	{		
		if (ent->client->pers.magic_power >= zyk_get_magic_cost(magic_number))
		{
			// zyk: magic usage effect
			zyk_spawn_magic_element_effect(ent, magic_number);

			// zyk: magic usage anim
			G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_FORCE_RAGE, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
			ent->client->ps.torsoTimer = MAGIC_ANIM_TIME;
			ent->client->ps.legsTimer = MAGIC_ANIM_TIME;
			ent->client->ps.weaponTime = MAGIC_ANIM_TIME;

			if (magic_number == MAGIC_MAGIC_SENSE)
			{
				magic_sense(ent);
			}
			else if (magic_number == MAGIC_HEALING_AREA)
			{
				healing_area(ent);
			}
			else if (magic_number == MAGIC_ENEMY_WEAKENING)
			{
				enemy_weakening(ent);
			}
			else if (magic_number == MAGIC_DOME_OF_DAMAGE)
			{
				dome_of_damage(ent);
			}
			else if (magic_number == MAGIC_WATER_MAGIC)
			{
				water_magic(ent);
			}
			else if (magic_number == MAGIC_EARTH_MAGIC)
			{
				earth_magic(ent);
			}
			else if (magic_number == MAGIC_FIRE_MAGIC)
			{
				fire_magic(ent);
			}
			else if (magic_number == MAGIC_AIR_MAGIC)
			{
				air_magic(ent);
			}
			else if (magic_number == MAGIC_DARK_MAGIC)
			{
				dark_magic(ent);
			}
			else if (magic_number == MAGIC_LIGHT_MAGIC)
			{
				light_magic(ent);
			}

			// zyk: magic powers cost mp
			ent->client->pers.magic_power -= zyk_get_magic_cost(magic_number);

			if (ent->s.number < MAX_CLIENTS)
			{
				display_yellow_bar(ent, (ent->client->pers.quest_power_usage_timer - level.time));
				send_rpg_events(2000);
			}
		}
		else
		{
			if (ent->s.number < MAX_CLIENTS)
				trap->SendServerCommand(ent->s.number, va("print \"You need %d mp to cast ^3%s\n\"", zyk_get_magic_cost(magic_number), zyk_skill_name(skill_index)));
		}
	}
	else
	{
		if (ent->s.number < MAX_CLIENTS)
			trap->SendServerCommand(ent->s.number, va("chat \"^3Magic Power: ^7%d seconds left!\"", ((ent->client->pers.quest_power_usage_timer - level.time) / 1000)));
	}
}

void Cmd_Magic_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You must pass a magic skill number.\n\"");
		return;
	}
	else
	{
		int magic_power_skill_number = 1;
		int skill_index = 0;

		trap->Argv( 1, arg1, sizeof( arg1 ) );
		magic_power_skill_number = atoi(arg1);

		skill_index = magic_power_skill_number - 1;

		if (zyk_is_magic_power_skill(skill_index) == qfalse)
		{
			trap->SendServerCommand(ent->s.number, "print \"Invalid magic skill number.\n\"" );
			return;
		}

		zyk_cast_magic(ent, skill_index);

		rpg_skill_counter(ent, 200);
	}
}

/*
==================
Cmd_DuelMode_f
==================
*/
extern void duel_tournament_end();
void Cmd_DuelMode_f(gentity_t *ent) {
	if (zyk_allow_duel_tournament.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Duel Tournament: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (level.duel_arena_loaded == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no duel arena in this map\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Sniper Battle\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Melee Battle\n\"");
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"This tournament is for non-rpg players\n\"");
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join tournament while being in nofight mode\n\"");
		return;
	}

	if (level.duel_players[ent->s.number] == -1 && level.duel_tournament_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the duel tournament now\n\"");
		return;
	}
	else if (level.duel_players[ent->s.number] == -1)
	{ // zyk: join the tournament
		if (level.duelists_quantity == MAX_CLIENTS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"There are already %d duelists in tournament\n\"", MAX_CLIENTS));
			return;
		}
		else
		{
			if (level.duelists_quantity == 0)
			{ // zyk: first duelist joined. Put the globe model in the duel arena and set its origin point
				gentity_t *new_ent = G_Spawn();

				zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
				zyk_set_entity_field(new_ent, "spawnflags", "0");
				zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]));
				zyk_set_entity_field(new_ent, "model", "models/map_objects/vjun/globe.md3");
				zyk_set_entity_field(new_ent, "targetname", "zyk_duel_globe");
				zyk_set_entity_field(new_ent, "zykmodelscale", G_NewString(zyk_duel_tournament_arena_scale.string));

				zyk_spawn_entity(new_ent);

				level.duel_tournament_model_id = new_ent->s.number;
			}

			level.duel_tournament_mode = 1;
			level.duel_tournament_timer = level.time + zyk_duel_tournament_time_to_start.integer;
			level.duel_players[ent->s.number] = 0;
			level.duel_players_hp[ent->s.number] = 0;
			
			level.duelists_quantity++;

			// zyk: removing allies from this player
			ent->client->sess.ally1 = 0;
			ent->client->sess.ally2 = 0;

			trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7joined the tournament!\n\"", ent->client->pers.netname));
		}
	}
	else if (level.duel_tournament_mode == 1)
	{ // zyk: leave the tournament
		level.duel_players[ent->s.number] = -1;
		level.duelists_quantity--;

		if (level.duelists_quantity == 0)
		{ // zyk: everyone left the tournament. End it
			duel_tournament_end();
		}

		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7left the tournament!\n\"", ent->client->pers.netname));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot leave the duel tournament after it started\n\"");
	}
}

/*
==================
Cmd_DuelTable_f
==================
*/
void duel_show_table(gentity_t *ent)
{
	int i = 0;
	int j = 0;
	int chosen_player_id = -1;
	int array_length = 0;
	char content[1024];
	int sorted_players[MAX_CLIENTS]; // zyk: used to show score of players by ordering from the highest score to lowest
	int show_table_id = -1;

	if (ent)
	{
		show_table_id = ent->s.number;
	}

	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(show_table_id, "print \"There is no duel tournament now\n\"");
		return;
	}

	// zyk: put the number of matches
	strcpy(content, va("\n^7Total: %d\nPlayed: %d\n\n", level.duel_matches_quantity, level.duel_matches_done));

	for (i = 0; i < MAX_CLIENTS; i++)
	{ // zyk: adding players to sorted_players and calculating the array length
		if (level.duel_players[i] != -1)
		{
			if (level.duel_allies[i] == -1 || i < level.duel_allies[i] || level.duel_allies[level.duel_allies[i]] != i)
			{ // zyk: do not sort the allies. Use the lower id to sort the score of a team if both players add themselves as a team
				sorted_players[array_length] = i;
				array_length++;
			}
		}
	}

	for (i = 0; i < array_length; i++)
	{ // zyk: sorting sorted_players array
		for (j = 1; j < array_length; j++)
		{
			if ((level.duel_players[sorted_players[j]] > level.duel_players[sorted_players[j - 1]]) ||
				(level.duel_players[sorted_players[j]] == level.duel_players[sorted_players[j - 1]] &&
					level.duel_players_hp[sorted_players[j]] > level.duel_players_hp[sorted_players[j - 1]]) ||
					(level.duel_players[sorted_players[j]] == level.duel_players[sorted_players[j - 1]] &&
						level.duel_players_hp[sorted_players[j]] == level.duel_players_hp[sorted_players[j - 1]] &&
						sorted_players[j] < sorted_players[j - 1]))
			{ // zyk: score of j is higher than j - 1, or remaining hp and shield of j higher than j - 1, or player id of j lower than j - 1
				chosen_player_id = sorted_players[j - 1];
				sorted_players[j - 1] = sorted_players[j];
				sorted_players[j] = chosen_player_id;
			}
		}
	}

	for (i = 0; i < array_length; i++)
	{
		gentity_t *player_ent = &g_entities[sorted_players[i]];
		char ally_name[36];

		if (level.duel_allies[sorted_players[i]] != -1 && level.duel_allies[level.duel_allies[sorted_players[i]]] == sorted_players[i])
		{ // zyk: show ally if they both added each other as a team
			strcpy(ally_name, va(" / %s", g_entities[level.duel_allies[sorted_players[i]]].client->pers.netname));
		}
		else
		{
			strcpy(ally_name, "");
		}

		strcpy(content, va("%s^7%s^7%s^7: ^3%d  ^1%d\n", content, player_ent->client->pers.netname, ally_name, level.duel_players[player_ent->s.number], level.duel_players_hp[player_ent->s.number]));
	}

	trap->SendServerCommand(show_table_id, va("print \"%s\n\"", content));
}

void Cmd_DuelTable_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int page = 0;
	int i = 0;
	int results_per_page = 8;
	char content[MAX_STRING_CHARS];

	strcpy(content, "");

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You must pass a page number. Example: ^3/dueltable 1^7\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	page = atoi(arg1);

	if (page < 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	if (page == 1)
	{
		duel_show_table(ent);
	}
	else
	{
		page--;

		// zyk: makes i start from the first result of the correct page
		i = results_per_page * (page - 1);

		while (i < (results_per_page * page) && i < level.duel_matches_quantity)
		{
			gentity_t *first_duelist = &g_entities[level.duel_matches[i][0]];
			gentity_t *second_duelist = &g_entities[level.duel_matches[i][1]];
			char first_ally_name[36];
			char second_ally_name[36];
			char first_name[96];
			char second_name[96];

			strcpy(first_ally_name, "");
			strcpy(second_ally_name, "");
			strcpy(first_name, "^3Left Tournament");
			strcpy(second_name, "^3Left Tournament");

			if (first_duelist && level.duel_allies[first_duelist->s.number] != -1)
			{
				strcpy(first_ally_name, va("^7 / %s", g_entities[level.duel_allies[first_duelist->s.number]].client->pers.netname));
			}

			if (second_duelist && level.duel_allies[second_duelist->s.number] != -1)
			{
				strcpy(second_ally_name, va("^7 / %s", g_entities[level.duel_allies[second_duelist->s.number]].client->pers.netname));
			}

			if (first_duelist && first_duelist->client && 
				first_duelist->client->pers.connected == CON_CONNECTED &&
				first_duelist->client->sess.sessionTeam != TEAM_SPECTATOR &&
				level.duel_players[first_duelist->s.number] != -1)
			{ // zyk: first duelist still in Tournament
				strcpy(first_name, va("^7%s%s", first_duelist->client->pers.netname, first_ally_name));
			}

			if (second_duelist && second_duelist->client &&
				second_duelist->client->pers.connected == CON_CONNECTED &&
				second_duelist->client->sess.sessionTeam != TEAM_SPECTATOR &&
				level.duel_players[second_duelist->s.number] != -1)
			{ // zyk: second duelist still in Tournament
				strcpy(second_name, va("^7%s%s", second_duelist->client->pers.netname, second_ally_name));
			}

			if (i < level.duel_matches_done)
			{ // zyk: this match was already played in this tournament cycle
				strcpy(content, va("%s%s ^3%d x %d %s\n", content, first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name));
			}
			else if (i == level.duel_matches_done)
			{ // zyk: current match
				char duel_time_remaining[32];

				strcpy(duel_time_remaining, "");

				if (level.duel_tournament_mode == 4)
				{ // zyk: this duel is the current one, show the time remaining in seconds
					strcpy(duel_time_remaining, va("   ^3Time: ^7%d", (level.duel_tournament_timer - level.time) / 1000));
				}

				strcpy(content, va("%s%s ^1%d x %d %s%s\n", content, first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name, duel_time_remaining));
			}
			else
			{ // zyk: match not played yet in this tournament cycle
				strcpy(content, va("%s%s ^7%d x %d %s\n", content, first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name));
			}

			i++;
		}

		trap->SendServerCommand(ent->s.number, va("print \"\n^7%s\n\"", content));
	}
}

/*
==================
Cmd_DuelArena_f
==================
*/
void Cmd_DuelArena_f(gentity_t *ent) {
	FILE *duel_arena_file;
	char content[1024];
	char zyk_info[MAX_INFO_STRING] = { 0 };
	char zyk_mapname[128] = { 0 };

	// zyk: getting the map name
	trap->GetServerinfo(zyk_info, sizeof(zyk_info));
	Q_strncpyz(zyk_mapname, Info_ValueForKey(zyk_info, "mapname"), sizeof(zyk_mapname));

	strcpy(content, "");

	if (!(ent->client->pers.bitvalue & (1 << ADM_DUELARENA)))
	{ // zyk: admin command
		trap->SendServerCommand(ent - g_entities, "print \"You don't have this admin command.\n\"");
		return;
	}

	zyk_create_dir("duelarena");

	duel_arena_file = fopen(va("zykmod/duelarena/%s/origin.txt", zyk_mapname), "r");
	if (duel_arena_file == NULL)
	{ // zyk: arena file does not exist yet, create one
		VectorCopy(ent->client->ps.origin, level.duel_tournament_origin);

		zyk_create_dir(va("duelarena/%s", zyk_mapname));

		duel_arena_file = fopen(va("zykmod/duelarena/%s/origin.txt", zyk_mapname), "w");
		fprintf(duel_arena_file, "%d\n%d\n%d\n", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]);
		fclose(duel_arena_file);

		level.duel_arena_loaded = qtrue;

		trap->SendServerCommand(ent - g_entities, va("print \"Added duel arena at %d %d %d\n\"", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]));
	}
	else
	{ // zyk: arena file already exists, remove it
		fclose(duel_arena_file);

		remove(va("zykmod/duelarena/%s/origin.txt", zyk_mapname));

		level.duel_arena_loaded = qfalse;

		trap->SendServerCommand(ent - g_entities, va("print \"Removed duel arena of this map\n\""));
	}
}

/*
==================
Cmd_DuelPause_f
==================
*/
void Cmd_DuelPause_f(gentity_t *ent) {
	if (!(ent->client->pers.bitvalue & (1 << ADM_DUELARENA)))
	{ // zyk: admin command
		trap->SendServerCommand(ent - g_entities, "print \"You don't have this admin command.\n\"");
		return;
	}

	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^3Duel Tournament: ^7There is no duel tournament now\n\"");
		return;
	}

	if (level.duel_tournament_paused == qfalse)
	{ // zyk: pauses the tournament
		level.duel_tournament_paused = qtrue;
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7pausing the tournament\"");
	}
	else
	{ // zyk: resumes the tournament
		level.duel_tournament_paused = qfalse;
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7resuming the tournament\"");
	}
}

/*
==================
Cmd_SniperMode_f
==================
*/
void Cmd_SniperMode_f(gentity_t *ent) {
	if (zyk_allow_sniper_battle.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Sniper Battle: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"You cannot be in RPG Mode to play the Sniper Battle.\n\"");
		return;
	}

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Duel Tournament\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Melee Battle\n\"");
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join sniper battle while being in nofight mode\n\"");
		return;
	}

	if (level.sniper_players[ent->s.number] == -1 && level.sniper_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the Sniper Battle now\n\"");
		return;
	}
	else if (level.sniper_players[ent->s.number] == -1)
	{ // zyk: join the sniper battle
		level.sniper_players[ent->s.number] = 0;
		level.sniper_mode = 1;
		level.sniper_mode_timer = level.time + zyk_sniper_battle_time_to_start.integer;
		level.sniper_mode_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3Sniper Battle: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		level.sniper_players[ent->s.number] = -1;
		level.sniper_mode_quantity--;
		trap->SendServerCommand(-1, va("chat \"^3Sniper Battle: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_SniperTable_f
==================
*/
void Cmd_SniperTable_f(gentity_t *ent) {
	int i = 0;
	char content[1024];

	strcpy(content, "\nSniper Battle Players\n\n");

	if (level.sniper_mode == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no Sniper Battle now\n\"");
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.sniper_players[i] != -1)
		{ // zyk: a player in Sniper Battle
			gentity_t *player_ent = &g_entities[i];

			strcpy(content, va("%s^7%s   ^3%d\n", content, player_ent->client->pers.netname, level.sniper_players[i]));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", content));
}

/*
==================
Cmd_MeleeMode_f
==================
*/
extern void melee_battle_end();
void Cmd_MeleeMode_f(gentity_t *ent) {
	if (zyk_allow_melee_battle.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Melee Battle: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"You cannot be in RPG Mode to play the Melee Battle.\n\"");
		return;
	}

	if (level.melee_arena_loaded == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no melee arena in this map\n\"");
		return;
	}

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Duel Tournament\n\"");
		return;
	}

	if (level.sniper_mode > 0 && level.sniper_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Sniper Battle\n\"");
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join melee battle while being in nofight mode\n\"");
		return;
	}

	if (level.melee_players[ent->s.number] == -1 && level.melee_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the Melee Battle now\n\"");
		return;
	}
	else if (level.melee_players[ent->s.number] == -1)
	{ // zyk: join the melee battle
		if (level.melee_mode_quantity == 0)
		{ // zyk: first player joined. Put the model in the melee arena and set its origin point
			gentity_t *new_ent = G_Spawn();

			zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
			zyk_set_entity_field(new_ent, "spawnflags", "65537");
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]));
			zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");
			zyk_set_entity_field(new_ent, "targetname", "zyk_melee_catwalk");
			zyk_set_entity_field(new_ent, "zykmodelscale", "400");
			zyk_set_entity_field(new_ent, "mins", "-256 -256 -32");
			zyk_set_entity_field(new_ent, "maxs", "256 256 32");

			zyk_spawn_entity(new_ent);

			level.melee_model_id = new_ent->s.number;
		}

		level.melee_players[ent->s.number] = 0;
		level.melee_mode = 1;
		level.melee_mode_timer = level.time + 12000;
		level.melee_mode_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		level.melee_players[ent->s.number] = -1;
		level.melee_mode_quantity--;

		if (level.melee_mode_quantity == 0)
		{ // zyk: everyone left the battle. Remove the catwalk
			melee_battle_end();
		}

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_MeleeArena_f
==================
*/
void Cmd_MeleeArena_f(gentity_t *ent) {
	FILE *duel_arena_file;
	char content[1024];
	char zyk_info[MAX_INFO_STRING] = { 0 };
	char zyk_mapname[128] = { 0 };

	// zyk: getting the map name
	trap->GetServerinfo(zyk_info, sizeof(zyk_info));
	Q_strncpyz(zyk_mapname, Info_ValueForKey(zyk_info, "mapname"), sizeof(zyk_mapname));

	strcpy(content, "");

	if (!(ent->client->pers.bitvalue & (1 << ADM_DUELARENA)))
	{ // zyk: admin command
		trap->SendServerCommand(ent - g_entities, "print \"You don't have this admin command.\n\"");
		return;
	}

	zyk_create_dir("meleearena");

	duel_arena_file = fopen(va("zykmod/meleearena/%s/origin.txt", zyk_mapname), "r");
	if (duel_arena_file == NULL)
	{ // zyk: arena file does not exist yet, create one
		VectorCopy(ent->client->ps.origin, level.melee_mode_origin);

		zyk_create_dir(va("meleearena/%s", zyk_mapname));

		duel_arena_file = fopen(va("zykmod/meleearena/%s/origin.txt", zyk_mapname), "w");
		fprintf(duel_arena_file, "%d\n%d\n%d\n", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]);
		fclose(duel_arena_file);

		level.melee_arena_loaded = qtrue;

		trap->SendServerCommand(ent - g_entities, va("print \"Added melee arena at %d %d %d\n\"", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]));
	}
	else
	{ // zyk: arena file already exists, remove it
		fclose(duel_arena_file);

		remove(va("zykmod/meleearena/%s/origin.txt", zyk_mapname));

		level.melee_arena_loaded = qfalse;

		trap->SendServerCommand(ent - g_entities, va("print \"Removed melee arena of this map\n\""));
	}
}

/*
==================
Cmd_RpgLmsMode_f
==================
*/
void Cmd_RpgLmsMode_f(gentity_t *ent) {
	if (zyk_allow_rpg_lms.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3RPG LMS: ^7this mode is not allowed in this server\n\""));
		return;
	}

	if (ent->client->pers.player_statuses & (1 << 26))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join RPG LMS while being in nofight mode\n\"");
		return;
	}

	if (level.rpg_lms_players[ent->s.number] == -1 && level.rpg_lms_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the RPG LMS now\n\"");
		return;
	}
	else if (level.rpg_lms_players[ent->s.number] == -1)
	{ // zyk: join the rpg lms battle
		level.rpg_lms_players[ent->s.number] = 0;
		level.rpg_lms_mode = 1;
		level.rpg_lms_timer = level.time + 15000;
		level.rpg_lms_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3RPG LMS: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		level.rpg_lms_players[ent->s.number] = -1;
		level.rpg_lms_quantity--;
		trap->SendServerCommand(-1, va("chat \"^3RPG LMS: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_RpgLmsTable_f
==================
*/
void Cmd_RpgLmsTable_f(gentity_t *ent) {
	int i = 0;
	char content[1024];

	strcpy(content, "\nRPG LMS Players\n\n");

	if (level.rpg_lms_mode == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no RPG LMS now\n\"");
		return;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.rpg_lms_players[i] != -1)
		{ // zyk: a player in RPG LMS Battle
			gentity_t *player_ent = &g_entities[i];

			strcpy(content, va("%s^7%s   ^3%d\n", content, player_ent->client->pers.netname, level.rpg_lms_players[i]));
		}
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", content));
}

/*
==================
Cmd_Tutorial_f
==================
*/
void Cmd_Tutorial_f(gentity_t *ent) {
	int page = 1; // zyk: page the user wants to see
	int i = 0;
	int results_per_page = zyk_list_cmds_results_per_page.integer; // zyk: number of results per page
	char arg1[MAX_STRING_CHARS];
	char file_content[MAX_STRING_CHARS * 4];
	char content[MAX_STRING_CHARS];
	FILE *tutorial_file = NULL;

	if (trap->Argc() < 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"You must pass a page number. Example: ^3/tutorial 1^7\n\"");
		return;
	}

	strcpy(file_content, "");
	strcpy(content, "");

	trap->Argv(1, arg1, sizeof(arg1));
	page = atoi(arg1);

	tutorial_file = fopen("zykmod/tutorial.txt", "r");
	if (tutorial_file != NULL)
	{
		if (page > 0)
		{ // zyk: show results of this page
			while (i < (results_per_page * (page - 1)) && fgets(content, sizeof(content), tutorial_file) != NULL)
			{ // zyk: reads the file until it reaches the position corresponding to the page number
				i++;
			}

			while (i < (results_per_page * page) && fgets(content, sizeof(content), tutorial_file) != NULL)
			{ // zyk: fgets returns NULL at EOF
				strcpy(file_content, va("%s%s", file_content, content));
				i++;
			}
		}
		else
		{ // zyk: search for the string
			while (i < results_per_page && fgets(content, sizeof(content), tutorial_file) != NULL)
			{ // zyk: fgets returns NULL at EOF
				if (strstr(content, arg1))
				{
					strcpy(file_content, va("%s%s", file_content, content));
					i++;
				}
			}
		}

		fclose(tutorial_file);
		trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\"", file_content));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Tutorial file does not exist\n\"");
	}
}

/*
==================
Cmd_NoFight_f
==================
*/
void Cmd_NoFight_f(gentity_t *ent) {
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		if (ent->client->pers.player_statuses & (1 << 26))
		{
			ent->client->pers.player_statuses &= ~(1 << 26);
			trap->SendServerCommand(ent->s.number, "print \"Deactivated\n\"");
		}
		else
		{
			ent->client->pers.player_statuses |= (1 << 26);
			trap->SendServerCommand(ent->s.number, "print \"Activated\n\"");
		}
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"This command must be used as spectator\n\"");
	}
}
/*
==================
Cmd_ModVersion_f
==================
*/

void Cmd_ModVersion_f(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\n\"", GAMEVERSION));
}

/*
==================
Cmd_ZykSound_f
==================
*/
void Cmd_ZykSound_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];

	if (zyk_allow_zyksound_command.integer < 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"This command is not allowed in this server\n\"");
		return;
	}

	if (trap->Argc() < 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Use ^3/zyksound <sound file path> ^7to play any sound file\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	G_Sound(ent, CHAN_VOICE, G_SoundIndex(G_NewString(arg1)));
}

// zyk: quantity of chars this player has
int zyk_char_count(gentity_t *ent)
{
	FILE *chars_file;
	char content[64];
	int i = 0;
	int count = 0;

#if defined(__linux__)
	system(va("cd zykmod/accounts ; ls %s_* > chars_%d.txt", ent->client->sess.filename, ent->s.number));
#else
	system(va("cd \"zykmod/accounts\" & dir /B %s_* > chars_%d.txt", ent->client->sess.filename, ent->s.number));
#endif

	chars_file = fopen(va("zykmod/accounts/chars_%d.txt", ent->s.number), "r");
	if (chars_file != NULL)
	{
		i = fscanf(chars_file, "%s", content);
		while (i != EOF)
		{
			if (Q_stricmp(content, va("chars_%d.txt", ent->s.number)) != 0)
			{ // zyk: counting this char
				count++;
			}
			i = fscanf(chars_file, "%s", content);
		}
		fclose(chars_file);
	}

	return count;
}

/*
==================
Cmd_RpgChar_f
==================
*/
void Cmd_RpgChar_f(gentity_t *ent) {
	int argc = trap->Argc();
	FILE *chars_file;
	char content[64];

	strcpy(content, "");

	if (argc == 1)
	{ // zyk: lists the chars and commands
		trap->SendServerCommand(ent->s.number, va("print \"\n^7Using %s\n\n^7%s\n^3/rpgchar new <charname>: ^7creates a new char\n^3/rpgchar rename <new name>: ^7renames current char\n^3/rpgchar use <charname>: ^7uses this char\n^3/rpgchar delete <charname>: ^7removes this char\n^3/rpgchar reset <quests or levels>: ^7resets either quests or levels of the current char\n\"", ent->client->sess.rpgchar, zyk_get_rpg_chars(ent, "\n")));
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char arg2[MAX_STRING_CHARS];

		trap->Argv(1, arg1, sizeof(arg1));

		if (argc == 2)
		{
			trap->SendServerCommand(ent->s.number, "print \"This command requires at least one more argument\n\"");
			return;
		}

		trap->Argv(2, arg2, sizeof(arg2));

		if (Q_stricmp(arg1, "new") == 0)
		{
			if (Q_stricmp(arg2, ent->client->sess.filename) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot overwrite the default char\n\"");
				return;
			}

			if (zyk_check_user_input(arg2, strlen(arg2)) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid charname. Only letters and numbers allowed.\n\"");
				return;
			}

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2), "r");
			if (chars_file != NULL)
			{
				fclose(chars_file);
				trap->SendServerCommand(ent->s.number, "print \"This char already exists\n\"");
				return;
			}

			if (zyk_char_count(ent) >= MAX_RPG_CHARS)
			{
				trap->SendServerCommand(ent->s.number, "print \"Reached the max limit of chars\n\"");
				return;
			}

			if (strlen(arg2) > MAX_ACC_NAME_SIZE)
			{
				trap->SendServerCommand(ent->s.number, va("print \"Char name must have a maximum of %d characters\n\"", MAX_ACC_NAME_SIZE));
				return;
			}

			add_new_char(ent);

			// zyk: saving the current char
			strcpy(ent->client->sess.rpgchar, arg2);

			save_account(ent, qfalse);
			save_account(ent, qtrue);

			trap->SendServerCommand(ent->s.number, va("print \"Char %s ^7created!\n\"", ent->client->sess.rpgchar));

			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->sess.amrpgmode == 2)
			{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
				G_Kill(ent);
			}
		}
		else if (Q_stricmp(arg1, "rename") == 0)
		{
			if (Q_stricmp(ent->client->sess.rpgchar, ent->client->sess.filename) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot rename the default char\n\"");
				return;
			}

			if (zyk_check_user_input(arg2, strlen(arg2)) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid charname. Only letters and numbers allowed.\n\"");
				return;
			}

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2), "r");
			if (chars_file != NULL)
			{
				fclose(chars_file);
				trap->SendServerCommand(ent->s.number, "print \"This char already exists\n\"");
				return;
			}

			if (strlen(arg2) > MAX_ACC_NAME_SIZE)
			{
				trap->SendServerCommand(ent->s.number, va("print \"Char name must have a maximum of %d characters\n\"", MAX_ACC_NAME_SIZE));
				return;
			}

#if defined(__linux__)
			system(va("mv zykmod/accounts/%s_%s.txt zykmod/accounts/%s_%s.txt", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, arg2));
#else
			system(va("cd \"zykmod/accounts\" & MOVE %s_%s.txt %s_%s.txt", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, arg2));
#endif

			// zyk: saving the current char
			strcpy(ent->client->sess.rpgchar, arg2);
			save_account(ent, qfalse);

			trap->SendServerCommand(ent->s.number, va("print \"Renamed to %s^7\n\"", ent->client->sess.rpgchar));
		}
		else if (Q_stricmp(arg1, "use") == 0)
		{
			if (Q_stricmp(arg2, ent->client->sess.rpgchar) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"You are already using this char\n\"");
				return;
			}

			if (zyk_check_user_input(arg2, strlen(arg2)) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid charname. Only letters and numbers allowed.\n\"");
				return;
			}

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2), "r");
			if (chars_file == NULL)
			{
				trap->SendServerCommand(ent->s.number, va("print \"Char %s does not exist\n\"", arg2));
				return;
			}

			fclose(chars_file);

			// zyk: saving the current char
			strcpy(ent->client->sess.rpgchar, arg2);

			save_account(ent, qfalse);

			load_account(ent);

			trap->SendServerCommand(ent->s.number, va("print \"Char %s ^7loaded!\n\"", ent->client->sess.rpgchar));

			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->sess.amrpgmode == 2)
			{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
				G_Kill(ent);
			}
		}
		else if (Q_stricmp(arg1, "delete") == 0)
		{
			if (Q_stricmp(arg2, ent->client->sess.filename) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot delete the default char\n\"");
				return;
			}

			if (Q_stricmp(arg2, ent->client->sess.rpgchar) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot remove the char you are using now. Change the char before deleting this one\n\"");
				return;
			}

			if (zyk_check_user_input(arg2, strlen(arg2)) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid charname. Only letters and numbers allowed.\n\"");
				return;
			}

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2), "r");
			if (chars_file == NULL)
			{
				trap->SendServerCommand(ent->s.number, "print \"This char does not exist\n\"");
				return;
			}
			fclose(chars_file);

			remove(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2));

			trap->SendServerCommand(ent->s.number, va("print \"Char %s ^7deleted!\n\"", arg2));
		}
		else if (Q_stricmp(arg1, "reset") == 0)
		{
			int i = 0;

			if (Q_stricmp(arg2, "quests") == 0)
			{
				ent->client->pers.main_quest_progress = 0;
				ent->client->pers.side_quest_progress = 0;

				save_account(ent, qtrue);

				trap->SendServerCommand(ent->s.number, "print \"Quests resetted.\n\"");

				if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
				{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
					G_Kill(ent);
				}
			}
			else if (Q_stricmp(arg2, "levels") == 0)
			{
				for (i = 0; i < NUMBER_OF_SKILLS; i++)
					ent->client->pers.skill_levels[i] = 0;

				// zyk: initializing RPG inventory
				for (i = 0; i < MAX_RPG_INVENTORY_ITEMS; i++)
				{
					ent->client->pers.rpg_inventory[i] = 0;
				}

				ent->client->pers.level = 1;
				ent->client->pers.level_up_score = 0;
				ent->client->pers.skillpoints = 1;

				ent->client->pers.credits = RPG_INITIAL_CREDITS;

				ent->client->sess.magic_fist_selection = 0;

				// zyk: in RPG Mode, player must actually buy these
				ent->client->ps.jetpackFuel = 0;
				ent->client->ps.cloakFuel = 0;
				ent->client->pers.jetpack_fuel = 0;

				save_account(ent, qtrue);

				trap->SendServerCommand(ent->s.number, "print \"Levels resetted.\n\"");

				if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
				{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
					G_Kill(ent);
				}
			}
			else
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid argument. Must be either ^3quests ^7or ^3levels^7\n\"");
			}
		}

		// zyk: syncronize info to the client menu
		Cmd_ZykChars_f(ent);
	}
}

/*
==================
Cmd_CustomQuest_f
==================
*/
void save_quest_file(int quest_number)
{
	FILE *quest_file = NULL;
	int i = 0;

	zyk_create_dir("customquests");

	quest_file = fopen(va("zykmod/customquests/%d.txt", quest_number), "w");
	fprintf(quest_file, "%s;%s;%s;\n", level.zyk_custom_quest_main_fields[quest_number][0], level.zyk_custom_quest_main_fields[quest_number][1], level.zyk_custom_quest_main_fields[quest_number][2]);

	for (i = 0; i < level.zyk_custom_quest_mission_count[quest_number]; i++)
	{
		int j = 0;

		for (j = 0; j < level.zyk_custom_quest_mission_values_count[quest_number][i]; j += 2)
		{
			fprintf(quest_file, "%s;%s;", level.zyk_custom_quest_missions[quest_number][i][j], level.zyk_custom_quest_missions[quest_number][i][j + 1]);
		}

		if (j > 0)
		{ // zyk: break line if the mission had at least one key/value pair to save
			fprintf(quest_file, "\n");
		}
	}

	fclose(quest_file);
}

// zyk: gets a key value for Custom Quest
char *zyk_get_mission_value(int custom_quest, int mission, char *key)
{
	int i = 0;

	for (i = 0; i < level.zyk_custom_quest_mission_values_count[custom_quest][mission]; i += 2)
	{
		if (Q_stricmp(level.zyk_custom_quest_missions[custom_quest][mission][i], key) == 0)
		{
			return G_NewString(level.zyk_custom_quest_missions[custom_quest][mission][i + 1]);
		}
	}

	return "";
}

// zyk: set the magic powers and unique abilities that this npc can use in custom quest
void zyk_set_quest_npc_abilities(gentity_t *zyk_npc)
{
	int j = 0;
	int k = 0;
	int zyk_power = 0;
	char value[256];
	char *zyk_magic = zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcmagic%d", level.zyk_custom_quest_counter));

	// zyk: magic powers
	zyk_npc->client->pers.custom_quest_magic = 0;

	// zyk: ultimate magic powers and quest powers
	zyk_npc->client->pers.custom_quest_more_magic = 0;

	// zyk: setting the timers
	zyk_npc->client->pers.quest_power_usage_timer = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcfirsttimer%d", level.zyk_custom_quest_counter)));

	// zyk: setting default values of these timers
	if (zyk_npc->client->pers.quest_power_usage_timer <= 0)
	{
		zyk_npc->client->pers.quest_power_usage_timer = level.time + 5000;
	}
	else
	{
		zyk_npc->client->pers.quest_power_usage_timer = level.time + zyk_npc->client->pers.quest_power_usage_timer;
	}

	for (j = 0; j < 256; j++)
	{
		if (zyk_magic[j] != ',' && zyk_magic[j] != '\0')
		{
			value[k] = zyk_magic[j];
			k++;
		}
		else
		{
			value[k] = '\0';
			zyk_power = atoi(value);

			if (zyk_power < MAX_MAGIC_POWERS)
			{
				zyk_npc->client->pers.custom_quest_magic |= (1 << zyk_power);
			}
			else if (zyk_power < MAX_MAGIC_POWERS + 8)
			{
				zyk_npc->client->pers.custom_quest_more_magic |= (1 << (zyk_power - MAX_MAGIC_POWERS));
			}

			k = 0;

			if (zyk_magic[j] == '\0')
			{ // zyk: no more values to parse
				return;
			}
		}
	}
}

void load_custom_quest_mission()
{
	int i = 0, j = 0;
	int current_mission = 0;

	for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		gentity_t *zyk_ent = &g_entities[i];

		if (zyk_ent && zyk_ent->NPC && zyk_ent->client && zyk_ent->health > 0 && zyk_ent->client->pers.player_statuses & (1 << 28))
		{ // zyk: kill any remaining quest npcs that may still be alive
			G_FreeEntity(zyk_ent);
		}

		if (zyk_ent && zyk_ent->item && zyk_ent->spawnflags & 262144)
		{ // zyk: remove quest items
			G_FreeEntity(zyk_ent);
		}
	}

	if (level.zyk_custom_quest_effect_id != -1)
	{
		G_FreeEntity(&g_entities[level.zyk_custom_quest_effect_id]);

		level.zyk_custom_quest_effect_id = -1;
	}

	// zyk: reset this value and try to find a mission
	level.custom_quest_map = -1;

	for (i = 0; i < MAX_CUSTOM_QUESTS; i++)
	{
		if (level.zyk_custom_quest_mission_count[i] != -1 && Q_stricmp(level.zyk_custom_quest_main_fields[i][1], "on") == 0)
		{ // zyk: only set the custom quest map if this is an active quest
			current_mission = atoi(G_NewString(level.zyk_custom_quest_main_fields[i][2]));

			for (j = 0; j < level.zyk_custom_quest_mission_values_count[i][current_mission]/2; j++)
			{ // zyk: goes through all keys of this mission to find the map keys
				char *zyk_map = zyk_get_mission_value(i, current_mission, va("map%d", j));

				if (Q_stricmp(level.zykmapname, zyk_map) == 0 && Q_stricmp(zyk_get_mission_value(i, current_mission, va("donemap%d", j)), "") == 0)
				{ // zyk: current mission of this quest is in the current map and the mission is not done yet
					int radius = atoi(zyk_get_mission_value(i, current_mission, "radius"));
					vec3_t vec;
					gentity_t *effect_ent = NULL;
					char *effect_path = zyk_get_mission_value(i, current_mission, "effect");

					if (radius <= 0)
					{ // zyk: default radius
						radius = 500;
					}

					level.zyk_quest_radius = radius;

					if (sscanf(zyk_get_mission_value(i, current_mission, "origin"), "%f %f %f", &vec[0], &vec[1], &vec[2]) == 3)
					{ // zyk: parsing origin point of this mission
						level.zyk_quest_test_origin = qtrue;
						VectorCopy(vec, level.zyk_quest_mission_origin);
					}
					else
					{
						level.zyk_quest_test_origin = qfalse;
						VectorSet(level.zyk_quest_mission_origin, 0, 0, 0);
					}

					// zyk: adding effect in custom quest origin
					if (Q_stricmp(effect_path, "") != 0)
					{
						effect_ent = G_Spawn();

						zyk_set_entity_field(effect_ent, "classname", "fx_runner");
						zyk_set_entity_field(effect_ent, "spawnflags", "0");
						zyk_set_entity_field(effect_ent, "origin", va("%f %f %f", level.zyk_quest_mission_origin[0], level.zyk_quest_mission_origin[1], level.zyk_quest_mission_origin[2]));

						effect_ent->s.modelindex = G_EffectIndex(effect_path);

						zyk_spawn_entity(effect_ent);

						level.zyk_custom_quest_effect_id = effect_ent->s.number;
					}

					// zyk: setting default values for other control variables
					level.zyk_hold_quest_mission = qfalse;

					// zyk: add some time before starting the mission
					level.zyk_custom_quest_timer = level.time + 2000;

					level.zyk_custom_quest_counter = 0;
					level.zyk_quest_npc_count = 0;
					level.zyk_quest_ally_npc_count = 0;
					level.zyk_quest_item_count = 0;
					level.custom_quest_map = i;
					level.zyk_custom_quest_current_mission = current_mission;
					return;
				}
			}
		}
	}
}

void zyk_set_quest_field(int quest_number, int mission_number, char *key, char *value)
{
	int i = 0;

	// zyk: new mission
	if (mission_number == level.zyk_custom_quest_mission_count[quest_number])
	{
		level.zyk_custom_quest_mission_values_count[quest_number][mission_number] = 0;
		level.zyk_custom_quest_mission_count[quest_number]++;
	}

	if (Q_stricmp(key, "zykremovemission") == 0)
	{
		// zyk: mission must be removed in this case
		for (i = mission_number; i < (level.zyk_custom_quest_mission_count[quest_number] - 1); i++)
		{
			int j = 0;

			for (j = 0; j < level.zyk_custom_quest_mission_values_count[quest_number][i + 1]; j += 2)
			{ // zyk: save all key/value pairs from next mission to this one, it will move all mission by one position
				level.zyk_custom_quest_missions[quest_number][i][j] = G_NewString(level.zyk_custom_quest_missions[quest_number][i + 1][j]);
				level.zyk_custom_quest_missions[quest_number][i][j + 1] = G_NewString(level.zyk_custom_quest_missions[quest_number][i + 1][j + 1]);
			}

			level.zyk_custom_quest_mission_values_count[quest_number][i] = level.zyk_custom_quest_mission_values_count[quest_number][i + 1];
		}

		level.zyk_custom_quest_mission_values_count[quest_number][i] = 0;
		level.zyk_custom_quest_mission_count[quest_number]--;

		return;
	}

	// zyk: see if this key already exists to edit it. If not, add a new one
	while (i < level.zyk_custom_quest_mission_values_count[quest_number][mission_number])
	{
		if (Q_stricmp(level.zyk_custom_quest_missions[quest_number][mission_number][i], key) == 0)
		{ // zyk: found the key
			if (Q_stricmp(value, "zykremovekey") == 0)
			{ // zyk: remove the key
			  // zyk: starts from the next key
				i += 2;

				// zyk: moves all keys after this one 2 positions to remove the key
				while (i < level.zyk_custom_quest_mission_values_count[quest_number][mission_number])
				{
					level.zyk_custom_quest_missions[quest_number][mission_number][i - 2] = G_NewString(level.zyk_custom_quest_missions[quest_number][mission_number][i]);
					level.zyk_custom_quest_missions[quest_number][mission_number][i - 1] = G_NewString(level.zyk_custom_quest_missions[quest_number][mission_number][i + 1]);

					i += 2;
				}

				// zyk: decrease the counter
				level.zyk_custom_quest_mission_values_count[quest_number][mission_number] -= 2;
			}
			else
			{ // zyk: edit the key
				level.zyk_custom_quest_missions[quest_number][mission_number][i] = G_NewString(key);
				level.zyk_custom_quest_missions[quest_number][mission_number][i + 1] = G_NewString(value);
			}

			return;
		}

		i += 2;
	}

	// zyk: a new key. Add it
	level.zyk_custom_quest_missions[quest_number][mission_number][i] = G_NewString(key);
	level.zyk_custom_quest_missions[quest_number][mission_number][i + 1] = G_NewString(value);

	// zyk: increases the counter
	level.zyk_custom_quest_mission_values_count[quest_number][mission_number] += 2;
}

void Cmd_CustomQuest_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char content[MAX_STRING_CHARS];
	int i = 0;
	int argc = trap->Argc();

	if (!(ent->client->pers.bitvalue & (1 << ADM_CUSTOMQUEST)))
	{ // zyk: admin command
		trap->SendServerCommand(ent->s.number, "print \"You don't have this admin command.\n\"");
		return;
	}

	strcpy(content, "");

	if (argc == 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"\n^3/customquest quests: ^7visualize custom quests\n^3/customquest new: ^7creates a new quest\n^3/customquest delete <quest number>: ^7removes the quest\n^3/customquest change <quest number> <field> <value>: ^7changes value of the main quest fields\n^3/customquest edit <quest number> [mission number] [field] [value]: ^7sets the value to the field of this quest mission. Can also be used to see quest info or mission info\n\"");
	}
	else
	{
		FILE *quest_file = NULL;
		char arg2[MAX_STRING_CHARS];
		char arg3[MAX_STRING_CHARS];
		char arg4[MAX_STRING_CHARS];
		char arg5[MAX_STRING_CHARS];
		int quest_number = -1;
		int mission_number = -1;

		trap->Argv(1, arg1, sizeof(arg1));

		if (Q_stricmp(arg1, "quests") == 0)
		{
			for (i = 0; i < MAX_CUSTOM_QUESTS; i++)
			{
				if (level.zyk_custom_quest_mission_count[i] != -1)
				{ // zyk: quest exists
					strcpy(content, va("%s%d - %s (%s)\n", content, i, level.zyk_custom_quest_main_fields[i][0], level.zyk_custom_quest_main_fields[i][1]));
				}
			}

			trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\"", content));
		}
		else if (Q_stricmp(arg1, "new") == 0)
		{
			for (i = 0; i < MAX_CUSTOM_QUESTS; i++)
			{
				if (level.zyk_custom_quest_mission_count[i] == -1)
				{ // zyk: found an empty position for the new quest
					level.zyk_custom_quest_mission_count[i] = 0;
					quest_number = i;
					break;
				}
			}

			if (quest_number == -1)
			{ // zyk: did not find an empty position for new quest
				trap->SendServerCommand(ent->s.number, "print \"Cannot create new quests. Max amount of quests reached.\n\"");
				return;
			}

			// zyk: setting default values of the quest main fields and saving in the quest file
			level.zyk_custom_quest_main_fields[quest_number][0] = G_NewString(va("Quest %d", quest_number));
			level.zyk_custom_quest_main_fields[quest_number][1] = G_NewString("off");
			level.zyk_custom_quest_main_fields[quest_number][2] = G_NewString("0");
			level.zyk_custom_quest_main_fields[quest_number][3] = G_NewString("");

			save_quest_file(quest_number);

			trap->SendServerCommand(ent->s.number, "print \"Quest created.\n\"");
		}
		else if (Q_stricmp(arg1, "delete") == 0)
		{
			if (argc == 2)
			{
				trap->SendServerCommand(ent->s.number, "print \"Must pass the quest number.\n\"");
				return;
			}

			trap->Argv(2, arg2, sizeof(arg2));

			quest_number = atoi(arg2);

			if (quest_number < 0 || quest_number >= MAX_CUSTOM_QUESTS || level.zyk_custom_quest_mission_count[quest_number] == -1)
			{ // zyk: this position is already empty, there is no quest to be removed in this position
				trap->SendServerCommand(ent->s.number, "print \"This quest does not exist.\n\"");
				return;
			}

			// zyk: now it will be an empty position
			level.zyk_custom_quest_mission_count[quest_number] = -1;

			zyk_create_dir("customquests");

			quest_file = fopen(va("zykmod/customquests/%d.txt", quest_number), "r");
			if (quest_file)
			{
				fclose(quest_file);

				remove(va("zykmod/customquests/%d.txt", quest_number));

				// zyk: search for a new active quest in this map
				if (level.custom_quest_map == quest_number)
				{
					load_custom_quest_mission();
				}

				trap->SendServerCommand(ent->s.number, "print \"Quest removed.\n\"");
			}
			else
			{
				trap->SendServerCommand(ent->s.number, "print \"File does not exist\n\"");
			}
		}
		else if (Q_stricmp(arg1, "change") == 0)
		{
			if (argc < 5)
			{
				trap->SendServerCommand(ent->s.number, "print \"Must pass the quest number, field and value.\n\"");
				return;
			}

			trap->Argv(2, arg2, sizeof(arg2));
			trap->Argv(3, arg3, sizeof(arg3));
			trap->Argv(4, arg4, sizeof(arg4));

			quest_number = atoi(arg2);

			if (quest_number < 0 || quest_number >= MAX_CUSTOM_QUESTS || level.zyk_custom_quest_mission_count[quest_number] == -1)
			{ // zyk: this position is already empty, there is no quest to be removed in this position
				trap->SendServerCommand(ent->s.number, "print \"This quest does not exist.\n\"");
				return;
			}

			// zyk: changing the main quest fields
			if (Q_stricmp(arg3, "name") == 0)
			{
				level.zyk_custom_quest_main_fields[quest_number][0] = G_NewString(arg4);
			}
			else if (Q_stricmp(arg3, "active") == 0)
			{
				if (Q_stricmp(level.zyk_custom_quest_main_fields[quest_number][1], "on") == 0)
				{
					level.zyk_custom_quest_main_fields[quest_number][1] = G_NewString("off");
				}
				else
				{
					level.zyk_custom_quest_main_fields[quest_number][1] = G_NewString("on");
				}

				// zyk: loads the quests in this map
				load_custom_quest_mission();
			}
			else if (Q_stricmp(arg3, "count") == 0)
			{
				level.zyk_custom_quest_main_fields[quest_number][2] = G_NewString(va("%d", atoi(arg4)));
			}
			else
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid field.\n\"");
				return;
			}

			// zyk: saving info in the quest file
			save_quest_file(quest_number);

			trap->SendServerCommand(ent->s.number, "print \"Changes saved.\n\"");
		}
		else if (Q_stricmp(arg1, "edit") == 0)
		{
			int k = 0;

			if (argc == 2)
			{
				trap->SendServerCommand(ent->s.number, "print \"Must pass at least the quest number\n\"");
				return;
			}
			else if (argc == 3)
			{ // zyk: quest info
				trap->Argv(2, arg2, sizeof(arg2));

				quest_number = atoi(arg2);

				if (quest_number < 0 || quest_number >= MAX_CUSTOM_QUESTS || level.zyk_custom_quest_mission_count[quest_number] == -1)
				{ // zyk: this position is already empty, there is no quest to be removed in this position
					trap->SendServerCommand(ent->s.number, "print \"This quest does not exist.\n\"");
					return;
				}

				trap->SendServerCommand(ent->s.number, va("print \"\n^3Name: ^7%s\n^3Status: ^7%s\n^3Number of missions: ^7%d\n^3Number of completed missions: ^7%s\n\"", level.zyk_custom_quest_main_fields[quest_number][0], level.zyk_custom_quest_main_fields[quest_number][1], level.zyk_custom_quest_mission_count[quest_number], level.zyk_custom_quest_main_fields[quest_number][2]));
				return;
			}
			else if (argc == 4)
			{ // zyk: mission info
				trap->Argv(2, arg2, sizeof(arg2));
				trap->Argv(3, arg3, sizeof(arg3));

				ent->client->pers.custom_quest_quest_number = atoi(arg2);
				ent->client->pers.custom_quest_mission_number = atoi(arg3);

				quest_number = ent->client->pers.custom_quest_quest_number;
				mission_number = ent->client->pers.custom_quest_mission_number;

				if (quest_number < 0 || quest_number >= MAX_CUSTOM_QUESTS || level.zyk_custom_quest_mission_count[quest_number] == -1)
				{ // zyk: this position is already empty, there is no quest to be removed in this position
					trap->SendServerCommand(ent->s.number, "print \"This quest does not exist.\n\"");
					return;
				}

				if (mission_number < 0 || mission_number >= MAX_CUSTOM_QUEST_MISSIONS || level.zyk_custom_quest_mission_count[quest_number] <= mission_number)
				{ // zyk: mission number must be valid
					trap->SendServerCommand(ent->s.number, "print \"This mission does not exist in this quest.\n\"");
					return;
				}

				if (ent->client->pers.custom_quest_print > 0)
				{
					trap->SendServerCommand(ent->s.number, "print \"Still printing info. Please wait.\n\"");
					return;
				}

				// zyk: starts printing info
				ent->client->pers.custom_quest_print = 1;
				ent->client->pers.custom_quest_print_timer = 0;

				return;
			}
			else if (argc > 4 && (argc % 2) != 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Must pass an even number of arguments.\n\"");
				return;
			}

			trap->Argv(2, arg2, sizeof(arg2));
			trap->Argv(3, arg3, sizeof(arg3));

			quest_number = atoi(arg2);
			mission_number = atoi(arg3);

			if (quest_number < 0 || quest_number >= MAX_CUSTOM_QUESTS || level.zyk_custom_quest_mission_count[quest_number] == -1)
			{ // zyk: this position is already empty, there is no quest to be removed in this position
				trap->SendServerCommand(ent->s.number, "print \"This quest does not exist.\n\"");
				return;
			}

			if (mission_number < 0 || mission_number >= MAX_CUSTOM_QUEST_MISSIONS || mission_number > level.zyk_custom_quest_mission_count[quest_number])
			{ // zyk: mission number must be valid
				trap->SendServerCommand(ent->s.number, "print \"This mission does not exist in this quest.\n\"");
				return;
			}

			for (k = 4; k < argc; k += 2)
			{
				trap->Argv(k, arg4, sizeof(arg4));
				trap->Argv(k + 1, arg5, sizeof(arg5));

				zyk_set_quest_field(quest_number, mission_number, G_NewString(arg4), G_NewString(arg5));
			}

			// zyk: saving info in the quest file
			save_quest_file(quest_number);

			trap->SendServerCommand(ent->s.number, "print \"Mission edited.\n\"");
		}
		else
		{
			trap->SendServerCommand(ent->s.number, "print \"Invalid option.\n\"");
		}
	}
}

/*
==================
Cmd_DuelBoard_f
==================
*/
void Cmd_DuelBoard_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int page = 1; // zyk: page the user wants to see
	char file_content[MAX_STRING_CHARS];
	char content[64];
	int i = 0;
	int results_per_page = zyk_list_cmds_results_per_page.integer; // zyk: number of results per page
	FILE *leaderboard_file;

	if (trap->Argc() < 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Use ^3/duelboard <page number> ^7to see the Duel Tournament Leaderboard, which shows the winners and their number of tournaments won\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (level.duel_leaderboard_step > 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Leaderboard is being generated. Please wait some seconds\n\"");
		return;
	}

	strcpy(file_content, "");
	strcpy(content, "");

	page = atoi(arg1);

	if (page == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	leaderboard_file = fopen("zykmod/leaderboard.txt", "r");
	if (leaderboard_file != NULL)
	{
		while (i < (results_per_page * (page - 1)) && fgets(content, sizeof(content), leaderboard_file) != NULL)
		{ // zyk: reads the file until it reaches the position corresponding to the page number
			if (content[strlen(content) - 1] == '\n')
				content[strlen(content) - 1] = '\0';
			fgets(content, sizeof(content), leaderboard_file);
			if (content[strlen(content) - 1] == '\n')
				content[strlen(content) - 1] = '\0';
			fgets(content, sizeof(content), leaderboard_file);
			if (content[strlen(content) - 1] == '\n')
				content[strlen(content) - 1] = '\0';
			i++;
		}

		while (i < (results_per_page * page) && fgets(content, sizeof(content), leaderboard_file) != NULL)
		{
			if (content[strlen(content) - 1] == '\n')
				content[strlen(content) - 1] = '\0';

			// zyk: player name
			fgets(content, sizeof(content), leaderboard_file);
			if (content[strlen(content) - 1] == '\n')
				content[strlen(content) - 1] = '\0';
			strcpy(file_content, va("%s%s     ", file_content, content));

			// zyk: number of tournaments won
			fgets(content, sizeof(content), leaderboard_file);
			if (content[strlen(content) - 1] == '\n')
				content[strlen(content) - 1] = '\0';
			strcpy(file_content, va("%s^3%s^7\n", file_content, content));

			i++;
		}

		fclose(leaderboard_file);
		trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\"", file_content));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"No leaderboard yet\n\"");
		return;
	}
}

/*
=================
ClientCommand
=================
*/

#define CMD_NOINTERMISSION		(1<<0)
#define CMD_CHEAT				(1<<1)
#define CMD_ALIVE				(1<<2)
#define CMD_LOGGEDIN			(1<<3) // zyk: player must be logged in his account
#define CMD_RPG					(1<<4) // zyk: player must be in RPG Mode

typedef struct command_s {
	const char	*name;
	void		(*func)(gentity_t *ent);
	int			flags;
} command_t;

int cmdcmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((command_t*)b)->name );
}


/* This array MUST be sorted correctly by alphabetical name field */
command_t commands[] = {
	{ "addbot",				Cmd_AddBot_f,				0 },
	{ "admindown",			Cmd_AdminDown_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "adminlist",			Cmd_AdminList_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "adminup",			Cmd_AdminUp_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "admkick",			Cmd_AdmKick_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "allyadd",			Cmd_AllyAdd_f,				CMD_NOINTERMISSION },
	{ "allychat",			Cmd_AllyChat_f,				CMD_NOINTERMISSION },
	{ "allylist",			Cmd_AllyList_f,				CMD_NOINTERMISSION },
	{ "allyremove",			Cmd_AllyRemove_f,			CMD_NOINTERMISSION },
	{ "bountyquest",		Cmd_BountyQuest_f,			CMD_RPG|CMD_NOINTERMISSION },
	{ "buy",				Cmd_Buy_f,					CMD_RPG|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "callseller",			Cmd_CallSeller_f,			CMD_RPG|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "callteamvote",		Cmd_CallTeamVote_f,			CMD_NOINTERMISSION },
	{ "callvote",			Cmd_CallVote_f,				CMD_NOINTERMISSION },
	{ "changepassword",		Cmd_ChangePassword_f,		CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "clientprint",		Cmd_ClientPrint_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "creditgive",			Cmd_CreditGive_f,			CMD_RPG|CMD_NOINTERMISSION },
	{ "customquest",		Cmd_CustomQuest_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "datetime",			Cmd_DateTime_f,				CMD_NOINTERMISSION },
	{ "debugBMove_Back",	Cmd_BotMoveBack_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Forward",	Cmd_BotMoveForward_f,		CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Left",	Cmd_BotMoveLeft_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Right",	Cmd_BotMoveRight_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Up",		Cmd_BotMoveUp_f,			CMD_CHEAT|CMD_ALIVE },
	{ "down",				Cmd_DownSkill_f,			CMD_RPG|CMD_NOINTERMISSION },
	{ "drop",				Cmd_Drop_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "duelarena",			Cmd_DuelArena_f,			CMD_LOGGEDIN|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "duelboard",			Cmd_DuelBoard_f,			CMD_NOINTERMISSION },
	{ "duelmode",			Cmd_DuelMode_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "duelpause",			Cmd_DuelPause_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "dueltable",			Cmd_DuelTable_f,			CMD_NOINTERMISSION },
	{ "duelteam",			Cmd_DuelTeam_f,				CMD_NOINTERMISSION },
	{ "emote",				Cmd_Emote_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "entadd",				Cmd_EntAdd_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entdeletefile",		Cmd_EntDeleteFile_f,		CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entedit",			Cmd_EntEdit_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entitysystem",		Cmd_EntitySystem_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entlist",			Cmd_EntList_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entload",			Cmd_EntLoad_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entnear",			Cmd_EntNear_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entorigin",			Cmd_EntOrigin_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entremove",			Cmd_EntRemove_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entsave",			Cmd_EntSave_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "entundo",			Cmd_EntUndo_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "follow",				Cmd_Follow_f,				CMD_NOINTERMISSION },
	{ "follownext",			Cmd_FollowNext_f,			CMD_NOINTERMISSION },
	{ "followprev",			Cmd_FollowPrev_f,			CMD_NOINTERMISSION },
	{ "forcechanged",		Cmd_ForceChanged_f,			0 },
	{ "gc",					Cmd_GameCommand_f,			CMD_NOINTERMISSION },
	{ "give",				Cmd_Give_f,					CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "god",				Cmd_God_f,					CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "ignore",				Cmd_Ignore_f,				CMD_NOINTERMISSION },
	{ "ignorelist",			Cmd_IgnoreList_f,			CMD_NOINTERMISSION },
	{ "jetpack",			Cmd_Jetpack_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "kill",				Cmd_Kill_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "killother",			Cmd_KillOther_f,			CMD_CHEAT|CMD_NOINTERMISSION },
//	{ "kylesmash",			TryGrapple,					0 },
	{ "levelshot",			Cmd_LevelShot_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "list",				Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "login",				Cmd_LoginAccount_f,			CMD_NOINTERMISSION },
	{ "logout",				Cmd_LogoutAccount_f,		CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "magic",				Cmd_Magic_f,				CMD_RPG|CMD_NOINTERMISSION },
	{ "maplist",			Cmd_MapList_f,				CMD_NOINTERMISSION },
	{ "meleearena",			Cmd_MeleeArena_f,			CMD_ALIVE|CMD_NOINTERMISSION },
	{ "meleemode",			Cmd_MeleeMode_f,			CMD_ALIVE|CMD_NOINTERMISSION },
	{ "modversion",			Cmd_ModVersion_f,			CMD_NOINTERMISSION },
	{ "new",				Cmd_NewAccount_f,			CMD_NOINTERMISSION },
	{ "noclip",				Cmd_Noclip_f,				CMD_LOGGEDIN|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "nofight",			Cmd_NoFight_f,				CMD_NOINTERMISSION },
	{ "notarget",			Cmd_Notarget_f,				CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "npc",				Cmd_NPC_f,					CMD_LOGGEDIN },
	{ "order",				Cmd_Order_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "paralyze",			Cmd_Paralyze_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "playermode",			Cmd_PlayerMode_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "players",			Cmd_Players_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "racemode",			Cmd_RaceMode_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "remap",				Cmd_Remap_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remapdeletefile",	Cmd_RemapDeleteFile_f,		CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remaplist",			Cmd_RemapList_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remapload",			Cmd_RemapLoad_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remapsave",			Cmd_RemapSave_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "rpgchar",			Cmd_RpgChar_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "rpglmsmode",			Cmd_RpgLmsMode_f,			CMD_RPG|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "rpglmstable",		Cmd_RpgLmsTable_f,			CMD_NOINTERMISSION },
	{ "saber",				Cmd_Saber_f,				CMD_NOINTERMISSION },
	{ "say",				Cmd_Say_f,					0 },
	{ "say_team",			Cmd_SayTeam_f,				0 },
	{ "scale",				Cmd_Scale_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "score",				Cmd_Score_f,				0 },
	{ "sell",				Cmd_Sell_f,					CMD_RPG|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "settings",			Cmd_Settings_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "setviewpos",			Cmd_SetViewpos_f,			CMD_CHEAT|CMD_NOINTERMISSION },
	{ "siegeclass",			Cmd_SiegeClass_f,			CMD_NOINTERMISSION },
	{ "silence",			Cmd_Silence_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "snipermode",			Cmd_SniperMode_f,			CMD_ALIVE|CMD_NOINTERMISSION },
	{ "snipertable",		Cmd_SniperTable_f,			CMD_NOINTERMISSION },
	{ "stuff",				Cmd_Stuff_f,				CMD_RPG|CMD_NOINTERMISSION },
	{ "team",				Cmd_Team_f,					CMD_NOINTERMISSION },
//	{ "teamtask",			Cmd_TeamTask_f,				CMD_NOINTERMISSION },
	{ "teamvote",			Cmd_TeamVote_f,				CMD_NOINTERMISSION },
	{ "tele",				Cmd_Teleport_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "teleport",			Cmd_Teleport_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "tell",				Cmd_Tell_f,					0 },
	{ "thedestroyer",		Cmd_TheDestroyer_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "tutorial",			Cmd_Tutorial_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "t_use",				Cmd_TargetUse_f,			CMD_CHEAT|CMD_ALIVE },
	{ "up",					Cmd_UpSkill_f,				CMD_RPG|CMD_NOINTERMISSION },
	{ "voice_cmd",			Cmd_VoiceCommand_f,			CMD_NOINTERMISSION },
	{ "vote",				Cmd_Vote_f,					CMD_NOINTERMISSION },
	{ "where",				Cmd_Where_f,				CMD_NOINTERMISSION },
	{ "zykchars",			Cmd_ZykChars_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "zykfile",			Cmd_ZykFile_f,				CMD_NOINTERMISSION },
	{ "zyklist",			Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "zyklogin",			Cmd_LoginAccount_f,			CMD_NOINTERMISSION },
	{ "zyklogout",			Cmd_LogoutAccount_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "zykmod",				Cmd_ZykMod_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "zyknew",				Cmd_NewAccount_f,			CMD_NOINTERMISSION },
	{ "zyksound",			Cmd_ZykSound_f,				CMD_NOINTERMISSION },
};
static const size_t numCommands = ARRAY_LEN( commands );

void ClientCommand( int clientNum ) {
	gentity_t	*ent = NULL;
	char		cmd[MAX_TOKEN_CHARS] = {0};
	command_t	*command = NULL;

	ent = g_entities + clientNum;
	if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
		G_SecurityLogPrintf( "ClientCommand(%d) without an active connection\n", clientNum );
		return;		// not fully in game yet
	}

	trap->Argv( 0, cmd, sizeof( cmd ) );

	//rww - redirect bot commands
	if ( strstr( cmd, "bot_" ) && AcceptBotCommand( cmd, ent ) )
		return;
	//end rww

	command = (command_t *)Q_LinearSearch( cmd, commands, numCommands, sizeof( commands[0] ), cmdcmp );
	if ( !command )
	{
		trap->SendServerCommand( clientNum, va( "print \"Unknown command %s\n\"", cmd ) );
		return;
	}

	else if ( (command->flags & CMD_NOINTERMISSION)
		&& ( level.intermissionQueued || level.intermissiontime ) )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s (%s)\n\"", G_GetStringEdString( "MP_SVGAME", "CANNOT_TASK_INTERMISSION" ), cmd ) );
		return;
	}

	else if ( (command->flags & CMD_CHEAT)
		&& !sv_cheats.integer )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOCHEATS" ) ) );
		return;
	}

	else if ( (command->flags & CMD_ALIVE)
		&& (ent->health <= 0
			|| ent->client->tempSpectate >= level.time
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR) )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}

	else if ( (command->flags & CMD_LOGGEDIN)
		&& ent->client->sess.amrpgmode == 0 )
	{ // zyk: new condition
		trap->SendServerCommand( clientNum, "print \"You must be logged in\n\"" );
		return;
	}

	else if ( (command->flags & CMD_RPG)
		&& ent->client->sess.amrpgmode < 2 )
	{ // zyk: new condition
		trap->SendServerCommand( clientNum, "print \"You must be in RPG Mode\n\"" );
		return;
	}

	else
		command->func( ent );
}
