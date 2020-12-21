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

// zyk: max levels of the RPG skills
const int max_skill_levels[NUMBER_OF_SKILLS] = {
	5, // Jump
	3, // Push
	3, // Pull
	4, // Speed
	3, // Sense
	5, // Saber Attack
	3, // Saber Defense
	4, // Saber Throw
	4, // Absorb
	3, // Heal
	4, // Protect
	3, // Mind Trick
	3, // Team Heal
	4, // Lightning
	3, // Grip
	3, // Drain
	4, // Rage
	3, // Team Energize
	3, // Stun Baton
	2, // Blaster Pistol
	2, // E11 Blaster Rifle
	2, // Disruptor
	2, // Bowcaster
	2, // Repeater
	2, // DEMP2
	2, // Flechette
	2, // Rocket Launcher
	2, // Concussion Rifle
	2, // Bryar Pistol
	3, // Melee
	5, // Max Shield
	4, // Shield Strength
	4, // Health Strength
	1, // Drain Shield
	3, // Jetpack
	3, // Sense Health
	3, // Shield Heal
	3, // Team Shield Heal
	1, // Unique Skill
	3, // Blaster Pack
	3, // Powercell
	3, // Metal Bolts
	3, // Rockets
	3, // Thermals
	3, // Trip Mines
	3, // Detpacks
	1, // Binoculars
	1, // Bacta Canister
	1, // Sentry Gun
	1, // Seeker Drone
	1, // E-Web
	1, // Big Bacta
	1, // Force Field
	1, // Cloak Item
	5, // Force Power
	4 // Improvements
};

// zyk: returns the name of a RPG skill
char* zyk_skill_name(int skill_index)
{
	char *skill_names[NUMBER_OF_SKILLS] = {
		"Jump",
		"Push",
		"Pull",
		"Speed",
		"Sense",
		"Saber Attack",
		"Saber Defense",
		"Saber Throw",
		"Absorb",
		"Heal",
		"Protect",
		"Mind Trick",
		"Team Heal",
		"Lightning",
		"Grip",
		"Drain",
		"Rage",
		"Team Energize",
		"Stun Baton",
		"Blaster Pistol",
		"E11 Blaster Rifle",
		"Disruptor",
		"Bowcaster",
		"Repeater",
		"DEMP2",
		"Flechette",
		"Rocket Launcher",
		"Concussion Rifle",
		"Bryar Pistol",
		"Melee",
		"Max Shield",
		"Shield Strength",
		"Health Strength",
		"Drain Shield",
		"Jetpack",
		"Sense Health",
		"Shield Heal",
		"Team Shield Heal",
		"Unique Skill",
		"Blaster Pack",
		"Powercell",
		"Metal Bolts",
		"Rockets",
		"Thermals",
		"Trip Mines",
		"Detpacks",
		"Binoculars",
		"Bacta Canister",
		"Sentry Gun",
		"Seeker Drone",
		"E-Web",
		"Big Bacta",
		"Force Field",
		"Cloak Item",
		"Force Power",
		"Improvements"
	};

	if (skill_index >= 0 && skill_index < NUMBER_OF_SKILLS)
	{
		return G_NewString(skill_names[skill_index]);
	}
	else
	{
		return "";
	}
}

// zy: returns the description of a RPG skill
char* zyk_skill_description(int skill_index)
{
	char* skill_description[NUMBER_OF_SKILLS] = {
		"makes you use the force to jump higher. Level 5 has no height limit, you can continue jumping up until you run out of force, and it also lets you jump out of water",
		"pushes the opponent forward",
		"pulls the opponent towards you",
		"increases your speed. Level 1 is 1.7 times normal speed. Level 2 is 2.1, level 3 is 2.5 times and level 4 is 2.9 times",
		"allows you to see people through walls, invisible people or cloaked people and you can dodge disruptor shots. Represents your mind strength to resist Mind Control if your sense level is equal or higher than the enemy's mind trick level",
		G_NewString(va("gives you the saber. If you are using Single Saber, gives you the saber styles. If using duals or staff, increases saber damage, which is increased by 20 per cent for each level. This server has a saber damage scale of %.2f", g_saberDamageScale.value)),
		"increases your ability to block, parry enemy saber attacks or enemy shots",
		G_NewString(va("throws your saber at enemy and gets it back. Each level increases max distance and saber throw speed. Has %d damage", zyk_saber_throw_damage.integer)),
		"allows you to absorb force power attacks done to you",
		"recover some Health. Level 1 restores 5 hp, level 2 restores 10 hp and level 3 restores 25 hp",
		"decreases damage done to you by non-force power attacks. At level 4 decreases force consumption when receiving damage",
		"makes yourself invisible to the players affected by this force power. Force User class can mind control a player or npc. Level 1 has a duration of 20 seconds, level 2 is 25 seconds and level 3 is 30 seconds",
		"restores some health to players near you",
		"attacks with a powerful electric attack at players near you. At level 4, does more damage and pushes the enemy back",
		"attacks a player by holding and damaging him",
		"drains force power from a player to restore your health",
		"makes you 1.3 times faster, increases your saber attack speed and damage and makes you get less damage. Force Guardian class, with Improvements skill at least on level 1, can regen some force when taking damage on health while Rage is active",
		"restores some force power to players near you. If Improvements skill is at least at level 1, regens blaster pack and power cell ammo of the target players",
		G_NewString(va("attacks someone with a small electric charge. Has %d damage multiplied by the stun baton level. Can fire the flame thrower when using alternate fire (does not work for Force User, Monk, Duelist or Magic Master). With Stun Baton Upgrade, it opens any door, even locked ones, and can destroy or move some other objects, and also decloaks enemies and decrease their moving speed for some seconds", zyk_stun_baton_damage.integer)),
		G_NewString(va("the popular Star Wars pistol used by Han Solo in the movies. Normal fire is a single blaster shot, alternate fire allows you to fire a powerful charged shot. The pistol shot does %d damage. The charged shot causes a lot more damage depending on how much it was charged", zyk_blaster_pistol_damage.integer)),
		G_NewString(va("the rifle used by the Storm Troopers. E11 shots do %d damage. Normal fire is a single shot, while the alternate fire is the rapid fire", zyk_e11_blaster_rifle_damage.integer)),
		G_NewString(va("the sniper, used by the rodians ingame. Normal fire is a shot that causes %d damage, alternate fire allows zoom and a charged shot that when fully charged, causes %d damage", zyk_disruptor_damage.integer, zyk_disruptor_alt_damage.integer)),
		G_NewString(va("the famous weapon used by Chewbacca. Normal fire can be charged to fire up to 5 shots at once. It does %d damage", zyk_bowcaster_damage.integer)),
		G_NewString(va("a powerful weapon with a rapid fire and a plasma bomb. Normal fire shoots the rapid fire, and does %d damage. Alt fire fires the plasma bomb and does %d damage", zyk_repeater_damage.integer, zyk_repeater_alt_damage.integer)),
		G_NewString(va("a very powerful weapon against machine npc and some vehicles, causing more damage to them and stunning them. Normal fire does %d damage and alt fire can be charged and does %d damage", zyk_demp2_damage.integer, zyk_demp2_alt_damage.integer)),
		G_NewString(va("this weapon is similar to a shotgun. Normal fire causes %d damage. Alt fire shoots 2 bombs and causes %d damage", zyk_flechette_damage.integer, zyk_flechette_alt_damage.integer)),
		G_NewString(va("a powerful explosive weapon. Normal fire shoots a rocket causing %d damage. Alt fire shoots a homing missile", zyk_rocket_damage.integer)),
		G_NewString(va("it shoots a powerful shot that has a big damage area. Alt fire shoots a ray similar to disruptor shots, but it can go through force fields and can throw the enemy on the ground. Normal fire does %d damage and alt fire does %d damage", zyk_concussion_damage.integer, zyk_concussion_alt_damage.integer)),
		G_NewString(va("very similar to the blaster pistol, but this one has a better fire rate with normal shot. Does %d damage", zyk_blaster_pistol_damage.integer)),
		G_NewString(va("allows you to attack with your fists and legs. You can punch, kick or do a special melee attack by holding both Attack and Alt Attack buttons (usually the mouse buttons). At level 0, melee attacks cause only half normal damage. Right hand punch causes %d normal damage, left hand punch causes %d normal damage and kick causes %d damage at level 1", zyk_melee_right_hand_damage.integer, zyk_melee_left_hand_damage.integer, zyk_melee_kick_damage.integer)),
		"The max shield (armor) the player can have. Each level increases 20 per cent of max shield the player can have",
		"Each level increases your shield resistance by 7 per cent",
		"Each level increases your health resistance by 7 per cent",
		"When using Drain force power, and your health is full, restores some shield. It also makes Drain suck hp/shield from the enemy to restore your hp/shield",
		"the jetpack, used by Boba Fett. Allows you to fly. To use it, jump and press the Use key (usually R) while in the middle of the jump. Each level uses less fuel, allowing you to fly for a longer time",
		"allows you to see info about someone, including npcs. Level 1 shows current health. Level 2 shows name, health and shield. Level 3 shows name, health and max health, shield and max shield, force and max force, mp and max mp. To use it, when you are near a player or npc, use ^3Sense ^7force power",
		G_NewString(va("recovers 4 shield at level 1, 8 shield at level 2 and 12 shield at level 3. To use it, use Heal force power when you have full HP. This skill uses %d force power", zyk_max_force_power.integer / 2)),
		"recovers 3 shield at level 1, 6 shield at level 2 and 9 shield at level 3 to players near you. To use it, when near players, use Team Heal force power. It will heal their shield after they have full HP",
		"Used by pressing Engage Duel key\nFree Warrior: recovers some hp, shield and mp\nForce User: creates a force shield around the player that greatly reduces damage and protects against force powers\nBounty Hunter: allows firing poison darts with melee by spending metal bolts ammo\nArmored Soldier: spends power cell ammo to increase auto-shield-heal rate\nMonk: increases auto-healing rate and disables enemy Grip\nStealth Attacker: spends power cell ammo to increase disruptor firerate\nDuelist: recovers some MP and disables jetpack, cloak, speed and force regen of enemies nearby\nForce Gunner: disarms enemies nearby\nMagic Master: increases magic bolts, Lightning Dome, Magic Explosion and Healing Area damage. Increases Magic Sense duration. Healing Area heals more\nForce Guardian: increases resistance to damage for some seconds",
		G_NewString(va("used as ammo for Blaster Pistol, Bryar Pistol and E11 Blaster Rifle. You can carry up to %d ammo",zyk_max_blaster_pack_ammo.integer)),
		G_NewString(va("used as ammo for Disruptor, Bowcaster and DEMP2. You can carry up to %d ammo",zyk_max_power_cell_ammo.integer)),
		G_NewString(va("used as ammo for Repeater, Flechette and Concussion Rifle. You can carry up to %d ammo",zyk_max_metal_bolt_ammo.integer)),
		G_NewString(va("used as ammo for Rocket Launcher. You can carry up to %d ammo",zyk_max_rocket_ammo.integer)),
		G_NewString(va("the famous detonator used by Leia in Ep 6 at the Jabba Palace. Normal fire throws it, which explodes after some seconds. Alt fire throws it and it explodes as soon as it touches something. Thermals cause %d damage", zyk_thermal_damage.integer)),
		G_NewString(va("a mine that can be planted somewhere. Normal fire plants a mine with a laser that when touched makes the mine explode, causing %d damage. Alt fire plants proximity mines", zyk_tripmine_damage.integer)),
		G_NewString(va("a very powerful explosive, which you can detonate remotely with the alt fire button. Causes %d damage", zyk_detpack_damage.integer)),
		"this item allows you to see distant things better with its zoom",
		"allows you to recover 25 HP",
		"after placed on the ground, shoots at any nearby enemy",
		"a flying ball that flies around you, shooting anyone in its range",
		"allows you to shoot at people with it, it has a good fire rate",
		"allows you to recover 50 HP",
		"a powerful shield that protects you from enemy attacks, it can resist a lot against any weapon",
		"makes you almost invisible to players and invisible to npcs. Can cloak your vehicle by pressing the Lightsaber Style key (default L) if you have the Holdable Items Upgrade",
		"increases the max force power you have. Necessary to allow you to use force powers and force-based skills",
		"\nFree Warrior ^7gets more damage and more resistance to damage\n^3Force User ^7gets more saber damage and force regens faster\n^3Bounty Hunter ^7gets more gun damage, max ammo, credits in battle, jetpack fuel, sentry gun health, and E-Web health\n^3Armored Soldier ^7gets more resistance to damage\n^3Monk ^7gets more run speed, melee damage and melee attack speed\n^3Stealth Attacker ^7gets more gun damage and more resistance to electric attacks\n^3Duelist ^7gets more saber and melee damage and faster force regen\n^3Force Gunner ^7gets more damage and more resistance to damage\n^3Magic Master ^7gets more Magic Points, new magic bolt types, new magic powers, recovers some jetpack fuel with Magic Points if it runs out and has less magic power cooldown\n^3Force Guardian ^7gets more resistance to damage"
	};

	if (skill_index >= 0 && skill_index < NUMBER_OF_SKILLS)
	{
		return G_NewString(skill_description[skill_index]);
	}
	else
	{
		return "";
	}
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1400];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, scoreFlags, accuracy, perfect;

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;
	scoreFlags = 0;

	numSorted = level.numConnectedClients;

	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

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

		Com_sprintf (entry, sizeof(entry),
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
		j = strlen(entry);
		if (stringlength + j > 1022)
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	//still want to know the total # of clients
	i = level.numConnectedClients;

	trap->SendServerCommand( ent-g_entities, va("scores %i %i %i%s", i,
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

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.guardian_mode > 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in boss battles\n\"" );
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

// zyk: tests which skills are allowed for each class
qboolean zyk_skill_allowed_for_class(int skill_index, int rpg_class)
{
	int i = 0;

	int classes_allowed_for_skills[NUMBER_OF_SKILLS][11] = { // zyk: each index is a skill, and contains an array of allowed RPG classes
		{0, 1, 4, 6, 7, 9, -1}, // Jump
		{0, 1, 4, 6, 7, 9, -1}, // Push
		{0, 1, 4, 6, 7, 9, -1}, // Pull
		{0, 1, 6, -1}, // Speed
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Sense
		{0, 1, 6, 9, -1}, // Saber Attack
		{0, 1, 6, 9, -1}, // Saber Defense
		{0, 1, 6, 9, -1}, // Saber Throw
		{0, 1, 6, 7, 9, -1}, // Absorb
		{0, 1, 4, 6, 7, -1}, // Heal
		{0, 1, 6, 9, -1}, // Protect
		{0, 1, 4, -1}, // Mind Trick
		{0, 1, 4, 7, -1}, // Team Heal
		{0, 1, 6, 7, -1}, // Lightning
		{0, 1, 4, 6, 9, -1}, // Grip
		{0, 1, 4, 6, 7, -1}, // Drain
		{0, 1, 9, -1}, // Rage
		{0, 1, 4, 7, -1}, // Team Energize
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Stun Baton
		{0, 2, 3, -1}, // Blaster Pistol
		{0, 2, 3, 7, 9, -1}, // E11 Blaster Rifle
		{0, 2, 3, 5, 7, -1}, // Disruptor
		{0, 2, 3, -1}, // Bowcaster
		{0, 2, 3, 5, 7, -1}, // Repeater
		{0, 2, 3, 5, -1}, // DEMP2
		{0, 2, 3, -1}, // Flechette
		{0, 2, 3, 7, -1}, // Rocket Launcher
		{0, 2, 3, 5, -1}, // Concussion Rifle
		{0, 2, 3, -1}, // Bryar Pistol
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Melee
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Max Shield
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Shield Strength
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Health Strength
		{0, 1, 4, 6, 7, -1}, // Drain Shield
		{0, 2, 3, 5, 7, 8, -1}, // Jetpack
		{0, 1, 4, 6, 7, 8, -1}, // Sense Health
		{0, 1, 4, 6, 7, -1}, // Shield Heal
		{0, 1, 4, 7, -1}, // Team Shield Heal
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}, // Unique Skill
		{0, 2, 3, 7, 9, -1}, // Blaster Pack
		{0, 2, 3, 5, 7, -1}, // Powercell
		{0, 2, 3, 5, 7, -1}, // Metal Bolts
		{0, 2, 3, 7, -1}, // Rockets
		{0, 2, 3, 7, -1}, // Thermals
		{0, 2, 3, 5, -1}, // Trip Mines
		{0, 2, 3, 5, -1}, // Detpacks
		{0, 2, 3, 5, 7, -1}, // Binoculars
		{0, 2, 3, 8, -1}, // Bacta Canister
		{0, 2, 7, -1}, // Sentry Gun
		{0, 2, 3, 5, 7, -1}, // Seeker Drone
		{0, 2, 3, -1}, // E-Web
		{0, 2, 7, 9, -1}, // Big Bacta
		{0, 2, -1}, // Force Field
		{0, 2, 5, -1}, // Cloak Item
		{0, 1, 4, 6, 7, 9, -1}, // Force Power
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1} // Improvements
	};

	for (i = 0; i < 10; i++)
	{
		if (classes_allowed_for_skills[skill_index][i] == -1)
		{ // zyk: end of classes array, stops the loop
			break;
		}

		if (classes_allowed_for_skills[skill_index][i] == rpg_class)
		{ // zyk: skill is allowed for this class
			return qtrue;
		}
	}

	return qfalse;
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

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
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

	if (g_entities[client_id].client->sess.amrpgmode == 2 && g_entities[client_id].client->pers.can_play_quest == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot scale players in quests.\n\"" );
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2 && g_entities[client_id].client->pers.guardian_mode > 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot scale players in boss battles.\n\"" );
		return;
	}

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && 
		g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
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

extern void save_account(gentity_t *ent, qboolean save_char_file);
extern void quest_get_new_player(gentity_t *ent);
qboolean zyk_answer(gentity_t *ent, char *arg1)
{
	if (ent->client->sess.amrpgmode == 2)
	{
		if (level.quest_map == 10 && ent->client->pers.can_play_quest == 1 && 
			ent->client->pers.eternity_quest_progress < (NUMBER_OF_ETERNITY_QUEST_OBJECTIVES - 1) && (int) ent->client->ps.origin[0] > -676 && 
			(int) ent->client->ps.origin[0] < -296 && (int) ent->client->ps.origin[1] > 1283 && (int) ent->client->ps.origin[1] < 1663 && 
			(int) ent->client->ps.origin[2] > 60 && (int) ent->client->ps.origin[2] < 120)
		{ // zyk: Eternity Quest
			char *answers[11] = { "key", "clock", "sword", "sun", "fire", "water", "time", "star", "nature", "love", NULL };

			// zyk: removing color chars which could not allow the correct answer
			Q_StripColor(arg1);

			if (Q_stricmp(arg1, answers[ent->client->pers.eternity_quest_progress]) == 0)
			{
				ent->client->pers.eternity_quest_progress++;
				save_account(ent, qtrue);

				quest_get_new_player(ent);

				zyk_text_message(ent, va("eternity/answered_%d", ent->client->pers.eternity_quest_progress - 1), qtrue, qtrue, ent->client->pers.netname);
				return qtrue;
			}
			else
			{
				return qfalse;
			}
		}
		else if (level.quest_map == 24 && ent->client->pers.can_play_quest == 1 && 
				 ent->client->pers.universe_quest_progress == 5 && ent->client->pers.universe_quest_messages == 101)
		{ // zyk: amulets mission of Universe Quest
		  // zyk: removing color chars which could not allow the correct answer
			Q_StripColor(arg1);

			if (Q_stricmp( arg1, "samir" ) == 0)
			{
				ent->client->pers.universe_quest_messages = 102;
			}
			else
			{
				ent->client->pers.universe_quest_messages = 103;
			}

			return qfalse;
		}
	}

	return qfalse;
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

		if (zyk_answer(ent, text) == qtrue)
		{ // zyk: if it is a riddle answer, do not say it
			return;
		}

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
	int player_it = 0;
	gentity_t *player_ent = NULL;

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

	for (player_it = 0; player_it < level.maxclients; player_it++)
	{
		player_ent = &g_entities[player_it];
		if (player_ent && player_ent->client && player_ent->client->sess.amrpgmode == 2 && player_ent->client->pers.guardian_mode > 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You cannot vote while someone is in a guardian battle\n\"");
			return;
		}
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
	int max_mp = ent->client->pers.level * 3;

	if (ent->client->pers.rpg_class == 8) // zyk: Magic Master has more Magic Power
	{
		max_mp = ent->client->pers.level * 4;
		return (max_mp + (40 * (ent->client->pers.skill_levels[55] + 1)));
	}

	return max_mp;
}

void zyk_show_magic_in_chat(gentity_t *ent, int magic_power)
{
	if (ent->client->pers.player_settings & (1 << 7))
	{ // zyk: do not show magic cast in chat
		return;
	}

	if (magic_power < MAGIC_MAGIC_SENSE)
	{ // zyk: Ultimate Power
		if (magic_power == -1)
		{ // zyk: Ultra Drain
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ultra Drain!\"", ent->client->pers.netname));
		}
		else if (magic_power == -2)
		{ // zyk: Immunity Power
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Immunity Power!\"", ent->client->pers.netname));
		}
		else if (magic_power == -3)
		{ // zyk: uses Chaos Power
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Chaos Power!\"", ent->client->pers.netname));
		}
		else if (magic_power == -4)
		{ // zyk: uses Time Power
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Time Power!\"", ent->client->pers.netname));
		}
	}
	else
	{ // zyk: Magic Power
		if (magic_power == MAGIC_MAGIC_SENSE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Magic Sense!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ULTRA_STRENGTH)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ultra Strength!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_POISON_MUSHROOMS)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Poison Mushrooms!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_WATER_SPLASH)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Water Splash!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ULTRA_FLAME)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ultra Flame!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ROCKFALL)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Rockfall!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_DOME_OF_DAMAGE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Dome of Damage!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_HURRICANE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Hurricane!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_SLOW_MOTION)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Slow Motion!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ULTRA_RESISTANCE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ultra Resistance!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_SLEEPING_FLOWERS)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Sleeping Flowers!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_HEALING_WATER)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Healing Water!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_FLAME_BURST)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Flame Burst!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_EARTHQUAKE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Earthquake!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_MAGIC_SHIELD)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Magic Shield!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_BLOWING_WIND)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Blowing Wind!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ULTRA_SPEED)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ultra Speed!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ICE_STALAGMITE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ice Stalagmite!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ICE_BOULDER)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ice Boulder!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_HEALING_AREA)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Healing Area!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_MAGIC_EXPLOSION)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Magic Explosion!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_LIGHTNING_DOME)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Lightning Dome!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_WATER_ATTACK)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Water Attack!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_SHIFTING_SAND)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Shifting Sand!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_TREE_OF_LIFE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Tree of Life!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_MAGIC_DISABLE)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Magic Disable!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_FAST_AND_SLOW)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Fast and Slow!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_FLAMING_AREA)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Flaming Area!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_REVERSE_WIND)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Reverse Wind!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ENEMY_WEAKENING)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Enemy Weakening!\"", ent->client->pers.netname));
		}
		else if (magic_power == MAGIC_ICE_BLOCK)
		{
			trap->SendServerCommand(ent->s.number, va("chat \"%s^7: ^7Ice Block!\"", ent->client->pers.netname));
		}
	}
}

void zyk_set_magic_power_cooldown_time(gentity_t *ent, int duration)
{
	// zyk: Magic Master has less cooldown time after casting magic powers
	if (ent->client->pers.rpg_class == 8)
		ent->client->pers.quest_power_usage_timer = level.time + (duration * (1.0 - (ent->client->pers.skill_levels[55] * 0.15)));
	else
		ent->client->pers.quest_power_usage_timer = level.time + duration;
}

extern void poison_mushrooms(gentity_t *ent, int min_distance, int max_distance);
extern void magic_sense(gentity_t *ent, int duration);
extern void healing_water(gentity_t *ent, int heal_amount);
extern void earthquake(gentity_t *ent, int stun_time, int strength, int distance);
extern void blowing_wind(gentity_t *ent, int distance, int duration);
extern void sleeping_flowers(gentity_t *ent, int stun_time, int distance);
extern void time_power(gentity_t *ent, int distance, int duration);
extern void chaos_power(gentity_t *ent, int distance, int duration);
extern void water_splash(gentity_t *ent, int distance, int damage);
extern void ultra_flame(gentity_t *ent, int distance, int damage);
extern void rock_fall(gentity_t *ent, int distance, int damage);
extern void dome_of_damage(gentity_t *ent, int distance, int damage);
extern void ice_stalagmite(gentity_t *ent, int distance, int damage);
extern void ice_boulder(gentity_t *ent, int distance, int damage);
extern void hurricane(gentity_t *ent, int distance, int duration);
extern void slow_motion(gentity_t *ent, int distance, int duration);
extern void ultra_speed(gentity_t *ent, int duration);
extern void ultra_strength(gentity_t *ent, int duration);
extern void ultra_resistance(gentity_t *ent, int duration);
extern void immunity_power(gentity_t *ent, int duration);
extern void ultra_drain(gentity_t *ent, int radius, int damage, int duration);
extern void magic_shield(gentity_t *ent, int duration);
extern void healing_area(gentity_t *ent, int damage, int duration);
extern void lightning_dome(gentity_t *ent, int damage);
extern void magic_explosion(gentity_t *ent, int radius, int damage, int duration);
extern void flame_burst(gentity_t *ent, int duration);
extern void water_attack(gentity_t *ent, int distance, int damage);
extern void shifting_sand(gentity_t *ent, int distance);
extern void tree_of_life(gentity_t *ent);
extern void magic_disable(gentity_t *ent, int distance);
extern void fast_and_slow(gentity_t *ent, int distance, int duration);
extern void flaming_area(gentity_t *ent, int damage);
extern void reverse_wind(gentity_t *ent, int distance, int duration);
extern void enemy_nerf(gentity_t *ent, int distance);
extern void ice_block(gentity_t *ent, int duration);
extern qboolean magic_master_has_this_power(gentity_t *ent, int selected_power);
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
		int use_this_power = -1; // zyk: if >= 0, sets the power this RPG player can use now

		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		ent->client->dangerTime = level.time;
		
		if (ent->client->sess.amrpgmode == 2)
		{ // zyk: if this is a RPG player, tests if he can use a magic power
			float universe_mp_cost_factor = 1.0; // zyk: if player has universe power, increases mp cost by this factor

			if (ent->client->pers.quest_power_status & (1 << 13) && zyk_universe_mp_cost_factor.value > 1.0)
			{ // zyk: Universe Power is active and this cvar is higher than 1.0
				universe_mp_cost_factor = zyk_universe_mp_cost_factor.value;
			}

			if (ent->client->pers.quest_power_usage_timer < level.time)
			{
				if (ent->client->pers.rpg_class == 8)
				{ // zyk: Magic Master has his own way of choosing a power
					if (ent->client->pers.cmd.forwardmove > 0)
					{ // zyk: Special Power Up direction
						use_this_power = ent->client->sess.selected_special_power;
					}
					else if (ent->client->pers.cmd.rightmove > 0)
					{ // zyk: Special Power Right direction
						use_this_power = ent->client->sess.selected_right_special_power;
					}
					else if (ent->client->pers.cmd.rightmove < 0)
					{ // zyk: Special Power Left direction
						use_this_power = ent->client->sess.selected_left_special_power;
					}

					if (magic_master_has_this_power(ent, use_this_power) == qfalse)
					{ // zyk: if the magic power is not enabled or player does not have it, do not use it
						use_this_power = -1;
					}
				}
				else
				{
					// zyk: each class can use a different set of powers
					if (ent->client->pers.cmd.rightmove > 0)
					{ // zyk: Magic Power Right direction
						// zyk: can use the power if he beat a specific light quest boss
						if (ent->client->pers.rpg_class == 0 && (ent->client->pers.defeated_guardians & (1 << 11) || 
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ultra Resistance
							use_this_power = MAGIC_ULTRA_RESISTANCE;
						}
						else if (ent->client->pers.rpg_class == 1 && (ent->client->pers.defeated_guardians & (1 << 6) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Sleeping Flowers
							use_this_power = MAGIC_SLEEPING_FLOWERS;
						}
						else if (ent->client->pers.rpg_class == 5 && (ent->client->pers.defeated_guardians & (1 << 4) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Healing Water
							use_this_power = MAGIC_HEALING_WATER;
						}
						else if (ent->client->pers.rpg_class == 4 && (ent->client->pers.defeated_guardians & (1 << 9) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Flame Burst
							use_this_power = MAGIC_FLAME_BURST;
						}
						else if (ent->client->pers.rpg_class == 3 && (ent->client->pers.defeated_guardians & (1 << 5) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Earthquake
							use_this_power = MAGIC_EARTHQUAKE;
						}
						else if (ent->client->pers.rpg_class == 6 && (ent->client->pers.defeated_guardians & (1 << 7) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Magic Shield
							use_this_power = MAGIC_MAGIC_SHIELD;
						}
						else if (ent->client->pers.rpg_class == 2 && (ent->client->pers.defeated_guardians & (1 << 10) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Blowing Wind
							use_this_power = MAGIC_BLOWING_WIND;
						}
						else if (ent->client->pers.rpg_class == 7 && (ent->client->pers.defeated_guardians & (1 << 8) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ultra Speed
							use_this_power = MAGIC_ULTRA_SPEED;
						}
						else if (ent->client->pers.rpg_class == 9 && (ent->client->pers.defeated_guardians & (1 << 12) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ice Boulder
							use_this_power = MAGIC_ICE_BOULDER;
						}
					}
					else if (ent->client->pers.cmd.rightmove < 0)
					{ // zyk: Magic Power Left direction
						// zyk: can use the power if he beat a specific light quest boss
						if (ent->client->pers.rpg_class == 0 && (ent->client->pers.defeated_guardians & (1 << 11) || 
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ultra Strength
							use_this_power = MAGIC_ULTRA_STRENGTH;
						}
						else if (ent->client->pers.rpg_class == 1 && (ent->client->pers.defeated_guardians & (1 << 6) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Poison Mushrooms
							use_this_power = MAGIC_POISON_MUSHROOMS;
						}
						else if (ent->client->pers.rpg_class == 5 && (ent->client->pers.defeated_guardians & (1 << 4) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Water Splash
							use_this_power = MAGIC_WATER_SPLASH;
						}
						else if (ent->client->pers.rpg_class == 4 && (ent->client->pers.defeated_guardians & (1 << 9) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ultra Flame
							use_this_power = MAGIC_ULTRA_FLAME;
						}
						else if (ent->client->pers.rpg_class == 3 && (ent->client->pers.defeated_guardians & (1 << 5) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Rockfall
							use_this_power = MAGIC_ROCKFALL;
						}
						else if (ent->client->pers.rpg_class == 6 && (ent->client->pers.defeated_guardians & (1 << 7) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Dome of Damage
							use_this_power = MAGIC_DOME_OF_DAMAGE;
						}
						else if (ent->client->pers.rpg_class == 2 && (ent->client->pers.defeated_guardians & (1 << 10) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Hurricane
							use_this_power = MAGIC_HURRICANE;
						}
						else if (ent->client->pers.rpg_class == 7 && (ent->client->pers.defeated_guardians & (1 << 8) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Slow Motion
							use_this_power = MAGIC_SLOW_MOTION;
						}
						else if (ent->client->pers.rpg_class == 9 && (ent->client->pers.defeated_guardians & (1 << 12) || 
								 ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ice Stalagmite
							use_this_power = MAGIC_ICE_STALAGMITE;
						}
					}
					else if (ent->client->pers.cmd.forwardmove > 0)
					{ // zyk: Magic Power Front direction
						// zyk: can use the power if he beat a specific light quest boss
						if (ent->client->pers.rpg_class == 0 && (ent->client->pers.defeated_guardians & (1 << 11) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Enemy Weakening
							use_this_power = MAGIC_ENEMY_WEAKENING;
						}
						else if (ent->client->pers.rpg_class == 1 && (ent->client->pers.defeated_guardians & (1 << 6) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Tree of Life
							use_this_power = MAGIC_TREE_OF_LIFE;
						}
						else if (ent->client->pers.rpg_class == 2 && (ent->client->pers.defeated_guardians & (1 << 10) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Reverse Wind
							use_this_power = MAGIC_REVERSE_WIND;
						}
						else if (ent->client->pers.rpg_class == 3 && (ent->client->pers.defeated_guardians & (1 << 5) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Shifting Sand
							use_this_power = MAGIC_SHIFTING_SAND;
						}
						else if (ent->client->pers.rpg_class == 4 && (ent->client->pers.defeated_guardians & (1 << 9) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Flaming Area
							use_this_power = MAGIC_FLAMING_AREA;
						}
						else if (ent->client->pers.rpg_class == 5 && (ent->client->pers.defeated_guardians & (1 << 4) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Water Attack
							use_this_power = MAGIC_WATER_ATTACK;
						}
						else if (ent->client->pers.rpg_class == 6 && (ent->client->pers.defeated_guardians & (1 << 7) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Magic Disable
							use_this_power = MAGIC_MAGIC_DISABLE;
						}
						else if (ent->client->pers.rpg_class == 7 && (ent->client->pers.defeated_guardians & (1 << 8) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Fast and Slow
							use_this_power = MAGIC_FAST_AND_SLOW;
						}
						else if (ent->client->pers.rpg_class == 9 && (ent->client->pers.defeated_guardians & (1 << 12) ||
							ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
						{ // zyk: Ice Block
							use_this_power = MAGIC_ICE_BLOCK;
						}
					}
				}

				if (ent->client->sess.magic_disabled_powers & (1 << use_this_power))
				{ // zyk: if the magic power is not enabled or player does not have it, do not use it
					use_this_power = -1;
				}

				if (ent->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES && ent->client->pers.universe_quest_counter & (1 << 3) && 
					!(ent->client->sess.magic_more_disabled_powers & (1 << 1)))
				{ // zyk: Magic Improvement. Decreases mp cost of Universe Power
					universe_mp_cost_factor = 1.0;
				}
				
				if (ent->client->pers.cmd.forwardmove < 0 && ent->client->pers.universe_quest_progress >= 14 && !(ent->client->sess.magic_more_disabled_powers & (1 << 0)))
				{ // zyk: Ultimate Power
					if (zyk_enable_ultra_drain.integer == 1 && ent->client->pers.universe_quest_counter & (1 << 0) && ent->client->pers.magic_power >= zyk_ultra_drain_mp_cost.integer)
					{ // zyk: Ultra Drain
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;
						ultra_drain(ent, 450, 30, 8000);
						ent->client->pers.magic_power -= zyk_ultra_drain_mp_cost.integer;
						
						zyk_set_magic_power_cooldown_time(ent, 28000);

						zyk_show_magic_in_chat(ent, -1);
					}
					else if (zyk_enable_immunity_power.integer == 1 && ent->client->pers.universe_quest_counter & (1 << 1) && ent->client->pers.magic_power >= zyk_immunity_power_mp_cost.integer)
					{ // zyk: Immunity Power
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;
						immunity_power(ent,25000);
						ent->client->pers.magic_power -= zyk_immunity_power_mp_cost.integer;
						
						zyk_set_magic_power_cooldown_time(ent, 28000);

						ent->client->pers.player_statuses |= (1 << 15);

						zyk_show_magic_in_chat(ent, -2);
					}
					else if (zyk_enable_chaos_power.integer == 1 && ent->client->pers.universe_quest_counter & (1 << 2) && ent->client->pers.magic_power >= zyk_chaos_power_mp_cost.integer)
					{ // zyk: uses Chaos Power
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;
						chaos_power(ent, 400, 4600);
						ent->client->pers.magic_power -= zyk_chaos_power_mp_cost.integer;
						
						zyk_set_magic_power_cooldown_time(ent, 28000);

						zyk_show_magic_in_chat(ent, -3);
					}
					else if (zyk_enable_time_power.integer == 1 && ent->client->pers.universe_quest_counter & (1 << 3) && ent->client->pers.magic_power >= zyk_time_power_mp_cost.integer)
					{ // zyk: uses Time Power
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 1000;
						time_power(ent, 400, 4000);
						ent->client->pers.magic_power -= zyk_time_power_mp_cost.integer;
						
						zyk_set_magic_power_cooldown_time(ent, 28000);

						zyk_show_magic_in_chat(ent, -4);
					}

					if (ent->client->pers.universe_quest_progress < 15)
					{ // zyk: before beating Guardian of Chaos, Ultimate Power has longer cooldown
						if (ent->client->pers.rpg_class == 8)
							ent->client->pers.quest_power_usage_timer += 1000;
						else
							ent->client->pers.quest_power_usage_timer += 4000;
					}
				}
				else if (use_this_power >= MAGIC_MAGIC_SENSE)
				{ // zyk: Magic Power
					if (use_this_power == MAGIC_MAGIC_SENSE && zyk_enable_magic_sense.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_magic_sense_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						magic_sense(ent,3000);
						ent->client->pers.magic_power -= (int)ceil((zyk_magic_sense_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 14000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ULTRA_STRENGTH && zyk_enable_ultra_strength.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ultra_strength_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ultra_strength(ent,30000);
						ent->client->pers.magic_power -= (int)ceil((zyk_ultra_strength_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 8000);

						ent->client->pers.player_statuses |= (1 << 16);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_POISON_MUSHROOMS && zyk_enable_poison_mushrooms.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_poison_mushrooms_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						poison_mushrooms(ent,100,600);
						ent->client->pers.magic_power -= (int)ceil((zyk_poison_mushrooms_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 10000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_WATER_SPLASH && zyk_enable_water_splash.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_water_splash_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						water_splash(ent,400,15);
						ent->client->pers.magic_power -= (int)ceil((zyk_water_splash_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 11000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ULTRA_FLAME && zyk_enable_ultra_flame.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ultra_flame_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ultra_flame(ent, 600, 35);
						ent->client->pers.magic_power -= (int)ceil((zyk_ultra_flame_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 10000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ROCKFALL && zyk_enable_rockfall.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_rockfall_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						rock_fall(ent, 500, 40);
						ent->client->pers.magic_power -= (int)ceil((zyk_rockfall_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 11000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_DOME_OF_DAMAGE && zyk_enable_dome_of_damage.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_dome_of_damage_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						dome_of_damage(ent, 500, 25);
						ent->client->pers.magic_power -= (int)ceil((zyk_dome_of_damage_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 16000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_HURRICANE && zyk_enable_hurricane.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_hurricane_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						hurricane(ent,400,5000);
						ent->client->pers.magic_power -= (int)ceil((zyk_hurricane_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 14000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_SLOW_MOTION && zyk_enable_slow_motion.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_slow_motion_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						slow_motion(ent,400,15000);
						ent->client->pers.magic_power -= (int)ceil((zyk_slow_motion_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 9000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ULTRA_RESISTANCE && zyk_enable_ultra_resistance.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ultra_resistance_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ultra_resistance(ent,30000);
						ent->client->pers.magic_power -= (int)ceil((zyk_ultra_resistance_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 8000);

						ent->client->pers.player_statuses |= (1 << 17);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_SLEEPING_FLOWERS && zyk_enable_sleeping_flowers.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_sleeping_flowers_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						sleeping_flowers(ent,2500,350);
						ent->client->pers.magic_power -= (int)ceil((zyk_sleeping_flowers_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 15000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_HEALING_WATER && zyk_enable_healing_water.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_healing_water_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						healing_water(ent,120);
						ent->client->pers.magic_power -= (int)ceil((zyk_healing_water_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 12000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_FLAME_BURST && zyk_enable_flame_burst.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_flame_burst_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						flame_burst(ent, 5000);
						ent->client->pers.magic_power -= (int)ceil((zyk_flame_burst_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 10000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_EARTHQUAKE && zyk_enable_earthquake.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_earthquake_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						earthquake(ent,2000,300,500);
						ent->client->pers.magic_power -= (int)ceil((zyk_earthquake_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 11000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_MAGIC_SHIELD && zyk_enable_magic_shield.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_magic_shield_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						magic_shield(ent, 6000);
						ent->client->pers.magic_power -= (int)ceil((zyk_magic_shield_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 24000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_BLOWING_WIND && zyk_enable_blowing_wind.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_blowing_wind_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						blowing_wind(ent,700,5000);
						ent->client->pers.magic_power -= (int)ceil((zyk_blowing_wind_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 12000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ULTRA_SPEED && zyk_enable_ultra_speed.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ultra_speed_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ultra_speed(ent,15000);
						ent->client->pers.magic_power -= (int)ceil((zyk_ultra_speed_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 9000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ICE_STALAGMITE && zyk_enable_ice_stalagmite.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ice_stalagmite_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ice_stalagmite(ent, 500, 130);
						ent->client->pers.magic_power -= (int)ceil((zyk_ice_stalagmite_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 20000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ICE_BOULDER && zyk_enable_ice_boulder.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ice_boulder_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ice_boulder(ent, 380, 40);
						ent->client->pers.magic_power -= (int)ceil((zyk_ice_boulder_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 20000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_HEALING_AREA && zyk_enable_healing_area.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_healing_area_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						healing_area(ent,2,5000);
						ent->client->pers.magic_power -= (int)ceil((zyk_healing_area_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 20000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_MAGIC_EXPLOSION && zyk_enable_magic_explosion.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_magic_explosion_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						magic_explosion(ent,320,130,900);
						ent->client->pers.magic_power -= (int)ceil((zyk_magic_explosion_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 24000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_LIGHTNING_DOME && zyk_enable_lightning_dome.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_lightning_dome_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						lightning_dome(ent, 70);
						ent->client->pers.magic_power -= (int)ceil((zyk_lightning_dome_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 25000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_WATER_ATTACK && zyk_enable_water_attack.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_water_attack_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						water_attack(ent, 500, 40);
						ent->client->pers.magic_power -= (int)ceil((zyk_water_attack_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 12000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_SHIFTING_SAND && zyk_enable_shifting_sand.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_shifting_sand_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						shifting_sand(ent, 1000);
						ent->client->pers.magic_power -= (int)ceil((zyk_shifting_sand_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 20000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_TREE_OF_LIFE && zyk_enable_tree_of_life.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_tree_of_life_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						tree_of_life(ent);
						ent->client->pers.magic_power -= (int)ceil((zyk_tree_of_life_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 20000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_MAGIC_DISABLE && zyk_enable_magic_disable.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_magic_disable_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						magic_disable(ent, 450);
						ent->client->pers.magic_power -= (int)ceil((zyk_magic_disable_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 6000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_FAST_AND_SLOW && zyk_enable_fast_and_slow.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_fast_and_slow_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						fast_and_slow(ent, 400, 6000);
						ent->client->pers.magic_power -= (int)ceil((zyk_fast_and_slow_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 12000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_FLAMING_AREA && zyk_enable_flaming_area.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_flaming_area_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						flaming_area(ent, 20);
						ent->client->pers.magic_power -= (int)ceil((zyk_flaming_area_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 18000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_REVERSE_WIND && zyk_enable_reverse_wind.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_reverse_wind_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						reverse_wind(ent, 700, 5000);
						ent->client->pers.magic_power -= (int)ceil((zyk_reverse_wind_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 12000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ENEMY_WEAKENING && zyk_enable_enemy_nerf.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_enemy_nerf_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						enemy_nerf(ent, 450);
						ent->client->pers.magic_power -= (int)ceil((zyk_enemy_nerf_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 12000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
					else if (use_this_power == MAGIC_ICE_BLOCK && zyk_enable_ice_block.integer == 1 && ent->client->pers.magic_power >= (int)ceil((zyk_ice_block_mp_cost.integer * universe_mp_cost_factor)))
					{
						ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 1000;
						ice_block(ent, 3500);
						ent->client->pers.magic_power -= (int)ceil((zyk_ice_block_mp_cost.integer * universe_mp_cost_factor));

						zyk_set_magic_power_cooldown_time(ent, 20000);

						zyk_show_magic_in_chat(ent, use_this_power);
					}
				}

				if (ent->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES && ent->client->pers.universe_quest_counter & (1 << 1) && 
					!(ent->client->sess.magic_more_disabled_powers & (1 << 1)))
				{ // zyk: Magic Boost, reward for completing quests in Guardians Sequel. Decreases cooldown time of magic powers
					if (ent->client->pers.rpg_class == 8)
						ent->client->pers.quest_power_usage_timer -= 500;
					else
						ent->client->pers.quest_power_usage_timer -= 3000;
				}

				display_yellow_bar(ent,(ent->client->pers.quest_power_usage_timer - level.time));
			}
			else
			{
				trap->SendServerCommand( ent->s.number, va("chat \"^3Magic Power: ^7%d seconds left!\"", ((ent->client->pers.quest_power_usage_timer - level.time)/1000)));
			}

			send_rpg_events(2000);
		}

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
	ent->client->pers.max_rpg_health = 100 + (ent->client->pers.level * 2);
	ent->client->ps.stats[STAT_MAX_HEALTH] = ent->client->pers.max_rpg_health;
}

// zyk: sets the Max Shield a player can have in RPG Mode
void set_max_shield(gentity_t *ent)
{
	ent->client->pers.max_rpg_shield = (int)ceil(((ent->client->pers.skill_levels[30] * 1.0)/5) * ent->client->pers.max_rpg_health);
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

// zyk: gives or removes jetpack from player
void zyk_jetpack(gentity_t *ent)
{
	// zyk: player starts with jetpack if it is enabled in player settings, is not in Siege Mode, and does not have all force powers through /give command
	if (!(ent->client->pers.player_settings & (1 << 12)) && zyk_allow_jetpack_command.integer && 
		(level.gametype != GT_SIEGE || zyk_allow_jetpack_in_siege.integer) && level.gametype != GT_JEDIMASTER && 
		!(ent->client->pers.player_statuses & (1 << 12)) &&
		((ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[34] > 0) || ent->client->sess.amrpgmode == 1))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
	}
	else
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK);
		if (ent->client->jetPackOn)
		{
			Jetpack_Off(ent);
		}
	}
}

// zyk: loads settings valid both to Admin-Only Mode and to RPG Mode
void zyk_load_common_settings(gentity_t *ent)
{
	// zyk: loading the starting weapon based in player settings
	if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER) && !(ent->client->pers.player_settings & (1 << 11)))
	{
		ent->client->ps.weapon = WP_SABER;
	}
	else
	{
		ent->client->ps.weapon = WP_MELEE;
	}
		
	zyk_jetpack(ent);

	if (!(ent->client->saber[0].model[0] && ent->client->saber[1].model[0]) && !(ent->client->saber[0].saberFlags&SFL_TWO_HANDED))
	{ // zyk: Single Saber
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];

		if (ent->client->pers.player_settings & (1 << 26) && 
			((ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 2) || 
			 (ent->client->sess.amrpgmode == 1 && ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))))
		{
			// ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = SS_MEDIUM;
			// ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			ent->client->ps.fd.saberAnimLevel = SS_MEDIUM;
		}
		else if (ent->client->pers.player_settings & (1 << 27) && 
				((ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 3) || 
				 (ent->client->sess.amrpgmode == 1 && ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))))
		{
			ent->client->ps.fd.saberAnimLevel = SS_STRONG;
		}
		else if (ent->client->pers.player_settings & (1 << 28) && ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 4)
		{
			ent->client->ps.fd.saberAnimLevel = SS_DESANN;
		}
		else if (ent->client->pers.player_settings & (1 << 29) && ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] == 5)
		{
			ent->client->ps.fd.saberAnimLevel = SS_TAVION;
		}
		else if (((ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 1) || 
				  (ent->client->sess.amrpgmode == 1 && ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))))
		{
			ent->client->ps.fd.saberAnimLevel = SS_FAST;
		}
	}
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

// zyk: loads the player account
void load_account(gentity_t *ent)
{
	FILE *account_file;
	char content[128];

	strcpy(content,"");
	account_file = fopen(va("zykmod/accounts/%s.txt",ent->client->sess.filename),"r");
	if (account_file != NULL)
	{
		int i = 0;
		// zyk: this variable will validate the skillpoints this player has
		// if he has more than the max skillpoints defined, then server must remove the exceeding ones
		int validate_skillpoints = 0;
		int max_skillpoints = 0;
		int j = 0;

		// zyk: loading the account password
		fscanf(account_file,"%s",content);
		strcpy(ent->client->pers.password,content);

		// zyk: loading the amrpgmode value
		fscanf(account_file,"%s",content);
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
		fscanf(account_file,"%s",content);
		ent->client->pers.player_settings = atoi(content);

		// zyk: loading the admin command bit value
		fscanf(account_file,"%s",content);
		ent->client->pers.bitvalue = atoi(content);

		// zyk: loading the current char
		fscanf(account_file, "%s", content);
		strcpy(ent->client->sess.rpgchar, content);

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
				if ((j % 10) == 0)
				{ // zyk: level divisible by 10 has more skillpoints
					max_skillpoints += (1 + j / 10);
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

			// zyk: Other RPG attributes
			// zyk: loading Light Quest Defeated Guardians number value
			fscanf(account_file, "%s", content);
			ent->client->pers.defeated_guardians = atoi(content);

			// zyk: compability with old mod versions, in which the players who completed the quest had a value of 9
			if (ent->client->pers.defeated_guardians == 9)
				ent->client->pers.defeated_guardians = NUMBER_OF_GUARDIANS;

			// zyk: loading Dark Quest completed objectives value
			fscanf(account_file, "%s", content);
			ent->client->pers.hunter_quest_progress = atoi(content);

			// zyk: loading Eternity Quest progress value
			fscanf(account_file, "%s", content);
			ent->client->pers.eternity_quest_progress = atoi(content);

			// zyk: loading secrets found value
			fscanf(account_file, "%s", content);
			ent->client->pers.secrets_found = atoi(content);

			// zyk: loading Universe Quest Progress value
			fscanf(account_file, "%s", content);
			ent->client->pers.universe_quest_progress = atoi(content);

			// zyk: loading Universe Quest Counter value
			fscanf(account_file, "%s", content);
			ent->client->pers.universe_quest_counter = atoi(content);

			// zyk: make sure Challenge Mode settings flag and counter flag are correct
			if (ent->client->pers.universe_quest_counter & (1 << 29))
			{
				ent->client->pers.player_settings |= (1 << 15);
			}
			else
			{
				ent->client->pers.player_settings &= ~(1 << 15);
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

			// zyk: loading RPG class
			fscanf(account_file, "%s", content);
			ent->client->pers.rpg_class = atoi(content);

			// zyk: loading disabled magic powers
			fscanf(account_file, "%s", content);
			ent->client->sess.magic_disabled_powers = atoi(content);

			// zyk: loading more disabled magic powers
			fscanf(account_file, "%s", content);
			ent->client->sess.magic_more_disabled_powers = atoi(content);

			// zyk: loading Magic Master first selection and selected powers
			fscanf(account_file, "%s", content);
			ent->client->sess.magic_fist_selection = atoi(content);

			fscanf(account_file, "%s", content);
			ent->client->sess.selected_special_power = atoi(content);

			fscanf(account_file, "%s", content);
			ent->client->sess.selected_left_special_power = atoi(content);

			fscanf(account_file, "%s", content);
			ent->client->sess.selected_right_special_power = atoi(content);

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
		{ // zyk: char file caould not be loaded
			trap->SendServerCommand(ent - g_entities, "print \"Char file could not be loaded!\n\"");
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"There is no account with this login or password.\n\"" ); 
	}
}

// zyk: saves info into the player account file. If save_char_file is qtrue, this function must save the char file
void save_account(gentity_t *ent, qboolean save_char_file)
{
	// zyk: used to prevent account save in map change time or before loading account after changing map
	if (level.voteExecuteTime < level.time && ent->client->pers.connected == CON_CONNECTED && 
		(ent->client->sess.amrpgmode == 1 || (ent->client->sess.amrpgmode == 2 && (zyk_rp_mode.integer != 1 || zyk_allow_saving_in_rp_mode.integer == 1)))
		)
	{ // zyk: players can only save things if server is not at RP Mode or if it is allowed in config
		if (save_char_file == qtrue)
		{  // zyk: save the RPG char
			FILE *account_file;
			gclient_t *client;
			char content[MAX_STRING_CHARS];
			int i = 0;

			client = ent->client;
			strcpy(content, "");

			for (i = 0; i < NUMBER_OF_SKILLS; i++)
			{
				strcpy(content, va("%s%d\n", content, client->pers.skill_levels[i]));
			}

			account_file = fopen(va("zykmod/accounts/%s_%s.txt",ent->client->sess.filename, ent->client->sess.rpgchar),"w");

			fprintf(account_file,"%d\n%d\n%d\n%s%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n",
			client->pers.level_up_score, client->pers.level, client->pers.skillpoints, content, client->pers.defeated_guardians, client->pers.hunter_quest_progress
			, client->pers.eternity_quest_progress, client->pers.secrets_found, client->pers.universe_quest_progress, client->pers.universe_quest_counter, client->pers.credits
			, client->pers.rpg_class, client->sess.magic_disabled_powers, client->sess.magic_more_disabled_powers, client->sess.magic_fist_selection, client->sess.selected_special_power
			, client->sess.selected_left_special_power, client->sess.selected_right_special_power);

			fclose(account_file);
		}
		else
		{ // zyk: save the main account file
			FILE *account_file;
			gclient_t *client;

			client = ent->client;
			account_file = fopen(va("zykmod/accounts/%s.txt", ent->client->sess.filename), "w");
			fprintf(account_file, "%s\n%d\n%d\n%d\n%s\n",
				client->pers.password, client->sess.amrpgmode, client->pers.player_settings, client->pers.bitvalue, client->sess.rpgchar);
			fclose(account_file);
		}
	}
}

qboolean validate_rpg_class(gentity_t *ent)
{
	if (ent->client->pers.rpg_class == 0 && zyk_allow_free_warrior.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Free Warrior not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 1 && zyk_allow_force_user.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Force User not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 2 && zyk_allow_bounty_hunter.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Bounty Hunter not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 3 && zyk_allow_armored_soldier.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Armored Soldier not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 4 && zyk_allow_monk.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Monk not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 5 && zyk_allow_stealth_attacker.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Stealth Attacker not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 6 && zyk_allow_duelist.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Duelist not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 7 && zyk_allow_force_gunner.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Force Gunner not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 8 && zyk_allow_magic_master.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Magic Master not allowed in this server\n\"" );
		return qfalse;
	}
	else if (ent->client->pers.rpg_class == 9 && zyk_allow_force_tank.integer == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Force Guardian not allowed in this server\n\"" );
		return qfalse;
	}

	return qtrue;
}

// zyk: gives rpg score to the player
void rpg_score(gentity_t *ent, qboolean admin_rp_mode)
{
	int send_message = 0; // zyk: if its 1, sends the message in player console
	char message[128];

	strcpy(message,"");

	if (admin_rp_mode == qfalse && zyk_rp_mode.integer == 1)
	{ // zyk: in RP Mode, only admins can give levels to RPG players
		return;
	}

	if (validate_rpg_class(ent) == qfalse)
		return;

	add_credits(ent, (10 + ent->client->pers.credits_modifier));

	if (ent->client->pers.level < zyk_rpg_max_level.integer)
	{
		ent->client->pers.level_up_score += (1 + ent->client->pers.score_modifier); // zyk: add score to the RPG mode score

		if (ent->client->pers.level_up_score >= (ent->client->pers.level * zyk_level_up_score_factor.integer))
		{ // zyk: player got a new level
			ent->client->pers.level_up_score -= (ent->client->pers.level * zyk_level_up_score_factor.integer);
			ent->client->pers.level++;

			if (ent->client->pers.level % 10 == 0) // zyk: every level divisible by 10 the player will get bonus skillpoints
				ent->client->pers.skillpoints+=(ent->client->pers.level/10) + 1;
			else
				ent->client->pers.skillpoints++;

			strcpy(message,va("^3New Level: ^7%d^3, Skillpoints: ^7%d\n", ent->client->pers.level, ent->client->pers.skillpoints));

			// zyk: got a new level, so change the max health and max shield
			set_max_health(ent);
			set_max_shield(ent);

			// zyk: mp also increased, so send event to client-side to display the magic power bar
			send_rpg_events(2000);

			send_message = 1;
		}
	}
	save_account(ent, qtrue); // zyk: saves new score and credits in the account file

	// zyk: cleaning the modifiers after they are applied
	ent->client->pers.credits_modifier = 0;
	ent->client->pers.score_modifier = 0;

	if (send_message == 1)
	{
		if (ent->client->pers.level == zyk_rpg_max_level.integer)
		{ // zyk: if this is the max level, show this message
			trap->SendServerCommand( ent-g_entities, va("chat \"^7Congratulations, %s^7! You reached the max level %d!\n\"", ent->client->pers.netname, zyk_rpg_max_level.integer) );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, va("chat \"%s\"", message) );
		}
	}
}

// zyk: increases the RPG skill counter by this amount
void rpg_skill_counter(gentity_t *ent, int amount)
{
	if (ent && ent->client && ent->client->sess.amrpgmode == 2 && ent->client->pers.level < zyk_rpg_max_level.integer)
	{ // zyk: now RPG mode increases level up score after a certain amount of attacks
		ent->client->pers.skill_counter += amount;
		if (ent->client->pers.skill_counter >= zyk_max_skill_counter.integer)
		{
			ent->client->pers.skill_counter = 0;

			// zyk: skill counter does not give credits, only Level Up Score
			ent->client->pers.credits_modifier = -10;

			rpg_score(ent, qfalse);
		}
	}
}

// zyk: number of artifacts collected by the player in Universe Quest
int number_of_artifacts(gentity_t *ent)
{
	int i = 0, collected_artifacts = 0;

	if (ent->client->pers.universe_quest_progress == 2)
	{
		for (i = 0; i < 10; i++)
		{
			if (ent->client->pers.universe_quest_counter & (1 << i))
			{
				collected_artifacts++;
			}
		}
	}

	return collected_artifacts;
}

// zyk: amount of amulets got by the player in Amulets Mission of Universe Quest
int number_of_amulets(gentity_t *ent)
{
	int i = 0, number_of_amulets = 0;

	for (i = 0; i < 3; i++)
	{
		if (ent->client->pers.universe_quest_counter & (1 << i))
		{
			number_of_amulets++;
		}
	}

	return number_of_amulets;
}

// zyk: amount of crystals got by the player in Crystals mission of Universe Quest
int number_of_crystals(gentity_t *ent)
{
	int i = 0, number_of_crystals = 0;

	for (i = 0; i < 3; i++)
	{
		if (ent->client->pers.universe_quest_counter & (1 << i))
		{
			number_of_crystals++;
		}
	}

	return number_of_crystals;
}

// zyk: initialize RPG skills of this player
void initialize_rpg_skills(gentity_t *ent)
{
	if (ent->client->sess.amrpgmode == 2)
	{
		int i = 0;

		if (validate_rpg_class(ent) == qfalse)
			return;

		// zyk: validating max skill levels. If for some reason a skill is above max, get the skillpoint back
		for (i = 0; i < NUMBER_OF_SKILLS; i++)
		{
			if (ent->client->pers.skill_levels[i] > max_skill_levels[i])
			{
				ent->client->pers.skill_levels[i]--;
				ent->client->pers.skillpoints++;
			}
		}

		// zyk: Challenge Mode player who changed to Normal after finishing old Universe Quest with 15 missions. Reset his settings to Challenge
		if (ent->client->pers.universe_quest_progress == 15 && !(ent->client->pers.player_settings & (1 << 15)) && 
			ent->client->pers.universe_quest_counter & (1 << 29))
		{
			ent->client->pers.player_settings |= (1 << 15);
		}

		// zyk: loading Jump value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)) && ent->client->pers.skill_levels[0] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LEVITATION);
		if (ent->client->pers.skill_levels[0] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LEVITATION);
		ent->client->ps.fd.forcePowerLevel[FP_LEVITATION] = ent->client->pers.skill_levels[0];

		// zyk: loading Push value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PUSH)) && ent->client->pers.skill_levels[1] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PUSH);
		if (ent->client->pers.skill_levels[1] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PUSH);
		ent->client->ps.fd.forcePowerLevel[FP_PUSH] = ent->client->pers.skill_levels[1];

		// zyk: loading Pull value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PULL)) && ent->client->pers.skill_levels[2] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PULL);
		if (ent->client->pers.skill_levels[2] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PULL);
		ent->client->ps.fd.forcePowerLevel[FP_PULL] = ent->client->pers.skill_levels[2];

		// zyk: loading Speed value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SPEED)) && ent->client->pers.skill_levels[3] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SPEED);
		if (ent->client->pers.skill_levels[3] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SPEED);
		ent->client->ps.fd.forcePowerLevel[FP_SPEED] = ent->client->pers.skill_levels[3];

		// zyk: loading Sense value
		if (ent->client->pers.rpg_class == 2 || ent->client->pers.rpg_class == 3 || ent->client->pers.rpg_class == 5 || ent->client->pers.rpg_class == 8)
		{ // zyk: these classes have no force, so they do not need Sense (although they can have the skill to resist Mind Control)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
			ent->client->ps.fd.forcePowerLevel[FP_SEE] = FORCE_LEVEL_0;
		}
		else
		{
			if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SEE)) && ent->client->pers.skill_levels[4] > 0)
				ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
			if (ent->client->pers.skill_levels[4] == 0)
				ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
			ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[4];
		}

		// zyk: loading Saber Offense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_OFFENSE)) && ent->client->pers.skill_levels[5] > 0)
		{
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_OFFENSE);
		}
		if (ent->client->pers.skill_levels[5] == 0)
		{
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_OFFENSE);
		}
		ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = ent->client->pers.skill_levels[5];

		// zyk: giving the saber if he has Saber Attack skill level greater than 0
		if (ent->client->pers.skill_levels[5] > 0)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		}
		else
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
		}

		// zyk: loading Saber Defense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_DEFENSE)) && ent->client->pers.skill_levels[6] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_DEFENSE);
		if (ent->client->pers.skill_levels[6] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_DEFENSE);
		ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = ent->client->pers.skill_levels[6];

		// zyk: loading Saber Throw value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABERTHROW)) && ent->client->pers.skill_levels[7] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABERTHROW);
		if (ent->client->pers.skill_levels[7] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABERTHROW);
		ent->client->ps.fd.forcePowerLevel[FP_SABERTHROW] = ent->client->pers.skill_levels[7];

		// zyk: loading Absorb value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_ABSORB)) && ent->client->pers.skill_levels[8] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_ABSORB);
		if (ent->client->pers.skill_levels[8] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_ABSORB);

		if (ent->client->pers.skill_levels[8] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = ent->client->pers.skill_levels[8];
		else
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = FORCE_LEVEL_3;

		// zyk: loading Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_HEAL)) && ent->client->pers.skill_levels[9] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_HEAL);
		if (ent->client->pers.skill_levels[9] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_HEAL] = ent->client->pers.skill_levels[9];

		// zyk: loading Protect value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PROTECT)) && ent->client->pers.skill_levels[10] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PROTECT);
		if (ent->client->pers.skill_levels[10] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PROTECT);

		if (ent->client->pers.skill_levels[10] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = ent->client->pers.skill_levels[10];
		else
			ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = FORCE_LEVEL_3;

		// zyk: loading Mind Trick value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TELEPATHY)) && ent->client->pers.skill_levels[11] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TELEPATHY);
		if (ent->client->pers.skill_levels[11] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TELEPATHY);
		ent->client->ps.fd.forcePowerLevel[FP_TELEPATHY] = ent->client->pers.skill_levels[11];

		// zyk: loading Team Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) && ent->client->pers.skill_levels[12] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_HEAL);
		if (ent->client->pers.skill_levels[12] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] = ent->client->pers.skill_levels[12];

		// zyk: loading Lightning value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LIGHTNING)) && ent->client->pers.skill_levels[13] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		if (ent->client->pers.skill_levels[13] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LIGHTNING);

		if (ent->client->pers.skill_levels[13] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = ent->client->pers.skill_levels[13];
		else
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = FORCE_LEVEL_3;

		// zyk: loading Grip value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_GRIP)) && ent->client->pers.skill_levels[14] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_GRIP);
		if (ent->client->pers.skill_levels[14] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_GRIP);
		ent->client->ps.fd.forcePowerLevel[FP_GRIP] = ent->client->pers.skill_levels[14];

		// zyk: loading Drain value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_DRAIN)) && ent->client->pers.skill_levels[15] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_DRAIN);
		if (ent->client->pers.skill_levels[15] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_DRAIN);
		ent->client->ps.fd.forcePowerLevel[FP_DRAIN] = ent->client->pers.skill_levels[15];

		// zyk: loading Rage value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_RAGE)) && ent->client->pers.skill_levels[16] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_RAGE);
		if (ent->client->pers.skill_levels[16] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_RAGE);

		if (ent->client->pers.skill_levels[16] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_RAGE] = ent->client->pers.skill_levels[16];
		else
			ent->client->ps.fd.forcePowerLevel[FP_RAGE] = FORCE_LEVEL_3;

		// zyk: loading Team Energize value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) && ent->client->pers.skill_levels[17] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_FORCE);
		if (ent->client->pers.skill_levels[17] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_FORCE);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] = ent->client->pers.skill_levels[17];

		// zyk: loading Melee
		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_MELEE)))
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);

		zyk_load_common_settings(ent);

		ent->client->pers.sense_health_timer = 0;

		// zyk: used to add a cooldown between each flame
		ent->client->cloakDebReduce = 0;

		ent->client->pers.max_force_power = (int)ceil((zyk_max_force_power.value/4.0) * ent->client->pers.skill_levels[54]);
		ent->client->ps.fd.forcePowerMax = ent->client->pers.max_force_power;
		ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;

		// zyk: setting rpg control attributes
		ent->client->pers.thermal_vision = qfalse;
		ent->client->pers.thermal_vision_cooldown_time = 0;

		ent->client->pers.quest_power_status = 0;

		ent->client->pers.magic_power = zyk_max_magic_power(ent);

		ent->client->pers.monk_unique_timer = 0;
		ent->client->pers.unique_skill_duration = 0;

		ent->client->pers.lightning_shield_timer = 0;

		ent->client->pers.credits_modifier = 0;
		ent->client->pers.score_modifier = 0;

		ent->client->pers.buy_sell_timer = 0;
		ent->client->pers.vertical_dfa_timer = 0;

		// zyk: setting default value of can_play_quest
		ent->client->pers.can_play_quest = 0;

		ent->client->pers.guardian_mode = 0;
		ent->client->pers.guardian_timer = 0;
		ent->client->pers.guardian_invoked_by_id = -1;

		ent->client->pers.eternity_quest_timer = 0;

		ent->client->pers.universe_quest_artifact_holder_id = -1;
		ent->client->pers.universe_quest_messages = 0;
		ent->client->pers.universe_quest_timer = 0;

		ent->client->pers.light_quest_timer = 0;
		ent->client->pers.light_quest_messages = 0;

		ent->client->pers.hunter_quest_timer = 0;
		ent->client->pers.hunter_quest_messages = 0;

		// zyk: loading initial RPG weapons
		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_STUN_BATON)) && ent->client->pers.skill_levels[18] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);
		if (ent->client->pers.skill_levels[18] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL)) && ent->client->pers.skill_levels[19] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		if (ent->client->pers.skill_levels[19] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BLASTER)) && ent->client->pers.skill_levels[20] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
		if (ent->client->pers.skill_levels[20] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BLASTER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DISRUPTOR)) && ent->client->pers.skill_levels[21] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);
		if (ent->client->pers.skill_levels[21] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DISRUPTOR);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER)) && ent->client->pers.skill_levels[22] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);
		if (ent->client->pers.skill_levels[22] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BOWCASTER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_REPEATER)) && ent->client->pers.skill_levels[23] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);
		if (ent->client->pers.skill_levels[23] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_REPEATER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DEMP2)) && ent->client->pers.skill_levels[24] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);
		if (ent->client->pers.skill_levels[24] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DEMP2);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_FLECHETTE)) && ent->client->pers.skill_levels[25] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);
		if (ent->client->pers.skill_levels[25] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_FLECHETTE);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)) && ent->client->pers.skill_levels[26] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);
		if (ent->client->pers.skill_levels[26] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_ROCKET_LAUNCHER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_CONCUSSION)) && ent->client->pers.skill_levels[27] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);
		if (ent->client->pers.skill_levels[27] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_CONCUSSION);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_OLD)) && ent->client->pers.skill_levels[28] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);
		if (ent->client->pers.skill_levels[28] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_OLD);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_THERMAL)) && ent->client->pers.skill_levels[43] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
		if (ent->client->pers.skill_levels[43] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_THERMAL);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_TRIP_MINE)) && ent->client->pers.skill_levels[44] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
		if (ent->client->pers.skill_levels[44] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_TRIP_MINE);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DET_PACK)) && ent->client->pers.skill_levels[45] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
		if (ent->client->pers.skill_levels[45] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DET_PACK);

		// zyk: loading initial RPG ammo at spawn
		ent->client->ps.ammo[AMMO_BLASTER] = ((int)ceil(zyk_max_blaster_pack_ammo.value/3.0) * ent->client->pers.skill_levels[39]);
		ent->client->ps.ammo[AMMO_POWERCELL] = ((int)ceil(zyk_max_power_cell_ammo.value/3.0) * ent->client->pers.skill_levels[40]);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = ((int)ceil(zyk_max_metal_bolt_ammo.value/3.0) * ent->client->pers.skill_levels[41]);
		ent->client->ps.ammo[AMMO_ROCKETS] = ((int)ceil(zyk_max_rocket_ammo.value/3.0) * ent->client->pers.skill_levels[42]);
		ent->client->ps.ammo[AMMO_THERMAL] = ((int)ceil(zyk_max_thermal_ammo.value/3.0) * ent->client->pers.skill_levels[43]);
		ent->client->ps.ammo[AMMO_TRIPMINE] = ((int)ceil(zyk_max_tripmine_ammo.value/3.0) * ent->client->pers.skill_levels[44]);
		ent->client->ps.ammo[AMMO_DETPACK] = ((int)ceil(zyk_max_detpack_ammo.value/3.0) * ent->client->pers.skill_levels[45]);
		
		if (ent->client->pers.rpg_class == 2)
		{ // zyk: modifying max ammo if the player is a Bounty Hunter
			gentity_t *this_ent = NULL;
			int sentry_guns_iterator = 0;

			// zyk: Bounty Hunter starts with 5 sentries if he has the Upgrade
			if (ent->client->pers.skill_levels[48] > 0)
			{
				if (ent->client->pers.secrets_found & (1 << 1))
					ent->client->pers.bounty_hunter_sentries = MAX_BOUNTY_HUNTER_SENTRIES;
				else
					ent->client->pers.bounty_hunter_sentries = 1;
			}
			else
			{
				ent->client->pers.bounty_hunter_sentries = 0;
			}

			ent->client->pers.bounty_hunter_placed_sentries = 0;

			for (sentry_guns_iterator = MAX_CLIENTS; sentry_guns_iterator < level.num_entities; sentry_guns_iterator++)
			{
				this_ent = &g_entities[sentry_guns_iterator];

				if (this_ent && Q_stricmp(this_ent->classname,"sentryGun") == 0 && this_ent->s.owner == ent->s.number)
					ent->client->pers.bounty_hunter_placed_sentries++;
			}

			ent->client->ps.ammo[AMMO_BLASTER] += ent->client->ps.ammo[AMMO_BLASTER]/8 * ent->client->pers.skill_levels[55];
			ent->client->ps.ammo[AMMO_POWERCELL] += ent->client->ps.ammo[AMMO_POWERCELL]/8 * ent->client->pers.skill_levels[55];
			ent->client->ps.ammo[AMMO_METAL_BOLTS] += ent->client->ps.ammo[AMMO_METAL_BOLTS]/8 * ent->client->pers.skill_levels[55];
			ent->client->ps.ammo[AMMO_ROCKETS] += ent->client->ps.ammo[AMMO_ROCKETS]/8 * ent->client->pers.skill_levels[55];
			ent->client->ps.ammo[AMMO_THERMAL] += ent->client->ps.ammo[AMMO_THERMAL]/8 * ent->client->pers.skill_levels[55];
			ent->client->ps.ammo[AMMO_TRIPMINE] += ent->client->ps.ammo[AMMO_TRIPMINE]/8 * ent->client->pers.skill_levels[55];
			ent->client->ps.ammo[AMMO_DETPACK] += ent->client->ps.ammo[AMMO_DETPACK]/8 * ent->client->pers.skill_levels[55];
		}

		// zyk: reseting initial holdable items of the player
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK) & ~(1 << HI_SHIELD) & ~(1 << HI_MEDPAC) & ~(1 << HI_MEDPAC_BIG);

		if (ent->client->pers.skill_levels[46] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);

		if (ent->client->pers.skill_levels[47] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);

		if (ent->client->pers.skill_levels[48] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

		if (ent->client->pers.skill_levels[49] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);

		if (ent->client->pers.skill_levels[50] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);

		if (ent->client->pers.skill_levels[51] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);

		if (ent->client->pers.skill_levels[52] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);

		if (ent->client->pers.skill_levels[53] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);

		// zyk: loading initial health of the player
		set_max_health(ent);
		ent->health = ent->client->pers.max_rpg_health;
		ent->client->ps.stats[STAT_HEALTH] = ent->health;

		// zyk: loading initial shield of the player
		set_max_shield(ent);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;

		// zyk: Light Power, Dark Power and Eternity Power use mp
		if (ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS && !(ent->client->pers.player_settings & (1 << 1)) && 
			zyk_enable_light_power.integer == 1)
		{
			ent->client->pers.magic_power--;
			ent->client->pers.quest_power_status |= (1 << 14);
		}
		else
		{
			ent->client->pers.quest_power_status &= ~(1 << 14);
		}

		if (ent->client->pers.hunter_quest_progress == NUMBER_OF_OBJECTIVES && !(ent->client->pers.player_settings & (1 << 2)) && 
			zyk_enable_dark_power.integer == 1)
		{
			ent->client->pers.magic_power--;
			ent->client->pers.quest_power_status |= (1 << 15);
		}
		else
		{
			ent->client->pers.quest_power_status &= ~(1 << 15);
		}

		if (ent->client->pers.eternity_quest_progress == NUMBER_OF_ETERNITY_QUEST_OBJECTIVES && !(ent->client->pers.player_settings & (1 << 3)) && 
			zyk_enable_eternity_power.integer == 1)
		{
			ent->client->pers.magic_power--;
			ent->client->pers.quest_power_status |= (1 << 16);
		}
		else
		{
			ent->client->pers.quest_power_status &= ~(1 << 16);
		}

		// zyk: Universe Power
		if (ent->client->pers.universe_quest_progress > 7 && !(ent->client->pers.player_settings & (1 << 4)) && 
			zyk_enable_universe_power.integer == 1)
		{
			ent->client->pers.quest_power_status |= (1 << 13);
		}
		else
		{
			ent->client->pers.quest_power_status &= ~(1 << 13);
		}

		// zyk: the player can have only one of the Unique Abilities. If for some reason he has more, remove all of them
		if ((ent->client->pers.secrets_found & (1 << 2) && ent->client->pers.secrets_found & (1 << 3)) || 
			(ent->client->pers.secrets_found & (1 << 2) && ent->client->pers.secrets_found & (1 << 4)) || 
			(ent->client->pers.secrets_found & (1 << 3) && ent->client->pers.secrets_found & (1 << 4)) || 
			(ent->client->pers.secrets_found & (1 << 2) && ent->client->pers.secrets_found & (1 << 3) && ent->client->pers.secrets_found & (1 << 4)))
		{
			ent->client->pers.secrets_found &= ~(1 << 2);
			ent->client->pers.secrets_found &= ~(1 << 3);
			ent->client->pers.secrets_found &= ~(1 << 4);
		}

		// zyk: update the rpg stuff info at the client-side game
		send_rpg_events(10000);
	}
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

	ent->client->pers.defeated_guardians = 0;
	ent->client->pers.hunter_quest_progress = 0;
	ent->client->pers.eternity_quest_progress = 0;
	ent->client->pers.secrets_found = 0;
	ent->client->pers.universe_quest_progress = 0;
	ent->client->pers.universe_quest_counter = 0;
	ent->client->pers.credits = 100;
	ent->client->pers.rpg_class = 0;
	ent->client->sess.magic_disabled_powers = 0;
	ent->client->sess.magic_more_disabled_powers = 0;
	ent->client->sess.selected_special_power = MAGIC_MAGIC_SENSE;
	ent->client->sess.selected_left_special_power = MAGIC_MAGIC_SENSE;
	ent->client->sess.selected_right_special_power = MAGIC_MAGIC_SENSE;
	ent->client->sess.magic_fist_selection = 0;

	// zyk: if creating new char, remove Challenge Mode flag from old one
	ent->client->pers.player_settings &= ~(1 << 15);
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
		initialize_rpg_skills(ent);
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

// zyk: loads the player account the old way
void legacy_load_account(gentity_t *ent)
{
	FILE *account_file;
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
			if ((j % 10) == 0)
			{ // zyk: level divisible by 10 has more skillpoints
				max_skillpoints += (1 + j / 10);
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

		// zyk: Other RPG attributes
		// zyk: loading Light Quest Defeated Guardians number value
		fscanf(account_file, "%s", content);
		ent->client->pers.defeated_guardians = atoi(content);

		// zyk: compability with old mod versions, in which the players who completed the quest had a value of 9
		if (ent->client->pers.defeated_guardians == 9)
			ent->client->pers.defeated_guardians = NUMBER_OF_GUARDIANS;

		// zyk: loading Dark Quest completed objectives value
		fscanf(account_file, "%s", content);
		ent->client->pers.hunter_quest_progress = atoi(content);

		// zyk: loading Eternity Quest progress value
		fscanf(account_file, "%s", content);
		ent->client->pers.eternity_quest_progress = atoi(content);

		// zyk: loading secrets found value
		fscanf(account_file, "%s", content);
		ent->client->pers.secrets_found = atoi(content);

		// zyk: loading Universe Quest Progress value
		fscanf(account_file, "%s", content);
		ent->client->pers.universe_quest_progress = atoi(content);

		// zyk: loading Universe Quest Counter value
		fscanf(account_file, "%s", content);
		ent->client->pers.universe_quest_counter = atoi(content);

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

		// zyk: loading RPG class
		fscanf(account_file, "%s", content);
		ent->client->pers.rpg_class = atoi(content);

		// zyk: loading disabled magic powers
		fscanf(account_file, "%s", content);
		ent->client->sess.magic_disabled_powers = atoi(content);

		// zyk: loading more disabled magic powers
		fscanf(account_file, "%s", content);
		ent->client->sess.magic_more_disabled_powers = atoi(content);

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
	{
		trap->SendServerCommand(ent - g_entities, "print \"There is no account with this login or password.\n\"");
	}
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
			trap->SendServerCommand( ent-g_entities, "print \"You must write your login and password.\n\"" ); 
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
				trap->SendServerCommand( ent-g_entities, "print \"There is already someone logged in this account.\n\"" );
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
			trap->SendServerCommand( ent-g_entities, "print \"Login does not exist.\n\"" );
			return;
		}

		// zyk: validating password
		fscanf(account_file,"%s",password);
		fclose(account_file);
		if (strlen(password) != strlen(arg2) || Q_strncmp(password, arg2, strlen(password)) != 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"The password is incorrect.\n\"" );
			return;
		}

		// zyk: valid login and password
		strcpy(ent->client->sess.filename, arg1);
		strcpy(ent->client->pers.password, arg2);

		account_file = fopen(va("zykmod/accounts/%s_%s.txt", arg1, arg1), "r");
		if (account_file == NULL)
		{ // zyk: old account. Use the legacy function to convert it to the Char system
			legacy_load_account(ent);

			// zyk: saving the default char
			strcpy(ent->client->sess.rpgchar, arg1);

			save_account(ent, qfalse);
			save_account(ent, qtrue);
		}
		else
		{
			fclose(account_file);

			load_account(ent);
		}

		if (ent->client->sess.amrpgmode == 1)
			trap->SendServerCommand( ent-g_entities, "print \"^7Account loaded succesfully in ^2Admin-Only Mode^7. Use command ^3/list^7.\n\"" );
		else if (ent->client->sess.amrpgmode == 2)
		{
			initialize_rpg_skills(ent);
			trap->SendServerCommand( ent-g_entities, "print \"^7Account loaded succesfully in ^2RPG Mode^7. Use command ^3/list^7.\n\"" );

			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
				G_Kill(ent);
			}
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"You are already logged in.\n\"" );
	}
}

// zyk: clean the guardian npcs
void clean_guardians(gentity_t *ent)
{
	int i = 0;
	gentity_t *this_ent = NULL;

	for (i = MAX_CLIENTS; i < level.num_entities; i++)
	{
		this_ent = &g_entities[i];
		if (this_ent && this_ent->client && this_ent->NPC && this_ent->client->pers.guardian_invoked_by_id != -1 && this_ent->client->pers.guardian_invoked_by_id == (ent-g_entities))
		{
			// zyk: if quest player dies in boss battle, makes the boss stop using force powers
			int j = 0;

			while (j < NUM_FORCE_POWERS)
			{
				if (this_ent->client->ps.fd.forcePowersActive & (1 << j))
				{
					WP_ForcePowerStop(this_ent, j);
				}

				j++;
			}

			G_FreeEntity(this_ent);
		}
	}
}

// zyk: tests if the player has beaten the guardians before the Guardian of Light in Light Quest
qboolean light_quest_defeated_guardians(gentity_t *ent)
{
	int j = 0, number_of_guardians_defeated = 0;

	for (j = 4; j <= 12; j++)
	{
		if (ent->client->pers.defeated_guardians & (1 << j))
		{
			number_of_guardians_defeated++;
		}
	}

	if (number_of_guardians_defeated == (NUMBER_OF_GUARDIANS - 1))
		return qtrue;
	else
		return qfalse;
}

// zyk: tests if the player has collected the 9 notes before the Guardian of Darkness in Dark Quest
qboolean dark_quest_collected_notes(gentity_t *ent)
{
	if (ent->client->pers.hunter_quest_progress & (1 << 4) && ent->client->pers.hunter_quest_progress & (1 << 5) && ent->client->pers.hunter_quest_progress & (1 << 6) && ent->client->pers.hunter_quest_progress & (1 << 7) && ent->client->pers.hunter_quest_progress & (1 << 8) && ent->client->pers.hunter_quest_progress & (1 << 9) && ent->client->pers.hunter_quest_progress & (1 << 10) && ent->client->pers.hunter_quest_progress & (1 << 11) && ent->client->pers.hunter_quest_progress & (1 << 12))
	{
		return qtrue;
	}
	else
	{
		return qfalse;
	}
}

extern void zyk_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_spawn_entity(gentity_t *ent);
extern void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_main_spawn_entity(gentity_t *ent);

// zyk: loads the datapad md3 model for the Dark Quest notes
void load_note_model(int x,int y,int z)
{
	gentity_t *new_ent = G_Spawn();

	zyk_set_entity_field(new_ent,"classname","misc_model_breakable");
	zyk_set_entity_field(new_ent,"origin",va("%d %d %d",x,y,z));
	zyk_set_entity_field(new_ent,"angles","0 90 0");
	zyk_set_entity_field(new_ent,"model","models/items/datapad.md3");

	zyk_spawn_entity(new_ent);

	if (x == 2780 && y == 3966 && z == 1411)
		level.universe_quest_note_id = new_ent->s.number;
	else
		level.quest_note_id = new_ent->s.number;
}

// zyk: loads the crystal md3 model for the Universe Quest crystals
gentity_t *load_crystal_model(int x,int y,int z, int yaw, int crystal_number)
{
	gentity_t *ent = G_Spawn();

	zyk_set_entity_field(ent,"classname","misc_model_breakable");
	zyk_set_entity_field(ent,"origin",va("%d %d %d",x,y,z));
	zyk_set_entity_field(ent,"angles",va("0 %d 0",yaw));

	if (crystal_number == 0)
		zyk_set_entity_field(ent,"model","models/map_objects/mp/crystal_blue.md3");
	else if (crystal_number == 1)
		zyk_set_entity_field(ent,"model","models/map_objects/mp/crystal_red.md3");
	else
		zyk_set_entity_field(ent,"model","models/map_objects/mp/crystal_green.md3");

	zyk_spawn_entity(ent);

	level.quest_crystal_id[crystal_number] = ent->s.number;

	return ent;
}

// zyk: load an effect used in quests
gentity_t *load_effect(int x,int y,int z, int spawnflags, char *fxFile)
{
	gentity_t *ent = G_Spawn();

	zyk_set_entity_field(ent,"classname","fx_runner");
	zyk_set_entity_field(ent,"spawnflags",va("%d",spawnflags));
	zyk_set_entity_field(ent,"origin",va("%d %d %d",x,y,z));

	ent->s.modelindex = G_EffectIndex( fxFile );

	zyk_spawn_entity(ent);

	level.quest_effect_id = ent->s.number;

	return ent;
}

// zyk: cleans the note model if player gets it
void clean_note_model()
{
	if (level.quest_note_id != -1)
	{
		G_FreeEntity(&g_entities[level.quest_note_id]);
		level.quest_note_id = -1;
	}

	if (level.universe_quest_note_id != -1)
	{
		G_FreeEntity(&g_entities[level.universe_quest_note_id]);
		level.universe_quest_note_id = -1;
	}
}

// zyk: cleans the crystal model if player gets it
void clean_crystal_model(int crystal_number)
{
	if (level.quest_crystal_id[crystal_number] != -1)
	{
		G_FreeEntity(&g_entities[level.quest_crystal_id[crystal_number]]);
		level.quest_crystal_id[crystal_number] = -1;
	}
}

// zyk: cleans the effect if player reaches it
void clean_effect()
{
	if (level.quest_effect_id != -1)
	{
		G_FreeEntity(&g_entities[level.quest_effect_id]);
		level.quest_effect_id = -1;
	}
}

// zyk: tests if player got all 3 amulets in Amulets Mission of Universe Quest
void got_all_amulets(gentity_t *ent)
{
	if (number_of_amulets(ent) == 3)
	{
		ent->client->pers.universe_quest_objective_control = -6;
		ent->client->pers.universe_quest_timer = level.time + 3000;
		ent->client->pers.universe_quest_messages = 199;
	}
}

// zyk: tests how many side quests completed by the player
int zyk_number_of_completed_quests(gentity_t *ent)
{
	int number_of_completed_quests = 0;

	if (ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS)
		number_of_completed_quests++;
	if (ent->client->pers.hunter_quest_progress == NUMBER_OF_OBJECTIVES)
		number_of_completed_quests++;
	if (ent->client->pers.eternity_quest_progress == NUMBER_OF_ETERNITY_QUEST_OBJECTIVES)
		number_of_completed_quests++;

	return number_of_completed_quests;
}

// zyk: used by the quest_get_new_player function to actually get the new player based on his quest settings
void choose_new_player(gentity_t *next_player)
{
	int found = 0;
	if (next_player && next_player->client && next_player->client->sess.amrpgmode == 2 && !(next_player->client->pers.player_settings & (1 << 0)) && next_player->client->pers.can_play_quest == 0 && next_player->client->pers.connected == CON_CONNECTED && next_player->client->sess.sessionTeam != TEAM_SPECTATOR && next_player->inuse == qtrue)
	{
		if (level.quest_map == 1 && ((next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 4))) || (next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 4))) || (next_player->client->pers.universe_quest_progress == 2 && (!(next_player->client->pers.universe_quest_counter & (1 << 1)) || !(next_player->client->pers.universe_quest_counter & (1 << 3)))) || next_player->client->pers.universe_quest_progress == 3))
			found = 1;
		else if (level.quest_map == 2 && next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 5)))
			found = 1;
		else if (level.quest_map == 3 && next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 6)))
			found = 1;
		else if (level.quest_map == 4 && (
			(next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 7))) || 
			(next_player->client->pers.universe_quest_progress >= 17 && next_player->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES &&
			 next_player->client->pers.universe_quest_counter & (1 << 3))))
			found = 1;
		else if (level.quest_map == 5 && ((next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 12))) || (next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 8))) || (next_player->client->pers.universe_quest_progress == 2 && !(next_player->client->pers.universe_quest_counter & (1 << 9)))))
			found = 1;
		else if (level.quest_map == 6 && (
			(next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 9))) ||
			(next_player->client->pers.universe_quest_progress == 2 && !(next_player->client->pers.universe_quest_counter & (1 << 6))) ||
			(next_player->client->pers.universe_quest_progress >= 18 && next_player->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES && 
			 next_player->client->pers.universe_quest_counter & (1 << 0)) || 
			(next_player->client->pers.universe_quest_progress == 16 && next_player->client->pers.universe_quest_counter & (1 << 1)) || 
			(next_player->client->pers.universe_quest_progress >= 15 && next_player->client->pers.universe_quest_progress <= 17 && next_player->client->pers.universe_quest_counter & (1 << 2))))
		{
			found = 1;
		}
		else if (level.quest_map == 7 && ((next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 7))) || (next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 10)))))
			found = 1;
		else if (level.quest_map == 8 && next_player->client->pers.universe_quest_progress == 4)
			found = 1;
		else if (level.quest_map == 9 && (next_player->client->pers.universe_quest_progress < 2 || (next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 12)))))
			found = 1;
		else if (level.quest_map == 10 && ((next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 6))) || 
				 light_quest_defeated_guardians(next_player) == qtrue || dark_quest_collected_notes(next_player) == qtrue || 
				 next_player->client->pers.eternity_quest_progress < NUMBER_OF_ETERNITY_QUEST_OBJECTIVES || 
				(next_player->client->pers.universe_quest_progress == 2 && !(next_player->client->pers.universe_quest_counter & (1 << 8))) || 
				(next_player->client->pers.universe_quest_progress == 15 && next_player->client->pers.universe_quest_counter & (1 << 0)) || 
				(next_player->client->pers.universe_quest_progress == 16 && next_player->client->pers.universe_quest_counter & (1 << 3))))
		{
			found = 1;
		}
		else if (level.quest_map == 11 && next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 9)))
			found = 1;
		else if (level.quest_map == 12 && (next_player->client->pers.universe_quest_progress == 7 ||
				(next_player->client->pers.universe_quest_progress == 8 && !(next_player->client->pers.universe_quest_counter & (1 << 1))) || 
				(next_player->client->pers.universe_quest_progress == 15 && next_player->client->pers.universe_quest_counter & (1 << 1)) || 
				(next_player->client->pers.universe_quest_progress >= 17 && next_player->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES && 
				 next_player->client->pers.universe_quest_counter & (1 << 1)) || 
				(next_player->client->pers.universe_quest_progress == 15 && next_player->client->pers.universe_quest_counter & (1 << 3))))
		{
			found = 1;
		}
		else if (level.quest_map == 13 && ((next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 5))) || (next_player->client->pers.universe_quest_progress == 2 && !(next_player->client->pers.universe_quest_counter & (1 << 5)))))
			found = 1;
		else if (level.quest_map == 14 && next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 11)))
			found = 1;
		else if (level.quest_map == 15 && next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 10)))
			found = 1;
		else if (level.quest_map == 17 && ((next_player->client->pers.universe_quest_progress == 8 && !(next_player->client->pers.universe_quest_counter & (1 << 2))) || (next_player->client->pers.universe_quest_progress == 9 && (!(next_player->client->pers.universe_quest_counter & (1 << 0)) || !(next_player->client->pers.universe_quest_counter & (1 << 1)) || !(next_player->client->pers.universe_quest_counter & (1 << 2)))) || (next_player->client->pers.universe_quest_progress >= 10 && next_player->client->pers.universe_quest_progress < 14) || (next_player->client->pers.universe_quest_progress == 14 && zyk_number_of_completed_quests(next_player) == 3)))
			found = 1;
		else if (level.quest_map == 18 && ((next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES && !(next_player->client->pers.hunter_quest_progress & (1 << 11))) || (next_player->client->pers.universe_quest_progress == 2 && !(next_player->client->pers.universe_quest_counter & (1 << 4)))))
			found = 1;
		else if (level.quest_map == 20 && ((next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS && !(next_player->client->pers.defeated_guardians & (1 << 8))) || (next_player->client->pers.universe_quest_progress == 2 && !(next_player->client->pers.universe_quest_counter & (1 << 7)))))
			found = 1;
		else if (level.quest_map == 24 && (next_player->client->pers.universe_quest_progress == 5 ||
				(next_player->client->pers.universe_quest_progress == 16 && next_player->client->pers.universe_quest_counter & (1 << 0)) || 
				(next_player->client->pers.universe_quest_progress == 17 && next_player->client->pers.universe_quest_counter & (1 << 0)) || 
				(next_player->client->pers.universe_quest_progress >= 18 && next_player->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES && 
				 next_player->client->pers.universe_quest_counter & (1 << 2))))
		{
			found = 1;
		}
		else if (level.quest_map == 25 && next_player->client->pers.universe_quest_progress == 6)
			found = 1;
	}

	if (found == 1)
	{ // zyk: clean quest npcs of this map
		int j = 0;
		for (j = MAX_CLIENTS; j < level.num_entities; j++)
		{
			if (&g_entities[j] && g_entities[j].NPC && g_entities[j].health > 0 && (Q_stricmp(g_entities[j].NPC_type, "ymir_boss") == 0 || Q_stricmp(g_entities[j].NPC_type, "thor_boss") == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "quest_ragnos" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "quest_jawa" ) == 0 || Q_stricmp(g_entities[j].NPC_type, "quest_mage") == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "quest_protocol_imp" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "quest_sand_raider_green" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "quest_sand_raider_brown" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "quest_sand_raider_blue" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "quest_sand_raider_red" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "quest_reborn" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "quest_reborn_blue" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "quest_reborn_boss" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "quest_reborn_red" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "sage_of_light" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "sage_of_darkness" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "sage_of_eternity" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "sage_of_universe" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "quest_super_soldier" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "guardian_of_time" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "guardian_boss_9" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "guardian_of_darkness" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "guardian_of_eternity" ) == 0 || 
				Q_stricmp( g_entities[j].NPC_type, "guardian_of_universe" ) == 0 || Q_stricmp( g_entities[j].NPC_type, "master_of_evil" ) == 0 || 
				Q_stricmp(g_entities[j].NPC_type, "soul_of_sorrow") == 0 || Q_stricmp(g_entities[j].NPC_type, "quest_citizen_warrior") == 0))
			{
				G_FreeEntity(&g_entities[j]);
			}
			else if (&g_entities[j] && (Q_stricmp(g_entities[j].targetname, "zyk_quest_models") == 0 || Q_stricmp(g_entities[j].targetname, "zyk_quest_artifact") == 0 || 
					 Q_stricmp(g_entities[j].targetname, "zyk_sage_prison") == 0 || g_entities[j].spawnflags & 131072))
			{ // zyk: cleans the models/effects/items spawned in quests
				G_FreeEntity(&g_entities[j]);
			}
		}

		// zyk: setting the attributes depending on the quests this player must complete in this map
		next_player->client->pers.guardian_mode = 0;
		next_player->client->pers.guardian_timer = 0;

		next_player->client->pers.universe_quest_artifact_holder_id = -1;
		next_player->client->pers.universe_quest_messages = 0;

		// zyk: give some seconds to the new player to start his quest
		next_player->client->pers.universe_quest_timer = level.time + 3000;

		next_player->client->pers.hunter_quest_timer = level.time + 3000;
		next_player->client->pers.hunter_quest_messages = 0;

		next_player->client->pers.light_quest_timer = level.time + 3000;
		next_player->client->pers.light_quest_messages = 0;

		if (next_player->client->pers.eternity_quest_progress < (NUMBER_OF_ETERNITY_QUEST_OBJECTIVES - 1))
		{ // zyk: give some time before the riddle appears to the quest player
			next_player->client->pers.eternity_quest_timer = level.time + 1000;
		}
		else
		{ 
			next_player->client->pers.eternity_quest_timer = 0;
		}

		// zyk: must clean here too so in yavin1b the correct note is spawned for this player
		if (level.quest_map == 1)
			clean_note_model();

		if (level.quest_map == 9 && next_player->client->pers.universe_quest_progress == 0) 
		{ // zyk: first Universe Quest objective
			next_player->client->pers.universe_quest_objective_control = 8; // zyk: player must kill quest reborn npcs to complete the first objective
			next_player->client->pers.light_quest_messages = 9;
		}
		else if (level.quest_map == 9 && next_player->client->pers.universe_quest_progress == 1)
		{ // zyk: second Universe Quest objective
			next_player->client->pers.universe_quest_objective_control = 2; // zyk: sets this player as playing the second objective of Universe Quest
		}
		else if (next_player->client->pers.universe_quest_progress == 2)
		{ // zyk: third Universe Quest objective
			next_player->client->pers.universe_quest_objective_control = 3; // zyk: sets this player as playing the third objective of Universe Quest
		}
		else if (level.quest_map == 1 && next_player->client->pers.universe_quest_progress == 3)
		{
			if (level.universe_quest_note_id == -1)
				load_note_model(2780,3966,1411);

			next_player->client->pers.universe_quest_objective_control = 4; // zyk: fourth Universe Quest objective
		}
		else if (level.quest_map == 8 && next_player->client->pers.universe_quest_progress == 4)
		{ // zyk: fifth Universe Quest objective
			next_player->client->pers.universe_quest_objective_control = 5;
			next_player->client->pers.universe_quest_timer = level.time + 2000;
		}
		else if (level.quest_map == 24 && next_player->client->pers.universe_quest_progress == 5)
		{
			next_player->client->pers.universe_quest_objective_control = -6;
			got_all_amulets(next_player);
		}
		else if (level.quest_map == 25 && next_player->client->pers.universe_quest_progress == 6)
		{ // zyk: seventh Universe Quest objective
			next_player->client->pers.universe_quest_timer = level.time + 3000;
			next_player->client->pers.universe_quest_objective_control = -7;
		}
		else if (level.quest_map == 17)
		{ // zyk: Universe Quest
			if (next_player->client->pers.universe_quest_progress == 9)
			{ // zyk: cleaning crystals that were in the map
				int zyk_it = 0;

				for (zyk_it = 0; zyk_it < 3; zyk_it++)
				{
					clean_crystal_model(zyk_it);
				}
			}

			if (next_player->client->pers.universe_quest_progress == 11)
			{ // zyk: player must defeat this quantity of quest_super_soldier npcs in this mission
				next_player->client->pers.universe_quest_timer = level.time + 3000;
				next_player->client->pers.universe_quest_objective_control = 20;
			}
		}

		// zyk: loading note models if player must find a Dark Quest note
		if (level.quest_note_id == -1 && next_player->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES)
		{
			if (level.quest_map == 1 && !(next_player->client->pers.hunter_quest_progress & (1 << 4)))
			{
				load_note_model(2375,4600,1810);
			}
			else if (level.quest_map == 2 && !(next_player->client->pers.hunter_quest_progress & (1 << 5)))
			{
				load_note_model(7500,-755,2);
			}
			else if (level.quest_map == 3 && !(next_player->client->pers.hunter_quest_progress & (1 << 6)))
			{
				load_note_model(-765,4790,196);
			}
			else if (level.quest_map == 4 && !(next_player->client->pers.hunter_quest_progress & (1 << 7)))
			{
				load_note_model(2400,2990,-2093);
			}
			else if (level.quest_map == 5 && !(next_player->client->pers.hunter_quest_progress & (1 << 8)))
			{
				load_note_model(-500,-4690,928);
			}
			else if (level.quest_map == 6 && !(next_player->client->pers.hunter_quest_progress & (1 << 9)))
			{
				load_note_model(-9838,-1547,2);
			}
			else if (level.quest_map == 7 && !(next_player->client->pers.hunter_quest_progress & (1 << 10)))
			{
				load_note_model(1905,1180,706);
			}
			else if (level.quest_map == 18 && !(next_player->client->pers.hunter_quest_progress & (1 << 11)))
			{
				load_note_model(-1148,-1458,593);
			}
			else if (level.quest_map == 9 && !(next_player->client->pers.hunter_quest_progress & (1 << 12)))
			{
				load_note_model(-1963,2633,-3005);
			}
		}

		// zyk: loading effects in guardian area
		if (level.quest_effect_id == -1 && next_player->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
		{
			if (level.quest_map == 1 && !(next_player->client->pers.defeated_guardians & (1 << 4)))
			{ // zyk: Guardian of Water
				load_effect(2062,4089,351,0,"env/btend");
			}
			else if (level.quest_map == 13 && !(next_player->client->pers.defeated_guardians & (1 << 5)))
			{ // zyk: Guardian of Earth
				load_effect(-2149,-4187,3645,0,"env/btend");
			}
			else if (level.quest_map == 10 && !(next_player->client->pers.defeated_guardians & (1 << 6)))
			{ // zyk: Guardian of Forest
				load_effect(512,4829,62,0,"env/btend");
			}
			else if (level.quest_map == 7 && !(next_player->client->pers.defeated_guardians & (1 << 7)))
			{ // zyk: Guardian of Intelligence
				load_effect(1920, 2068, 720,0,"env/btend");
			}
			else if (level.quest_map == 20 && !(next_player->client->pers.defeated_guardians & (1 << 8)))
			{ // zyk: Guardian of Agility
				load_effect(8474,-1322,-159,0,"env/btend");
			}
			else if (level.quest_map == 11 && !(next_player->client->pers.defeated_guardians & (1 << 9)))
			{ // zyk: Guardian of Fire
				load_effect(0,5,-375,0,"env/btend");
			}
			else if (level.quest_map == 15 && !(next_player->client->pers.defeated_guardians & (1 << 10)))
			{ // zyk: Guardian of Wind
				load_effect(-153,-455,216,0,"env/btend");
			}
			else if (level.quest_map == 14 && !(next_player->client->pers.defeated_guardians & (1 << 11)))
			{ // zyk: Guardian of Resistance
				load_effect(0,1135,9,0,"env/btend");
			}
			else if (level.quest_map == 5 && !(next_player->client->pers.defeated_guardians & (1 << 12)))
			{ // zyk: Guardian of Ice
				load_effect(-5548,11548,990,0,"env/btend");
			}
		}

		next_player->client->pers.quest_afk_timer = level.time + zyk_quest_afk_timer.integer;
		next_player->client->pers.can_play_quest = 1;

		do_scale(next_player, 100);

		trap->SendServerCommand( -1, va("chat \"^3Quest System: ^7%s ^7turn.\"",next_player->client->pers.netname));
	}
}

// zyk: searches for a new player to play a quest if he died or failed
void quest_get_new_player(gentity_t *ent)
{
	int i = 0;
	gentity_t *next_player = NULL;

	ent->client->pers.can_play_quest = 0;

	if (zyk_allow_quests.integer != 1)
		return;

	if (zyk_rp_mode.integer == 1)
		return;

	if (level.gametype != GT_FFA)
	{ // zyk: quests can only be played at FFA gametype
		return;
	}

	for (i = 0; i < level.maxclients; i++)
	{ // zyk: verify if there is someone who is already playing a quest and is not in spectator mode
		next_player = &g_entities[i];
		if (next_player && next_player->client && next_player->client->sess.amrpgmode == 2 && next_player->client->pers.can_play_quest == 1 && next_player->client->sess.sessionTeam != TEAM_SPECTATOR)
			return;
	}

	for (i = 0; i < level.maxclients; i++)
	{ // zyk: remove guardian_mode from all players that were fighting a boss battle
		next_player = &g_entities[i];
		if (next_player && next_player->client && next_player->client->sess.amrpgmode == 2 && next_player->client->pers.guardian_mode != 0)
			next_player->client->pers.guardian_mode = 0;
	}

	// zyk: no one is already playing the quest, so choose a new player

	for (i = ((ent-g_entities) + 1); i < level.maxclients; i++)
	{
		next_player = &g_entities[i];

		choose_new_player(next_player);

		if (next_player && next_player->client && next_player->client->sess.amrpgmode == 2 && next_player->client->pers.can_play_quest == 1) // zyk: found the player
			return;
	}

	for (i = 0; i < (ent-g_entities); i++)
	{
		next_player = &g_entities[i];

		choose_new_player(next_player);

		if (next_player && next_player->client && next_player->client->sess.amrpgmode == 2 && next_player->client->pers.can_play_quest == 1) // zyk: found the player
			return;
	}

	// zyk: didnt find anyone to play this quest, so choose the same player again if he still needs to play a quest in this map
	choose_new_player(ent);
}

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
	if (ent->client->pers.being_mind_controlled != -1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You cant logout while being mind-controlled.\n\"" );
		return;
	}

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 1 && ent->client->pers.mind_controlled1_id != -1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You cant logout while using Mind Control on someone.\n\"" );
		return;
	}

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

	// zyk: if player was fighting a guardian, allow other players to fight the guardian now
	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.guardian_mode > 0)
	{
		clean_guardians(ent);
		ent->client->pers.guardian_mode = 0;
	}

	// zyk: saving the not logged player mode in session
	ent->client->sess.amrpgmode = 0;

	// zyk: if this player was playing a quest, find a new one to play quests in this map
	if (ent->client->pers.can_play_quest == 1)
	{
		// zyk: if this is the quest player, reset the boss battle music
		level.boss_battle_music_reset_timer = level.time + 1000;
		quest_get_new_player(ent);
	}

	ent->client->pers.bitvalue = 0;

	// zyk: initializing mind control attributes used in RPG mode
	ent->client->pers.being_mind_controlled = -1;
	ent->client->pers.mind_controlled1_id = -1;

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
	if (ent->client->pers.skill_levels[upgrade_value - 1] < max_skill_levels[upgrade_value - 1])
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

char *zyk_rpg_class(gentity_t *ent)
{
	if (ent->client->pers.rpg_class == 0)
		return "Free Warrior";
	else if (ent->client->pers.rpg_class == 1)
		return "Force User";
	else if (ent->client->pers.rpg_class == 2)
		return "Bounty Hunter";
	else if (ent->client->pers.rpg_class == 3)
		return "Armored Soldier";
	else if (ent->client->pers.rpg_class == 4)
		return "Monk";
	else if (ent->client->pers.rpg_class == 5)
		return "Stealth Attacker";
	else if (ent->client->pers.rpg_class == 6)
		return "Duelist";
	else if (ent->client->pers.rpg_class == 7)
		return "Force Gunner";
	else if (ent->client->pers.rpg_class == 8)
		return "Magic Master";
	else if (ent->client->pers.rpg_class == 9)
		return "Force Guardian";
	else
		return "";
}

char *zyk_get_settings_values(gentity_t *ent)
{
	int i = 0;
	char content[1024];

	strcpy(content,"");

	for (i = 0; i < 16; i++)
	{ // zyk: settings values
		if (i != 5 && i != 8 && i != 14 && i != 15)
		{
			if (!(ent->client->pers.player_settings & (1 << i)))
			{
				strcpy(content,va("%sON-",content));
			}
			else
			{
				strcpy(content,va("%sOFF-",content));
			}
		}
		else if (i == 5)
		{
			if (!(ent->client->pers.player_settings & (1 << i)))
			{
				strcpy(content, va("%sEnglish-", content));
			}
			else
			{
				strcpy(content, va("%sCustom-", content));
			}
		}
		else if (i == 14)
		{
			if (ent->client->pers.player_settings & (1 << 24))
			{
				strcpy(content,va("%sKorriban Action-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 25))
			{
				strcpy(content,va("%sMP Duel-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 14))
			{
				strcpy(content,va("%sCustom-",content));
			}
			else 
			{
				strcpy(content,va("%sHoth2 Action-",content));
			}

		}
		else if (i == 15)
		{
			if (!(ent->client->pers.player_settings & (1 << i)))
			{
				strcpy(content,va("%sNormal-",content));
			}
			else
			{
				strcpy(content,va("%sChallenge-",content));
			}
		}
		else
		{ // zyk: starting saber style has its own handling code
			if (ent->client->pers.player_settings & (1 << 27))
			{
				strcpy(content,va("%sRed-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 28))
			{
				strcpy(content,va("%sDesann-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 29))
			{
				strcpy(content,va("%sTavion-",content));
			}
			else if (ent->client->pers.player_settings & (1 << 26))
			{
				strcpy(content,va("%sYellow-",content));
			}
			else
			{
				strcpy(content,va("%sBlue-",content));
			}
		}
	}

	// zyk: for compability with older versions, keeping a 0 value here
	strcpy(content, va("%sON-", content));

	return G_NewString(content);
}

/*
==================
Cmd_ZykMod_f
==================
*/
void Cmd_ZykMod_f( gentity_t *ent ) {
	// zyk: sends info to the client-side menu if player has the client-side plugin
	int universe_quest_counter_value = 0;

	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	if (ent->client->sess.amrpgmode == 2)
	{
		int i = 0;
		int quest_player_id = MAX_CLIENTS;
		char content[1024];
		int unique_duration = 0;

		strcpy(content,"");

		for (i = 0; i < level.maxclients; i++)
		{
			gentity_t *player_ent = &g_entities[i];

			if (player_ent && player_ent->client && player_ent->client->sess.amrpgmode == 2 && player_ent->client->pers.can_play_quest == 1)
			{ // zyk: found the quest player
				quest_player_id = i;
				break;
			}
		}

		for (i = 0; i < NUMBER_OF_SKILLS; i++)
		{
			strcpy(content, va("%s%d/%d-", content, ent->client->pers.skill_levels[i], max_skill_levels[i]));
		}

		strcpy(content, va("%s%s", content, zyk_get_settings_values(ent)));

		if (ent->client->pers.universe_quest_progress == 2)
		{
			universe_quest_counter_value = number_of_artifacts(ent);
		}
		else if (ent->client->pers.universe_quest_progress == 5)
		{
			universe_quest_counter_value = number_of_amulets(ent);
		}
		else if (ent->client->pers.universe_quest_progress == 8)
		{
			universe_quest_counter_value = ent->client->pers.universe_quest_counter;
		}
		else if (ent->client->pers.universe_quest_progress == 9)
		{
			universe_quest_counter_value = number_of_crystals(ent);
		}
		else if (ent->client->pers.universe_quest_progress > 14)
		{
			universe_quest_counter_value = ent->client->pers.universe_quest_counter;

			if (ent->client->pers.unique_skill_timer > level.time && ent->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES &&
				ent->client->pers.universe_quest_counter & (1 << 2) && !(ent->client->sess.magic_more_disabled_powers & (1 << 1)))
			{ // zyk: Unique Boost decreases unique cooldown time
				universe_quest_counter_value |= (1 << 30);
			}
		}

		if (ent->client->pers.unique_skill_duration > level.time)
		{
			unique_duration = ent->client->pers.unique_skill_duration - level.time;
		}

		strcpy(content,va("%s%d-%d-%d-%d-%d-%d-%d-%d-",content,ent->client->pers.secrets_found,ent->client->pers.defeated_guardians,ent->client->pers.hunter_quest_progress,
			ent->client->pers.eternity_quest_progress,ent->client->pers.universe_quest_progress,universe_quest_counter_value,quest_player_id,unique_duration));

		trap->SendServerCommand(ent->s.number, va("zykmod \"%d/%d-%d/%d-%d-%d/%d-%d/%d-%d-%s-%s\"",ent->client->pers.level, zyk_rpg_max_level.integer,ent->client->pers.level_up_score,(ent->client->pers.level * zyk_level_up_score_factor.integer),ent->client->pers.skillpoints,ent->client->pers.skill_counter,zyk_max_skill_counter.integer,ent->client->pers.magic_power,zyk_max_magic_power(ent),ent->client->pers.credits,zyk_rpg_class(ent),content));
	}
	else if (ent->client->sess.amrpgmode == 1)
	{ // zyk: just sends the player settings
		int i = 0;
		char content[1024];

		strcpy(content,"");

		for (i = 0; i < 69; i++)
		{
			if (i == 63)
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
			trap->SendServerCommand( ent->s.number, "print \"You dont have enough skillpoints.\n\"" );
		return qfalse;
	}

	// zyk: skill must be allowed to the player RPG class
	if (zyk_skill_allowed_for_class(upgrade_value - 1, ent->client->pers.rpg_class) == qfalse)
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, va("print \"%s class doesn't allow ^3%s ^7skill.\n\"", zyk_rpg_class(ent), zyk_skill_name(upgrade_value - 1)));
		return qfalse;
	}

	// zyk: validation on skills that require certain conditions to be upgraded
	if (upgrade_value == 20 && ent->client->pers.skill_levels[19] == 1 && !(ent->client->pers.secrets_found & (1 << 12)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Blaster Pack Weapons Upgrade to get 2/2 in Blaster Pistol.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 21 && ent->client->pers.skill_levels[20] == 1 && !(ent->client->pers.secrets_found & (1 << 12)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Blaster Pack Weapons Upgrade to get 2/2 in E11 Blaster Rifle.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 22 && ent->client->pers.skill_levels[21] == 1 && !(ent->client->pers.secrets_found & (1 << 11)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Power Cell Weapons Upgrade to get 2/2 in Disruptor.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 23 && ent->client->pers.skill_levels[22] == 1 && !(ent->client->pers.secrets_found & (1 << 11)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Power Cell Weapons Upgrade to get 2/2 in Bowcaster.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 24 && ent->client->pers.skill_levels[23] == 1 && !(ent->client->pers.secrets_found & (1 << 13)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Metal Bolts Weapons Upgrade to get 2/2 in Repeater.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 25 && ent->client->pers.skill_levels[24] == 1 && !(ent->client->pers.secrets_found & (1 << 11)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Power Cell Weapons Upgrade to get 2/2 in DEMP2.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 26 && ent->client->pers.skill_levels[25] == 1 && !(ent->client->pers.secrets_found & (1 << 13)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Metal Bolts Weapons Upgrade to get 2/2 in Flechette.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 27 && ent->client->pers.skill_levels[26] == 1 && !(ent->client->pers.secrets_found & (1 << 14)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Rocket Upgrade to get 2/2 in Rocket Launcher.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 28 && ent->client->pers.skill_levels[27] == 1 && !(ent->client->pers.secrets_found & (1 << 13)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Metal Bolts Weapons Upgrade to get 2/2 in Concussion Rifle.\n\"" );
		return qfalse;
	}

	if (upgrade_value == 29 && ent->client->pers.skill_levels[28] == 1 && !(ent->client->pers.secrets_found & (1 << 12)))
	{
		if (dont_show_message == qfalse)
			trap->SendServerCommand( ent->s.number, "print \"You must buy the Blaster Pack Weapons Upgrade to get 2/2 in Bryar Pistol.\n\"" );
		return qfalse;
	}

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

	if (zyk_rp_mode.integer == 1)
	{
		trap->SendServerCommand( ent->s.number, "print \"You can't upgrade skill when RP Mode is activated by an admin.\n\"" );
		return;
	}

	if (validate_rpg_class(ent) == qfalse)
		return;

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

	if (validate_rpg_class(ent) == qfalse)
		return;

	if (ent->client->pers.skill_levels[downgrade_value - 1] > 0)
	{
		ent->client->pers.skill_levels[downgrade_value - 1]--;
		ent->client->pers.skillpoints++;

		if (ent->client->pers.rpg_class == 8 && downgrade_value == 56)
		{ // zyk: resetting selected powers if this is the Improvements skill for Magic Master class
			ent->client->sess.selected_special_power = 1;
			ent->client->sess.selected_left_special_power = 1;
			ent->client->sess.selected_right_special_power = 1;
		}
	}
	else
	{
		trap->SendServerCommand( ent->s.number, va("print \"You reached the minimum level of ^3%s ^7skill.\n\"", zyk_skill_name(downgrade_value - 1)));
		return;
	}

	// zyk: saving the account file with the downgraded skill
	save_account(ent, qtrue);

	trap->SendServerCommand( ent->s.number, "print \"Skill downgraded successfully.\n\"" );

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

	if (zyk_rp_mode.integer == 1)
	{
		trap->SendServerCommand( ent->s.number, "print \"You can't downgrade skill when RP Mode is activated by an admin.\n\"" );
		return;
	}

	do_downgrade_skill(ent, downgrade_value);
}

// zyk: returns the special jka color chars for RPG skills
char *zyk_allowed_skill_color(int skill_index, int rpg_class)
{
	if (zyk_skill_allowed_for_class(skill_index, rpg_class) == qtrue)
	{
		if ((skill_index >= 0 && skill_index <= 4) || skill_index == 38)
		{
			return "^7";
		}
		else if ((skill_index >= 8 && skill_index <= 12) || skill_index == 54)
		{
			return "^5";
		}
		else if ((skill_index >= 13 && skill_index <= 17) || skill_index == 32)
		{
			return "^1";
		}
		else if (skill_index == 30 || skill_index == 31)
		{
			return "^2";
		}
		else if (skill_index >= 35 && skill_index <= 37)
		{
			return "^6";
		}
		else
		{
			return "^3";
		}
	}
	else
	{
		return "^0";
	}
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

void zyk_list_player_skills(gentity_t *ent, gentity_t *target_ent, char *arg1)
{
	char message[1024];
	char message_content[12][100];
	int i = 0;

	strcpy(message,"");
				
	while (i < 11)
	{
		strcpy(message_content[i],"");
		i++;
	}
	message_content[11][0] = '\0';
	i = 0;

	if (Q_stricmp( arg1, "force" ) == 0)
	{
		int first_skill_index = 0;

		for (i = 0; i < 9; i++)
		{
			strcpy(message_content[i], va("%s %d - %s: %d/%d%s ", 
				zyk_allowed_skill_color(first_skill_index, ent->client->pers.rpg_class), (first_skill_index + 1), zyk_skill_name(first_skill_index), 
				ent->client->pers.skill_levels[first_skill_index], max_skill_levels[first_skill_index], zyk_add_whitespaces(first_skill_index, 13)));

			strcpy(message_content[i], va("%s%s%d - %s: %d/%d\n", message_content[i], zyk_allowed_skill_color((first_skill_index + 9), ent->client->pers.rpg_class), 
				(first_skill_index + 10), zyk_skill_name(first_skill_index + 9), ent->client->pers.skill_levels[first_skill_index + 9], max_skill_levels[first_skill_index + 9]));

			first_skill_index++;

			strcpy(message, va("%s%s",message,message_content[i]));
		}

		trap->SendServerCommand( target_ent->s.number, va("print \"%s\"", message) );
	}
	else if (Q_stricmp( arg1, "weapons" ) == 0)
	{
		int first_skill_index = 18;

		for (i = 0; i < 6; i++)
		{
			strcpy(message_content[i], va("%s %d - %s: %d/%d%s ",
				zyk_allowed_skill_color(first_skill_index, ent->client->pers.rpg_class), (first_skill_index + 1), zyk_skill_name(first_skill_index),
				ent->client->pers.skill_levels[first_skill_index], max_skill_levels[first_skill_index], zyk_add_whitespaces(first_skill_index, 17)));

			strcpy(message_content[i], va("%s%s%d - %s: %d/%d\n", message_content[i], zyk_allowed_skill_color((first_skill_index + 6), ent->client->pers.rpg_class),
				(first_skill_index + 7), zyk_skill_name(first_skill_index + 6), ent->client->pers.skill_levels[first_skill_index + 6], max_skill_levels[first_skill_index + 6]));

			first_skill_index++;

			strcpy(message, va("%s%s", message, message_content[i]));
		}

		trap->SendServerCommand( target_ent->s.number, va("print \"%s\"", message) );
	}
	else if (Q_stricmp( arg1, "other" ) == 0)
	{
		int first_skill_index = 30;

		for (i = 0; i < 11; i++)
		{
			if (i == 9)
			{ // zyk: in this moment the next skill to be listed is at index 54
				first_skill_index = 54;
			}

			strcpy(message, va("%s%s%d - %s: %d/%d\n", message, zyk_allowed_skill_color((first_skill_index), ent->client->pers.rpg_class), (first_skill_index + 1), 
				zyk_skill_name(first_skill_index), ent->client->pers.skill_levels[first_skill_index], max_skill_levels[first_skill_index]));

			first_skill_index++;
		}

		trap->SendServerCommand( target_ent->s.number, va("print \"%s\"", message) );
	}
	else if (Q_stricmp( arg1, "ammo" ) == 0)
	{
		int first_skill_index = 39;

		for (i = 0; i < 7; i++)
		{
			strcpy(message, va("%s%s%d - %s: %d/%d\n", message, zyk_allowed_skill_color((first_skill_index), ent->client->pers.rpg_class),
				(first_skill_index + 1), zyk_skill_name(first_skill_index), ent->client->pers.skill_levels[first_skill_index], max_skill_levels[first_skill_index]));

			first_skill_index++;
		}

		trap->SendServerCommand( target_ent->s.number, va("print \"%s\"", message) );
	}
	else if (Q_stricmp( arg1, "items" ) == 0)
	{
		int first_skill_index = 46;

		for (i = 0; i < 8; i++)
		{
			strcpy(message, va("%s%s%d - %s: %d/%d\n", message, zyk_allowed_skill_color((first_skill_index), ent->client->pers.rpg_class),
				(first_skill_index + 1), zyk_skill_name(first_skill_index), ent->client->pers.skill_levels[first_skill_index], max_skill_levels[first_skill_index]));

			first_skill_index++;
		}

		trap->SendServerCommand( target_ent->s.number, va("print \"%s\"", message) );
	}
}

void zyk_list_stuff(gentity_t *ent, gentity_t *target_ent)
{
	char stuff_message[1024];
	strcpy(stuff_message, "");

	if (ent->client->pers.secrets_found & (1 << 0))
		strcpy(stuff_message, va("%s^3\nHoldable Items Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3\nHoldable Items Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 1))
		strcpy(stuff_message, va("%s^3Bounty Hunter Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Bounty Hunter Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 2))
		strcpy(stuff_message, va("%s^3Unique Ability 1 - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Unique Ability 1 - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 3))
		strcpy(stuff_message, va("%s^3Unique Ability 2 - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Unique Ability 2 - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 4))
		strcpy(stuff_message, va("%s^3Unique Ability 3 - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Unique Ability 3 - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 7))
		strcpy(stuff_message, va("%s^3Stealth Attacker Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Stealth Attacker Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 8))
		strcpy(stuff_message, va("%s^3Force Gunner Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Force Gunner Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 9))
		strcpy(stuff_message, va("%s^3Impact Reducer - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Impact Reducer - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 10))
		strcpy(stuff_message, va("%s^3Flame Thrower - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Flame Thrower - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 11))
		strcpy(stuff_message, va("%s^3Power Cell Weapons Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Power Cell Weapons Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 12))
		strcpy(stuff_message, va("%s^3Blaster Pack Weapons Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Blaster Pack Weapons Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 13))
		strcpy(stuff_message, va("%s^3Metal Bolts Weapons Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Metal Bolts Weapons Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 14))
		strcpy(stuff_message, va("%s^3Rocket Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Rocket Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 15))
		strcpy(stuff_message, va("%s^3Stun Baton Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Stun Baton Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 16))
		strcpy(stuff_message, va("%s^3Armored Soldier Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Armored Soldier Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 17))
		strcpy(stuff_message, va("%s^3Jetpack Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Jetpack Upgrade - ^1no\n", stuff_message));

	if (ent->client->pers.secrets_found & (1 << 19))
		strcpy(stuff_message, va("%s^3Force Guardian Upgrade - ^2yes\n", stuff_message));
	else
		strcpy(stuff_message, va("%s^3Force Guardian Upgrade - ^1no\n", stuff_message));

	trap->SendServerCommand(target_ent - g_entities, va("print \"%s\n\"", stuff_message));
}

void list_rpg_info(gentity_t *ent, gentity_t *target_ent)
{ // zyk: lists general RPG info of this player
	trap->SendServerCommand(target_ent->s.number, va("print \"\n^2Account: ^7%s\n^2Char: ^7%s\n\n^3Level: ^7%d/%d\n^3Level Up Score: ^7%d/%d\n^3Skill Points: ^7%d\n^3Skill Counter: ^7%d/%d\n^3Magic Points: ^7%d/%d\n^3Credits: ^7%d\n^3RPG Class: ^7%s\n\n^7Use ^2/list rpg ^7to see console commands\n\n\"", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->pers.level, zyk_rpg_max_level.integer, ent->client->pers.level_up_score, (ent->client->pers.level * zyk_level_up_score_factor.integer), ent->client->pers.skillpoints, ent->client->pers.skill_counter, zyk_max_skill_counter.integer, ent->client->pers.magic_power, zyk_max_magic_power(ent), ent->client->pers.credits, zyk_rpg_class(ent)));
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
			char message[MAX_STRING_CHARS];
			char arg1[MAX_STRING_CHARS];
			int i = 0;

			strcpy(message,"");

			trap->Argv(1, arg1, sizeof( arg1 ));

			if (Q_stricmp( arg1, "rpg" ) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"\n^2/list force: ^7lists force power skills\n^2/list weapons: ^7lists weapon skills\n^2/list other: ^7lists miscellaneous skills\n^2/list ammo: ^7lists ammo skills\n^2/list items: ^7lists holdable items skills\n^2/list special: ^7list special powers got in quests\n^2/list [skill number]: ^7lists info about a skill\n^2/list quests: ^7lists the quests\n^2/list commands: ^7lists the RPG Mode console commands\n^2/list classes: ^7lists the RPG classes\n^2/list stuff: ^7lists stuff bought from the seller\n\n\"");
			}
			else if (Q_stricmp( arg1, "force" ) == 0 || Q_stricmp( arg1, "weapons" ) == 0 || Q_stricmp( arg1, "other" ) == 0 || 
					 Q_stricmp( arg1, "ammo" ) == 0 || Q_stricmp( arg1, "items" ) == 0)
			{
				zyk_list_player_skills(ent, ent, G_NewString(arg1));
			}
			else if (Q_stricmp(arg1, "special") == 0)
			{
				char message_content[8][100];

				while (i < 7)
				{
					strcpy(message_content[i], "");
					i++;
				}
				message_content[7][0] = '\0';
				i = 0;

				if (ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS)
					strcpy(message_content[0], va("%s^3l  ^7- Light Power: ^2yes\n", message_content[0]));
				else
					strcpy(message_content[0], va("%s^3l  ^7- Light Power: ^1no\n", message_content[0]));

				if (ent->client->pers.hunter_quest_progress == NUMBER_OF_OBJECTIVES)
					strcpy(message_content[1], va("%s^3d  ^1- Dark Power: ^2yes\n", message_content[1]));
				else
					strcpy(message_content[1], va("%s^3d  ^1- Dark Power: ^1no\n", message_content[1]));

				if (ent->client->pers.eternity_quest_progress == NUMBER_OF_ETERNITY_QUEST_OBJECTIVES)
					strcpy(message_content[2], va("%s^3e  - Eternity Power: ^2yes\n", message_content[2]));
				else
					strcpy(message_content[2], va("%s^3e  - Eternity Power: ^1no\n", message_content[2]));

				if (ent->client->pers.universe_quest_progress >= 8)
					strcpy(message_content[3], va("%s^3u  ^2- Universe Power: ^2yes\n", message_content[3]));
				else
					strcpy(message_content[3], va("%s^3u  ^2- Universe Power: ^1no\n", message_content[3]));

				if (ent->client->pers.universe_quest_progress >= 14)
					strcpy(message_content[4], va("%s^3!  ^5- Ultimate Power: ^2yes\n", message_content[4]));
				else
					strcpy(message_content[4], va("%s^3!  ^5- Ultimate Power: ^1no\n", message_content[4]));

				if (ent->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES)
					strcpy(message_content[5], va("%s^3r  ^4- Final Power: ^2yes\n", message_content[5]));
				else
					strcpy(message_content[5], va("%s^3r  ^4- Final Power: ^1no\n", message_content[5]));

				if (ent->client->pers.rpg_class == 0 && (ent->client->pers.defeated_guardians & (1 << 11) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 1 && (ent->client->pers.defeated_guardians & (1 << 6) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 5 && (ent->client->pers.defeated_guardians & (1 << 4) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 4 && (ent->client->pers.defeated_guardians & (1 << 9) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 3 && (ent->client->pers.defeated_guardians & (1 << 5) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 6 && (ent->client->pers.defeated_guardians & (1 << 7) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 2 && (ent->client->pers.defeated_guardians & (1 << 10) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 7 && (ent->client->pers.defeated_guardians & (1 << 8) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 9 && (ent->client->pers.defeated_guardians & (1 << 12) ||
					ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS))
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else if (ent->client->pers.rpg_class == 8)
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^2yes\n", message_content[6]));
				else
					strcpy(message_content[6], va("%s^3s  ^6- Magic Powers: ^1no\n", message_content[6]));

				for (i = 0; i < 7; i++)
				{
					strcpy(message, va("%s%s", message, message_content[i]));
				}

				trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", message));
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

						if (player && player->client && player->client->sess.amrpgmode == 2 && player->client->pers.can_play_quest == 1)
						{
							strcpy(quest_player, va("%s", player->client->pers.netname));
						}
						if (player && player->client && player->client->sess.amrpgmode == 2 && level.bounty_quest_choose_target == qfalse && player->s.number == level.bounty_quest_target_id)
						{
							strcpy(target_player, va("%s", player->client->pers.netname));
						}
					}

					trap->SendServerCommand(ent-g_entities, va("print \"\n^3RPG Mode Quests\n\n^2/list universe: ^7Universe Quest (Main Quest)\n\n^2/list light: ^7Light Quest\n^2/list dark: ^7Dark Quest\n^2/list eternity: ^7Eternity Quest\n\n^3/list bounty: ^7The Bounty Hunter quest\n^3/list guardian: ^7The Guardian Quest\n^3/list custom: ^7lists custom quests\n\n^3Quest Player: ^7%s\n^3Bounty Quest Target: ^7%s^7\n\n\"", quest_player, target_player));
				}
				else
					trap->SendServerCommand(ent-g_entities, "print \"\n^3RPG Mode Quests\n\n^1Quests are not allowed in this server^7\n\n\"");
			}
			else if (Q_stricmp( arg1, "light" ) == 0)
			{
				char guardian_message[MAX_STRING_CHARS];
				strcpy(guardian_message, "");

				if (ent->client->pers.defeated_guardians != NUMBER_OF_GUARDIANS)
				{
					if (light_quest_defeated_guardians(ent) == qtrue)
					{
						strcpy(guardian_message, "^3\n2 - Guardian of Light\n\n^7Go to the sacred monument in ^3yavin2^7 and defeat the Guardian of Light.");
					}
					else
					{
						strcpy(guardian_message, "^3\n1 - The Guardians\n\n^7Defeat the guardians in their respective maps\n");

						if (ent->client->pers.defeated_guardians & (1 << 4))
							strcpy(guardian_message, va("%s\n^4Guardian of Water ^7(yavin1b) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^4Guardian of Water ^7(yavin1b) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 5))
							strcpy(guardian_message, va("%s\n^3Guardian of Earth ^7(t1_fatal) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^3Guardian of Earth ^7(t1_fatal) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 6))
							strcpy(guardian_message, va("%s\n^2Guardian of Forest ^7(yavin2) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^2Guardian of Forest ^7(yavin2) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 7))
							strcpy(guardian_message, va("%s\n^5Guardian of Intelligence ^7(t2_rogue) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^5Guardian of Intelligence ^7(t2_rogue) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 8))
							strcpy(guardian_message, va("%s\n^6Guardian of Agility ^7(hoth3) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^6Guardian of Agility ^7(hoth3) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 9))
							strcpy(guardian_message, va("%s\n^1Guardian of Fire ^7(mp/duel5) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^1Guardian of Fire ^7(mp/duel5) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 10))
							strcpy(guardian_message, va("%s\n^7Guardian of Wind ^7(mp/duel9) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^7Guardian of Wind ^7(mp/duel9) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 11))
							strcpy(guardian_message, va("%s\n^3Guardian of Resistance ^7(mp/duel8) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^3Guardian of Resistance ^7(mp/duel8) - ^1no",guardian_message));

						if (ent->client->pers.defeated_guardians & (1 << 12))
							strcpy(guardian_message, va("%s\n^5Guardian of Ice ^7(hoth2) - ^2yes",guardian_message));
						else
							strcpy(guardian_message, va("%s\n^5Guardian of Ice ^7(hoth2) - ^1no",guardian_message));
					}
				}
				else
				{
					strcpy(guardian_message, "^7Completed!");
				}

				strcpy(message, va("\n^5Light Quest\n%s^7\n\n", guardian_message));

				trap->SendServerCommand( ent->s.number, va("print \"%s\"", message) );
			}
			else if (Q_stricmp( arg1, "dark" ) == 0)
			{
				char dark_quest_message[MAX_STRING_CHARS];
				strcpy(dark_quest_message, "");

				if (ent->client->pers.hunter_quest_progress != NUMBER_OF_OBJECTIVES)
				{
					if (dark_quest_collected_notes(ent) == qtrue)
					{
						strcpy(dark_quest_message, "^3\n2 - Guardian of Darkness\n\n^7Defeat the Guardian of Darkness in the dark room in ^3yavin2^7.");
					}
					else
					{
						strcpy(dark_quest_message, "^3\n1 - The Notes\n\n^7Find the notes in their respective maps\n");

						if (ent->client->pers.hunter_quest_progress & (1 << 4))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the temple of the forest - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the temple of the forest - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 5))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in a spaceport of a desert planet - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in a spaceport of a desert planet - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 6))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the desert with the sand people - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the desert with the sand people - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 7))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in a very deep burial location - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in a very deep burial location - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 8))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in a very cold place - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in a very cold place - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 9))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in an abandoned and forgotten city - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in an abandoned and forgotten city - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 10))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the office of a crime lord - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the office of a crime lord - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 11))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the sand worm desert - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the sand worm desert - ^1no",dark_quest_message));

						if (ent->client->pers.hunter_quest_progress & (1 << 12))
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the sith tombs - ^2yes",dark_quest_message));
						else
							strcpy(dark_quest_message, va("%s\n^7Note ^3in the sith tombs - ^1no",dark_quest_message));
					}
				}
				else
				{
					strcpy(dark_quest_message, "^7Completed!");
				}

				strcpy(message, va("\n^1Dark Quest\n^7%s\n\n", dark_quest_message));

				trap->SendServerCommand( ent->s.number, va("print \"%s\"", message) );
			}
			else if (Q_stricmp( arg1, "eternity" ) == 0)
			{
				char eternity_message[512];
				strcpy(eternity_message, "");

				if (ent->client->pers.eternity_quest_progress < NUMBER_OF_ETERNITY_QUEST_OBJECTIVES)
				{
					if (ent->client->pers.eternity_quest_progress < 10)
						strcpy(eternity_message, va("^3\n1 - Riddles\n\n^7Find the ^3riddles ^7near the waterfall in ^3yavin2^7. Use chat to answer the riddle.\nAnswered riddles: ^3%d^7.",ent->client->pers.eternity_quest_progress));
					else if (ent->client->pers.eternity_quest_progress == 10)
						strcpy(eternity_message, "^3\n2 - Guardian of Eternity\n\n^7Defeat the ^3Guardian of Eternity ^7near the waterfall in ^3yavin2^7.");
				}
				else
				{
					strcpy(eternity_message, "^7Completed!");
				}

				strcpy(message, va("\n^3Eternity Quest\n^7%s\n\n", eternity_message));

				trap->SendServerCommand( ent->s.number, va("print \"%s\"", message) );
			}
			else if (Q_stricmp( arg1, "universe" ) == 0)
			{
				char universe_message[MAX_STRING_CHARS];
				strcpy(universe_message, "");

				if (ent->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES)
				{
					if (ent->client->pers.universe_quest_progress == 0)
						strcpy(universe_message, "^3\n1. The Hero's Quest Begins\n\n^7Go to ^3kor1 ^7to save the Guardian Sages.");
					else if (ent->client->pers.universe_quest_progress == 1)
						strcpy(universe_message, "^3\n2. The Rise of an Evil Force\n\n^7Talk to the sages at ^3kor1 ^7to know more about your quest.");
					else if (ent->client->pers.universe_quest_progress == 2)
						strcpy(universe_message, va("^3\n3. The Life-Force Artifacts\n\n^7Find the 8 artifacts in SP maps. One of them is with the sages at ^3yavin1b^7.\nCollected artifacts: ^3%d^7",number_of_artifacts(ent)));
					else if (ent->client->pers.universe_quest_progress == 3)
						strcpy(universe_message, "^3\n4. In Search for Answers\n\n^7Go to ^3yavin1b ^7to talk to the sages about the mysterious voice you heard at the beginning of the quest.");
					else if (ent->client->pers.universe_quest_progress == 4)
						strcpy(universe_message, "^3\n5. The Hidden Sage\n\n^7Find the Sage of Universe at ^3t3_hevil^7.");
					else if (ent->client->pers.universe_quest_progress == 5)
						strcpy(universe_message, va("^3\n6. The Guardian Amulets\n\n^7Find the three guardian amulets at the City of the Merchants in ^3mp/siege_desert^7. Amulets collected: ^3%d^7.", number_of_amulets(ent)));
					else if (ent->client->pers.universe_quest_progress == 6)
						strcpy(universe_message, "^3\n7. The Decisive Battle\n\n^7Defeat the ^1Master of Evil ^7at ^3taspir1^7.");
					else if (ent->client->pers.universe_quest_progress == 7)
						strcpy(universe_message, "^3\n8. The Guardian of Universe\n\n^7Defeat the ^2Guardian of Universe ^7at ^3mp/siege_korriban^7.");
					else if (ent->client->pers.universe_quest_progress == 8)
					{
						strcpy(universe_message, "^3\n9. Revelations\n\n^7You had a strange vision of someone calling you from a sacred place. You must talk to the sages and guardians about it, and you also have to go there.\n");

						if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, va("%s\n^3Guardians and Sages - ^7Go to ^3mp/siege_korriban ^7and speak to the guardians and the sages - ^2yes",universe_message));
						else
							strcpy(universe_message, va("%s\n^3Guardians and Sages - ^7Go to ^3mp/siege_korriban ^7and speak to the guardians and the sages - ^1no",universe_message));

						if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, va("%s\n^3Sacred Monument - ^7Go to ^3t2_trip ^7, at the sacred obelisk, to find out the meaning of the vision - ^2yes",universe_message));
						else
							strcpy(universe_message, va("%s\n^3Sacred Monument - ^7Go to ^3t2_trip ^7, at the sacred obelisk, to find out the meaning of the vision - ^1no",universe_message));
					}
					else if (ent->client->pers.universe_quest_progress == 9)
					{
						strcpy(universe_message, va("^3\n10. The Sacred Crystals\n\n^7Find the sacred crystals in ^3t2_trip^7. You need them to free the Guardian of Time. Crystals: %d\n", number_of_crystals(ent)));
					}
					else if (ent->client->pers.universe_quest_progress == 10)
					{
						strcpy(universe_message, "^3\n11. Finally Free\n\n^7You have the sacred crystals.\nGo to ^3t2_trip ^7and free the Guardian of Time.");
					}
					else if (ent->client->pers.universe_quest_progress == 11)
					{
						strcpy(universe_message, va("^3\n12. Battle for the Temple\n\n^7Master of Evil sent his entire army to the temple.\nDefeat all of them with the help of your allies.\nEnemies: ^3%d^7", ent->client->pers.universe_quest_objective_control));
					}
					else if (ent->client->pers.universe_quest_progress == 12)
					{
						strcpy(universe_message, "^3\n13. Unpleasant Revelation\n\n^7Listen to the revelation that will be decisive to the fate of the Universe.");
					}
					else if (ent->client->pers.universe_quest_progress == 13)
					{
						strcpy(universe_message, "^3\n14. The Hero's Choice\n\n^7Listen to the sages, guardians, Guardian of Time and the Master of Evil.\nAfter that, press the Use key on the one you choose.");
					}
					else if (ent->client->pers.universe_quest_progress == 14)
					{
						if (zyk_number_of_completed_quests(ent) == 3)
							strcpy(universe_message, "^3\n15. The Fate of the Universe\n\n^7Go to the Sacred Dimension in ^3t2_trip^7 to fight the ^1Guardian of Chaos^7.");
						else
							strcpy(universe_message, "^3\nRequirements\n\n^7Complete Light, Dark and Eternity quests.");
					}
					else if (ent->client->pers.universe_quest_progress == 15)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n16. Sages Sequel: The Ancient Threat\n\n^7Sage of Universe telepathically asks you to talk to the sages at the sacred monument in ^3yavin2^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n16. Guardians Sequel: A Smart Move\n\n^7Guardian of Universe telepathically asks you to talk to the guardians at the central area in ^3mp/siege_korriban^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n16. Thor Sequel: The Betrayal\n\n^7Thor (Master of Evil) telepathically asks you to go to ^3t3_bounty^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n16. Time Sequel: The Most Important Task\n\n^7Guardian of Time telepathically asks you to go to ^3mp/siege_korriban^7.");
					}
					else if (ent->client->pers.universe_quest_progress == 16)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n17. Sages Sequel: Save the City!\n\n^7Go to ^3mp/siege_desert^7 and defeat all mages with the help from some citizens.");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n17. Guardians Sequel: The Confrontation\n\n^7Go to ^3t3_bounty^7 and defeat Ymir and Thor.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n17. Thor Sequel: Survival of the Strongest\n\n^7Defeat Ymir in ^3t3_bounty^7 water arena.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n17. Time Sequel: The Well of Truth\n\n^7'The Well of truth, surrounded by the forest and the sacred monument...shall contain the answers to the Universe salvation'.");
					}
					else if (ent->client->pers.universe_quest_progress == 17)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n18. Sages Sequel: The Terrible Truth\n\n^7Talk to the sages inside the mayor's house in ^3mp/siege_desert^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n18. Guardians Sequel: The Hero's Destiny\n\n^7Go to ^3mp/siege_korriban^7 and talk to the guardians in the blue crystal room.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n18. Thor Sequel: The New Leader\n\n^7Talk to Thor in ^3t3_bounty^7 water arena.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n18. Time Sequel: The Legendary Puzzle\n\n^7'The hero shall stand in the buried deep sanctuary...and solve the puzzle to enter the Realm of Souls'.");
					}
					else if (ent->client->pers.universe_quest_progress == 18)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n19. Sages Sequel: To Settle the Score\n\n^7Go to ^3t3_bounty^7 and find Ymir.");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n19. Guardians Sequel: The Guardian Trials\n\n^7Go to ^3mp/siege_korriban^7 and win the Guardian Trials battle.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n19. Thor Sequel: War at the City\n\n^7Defeat the citizens, sages and guardians in ^3mp/siege_desert^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n19. Time Sequel: The Realm of Souls\n\n^7Go through the gate in ^3t3_rift^7 sanctuary, which leads to the Realm of Souls.");
					}
					else if (ent->client->pers.universe_quest_progress == 19)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n20. Sages Sequel: The Crystal of Magic\n\n^7Go to ^3t3_bounty^7 and talk to the sages and guardians where Ymir was");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n20. Guardians Sequel: The Great Moment\n\n^7Go to ^3mp/siege_korriban^7 and talk to the guardians in the blue crystal room.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n20. Thor Sequel: The Path of Evil\n\n^7Go to the rancor arena in ^3mp/siege_desert^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n20. Time Sequel: The Soul of Sorrow\n\n^7Speak to the Soul of Sorrow in the Realm of Souls in ^3t3_rift^7.");
					}
					else if (ent->client->pers.universe_quest_progress == 20)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n21. Sages Sequel: Wrath of the Mages\n\n^7Go to ^3t3_bounty^7 and defeat Ymir and Thor in water arena");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n21. Guardians Sequel: The Final Challenge\n\n^7Go to ^3mp/siege_korriban^7 and win the battle.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n21. Thor Sequel: Victory!\n\n^7Defeat the Guardian of Time in the rancor arena in ^3mp/siege_desert^7.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n21. Time Sequel: The Hero's Test\n\n^7Defeat the Soul of Sorrow in the Realm of Souls in ^3t3_rift^7.");
					}
					else if (ent->client->pers.universe_quest_progress == 21)
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							strcpy(universe_message, "^3\n22. Sages Sequel: A New Prosperous Age\n\n^7Go to ^3t3_bounty^7 and talk to the sages and guardians in water arena");
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							strcpy(universe_message, "^3\n22. Guardians Sequel: The Guardian of Peace\n\n^7Go to ^3mp/siege_korriban^7 and talk to the guardians in the blue crystal room.");
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							strcpy(universe_message, "^3\n22. Thor Sequel: Full Power\n\n^7Go to the rancor arena in ^3mp/siege_desert^7 and talk to Thor.");
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							strcpy(universe_message, "^3\n22. Time Sequel: Salvation of the Universe\n\n^7Speak to the Soul of Sorrow in the Realm of Souls in ^3t3_rift^7.");
					}
				}
				else
				{
					strcpy(universe_message, "^7Completed!");
				}

				strcpy(message, va("\n^2Universe Quest\n%s\n\n", universe_message));

				trap->SendServerCommand( ent->s.number, va("print \"%s\"", message) );
			}
			else if (Q_stricmp( arg1, "bounty" ) == 0)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"\n^3Bounty Quest\n^7Use ^3/bountyquest ^7so the server chooses a player to be the target. If the target defeats a RPG player, he receives 200 bonus credits. If a bounty hunter kills the target, he receives bonus credits based in the target player level.\n\n\"") );
			}
			else if (Q_stricmp( arg1, "guardian" ) == 0)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"\n^3Guardian Quest\n^7Use ^3/guardianquest ^7so the server spawns the map guardian somewhere. If the player defeats it, he gets 3 experience points (Level Up Score) and 1000 credits.\n\n\"") );
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
				trap->SendServerCommand( ent-g_entities, "print \"\n^2RPG Mode commands\n\n^3/new [login] [password]: ^7creates a new account.\n^3/login [login] [password]: ^7loads the account.\n^3/playermode: ^7switches between ^2Admin-Only Mode ^7and ^2RPG Mode^7.\n^3/up [skill number]: ^7upgrades a skill. Passing ^3all ^7as parameter upgrades all skills.\n^3/down [skill number]: ^7downgrades a skill.\n^3/resetaccount: ^7resets account stuff of the player.\n^3/adminlist: ^7lists admin commands.\n^3/adminup [player id or name] [command number]: ^7gives the player an admin command.\n^3/admindown [player id or name] [command number]: ^7removes an admin command from a player.\n^3/settings: ^7turn on or off player settings.\n^3/callseller: ^7calls the jawa seller.\n^3/creditgive [player id or name] [amount]: ^7gives credits to a player.\n^3/changepassword <new_password>: ^7changes the account password.\n^3/tutorial: ^7shows all info about the mod.\n^3/logout: ^7logs out the account.\n\n\"" );
			}
			else if (Q_stricmp( arg1, "classes" ) == 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^30 - Free Warrior\n^7 Can have all 56 skills. All-round class\n^31 - Force User\n^7 Saber/force class. Force powers use less force. Regens force faster\n^32 - Bounty Hunter\n^7 Gun class. Higher max ammo, stronger items and more credits in battles\n^33 - Armored Soldier\n^7 Gun class. High resistance to damage, shot deflection and auto-shield-heal\n^34 - Monk\n^7 Force class. Highest melee damage and faster run speed. Has auto-hp-heal\n^35 - Stealth Attacker\n^7 Gun class. Highest gun damage. Resistant to electric attacks\n^36 - Duelist\n^7 Saber/force class. Highest saber damage. Regens force faster\n^37 - Force Gunner\n^7 Gun/force class. Can do acrobatic moves (like wall run) while holding guns and shooting\n^38 - Magic Master\n^7 Has no saber/force/guns. Shoots magic bolts from melee. Learns all magic powers\n^39 - Force Guardian\n^7 Saber/force class. High resistance to damage. Can grab some guns and items on map\n\n^3/rpgclass <class number>\n\"" );
			}
			else if (Q_stricmp( arg1, "stuff" ) == 0)
			{
				zyk_list_stuff(ent, ent);
			}
			else
			{ // zyk: the player can also list the specific info of a skill passing the skill number as argument
				i = atoi(arg1);

				if (i >= 1 && i <= NUMBER_OF_SKILLS)
				{
					trap->SendServerCommand( ent->s.number, va("print \"^3%s: ^7%s\n\"", zyk_skill_name(i - 1), zyk_skill_description(i - 1)));
				}
				else if (Q_stricmp( arg1, "l" ) == 0)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"^3Light Power: ^7Regens 1 hp per second. Regens shield if hp is full. You must finish ^5Light Quest ^7to have it\n\"") );
				}
				else if (Q_stricmp( arg1, "d" ) == 0)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"^3Dark Power: ^7Increases damage by 10 per cent. You must finish ^1Dark Quest ^7to have it\n\"") );
				}
				else if (Q_stricmp( arg1, "e" ) == 0)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"^3Eternity Power: ^7Absorbs 10 per cent of damage. You must finish the ^3Eternity Quest ^7to have it\n\"") );
				}
				else if (Q_stricmp( arg1, "u" ) == 0)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"^3Universe Power: ^7Increases strength of your magic powers, except ultimate power. You must defeat the ^2Guardian of Universe ^7to have it\n\"") );
				}
				else if (Q_stricmp( arg1, "!" ) == 0)
				{
					if (ent->client->pers.universe_quest_progress < 14)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"^3Ultimate Power: ^7You must finish the 14th mission of ^2Universe Quest ^7to have this power\n\"") );
					}
					else
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							trap->SendServerCommand(ent->s.number, va("print \"^3Ultra Drain: ^7damages enemies in the area and recovers your hp. Attack with S + special melee to use this power\n\"") );
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							trap->SendServerCommand(ent->s.number, va("print \"^3Immunity Power: ^7protects you from other magic powers. Attack S + with special melee to use this power\n\"") );
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							trap->SendServerCommand(ent->s.number, va("print \"^3Chaos Power: ^7damages, stuns, slowers and electrifies enemies. Attack with S + special melee to use this power\n\"") );
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							trap->SendServerCommand(ent->s.number, va("print \"^3Time Power: ^7paralyzes enemies for some seconds. Disables target enemies force powers, force regen, mp regen and hp/shield regen. Increases target enemies magic cooldown of enemies. Enemies take less damage while paralyzed. Attack with S + special melee to use this power\n\"") );
					}
				}
				else if (Q_stricmp( arg1, "r" ) == 0)
				{
					if (ent->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES)
					{
						trap->SendServerCommand(ent->s.number, va("print \"^3Final Power: ^7You must finish ^2Universe Quest ^7to have this power\n\""));
					}
					else
					{
						if (ent->client->pers.universe_quest_counter & (1 << 0))
							trap->SendServerCommand(ent->s.number, va("print \"^3Magic Regen: ^7regens 1 mp per second. Finish Universe Quest to have it\n\""));
						else if (ent->client->pers.universe_quest_counter & (1 << 1))
							trap->SendServerCommand(ent->s.number, va("print \"^3Magic Boost: ^7Decreases cooldown time of magic powers. Finish Universe Quest to have it\n\""));
						else if (ent->client->pers.universe_quest_counter & (1 << 2))
							trap->SendServerCommand(ent->s.number, va("print \"^3Unique Boost: ^7decreases cooldown time of unique skill and unique abilities. Finish Universe Quest to have it\n\""));
						else if (ent->client->pers.universe_quest_counter & (1 << 3))
							trap->SendServerCommand(ent->s.number, va("print \"^3Magic Improvement: ^7makes Universe Power have no additional mp cost. Finish Universe Quest to have it\n\""));
					}
				}
				else if (Q_stricmp( arg1, "s" ) == 0)
				{
					if (ent->client->pers.rpg_class == 0)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Ultra Resistance: ^7increases resistance to damage. Attack with D + special melee to use this power. MP cost: %d\n^3Ultra Strength: ^7increases damage. Attack with A + special melee to use this power. MP cost: %d\n^3Enemy Weakening: ^7decreases damage and resistance of enemies nearby. Attack with W + special melee to use this power. MP cost: %d\n\n\"", zyk_ultra_resistance_mp_cost.integer, zyk_ultra_strength_mp_cost.integer, zyk_enemy_nerf_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 1)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Sleeping Flowers: ^7knocks down enemies for some seconds. Attack with D + special melee to use this power. MP cost: %d\n^3Poison Mushrooms: ^7keep damaging the enemies for some time. Attack with A + special melee to use this power. MP cost: %d\n^3Tree of Life: ^7a big tree appears, protecting you from attacks and healing you. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_sleeping_flowers_mp_cost.integer, zyk_poison_mushrooms_mp_cost.integer, zyk_tree_of_life_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 2)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Blowing Wind: ^7blows people away for some seconds. Attack with D + special melee to use this power. MP cost: %d\n^3Hurricane: ^7makes enemies fly up like if they were inside a tornado. Attack with A + special melee to use this power. MP cost: %d\n^3Reverse Wind: ^7makes people go towards you. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_blowing_wind_mp_cost.integer, zyk_hurricane_mp_cost.integer, zyk_reverse_wind_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 3)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Earthquake: ^7knocks people down causing damage. Attack with D + special melee to use this power. MP cost: %d\n^3Rockfall: ^7rocks keep falling at the enemies. Attack with A + special melee to use this power. MP cost: %d\n^3Shifting Sand: ^7a shifting sand appears, sending you to your nearest enemy. Stand near the sand to be transported to the enemy. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_earthquake_mp_cost.integer, zyk_rockfall_mp_cost.integer, zyk_shifting_sand_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 4)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Flame Burst: ^7fires a flame burst for some seconds. Attack with D + special melee to use this power. MP cost: %d\n^3Ultra Flame: ^7a flame jet appears at the enemies and damages them. Attack with A + special melee to use this power. MP cost: %d\n^3Flaming Area: ^7creates a big area of flames around you, with high damage to enemies. Makes targets who touch the flames catch fire for some seconds. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_flame_burst_mp_cost.integer, zyk_ultra_flame_mp_cost.integer, zyk_flaming_area_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 5)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Healing Water: ^7instantly recovers some hp. Attack with D + special melee to use this power. MP cost: %d\n^3Water Splash: ^7damages enemies, draining their hp and healing you. Attack with A + special melee to use this power. MP cost: %d\n^3Water Attack: ^7attacks enemies nearby with water, with high damage. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_healing_water_mp_cost.integer, zyk_water_splash_mp_cost.integer, zyk_water_attack_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 6)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Magic Shield: ^7creates a shield that makes you take very little dmage from enemies for a short time. Also protects from Push, Pull and Grip force powers. Attack with D + special melee to use this power. MP cost: %d\n^3Dome of Damage: ^7an energy dome appears at enemies, damaging anyone inside the dome. Attack with A + special melee to use this power. MP cost: %d\n^3Magic Disable: ^7makes enemies unable to use magic powers for some seconds. Not so effective against magic using npcs, like bosses. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_magic_shield_mp_cost.integer, zyk_dome_of_damage_mp_cost.integer, zyk_magic_disable_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 7)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Ultra Speed: ^7increases your run speed. Attack with D + special melee to use this power. MP cost: %d\n^3Slow Motion: ^7decreases the run speed of enemies nearby. Attack with A + special melee to use this power. MP cost: %d\n^3Fast and Slow: ^7increases your speed and decreases enemies speed, with less duration than the other two magic powers. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_ultra_speed_mp_cost.integer, zyk_slow_motion_mp_cost.integer, zyk_fast_and_slow_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 8)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Magic Sense: ^7similar to Sense and Sense Health skills, but with less duration. Benefits from Sense, Sense Health and Improvements skill levels. MP cost: %d\n^3Healing Area: ^7creates an energy area that heals you and your allies and damage enemies. MP cost: %d\n^3Lightning Dome: ^7creates a dome that does lightning damage. Damage is based on player level. MP cost: %d\n^3Magic Explosion: ^7creates an explosion that does a lot of damage. MP cost: %d\n\nThis class can use any of the Light Quest special powers. Use A, W or D and melee kata to use a power. You can set each of A, W and D powers with the force power keys (usually the F3, F4, F5, F6, F7 and F8 keys)\n\"", zyk_magic_sense_mp_cost.integer, zyk_healing_area_mp_cost.integer, zyk_lightning_dome_mp_cost.integer, zyk_magic_explosion_mp_cost.integer) );
					else if (ent->client->pers.rpg_class == 9)
						trap->SendServerCommand( ent-g_entities, va("print \"^3Ice Boulder: ^7creates a boulder that damages and traps enemies nearby for some seconds. Attack with D + special melee to use this power. MP cost: %d\n^3Ice Stalagmite: ^7greatly damages enemies nearby with a stalagmite. Attack with A + special melee to use this power. MP cost: %d\n^3Ice Block: ^7creates a block of ice around you, protecting you from attacks and increasing your resistance to damage. Attack with W + special melee to use this power. MP cost: %d\n\"", zyk_ice_boulder_mp_cost.integer, zyk_ice_stalagmite_mp_cost.integer, zyk_ice_block_mp_cost.integer) );
				}
				else
				{
					trap->SendServerCommand( ent-g_entities, "print \"Invalid skill number.\n\"" );
				}
			}
		}
	}
	else if (ent->client->sess.amrpgmode == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"\n^2Admin-Only Mode commands\n\n^3/new [login] [password]: ^7creates a new account.\n^3/login [login] [password]: ^7loads the account of the player.\n^3/playermode: ^7switches between the ^2Admin-Only Mode ^7and the ^2RPG Mode^7.\n^3/adminlist: ^7lists admin commands.\n^3/adminup [player id or name] [admin command number]: ^7gives the player a new admin command.\n^3/admindown [player id or name] [admin command number]: ^7removes an admin command from the player.\n^3/settings: ^7turn on or off player settings.\n^3/changepassword <new_password>: ^7changes the account password.\n^3/tutorial: ^7shows all info about the mod.\n^3/logout: ^7logs out the account.\n\n\"" );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"\n^1Account System\n\n^7The account system has 2 modes:\n^2Admin-Only Mode: ^7allows you to use admin commands\n^2RPG Mode: ^7allows you to use admin commands and to play the Level System\n\n^7Create a new account with ^3/new <login> <password>\n^7where login and password are of your choice.\n\n\"" );
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

/*
==================
Cmd_Stuff_f
==================
*/
void Cmd_Stuff_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{ // zyk: shows the categories of stuff
		trap->SendServerCommand( ent-g_entities, "print \"\n^7Use ^2/stuff <category> ^7to buy or sell stuff\nThe Category may be ^3ammo^7, ^3items^7, ^3misc^7, ^3weapons ^7or ^3upgrades\n^7Use ^3/stuff <number> ^7to see info about the item\n\n^7Use ^2/buy <number> ^7to buy or ^2/sell <number> ^7to sell\nStuff bought from ^3upgrades ^7category are permanent\n\n\"");
		return;
	}
	else
	{
		char arg1[1024];
		int i = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		i = atoi(arg1);

		if (Q_stricmp(arg1, "ammo" ) == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^31 - Blaster Pack: ^7Buy: 15 - Sell: 10\n^32 - Power Cell: ^7Buy: 20 - Sell: 15\n^33 - Metal Bolts: ^7Buy: 25 - Sell: 20\n^34 - Rockets: ^7Buy: 40 - Sell: 30\n^35 - Thermals: ^7Buy: 80 - Sell: 35\n^36 - Trip Mines: ^7Buy: 120 - Sell: 40\n^37 - Det Packs: ^7Buy: 150 - Sell: 45\n^330 - Flame Thrower Fuel: ^7Buy: 200 - Sell: ^1no\n^348 - Ammo All: ^7Buy: 700 - Sell: ^1no^7\n\n\"");
		}
		else if (Q_stricmp(arg1, "items" ) == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^39 - Shield Booster: ^7Buy: 150 - Sell: ^1no\n^310 - Sentry Gun: ^7Buy: 170 - Sell: 60\n^311 - Seeker Drone: ^7Buy: 180 - Sell: 65\n^312 - Big Bacta: ^7Buy: 200 - Sell: 70\n^313 - Force Field: ^7Buy: 300 - Sell: 80\n^334 - Bacta Canister: ^7Buy: 100 - Sell: 20\n^335 - E-Web: ^7Buy: 150 - Sell: 30\n^338 - Binoculars: ^7Buy: 10 - Sell: 5\n^341 - Jetpack: ^7Buy: 50 - Sell: ^1no\n^342 - Cloak Item: ^7Buy: 50 - Sell: 20\n\n\"");
		}
		else if (Q_stricmp(arg1, "misc") == 0)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\n^314 - Ysalamiri: ^7Buy: 200 - Sell: 50\n^331 - Jetpack Fuel: ^7Buy: 200 - Sell: ^1no\n^343 - Force Boon: ^7Buy: 200 - Sell: 50\n^344 - Magic Potion: ^7Buy: 50 - Sell: ^1no\n^349 - Saber Armor: ^7Buy: 2000 - Sell: ^1no\n^350 - Gun Armor: ^7Buy: 2000 - Sell: ^1no\n^351 - Healing Crystal: ^7Buy: 2000 - Sell: ^1no\n^352 - Energy Crystal: ^7Buy: 2000 - Sell: ^1no\n^356 - Book of Riddles: ^7Buy: 100000 - Sell: ^1no^7\n\n\"");
		}
		else if (Q_stricmp(arg1, "weapons" ) == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^317 - E11 Blaster Rifle: ^7Buy: 100 - Sell: 50\n^318 - Disruptor: ^7Buy: 120 - Sell: 60\n^319 - Repeater: ^7Buy: 150 - Sell: 70\n^320 - Rocket Launcher: ^7Buy: 200 - Sell: 100\n^321 - Bowcaster: ^7Buy: 110 - Sell: 50\n^322 - Blaster Pistol: ^7Buy: 90 - Sell: 45\n^323 - Flechette: ^7Buy: 170 - Sell: 90\n^324 - Concussion Rifle: ^7Buy: 300 - Sell: 150\n^332 - Stun Baton: ^7Buy: 20 - Sell: 10\n^336 - DEMP2: ^7Buy: 150 - Sell: 90\n^337 - Bryar Pistol: ^7Buy: 90 - Sell: 45^7\n\n\"");
		}
		else if (Q_stricmp(arg1, "upgrades" ) == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^38 - Stealth Attacker Upgrade: ^7Buy: 5000\n^315 - Impact Reducer: ^7Buy: 4000\n^316 - Flame Thrower: ^7Buy: 3000\n^325 - Power Cell Weapons Upgrade: ^7Buy: 2000\n^326 - Blaster Pack Weapons Upgrade: ^7Buy: 1800\n^327 - Metal Bolts Weapons Upgrade: ^7Buy: 2200\n^328 - Rocket Upgrade: ^7Buy: 2500\n^329 - Bounty Hunter Upgrade: ^7Buy: 5000\n^333 - Stun Baton Upgrade: ^7Buy: 1500\n^339 - Armored Soldier Upgrade: ^7Buy: 5000\n^340 - Holdable Items Upgrade: ^7Buy: 3000\n^345 - Force Gunner Upgrade: ^7Buy: 5000\n^346 - Jetpack Upgrade: ^7Buy: 10000\n^347 - Force Guardian Upgrade: ^7Buy: 5000\n^353 - Unique Ability 1: ^7Buy: 7000\n^354 - Unique Ability 2: ^7Buy: 7000\n^355 - Unique Ability 3: ^7Buy: 7000\n\n\"");
		}
		else if (i == 1)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Blaster Pack: ^7recovers 100 ammo of E11 Blaster Rifle, Blaster Pistol and Bryar Pistol weapons\n\n\"");
		}
		else if (i == 2)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Power Cell: ^7recovers 100 ammo of Disruptor, Bowcaster and DEMP2 weapons\n\n\"");
		}
		else if (i == 3)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Metal Bolts: ^7recovers 100 ammo of Repeater, Flechette and Concussion Rifle weapons\n\n\"");
		}
		else if (i == 4)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Rockets: ^7recovers 10 ammo of Rocket Launcher weapon\n\n\"");
		}
		else if (i == 5)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Thermals: ^7recovers 4 ammo of thermals\n\n\"");
		}
		else if (i == 6)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Trip Mines: ^7recovers 3 ammo of trip mines\n\n\"");
		}
		else if (i == 7)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Det Packs: ^7recovers 2 ammo of det packs\n\n\"");
		}
		else if (i == 8)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Stealth Attacker Upgrade: ^7Stealth Attacker will be invulnerable to electric attacks and will have 20 per cent more damage in his starting weapons. Protects from stun baton speed decrease and also from losing guns to force pull. Adds the thermal detector in his disruptor scope. Makes him invisible to the Bounty Hunter Upgrade Radar\n\n\"");
		}
		else if (i == 9)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Shield Booster: ^7recovers 50 shield\n\n\"");
		}
		else if (i == 10)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Sentry Gun: ^7portable gun which is placed in the ground and shoots nearby enemies\n\n\"");
		}
		else if (i == 11)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Seeker Drone: ^7portable remote drone that shoots enemies at sight\n\n\"");
		}
		else if (i == 12)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Big Bacta: ^7recovers 50 HP\n\n\"");
		}
		else if (i == 13)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Force Field: ^7creates a force field wall in front of the player that can hold almost any attack, except the concussion rifle alternate fire, which can get through\n\n\"");
		}
		else if (i == 14)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Ysalamiri: ^7disables the player force powers but also protects the player from enemy force powers\n\n\"");
		}
		else if (i == 15)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Impact Reducer: ^7reduces the knockback of some weapons attacks by 80 per cent\n\n\"");
		}
		else if (i == 16)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Flame Thrower: ^7gives you the flame thrower. To use it, get stun baton and use alternate fire\n\n\"");
		}
		else if (i == 17)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3E11 Blaster Rifle: ^7Rifle that is used by the stormtroopers. Uses blaster pack ammo\n\n\"");
		}
		else if (i == 18)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Disruptor: ^7Sniper rifle which can desintegrate the enemy. Uses power cell ammo\n\n\"");
		}
		else if (i == 19)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Repeater: ^7imperial weapon that shoots orbs and a plasma bomb with alt fire. Uses metal bolts ammo\n\n\"");
		}
		else if (i == 20)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Rocket Launcher: ^7weapon that shoots rockets and a homing missile with alternate fire. Uses rockets ammo\n\n\"");
		}
		else if (i == 21)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bowcaster: ^7weapon that shoots green bolts, normal fire can be charged, and alt fire shoots a bouncing bolt. Uses power cell ammo\n\n\"");
		}
		else if (i == 22)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Blaster Pistol: ^7pistol that can shoot a charged shot with alt fire. Uses blaster pack ammo\n\n\"");
		}
		else if (i == 23)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Flechette: ^7it is the shotgun of the game, and can shoot 2 bombs with alt fire. Uses metal bolts ammo\n\n\"");
		}
		else if (i == 24)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Concussion Rifle: ^7powerful weapon, alt fire can shoot a beam that gets through force fields. Uses metal bolts ammo\n\n\"");
		}
		else if (i == 25)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Power Cell Weapons Upgrade: ^7increases damage by 12 percent and other features of weapons that use power cell ammo\n\n\"");
		}
		else if (i == 26)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Blaster Pack Weapons Upgrade: ^7increases damage by 12 percent and other features of weapons that use blaster pack ammo\n\n\"");
		}
		else if (i == 27)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Metal Bolts Weapons Upgrade: ^7increases damage by 12 percent and other features of weapons that use metal bolts ammo. Concussion rifle can break saber-only damage objects and can move pushable/pullable objects\n\n\"");
		}
		else if (i == 28)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Rocket Upgrade: ^7increases rocket launcher damage by 12 percent which uses rockets as ammo. Makes rockets and detpacks damage saber-only damage objects and move pushable/pullable objects\n\n\"");
		}
		else if (i == 29)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bounty Hunter Upgrade: ^7increases Bounty Hunter resistance to damage by 7 per cent. Seeker Drone lasts 20 seconds more, has fast shooting rate and more damage. Allows getting seeker drone back with Saber Style key. Sentry Gun has more damage and range. Allows placing more sentry guns and recovering them by pressing Use key on each one. Allows recovering force fields by pressing Use key on them. Allows buying and selling from seller remotely, so no need to call him. Gives the Thermal Vision, used with Binoculars. Gives the Radar (requires Zyk OpenJK Client installed)\n\n\"");
		}
		else if (i == 30)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Flame Thrower Fuel: ^7recovers all fuel of the flame thrower\n\n\"");
		}
		else if (i == 31)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Jetpack Fuel: ^7recovers all fuel of the jetpack\n\n\"");
		}
		else if (i == 32)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Stun Baton: ^7weapon that fires a small electric charge\n\n\"");
		}
		else if (i == 33)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Stun Baton Upgrade: ^7allows stun baton to open any door, including locked ones, move elevators, and move or destroy other objects. Also makes stun baton decloak enemies and decrease their running speed for some seconds\n\n\"");
		}
		else if (i == 34)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bacta Canister: ^7recovers 25 HP\n\n\"");
		}
		else if (i == 35)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3E-Web: ^7portable emplaced gun\n\n\"");
		}
		else if (i == 36)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3DEMP2: ^7fires an electro magnetic pulse that causes bonus damage against droids. Uses power cell ammo\n\n\"");
		}
		else if (i == 37)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Bryar Pistol: ^7similar to blaster pistol, but has a faster fire rate with normal fire. Uses blaster pack ammo\n\n\"");
		}
		else if (i == 38)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Binoculars: ^7allows the player to see far things better with the zoom feature\n\n\"");
		}
		else if (i == 39)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Armored Soldier Upgrade: ^7increases damage resistance by 5 per cent, deflects some gun shots, cuts flame thrower fuel usage by half, has less chance of losing gun to force pull, has a chance of setting ysalamiri for some seconds if attacked by force powers. It also protects from drowning\n\n\"");
		}
		else if (i == 40)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Holdable Items Upgrade: ^7Bacta Canister recovers all Magic Power, Big Bacta recovers more HP, Force Field resists more and Cloak Item will be able to cloak vehicles\n\n\"");
		}
		else if (i == 41)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Jetpack: ^7allows the player to fly. Jump and press Use Key to use it\n\n\"");
		}
		else if (i == 42)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Cloak Item: ^7allows the player to cloak himself\n\n\"");
		}
		else if (i == 43)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Force Boon: ^7allows the player to regenerate force faster\n\n\"");
		}
		else if (i == 44)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Magic Potion: ^7recovers all Magic Power\n\n\"");
		}
		else if (i == 45)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Force Gunner Upgrade: ^7increases run speed by 20 per cent. Force power regens 2x faster. Unique Skill will restore 25 shield\n\n\"");
		}
		else if (i == 46)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Jetpack Upgrade: ^7decreases jetpack fuel consumption a bit and makes the jetpack more stable and faster\n\n\"");
		}
		else if (i == 47)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Force Guardian Upgrade: ^7increases damage resistance. Saber can no longer be dropped out of hand. Decreases knockback a bit\n\n\"");
		}
		else if (i == 48)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Ammo All: ^7recovers all ammo types, including flame thrower fuel\n\n\"");
		}
		else if (i == 49)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Saber Armor: ^7increases damage resistance to saber attacks. If the player dies, he loses the armor\n\n\"");
		}
		else if (i == 50)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Gun Armor: ^7increases damage resistance to gun attacks and melee attacks. If the player dies, he loses the armor\n\n\"");
		}
		else if (i == 51)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Healing Crystal: ^7regens hp, mp and force. If the player dies, he loses the crystal\n\n\"");
		}
		else if (i == 52)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3Energy Crystal: ^7regens shield, blaster pack ammo and power cell ammo. If the player dies, he loses the crystal\n\n\"");
		}
		else if (i == 53)
		{
			if (ent->client->pers.rpg_class == 0)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Free Warrior gets Mimic Damage. If you take damage, does part of the damage back to the enemy. Spends 50 force and 25 mp\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 1)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Force User gets Force Maelstrom, which grips enemies nearby, damages them, sets force shield and uses lightning if player has the force power. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 2)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Bounty Hunter gets Homing Rocket, which shoots a powerful rocket that automatically goes after the nearest target. Spends 2 rockets and 2 power cell ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 3)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Armored Soldier gets the Lightning Shield, which increases resistance to damage and does a bit of damage to enemies nearby. Using /unique again will release a small lightning dome. Spends 5 power cell ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 4)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Monk gets Meditation Strength, which increases auto-healing, force regen, and his own resistance is heavily increased. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 5)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Stealth Attacker gets Ultra Cloak, which makes him completely invisible. Spends 5 power cell ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 6)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Duelist gets Impale Stab, which hits the enemy with his saber doing a lot of damage. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 7)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Force Gunner gets Thermal Throw, which throws 3 thermal detonators with higher damage. Spends 3 thermals and 3 power cell ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 8)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Magic Master gets Faster Bolts, which increases speed and firerate of magic bolts\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 9)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 1: ^7used with /unique command. You can only have one Unique Ability at a time. Force Guardian gets Force Armor, which activates his resistance shield, with damage resistance, gun shot deflection, and ability to resist force powers. Spends 50 force\n\n\"");
			}
		}
		else if (i == 54)
		{
			if (ent->client->pers.rpg_class == 0)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Free Warrior gets Super Beam, a powerful beam with high damage. Spends 100 force and 25 mp\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 1)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Force User gets Force Repulse, which damages and pushes everyone away from you. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 2)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Bounty Hunter gets Wrist Shot, which allows shooting up to five powerful blaster shots. Spends 5 blaster pack ammo and 5 more per shot\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 3)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Armored Soldier gets Shield to Ammo, which recovers some ammo by spending his shield. Spends 20 shield\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 4)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Monk gets Spin Kick ability. Kicks everyone around the Monk with very high damage. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 5)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Stealth Attacker gets Timed Bomb, which places a powerful bomb that explodes after some seconds. Spends 5 power cell ammo and 5 metal bolts ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 6)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Duelist gets Vertical DFA, which makes him jump and hit the ground with the saber, with high damage, and creating a powerful shockwave that damages enemies. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 7)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Force Gunner gets No Attack, which makes the nearby enemies not able to attack for some seconds. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 8)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Magic Master gets Elemental Attack, a magic power that hits enemies with the power of the elements. Spends 20 mp\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 9)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 2: ^7used with /unique command. You can only have one Unique Ability at a time. Force Guardian gets Force Scream, which sets the resistance shield during 6 seconds. Player makes a scream that damages nearby enemies and may cause stun anim on them. Spends 50 force\n\n\"");
			}
		}
		else if (i == 55)
		{
			if (ent->client->pers.rpg_class == 0)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Free Warrior gets Flee to Safety, which sets an area in the map to where the player will be transported to after using /unique again. Spends 50 force and 20 mp\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 1)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Force User gets Force Storm, which protects you with Force Shield and attacks enemies nearby with powerful lightning strikes. The strikes slows down enemies and disable jetpack and cloak item. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 2)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Bounty Hunter gets Ice Bomb, which places a bomb in the map, which is detonated by using /unique again. The bomb then spills ice on the ground, making enemies stuck in it. Spends 1 det pack and 10 power cell ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 3)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Armored Soldier gets Faster E11, which makes E11 Blaster Rifle have a faster alt firerate. Spends 5 blaster pack ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 4)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Monk gets Meditation Drain, which heavily increases resistance and drains shield and health from enemies nearby to restore health and shield. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 5)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Stealth Attacker gets Aimed Shot, in which he aims with the disruptor rifle and fires a charged shot with 100 per cent accuracy. Spends 30 power cell ammo\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 6)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Duelist gets Spin Throw, which spins the saber and does a slash attack, doing a lot of damage. If used again during the first spin slash move, does a spin throw attack. Spends 50 force\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 7)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Force Gunner gets Fast Dash, which makes him do a dash towards where he is looking at. If he hits someone, damages and knocks the target down. Spends 50 force and 10 mp\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 8)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Magic Master gets Healing Improvement, which makes Healing Area restore more hp, shield, force, has more damage, and longer duration. Spends 15 mp\n\n\"");
			}
			else if (ent->client->pers.rpg_class == 9)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n^3Unique Ability 3: ^7used with /unique command. You can only have one Unique Ability at a time. Force Guardian gets Force Attraction, which damages and pulls enemies towards the user. Spends 50 force\n\n\"");
			}
		}
		else if (i == 56)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\n^3Book of Riddles: ^7a legendary book that shows the answers to the riddles created by the Guardian of Eternity\n\n\"");
		}
	}
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
	int item_costs[NUMBER_OF_SELLER_ITEMS] = {15,20,25,40,80,120,150,5000,150,170,180,200,300,200,4000,3000,100,120,150,200,110,90,170,300,2000,1800,2200,2500,5000,200,200,20,1500,100,150,150,90,10,5000,3000,50,50,200,50,5000,10000,5000,700,2000,2000,2000,2000,7000,7000,7000,100000};

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

	if (ent->client->pers.quest_power_status & (1 << 2))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot buy while frozen by Time Power.\n\"");
		return;
	}

	if (value < 1 || value > NUMBER_OF_SELLER_ITEMS)
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

		if (found == 0 && !(ent->client->pers.rpg_class == 2 && ent->client->pers.secrets_found & (1 << 1)))
		{ // zyk: Bounty Hunter Upgrade allows buying and selling without the need to call the jawa seller
			trap->SendServerCommand(ent->s.number, "print \"You must be near the jawa seller to buy from him.\n\"" );
			return;
		}
	}

	// zyk: class validations. Some items require certain conditions to be bought
	if (ent->client->pers.rpg_class == 1 && ((value >= 1 && value <= 7) || (value >= 10 && value <= 13) || 
		(value >= 17 && value <= 24) || (value >= 34 && value <= 38) || (value >= 41 && value <= 42) || value == 48))
	{
		trap->SendServerCommand(ent->s.number, va("print \"%s can't buy this item.\n\"", zyk_rpg_class(ent)));
		return;
	}
	else if (ent->client->pers.rpg_class == 4 && ((value >= 1 && value <= 7) || (value >= 10 && value <= 13) || 
			(value >= 17 && value <= 24) || (value >= 34 && value <= 38) || (value >= 41 && value <= 42) || value == 48))
	{
		trap->SendServerCommand(ent->s.number, va("print \"%s can't buy this item.\n\"", zyk_rpg_class(ent)));
		return;
	}
	else if (ent->client->pers.rpg_class == 6 && ((value >= 1 && value <= 7) || (value >= 10 && value <= 13) || 
			(value >= 17 && value <= 24) || (value >= 34 && value <= 38) || (value >= 41 && value <= 42) || value == 48))
	{
		trap->SendServerCommand(ent->s.number, va("print \"%s can't buy this item.\n\"", zyk_rpg_class(ent)));
		return;
	}
	else if (ent->client->pers.rpg_class == 8 && ((value >= 1 && value <= 7) || (value >= 10 && value <= 13) || 
			(value >= 17 && value <= 24) || (value >= 35 && value <= 38) || value == 48))
	{
		trap->SendServerCommand(ent->s.number, va("print \"%s can't buy this item.\n\"", zyk_rpg_class(ent)));
		return;
	}
	else if (ent->client->pers.rpg_class == 9 && ((value >= 2 && value <= 7) || (value >= 10 && value <= 11) || value == 13 ||
		(value >= 18 && value <= 24) || (value >= 34 && value <= 38) || value == 48))
	{
		trap->SendServerCommand(ent->s.number, va("print \"%s can't buy this item.\n\"", zyk_rpg_class(ent)));
		return;
	}

	// zyk: general validations. Some items require certain conditions to be bought
	if (value == 8 && ent->client->pers.secrets_found & (1 << 7))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Stealth Attacker Upgrade.\n\"" );
		return;
	}
	else if (value == 15 && ent->client->pers.secrets_found & (1 << 9))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Impact Reducer.\n\"" );
		return;
	}
	else if (value == 16 && ent->client->pers.secrets_found & (1 << 10))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Flame Thrower.\n\"" );
		return;
	}
	else if (value == 25 && ent->client->pers.secrets_found & (1 << 11))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Power Cell Weapons Upgrade.\n\"" );
		return;
	}
	else if (value == 26 && ent->client->pers.secrets_found & (1 << 12))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Blaster Pack Weapons Upgrade.\n\"" );
		return;
	}
	else if (value == 27 && ent->client->pers.secrets_found & (1 << 13))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Metal Bolts Weapons Weapons Upgrade.\n\"" );
		return;
	}
	else if (value == 28 && ent->client->pers.secrets_found & (1 << 14))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Rocket Upgrade.\n\"" );
		return;
	}
	else if (value == 29 && ent->client->pers.secrets_found & (1 << 1))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Bounty Hunter Upgrade.\n\"" );
		return;
	}
	else if (value == 33 && ent->client->pers.secrets_found & (1 << 15))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Stun Baton Upgrade.\n\"" );
		return;
	}
	else if (value == 39 && ent->client->pers.secrets_found & (1 << 16))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Armored Soldier Upgrade.\n\"" );
		return;
	}
	else if (value == 40 && ent->client->pers.secrets_found & (1 << 0))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Holdable Items Upgrade.\n\"" );
		return;
	}
	else if (value == 45 && ent->client->pers.secrets_found & (1 << 8))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Force Gunner Upgrade.\n\"" );
		return;
	}
	else if (value == 46 && ent->client->pers.secrets_found & (1 << 17))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Jetpack Upgrade.\n\"");
		return;
	}
	else if (value == 47 && ent->client->pers.secrets_found & (1 << 19))
	{
		trap->SendServerCommand( ent-g_entities, "print \"You already have the Force Guardian Upgrade.\n\"" );
		return;
	}
	else if (value == 53 && ent->client->pers.secrets_found & (1 << 2))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Unique Ability 1.\n\"");
		return;
	}
	else if (value == 54 && ent->client->pers.secrets_found & (1 << 3))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Unique Ability 2.\n\"");
		return;
	}
	else if (value == 55 && ent->client->pers.secrets_found & (1 << 4))
	{
		trap->SendServerCommand(ent - g_entities, "print \"You already have the Unique Ability 3.\n\"");
		return;
	}

	// zyk: buying the item if player has enough credits
	if (ent->client->pers.credits >= item_costs[value-1])
	{
		if (value == 1)
		{
			Add_Ammo(ent,AMMO_BLASTER,100);
		}
		else if (value == 2)
		{
			Add_Ammo(ent,AMMO_POWERCELL,100);
		}
		else if (value == 3)
		{
			Add_Ammo(ent,AMMO_METAL_BOLTS,100);
		}
		else if (value == 4)
		{
			Add_Ammo(ent,AMMO_ROCKETS,10);
		}
		else if (value == 5)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent,AMMO_THERMAL,4);
		}
		else if (value == 6)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent,AMMO_TRIPMINE,3);
		}
		else if (value == 7)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent,AMMO_DETPACK,2);
		}
		else if (value == 8)
		{
			ent->client->pers.secrets_found |= (1 << 7);

			// zyk: update the rpg stuff info at the client-side game
			send_rpg_events(10000);
		}
		else if (value == 9)
		{
			if (ent->client->ps.stats[STAT_ARMOR] < ent->client->pers.max_rpg_shield)
			{ // zyk: RPG Mode has the Max Shield skill that doesnt allow someone to heal shields above this value
				ent->client->ps.stats[STAT_ARMOR] += 50;
			}

			if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield)
				ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
		}
		else if (value == 10)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);
			if (ent->client->pers.rpg_class == 2 && ent->client->pers.bounty_hunter_sentries < MAX_BOUNTY_HUNTER_SENTRIES)
				ent->client->pers.bounty_hunter_sentries++;
		}
		else if (value == 11)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);
		}
		else if (value == 12)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);
		}
		else if (value == 13)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);
		}
		else if (value == 14)
		{
			if (ent->client->ps.powerups[PW_YSALAMIRI] < level.time)
				ent->client->ps.powerups[PW_YSALAMIRI] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_YSALAMIRI] += 60000;
		}
		else if (value == 15)
		{
			ent->client->pers.secrets_found |= (1 << 9);
		}
		else if (value == 16)
		{
			ent->client->pers.secrets_found |= (1 << 10);
		}
		else if (value == 17)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
		}
		else if (value == 18)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);
		}
		else if (value == 19)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);
		}
		else if (value == 20)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);
		}
		else if (value == 21)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);
		}
		else if (value == 22)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		}
		else if (value == 23)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);
		}
		else if (value == 24)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);
		}
		else if (value == 25)
		{
			ent->client->pers.secrets_found |= (1 << 11);
		}
		else if (value == 26)
		{
			ent->client->pers.secrets_found |= (1 << 12);
		}
		else if (value == 27)
		{
			ent->client->pers.secrets_found |= (1 << 13);
		}
		else if (value == 28)
		{
			ent->client->pers.secrets_found |= (1 << 14);
		}
		else if (value == 29)
		{
			ent->client->pers.secrets_found |= (1 << 1);

			// zyk: update the rpg stuff info at the client-side game
			send_rpg_events(10000);
		}
		else if (value == 30)
		{
			ent->client->ps.cloakFuel = 100;
		}
		else if (value == 31)
		{
			ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
			ent->client->ps.jetpackFuel = 100;
		}
		else if (value == 32)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);
		}
		else if (value == 33)
		{
			ent->client->pers.secrets_found |= (1 << 15);
		}
		else if (value == 34)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);
		}
		else if (value == 35)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);
		}
		else if (value == 36)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);
		}
		else if (value == 37)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);
		}
		else if (value == 38)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);
		}
		else if (value == 39)
		{
			ent->client->pers.secrets_found |= (1 << 16);
		}
		else if (value == 40)
		{
			ent->client->pers.secrets_found |= (1 << 0);
		}
		else if (value == 41)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
		}
		else if (value == 42)
		{
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);
		}
		else if (value == 43)
		{
			if (ent->client->ps.powerups[PW_FORCE_BOON] < level.time)
				ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_FORCE_BOON] += 60000;
		}
		else if (value == 44)
		{
			ent->client->pers.magic_power = zyk_max_magic_power(ent);

			send_rpg_events(2000);
		}
		else if (value == 45)
		{
			ent->client->pers.secrets_found |= (1 << 8);
		}
		else if (value == 46)
		{
			ent->client->pers.secrets_found |= (1 << 17);

			// zyk: update the rpg stuff info at the client-side game
			send_rpg_events(10000);
		}
		else if (value == 47)
		{
			ent->client->pers.secrets_found |= (1 << 19);
		}
		else if (value == 48)
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

			ent->client->ps.cloakFuel = 100;
		}
		else if (value == 49)
		{
			ent->client->pers.player_statuses |= (1 << 8);
		}
		else if (value == 50)
		{
			ent->client->pers.player_statuses |= (1 << 9);
		}
		else if (value == 51)
		{
			ent->client->pers.player_statuses |= (1 << 10);
		}
		else if (value == 52)
		{
			ent->client->pers.player_statuses |= (1 << 11);
		}
		else if (value == 53)
		{
			ent->client->pers.secrets_found |= (1 << 2);
			ent->client->pers.secrets_found &= ~(1 << 3);
			ent->client->pers.secrets_found &= ~(1 << 4);
		}
		else if (value == 54)
		{
			ent->client->pers.secrets_found &= ~(1 << 2);
			ent->client->pers.secrets_found |= (1 << 3);
			ent->client->pers.secrets_found &= ~(1 << 4);
		}
		else if (value == 55)
		{
			ent->client->pers.secrets_found &= ~(1 << 2);
			ent->client->pers.secrets_found &= ~(1 << 3);
			ent->client->pers.secrets_found |= (1 << 4);
		}
		else if (value == 56)
		{
			trap->SendServerCommand(ent - g_entities, "chat \"^3Book of Riddles: ^7key clock sword sun fire water time star nature\n\"");
		}

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		ent->client->pers.credits -= item_costs[value-1];
		save_account(ent, qtrue);

		// zyk: setting the cooldown time to buy and sell stuff. Bounty Hunter remote buying system has its own cooldown time cvar
		if (ent->client->pers.rpg_class == 2 && ent->client->pers.secrets_found & (1 << 1) && found == 0)
		{
			ent->client->pers.buy_sell_timer = level.time + zyk_bh_remote_buying_cooldown.integer;
		}
		else
		{
			ent->client->pers.buy_sell_timer = level.time + zyk_buying_selling_cooldown.integer;
		}

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
	int items_costs[NUMBER_OF_SELLER_ITEMS] = {10,15,20,30,35,40,45,0,0,60,65,70,80,50,0,0,50,60,70,100,50,45,90,150,0,0,0,0,0,0,0,10,0,20,30,90,45,5,0,0,0,20,50,0,0,0,0,0,0,0,0,0,0,0,0,0};

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

	if (ent->client->pers.quest_power_status & (1 << 2))
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot sell while frozen by Time Power.\n\"");
		return;
	}

	if (value < 1 || value > NUMBER_OF_SELLER_ITEMS || items_costs[value-1] == 0)
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

		// zyk: Bounty Hunter Upgrade allows buying and selling without the need to call the jawa seller
		if (found == 0 && !(ent->client->pers.rpg_class == 2 && ent->client->pers.secrets_found & (1 << 1)))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You must be near the jawa seller to sell to him.\n\"" );
			return;
		}
	}

	if (value == 1 && ent->client->ps.ammo[AMMO_BLASTER] >= 100)
	{
		ent->client->ps.ammo[AMMO_BLASTER] -= 100;
		sold = 1;
	}
	else if (value == 2 && ent->client->ps.ammo[AMMO_POWERCELL] >= 100)
	{
		ent->client->ps.ammo[AMMO_POWERCELL] -= 100;
		sold = 1;
	}
	else if (value == 3 && ent->client->ps.ammo[AMMO_METAL_BOLTS] >= 100)
	{
		ent->client->ps.ammo[AMMO_METAL_BOLTS] -= 100;
		sold = 1;
	}
	else if (value == 4 && ent->client->ps.ammo[AMMO_ROCKETS] >= 10)
	{
		ent->client->ps.ammo[AMMO_ROCKETS] -= 10;
		sold = 1;
	}
	else if (value == 5 && ent->client->ps.ammo[AMMO_THERMAL] >= 4)
	{
		ent->client->ps.ammo[AMMO_THERMAL] -= 4;
		sold = 1;
	}
	else if (value == 6 && ent->client->ps.ammo[AMMO_TRIPMINE] >= 3)
	{
		ent->client->ps.ammo[AMMO_TRIPMINE] -= 3;
		sold = 1;
	}
	else if (value == 7 && ent->client->ps.ammo[AMMO_DETPACK] >= 2)
	{
		ent->client->ps.ammo[AMMO_DETPACK] -= 2;
		sold = 1;
	}
	else if (value == 10 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SENTRY_GUN);

		if (ent->client->pers.rpg_class == 2 && ent->client->pers.bounty_hunter_sentries > 0)
			ent->client->pers.bounty_hunter_sentries--;

		sold = 1;
	}
	else if (value == 11 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER);
		sold = 1;
	}
	else if (value == 12 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC_BIG);
		sold = 1;
	}
	else if (value == 13 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SHIELD);
		sold = 1;
	}
	else if (value == 14 && ent->client->ps.powerups[PW_YSALAMIRI])
	{
		ent->client->ps.powerups[PW_YSALAMIRI] = 0;
		sold = 1;
	}
	else if (value == 17 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BLASTER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BLASTER);
		if (ent->client->ps.weapon == WP_BLASTER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 18 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DISRUPTOR)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DISRUPTOR);
		if (ent->client->ps.weapon == WP_DISRUPTOR)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 19 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_REPEATER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_REPEATER);
		if (ent->client->ps.weapon == WP_REPEATER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 20 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_ROCKET_LAUNCHER);
		if (ent->client->ps.weapon == WP_ROCKET_LAUNCHER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 21 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BOWCASTER);
		if (ent->client->ps.weapon == WP_BOWCASTER)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 22 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);
		if (ent->client->ps.weapon == WP_BRYAR_PISTOL)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 23 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_FLECHETTE)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_FLECHETTE);
		if (ent->client->ps.weapon == WP_FLECHETTE)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 24 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_CONCUSSION)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_CONCUSSION);
		if (ent->client->ps.weapon == WP_CONCUSSION)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 32 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_STUN_BATON)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON);
		if (ent->client->ps.weapon == WP_STUN_BATON)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 34 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_MEDPAC);
		sold = 1;
	}
	else if (value == 35 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_EWEB);
		sold = 1;
	}
	else if (value == 36 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DEMP2)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DEMP2);
		if (ent->client->ps.weapon == WP_DEMP2)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 37 && (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_OLD)))
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_OLD);
		if (ent->client->ps.weapon == WP_BRYAR_OLD)
			ent->client->ps.weapon = WP_MELEE;
		sold = 1;
	}
	else if (value == 38 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_BINOCULARS);
		sold = 1;
	}
	else if (value == 42 && ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK))
	{
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_CLOAK);
		sold = 1;
	}
	else if (value == 43 && ent->client->ps.powerups[PW_FORCE_BOON])
	{
		ent->client->ps.powerups[PW_FORCE_BOON] = 0;
		sold = 1;
	}

	zyk_adjust_holdable_items(ent);
			
	if (sold == 1)
	{
		add_credits(ent,items_costs[value-1]);
		save_account(ent, qtrue);

		// zyk: setting the cooldown time to buy and sell stuff. Bounty Hunter remote buying system has its own cooldown time cvar
		if (ent->client->pers.rpg_class == 2 && ent->client->pers.secrets_found & (1 << 1) && found == 0)
		{
			ent->client->pers.buy_sell_timer = level.time + zyk_bh_remote_buying_cooldown.integer;
		}
		else
		{
			ent->client->pers.buy_sell_timer = level.time + zyk_buying_selling_cooldown.integer;
		}

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
		trap->SendServerCommand( ent-g_entities, "print \"You must be at Spectator Mode to change password.\n\"" );
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Use ^3changepassword <new_password> ^7to change it.\n\"" );
		return;
	}

	// zyk: gets the new password
	trap->Argv(1, arg1, sizeof( arg1 ));

	if (strlen(arg1) > 30)
	{
		trap->SendServerCommand( ent-g_entities, "print \"The password must have a maximum of 30 characters.\n\"" );
		return;
	}

	strcpy(ent->client->pers.password,arg1);
	save_account(ent, qfalse);

	trap->SendServerCommand( ent-g_entities, "print \"Your password was changed successfully.\n\"" );
}

void zyk_remove_configs(gentity_t *ent)
{
#if defined(__linux__)
	system(va("rm -f zykmod/configs/%s_%s_freewarrior.txt zykmod/configs/%s_%s_forceuser.txt zykmod/configs/%s_%s_bountyhunter.txt zykmod/configs/%s_%s_armoredsoldier.txt zykmod/configs/%s_%s_monk.txt zykmod/configs/%s_%s_stealthattacker.txt zykmod/configs/%s_%s_duelist.txt zykmod/configs/%s_%s_forcegunner.txt zykmod/configs/%s_%s_magicmaster.txt zykmod/configs/%s_%s_forcetank.txt", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar));
#else
	system(va("DEL /F \"zykmod\\configs\\%s_%s_freewarrior.txt\" \"zykmod\\configs\\%s_%s_forceuser.txt\" \"zykmod\\configs\\%s_%s_bountyhunter.txt\" \"zykmod\\configs\\%s_%s_armoredsoldier.txt\" \"zykmod\\configs\\%s_%s_monk.txt\" \"zykmod\\configs\\%s_%s_stealthattacker.txt\" \"zykmod\\configs\\%s_%s_duelist.txt\" \"zykmod\\configs\\%s_%s_forcegunner.txt\" \"zykmod\\configs\\%s_%s_magicmaster.txt\" \"zykmod\\configs\\%s_%s_forcetank.txt\"", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, ent->client->sess.rpgchar));
#endif
}

/*
==================
Cmd_ResetAccount_f
==================
*/
void Cmd_ResetAccount_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
			
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^2Choose one of the options below\n\n^3/resetaccount rpg: ^7resets your entire account except admin commands.\n^3/resetaccount quests: ^7resets your RPG quests.\n^3/resetaccount levels: ^7resets your levels and upgrades.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (Q_stricmp( arg1, "rpg") == 0)
	{
		int i = 0;

		for (i = 0; i < NUMBER_OF_SKILLS; i++)
			ent->client->pers.skill_levels[i] = 0;

		ent->client->pers.max_rpg_shield = 0;
		ent->client->pers.secrets_found = 0;

		ent->client->pers.defeated_guardians = 0;
		ent->client->pers.hunter_quest_progress = 0;
		ent->client->pers.eternity_quest_progress = 0;
		ent->client->pers.universe_quest_progress = 0;
		ent->client->pers.universe_quest_counter = 0;

		// zyk: Challenge Mode, must keep the flag
		if (ent->client->pers.player_settings & (1 << 15))
			ent->client->pers.universe_quest_counter |= (1 << 29);

		ent->client->pers.level = 1;
		ent->client->pers.level_up_score = 0;
		ent->client->pers.skillpoints = 1;

		ent->client->pers.credits = 100;

		ent->client->sess.selected_special_power = MAGIC_MAGIC_SENSE;
		ent->client->sess.selected_left_special_power = MAGIC_MAGIC_SENSE;
		ent->client->sess.selected_right_special_power = MAGIC_MAGIC_SENSE;
		ent->client->sess.magic_fist_selection = 0;
		ent->client->sess.magic_disabled_powers = 0;
		ent->client->sess.magic_more_disabled_powers = 0;

		save_account(ent, qtrue);

		zyk_remove_configs(ent);

		trap->SendServerCommand( ent-g_entities, "print \"Your entire account is reset.\n\"" );

		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
			G_Kill(ent);
		}
	}
	else if (Q_stricmp( arg1, "quests") == 0)
	{
		ent->client->pers.defeated_guardians = 0;
		ent->client->pers.hunter_quest_progress = 0;
		ent->client->pers.eternity_quest_progress = 0;
		ent->client->pers.universe_quest_progress = 0;
		ent->client->pers.universe_quest_counter = 0;

		// zyk: Challenge Mode, must keep the flag
		if (ent->client->pers.player_settings & (1 << 15))
			ent->client->pers.universe_quest_counter |= (1 << 29);

		save_account(ent, qtrue);

		trap->SendServerCommand( ent-g_entities, "print \"Your quests are reset.\n\"" );

		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
			G_Kill(ent);
		}
	}
	else if (Q_stricmp( arg1, "levels") == 0)
	{
		int i = 0;

		for (i = 0; i < NUMBER_OF_SKILLS; i++)
			ent->client->pers.skill_levels[i] = 0;

		ent->client->pers.max_rpg_shield = 0;
		ent->client->pers.secrets_found = 0;

		ent->client->pers.level = 1;
		ent->client->pers.level_up_score = 0;
		ent->client->pers.skillpoints = 1;

		ent->client->pers.credits = 100;

		ent->client->sess.selected_special_power = MAGIC_MAGIC_SENSE;
		ent->client->sess.selected_left_special_power = MAGIC_MAGIC_SENSE;
		ent->client->sess.selected_right_special_power = MAGIC_MAGIC_SENSE;
		ent->client->sess.magic_fist_selection = 0;
		ent->client->sess.magic_disabled_powers = 0;
		ent->client->sess.magic_more_disabled_powers = 0;

		save_account(ent, qtrue);

		zyk_remove_configs(ent);

		trap->SendServerCommand( ent-g_entities, "print \"Your levels are reset.\n\"" );

		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
		{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
			G_Kill(ent);
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid option.\n\"" );
	}
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

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.guardian_mode > 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot teleport while in a guardian battle.\n\"" );
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

			if (g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
				return;
			}

			if (g_entities[client_id].client->sess.amrpgmode == 2 && g_entities[client_id].client->pers.guardian_mode > 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"Cannot teleport to a player in a guardian battle.\n\"" );
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

		if (g_entities[client1_id].client->sess.amrpgmode > 0 && g_entities[client1_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client1_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		if (g_entities[client1_id].client->sess.amrpgmode == 2 && g_entities[client1_id].client->pers.guardian_mode > 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Cannot teleport to a player in a guardian battle.\n\"" );
			return;
		}

		if (client2_id == -1)
		{
			return;
		}

		if (g_entities[client2_id].client->sess.amrpgmode > 0 && g_entities[client2_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client2_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		if (g_entities[client2_id].client->sess.amrpgmode == 2 && g_entities[client2_id].client->pers.guardian_mode > 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Cannot teleport to a player in a guardian battle.\n\"" );
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

		if (g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		if (g_entities[client_id].client->sess.amrpgmode == 2 && g_entities[client_id].client->pers.guardian_mode > 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Cannot teleport a player in a guardian battle.\n\"" );
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

		if (ent->client->sess.amrpgmode == 2 && ent->client->pers.guardian_mode > 0)
		{
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add allies during a boss battle.\n\"") );
			return;
		}

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

		if (ent->client->pers.player_settings & (1 << 0))
		{
			strcpy(message,"\n^3 0 - RPG quests - ^1OFF");
		}
		else
		{
			strcpy(message,"\n^3 0 - RPG quests - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 1))
		{
			strcpy(message, va("%s\n^3 1 - Light Power - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 1 - Light Power - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 2))
		{
			strcpy(message, va("%s\n^3 2 - Dark Power - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 2 - Dark Power - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 3))
		{
			strcpy(message, va("%s\n^3 3 - Eternity Power - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 3 - Eternity Power - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 4))
		{
			strcpy(message, va("%s\n^3 4 - Universe Power - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 4 - Universe Power - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 5))
		{
			strcpy(message, va("%s\n^3 5 - Language - ^1Custom", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 5 - Language - ^3English", message));
		}

		if (ent->client->pers.player_settings & (1 << 6))
		{
			strcpy(message, va("%s\n^3 6 - Allow Force Powers from allies - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 6 - Allow Force Powers from allies - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 7))
		{
			strcpy(message, va("%s\n^3 7 - Show magic cast in chat - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 7 - Show magic cast in chat - ^2ON", message));
		}

		// zyk: Saber Style flags
		if (ent->client->pers.player_settings & (1 << 26))
			strcpy(message, va("%s\n^3 8 - Starting Single Saber Style - ^3Yellow", message));
		else if (ent->client->pers.player_settings & (1 << 27))
			strcpy(message, va("%s\n^3 8 - Starting Single Saber Style - ^1Red", message));
		else if (ent->client->pers.player_settings & (1 << 28))
			strcpy(message, va("%s\n^3 8 - Starting Single Saber Style - ^1Desann", message));
		else if (ent->client->pers.player_settings & (1 << 29))
			strcpy(message, va("%s\n^3 8 - Starting Single Saber Style - ^5Tavion", message));
		else
			strcpy(message, va("%s\n^3 8 - Starting Single Saber Style - ^5Blue", message));

		if (ent->client->pers.player_settings & (1 << 9))
		{
			strcpy(message, va("%s\n^3 9 - Allow Screen Message - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^3 9 - Allow Screen Message - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 10))
		{
			strcpy(message, va("%s\n^310 - Use healing force only at allied players - ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^310 - Use healing force only at allied players - ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 11))
		{
			strcpy(message, va("%s\n^311 - Start With Saber ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^311 - Start With Saber ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 12))
		{
			strcpy(message, va("%s\n^312 - Jetpack ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^312 - Jetpack ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 13))
		{
			strcpy(message, va("%s\n^313 - Admin Protect ^1OFF", message));
		}
		else
		{
			strcpy(message, va("%s\n^313 - Admin Protect ^2ON", message));
		}

		if (ent->client->pers.player_settings & (1 << 14))
		{
			strcpy(message, va("%s\n^314 - Boss Battle Music ^1Custom", message));
		}
		else if (ent->client->pers.player_settings & (1 << 24))
		{
			strcpy(message, va("%s\n^314 - Boss Battle Music ^7Korriban Action", message));
		}
		else if (ent->client->pers.player_settings & (1 << 25))
		{
			strcpy(message, va("%s\n^314 - Boss Battle Music ^3MP Duel", message));
		}
		else
		{
			strcpy(message, va("%s\n^314 - Boss Battle Music ^2Hoth2 Action", message));
		}

		if (ent->client->pers.player_settings & (1 << 15))
		{
			strcpy(message, va("%s\n^315 - Difficulty ^1Challenge", message));
		}
		else
		{
			strcpy(message, va("%s\n^315 - Difficulty ^2Normal", message));
		}

		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\n^7Choose a setting above and use ^3/settings <number> ^7to turn it ^2ON ^7or ^1OFF^7\n\"", message) );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char new_status[32];
		int value = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		value = atoi(arg1);

		if (value < 0 || value > 15)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid settings value.\n\"" );
			return;
		}

		if (value != 8 && value != 14 && value != 15)
		{
			if (ent->client->pers.player_settings & (1 << value))
			{
				ent->client->pers.player_settings &= ~(1 << value);

				if (value == 1 && ent->client->pers.defeated_guardians == NUMBER_OF_GUARDIANS && zyk_enable_light_power.integer == 1)
				{
					ent->client->pers.magic_power--;
					ent->client->pers.quest_power_status |= (1 << 14);
					send_rpg_events(1000);
				}
				else if (value == 2 && ent->client->pers.hunter_quest_progress == NUMBER_OF_OBJECTIVES && zyk_enable_dark_power.integer == 1)
				{
					ent->client->pers.magic_power--;
					ent->client->pers.quest_power_status |= (1 << 15);
					send_rpg_events(1000);
				}
				else if (value == 3 && ent->client->pers.eternity_quest_progress == NUMBER_OF_ETERNITY_QUEST_OBJECTIVES && zyk_enable_eternity_power.integer == 1)
				{
					ent->client->pers.magic_power--;
					ent->client->pers.quest_power_status |= (1 << 16);
					send_rpg_events(1000);
				}
				else if (value == 4 && ent->client->pers.universe_quest_progress > 7 && zyk_enable_universe_power.integer == 1)
				{
					ent->client->pers.quest_power_status |= (1 << 13);
				}

				if (value == 5)
					strcpy(new_status, "^3English^7");
				else
					strcpy(new_status,"^2ON^7");
			}
			else
			{
				ent->client->pers.player_settings |= (1 << value);

				if (value == 1)
					ent->client->pers.quest_power_status &= ~(1 << 14);
				else if (value == 2)
					ent->client->pers.quest_power_status &= ~(1 << 15);
				else if (value == 3)
					ent->client->pers.quest_power_status &= ~(1 << 16);
				else if (value == 4)
					ent->client->pers.quest_power_status &= ~(1 << 13);

				if (value == 5)
					strcpy(new_status, "^1Custom^7");
				else
					strcpy(new_status,"^1OFF^7");
			}
		}
		else if (value == 14)
		{
			if (ent->client->pers.player_settings & (1 << 14))
			{
				ent->client->pers.player_settings &= ~(1 << 14);
				ent->client->pers.player_settings |= (1 << 24);
				strcpy(new_status,"^7Korriban Action^7");
			}
			else if (ent->client->pers.player_settings & (1 << 24))
			{
				ent->client->pers.player_settings &= ~(1 << 24);
				ent->client->pers.player_settings |= (1 << 25);
				strcpy(new_status,"^3MP Duel^7");
			}
			else if (ent->client->pers.player_settings & (1 << 25))
			{
				ent->client->pers.player_settings &= ~(1 << 25);
				strcpy(new_status,"^2Hoth2 Action^7");
			}
			else
			{
				ent->client->pers.player_settings |= (1 << 14);
				strcpy(new_status,"^1Custom^7");
			}
		}
		else if (value == 15)
		{
			if (!(ent->client->pers.player_settings & (1 << value)) && ent->client->pers.defeated_guardians == 0 && 
				ent->client->pers.hunter_quest_progress == 0 && ent->client->pers.eternity_quest_progress == 0 && 
				ent->client->pers.universe_quest_progress == 0 && ent->client->pers.can_play_quest == 0)
			{ // zyk: player can only activate Challenge Mode if he did not complete any quest mission
				ent->client->pers.player_settings |= (1 << value);
				ent->client->pers.universe_quest_counter |= (1 << 29);
			}
			else if (ent->client->pers.universe_quest_progress < NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES)
			{ // zyk: setting Normal Mode removes the Challenge Mode flag. Cannot change after completing all quests
				ent->client->pers.player_settings &= ~(1 << value);
				ent->client->pers.universe_quest_counter &= ~(1 << 29);
			}

			if (ent->client->pers.player_settings & (1 << value))
			{
				strcpy(new_status, "^1Challenge^7");
			}
			else
			{
				strcpy(new_status, "^2Normal^7");
			}
		}
		else
		{ // zyk: starting saber style has its own handling code
			if (ent->client->pers.player_settings & (1 << 26))
			{
				ent->client->pers.player_settings &= ~(1 << 26);
				ent->client->pers.player_settings |= (1 << 27);
				strcpy(new_status,"^1Red^7");
			}
			else if (ent->client->pers.player_settings & (1 << 27))
			{
				ent->client->pers.player_settings &= ~(1 << 27);
				ent->client->pers.player_settings |= (1 << 28);
				strcpy(new_status,"^1Desann^7");
			}
			else if (ent->client->pers.player_settings & (1 << 28))
			{
				ent->client->pers.player_settings &= ~(1 << 28);
				ent->client->pers.player_settings |= (1 << 29);
				strcpy(new_status,"^5Tavion");
			}
			else if (ent->client->pers.player_settings & (1 << 29))
			{
				ent->client->pers.player_settings &= ~(1 << 29);
				strcpy(new_status,"^5Blue^7");
			}
			else
			{
				ent->client->pers.player_settings |= (1 << 26);
				strcpy(new_status,"^3Yellow^7");
			}
		}

		save_account(ent, qfalse);

		if (value == 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Quests %s\n\"", new_status) );
		}
		else if (value == 1)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Light Power %s\n\"", new_status) );
		}
		else if (value == 2)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Dark Power %s\n\"", new_status) );
		}
		else if (value == 3)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Eternity Power %s\n\"", new_status) );
		}
		else if (value == 4)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Universe Power %s\n\"", new_status) );
		}
		else if (value == 5)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Language %s\n\"", new_status) );
		}
		else if (value == 6)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Allow Force Powers from allies %s\n\"", new_status) );
		}
		else if (value == 7)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Show magic cast in chat %s\n\"", new_status) );
		}
		else if (value == 8)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Starting Single Saber Style %s\n\"", new_status) );
		}
		else if (value == 9)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Allow Screen Message %s\n\"", new_status) );
		}
		else if (value == 10)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Use healing force only at allied players %s\n\"", new_status) );
		}
		else if (value == 11)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Start With Saber %s\n\"", new_status) );
		}
		else if (value == 12)
		{
			zyk_jetpack(ent);
			trap->SendServerCommand( ent-g_entities, va("print \"Jetpack %s\n\"", new_status) );
		}
		else if (value == 13)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Admin Protect %s\n\"", new_status) );
		}
		else if (value == 14)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Boss Battle Music %s\n\"", new_status) );
		}
		else if (value == 15)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Difficulty %s\n\"", new_status) );
			save_account(ent, qtrue);
		}

		if (value == 0 && ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->sess.amrpgmode == 2)
		{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
			G_Kill(ent);
		}

		Cmd_ZykMod_f(ent);
	}
}

char *zyk_config_filename(gclient_t *client)
{
	if (client->pers.rpg_class == 0)
		return va("zykmod/configs/%s_%s_freewarrior.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 1)
		return va("zykmod/configs/%s_%s_forceuser.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 2)
		return va("zykmod/configs/%s_%s_bountyhunter.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 3)
		return va("zykmod/configs/%s_%s_armoredsoldier.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 4)
		return va("zykmod/configs/%s_%s_monk.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 5)
		return va("zykmod/configs/%s_%s_stealthattacker.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 6)
		return va("zykmod/configs/%s_%s_duelist.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 7)
		return va("zykmod/configs/%s_%s_forcegunner.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 8)
		return va("zykmod/configs/%s_%s_magicmaster.txt", client->sess.filename, client->sess.rpgchar);
	else if (client->pers.rpg_class == 9)
		return va("zykmod/configs/%s_%s_forcetank.txt", client->sess.filename, client->sess.rpgchar);
	else
		return "";
}

char *zyk_legacy_config_filename(gclient_t *client)
{
	if (client->pers.rpg_class == 0)
		return va("zykmod/configs/%s_freewarrior.txt", client->sess.filename);
	else if (client->pers.rpg_class == 1)
		return va("zykmod/configs/%s_forceuser.txt", client->sess.filename);
	else if (client->pers.rpg_class == 2)
		return va("zykmod/configs/%s_bountyhunter.txt", client->sess.filename);
	else if (client->pers.rpg_class == 3)
		return va("zykmod/configs/%s_armoredsoldier.txt", client->sess.filename);
	else if (client->pers.rpg_class == 4)
		return va("zykmod/configs/%s_monk.txt", client->sess.filename);
	else if (client->pers.rpg_class == 5)
		return va("zykmod/configs/%s_stealthattacker.txt", client->sess.filename);
	else if (client->pers.rpg_class == 6)
		return va("zykmod/configs/%s_duelist.txt", client->sess.filename);
	else if (client->pers.rpg_class == 7)
		return va("zykmod/configs/%s_forcegunner.txt", client->sess.filename);
	else if (client->pers.rpg_class == 8)
		return va("zykmod/configs/%s_magicmaster.txt", client->sess.filename);
	else if (client->pers.rpg_class == 9)
		return va("zykmod/configs/%s_forcetank.txt", client->sess.filename);
	else
		return "";
}

void load_config(gentity_t *ent)
{
	FILE *config_file = NULL;
	gclient_t *client;
	char content[32];
	int value = 0;
	int i = 0;
	int unique_ability_flag = 0;

	strcpy(content,"");
	client = ent->client;

	config_file = fopen(zyk_config_filename(client),"r");

	if (config_file == NULL)
	{ // zyk: if did not find file, try getting the legacy version
		config_file = fopen(zyk_legacy_config_filename(client), "r");
	}

	if (config_file != NULL)
	{
		fscanf(config_file,"%s",content);
		value = atoi(content);

		if (client->pers.level >= value)
		{ // zyk: only loads config if current level is at least the one read from the file
		  // if the value is lower, it means the player reset his account. In this case, do not load anything
			for (i = 1; i <= NUMBER_OF_SKILLS; i++)
			{
				fscanf(config_file,"%s",content);
				value = atoi(content); // zyk: the skill levels the player has in this skill

				while (value > 0)
				{
					rpg_upgrade_skill(ent, i, qtrue);
					value--;
				}
			}

			if (fscanf(config_file, "%s", content) != EOF)
			{
				unique_ability_flag = atoi(content);
			}

			// zyk: loading Unique Ability bought for this class
			client->pers.secrets_found &= ~(1 << 2);
			client->pers.secrets_found &= ~(1 << 3);
			client->pers.secrets_found &= ~(1 << 4);
			if (unique_ability_flag == 2)
			{
				client->pers.secrets_found |= (1 << 2);
			}
			else if (unique_ability_flag == 3)
			{
				client->pers.secrets_found |= (1 << 3);
			}
			else if (unique_ability_flag == 4)
			{
				client->pers.secrets_found |= (1 << 4);
			}
		}

		fclose(config_file);
	}
}

void save_config(gentity_t *ent)
{
	FILE *config_file = NULL;
	gclient_t *client;
	int unique_ability_flag = 0;
	char content[MAX_STRING_CHARS];
	int i = 0;

	client = ent->client;

	zyk_create_dir("configs");

	config_file = fopen(zyk_config_filename(client),"w");

	if (config_file != NULL)
	{
		// zyk: saving Unique Ability bought for this class
		if (client->pers.secrets_found & (1 << 2))
		{
			unique_ability_flag = 2;
		}
		else if (client->pers.secrets_found & (1 << 3))
		{
			unique_ability_flag = 3;
		}
		else if (client->pers.secrets_found & (1 << 4))
		{
			unique_ability_flag = 4;
		}

		strcpy(content, "");

		for (i = 0; i < NUMBER_OF_SKILLS; i++)
		{
			strcpy(content, va("%s%d\n", content, client->pers.skill_levels[i]));
		}

		fprintf(config_file,"%d\n%s%d\n", client->pers.level, content, unique_ability_flag);
		
		fclose(config_file);
	}
}

void do_change_class(gentity_t *ent, int value)
{
	int i = 0;
	int old_class = ent->client->pers.rpg_class;

	if (value < 0 || value > 9)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid RPG Class.\n\"" );
		return;
	}

	if (zyk_allow_class_change.integer != 1 && ent->client->pers.level > 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You cannot change class after level 1 in this server.\n\"" );
		return;
	}

	// zyk validating the new class
	ent->client->pers.rpg_class = value;
	if (validate_rpg_class(ent) == qfalse)
	{
		ent->client->pers.rpg_class = old_class;
		return;
	}

	ent->client->pers.rpg_class = old_class;

	if (zyk_allow_class_change.integer == 1)
	{
		if (ent->client->pers.credits < 20)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You don't have enough credits to change your class.\n\"" );
			return;
		}

		remove_credits(ent, 20);
	}

	save_config(ent);

	ent->client->pers.rpg_class = value;

	// zyk: resetting skills
	for (i = 0; i < NUMBER_OF_SKILLS; i++)
	{
		while (ent->client->pers.skill_levels[i] > 0)
		{
			ent->client->pers.skill_levels[i]--;
			ent->client->pers.skillpoints++;
		}
	}

	load_config(ent);

	save_account(ent, qtrue);

	trap->SendServerCommand( ent-g_entities, va("print \"You are now a %s\n\"", zyk_rpg_class(ent)) );

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ // zyk: this command must kill the player if he is not in spectator mode to prevent exploits
		G_Kill(ent);
	}
}

/*
==================
Cmd_RpgClass_f
==================
*/
void Cmd_RpgClass_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int value = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Look at ^3/list classes ^7to see the class number, then ^2/rpgclass <class number>^7\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));
	value = atoi(arg1);

	if (zyk_rp_mode.integer == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You can't change class when RP Mode is activated by an admin.\n\"" );
		return;
	}

	do_change_class(ent, value);
}

/*
==================
Cmd_GuardianQuest_f
==================
*/
extern void zyk_start_boss_battle_music(gentity_t *ent);
extern gentity_t *Zyk_NPC_SpawnType( char *npc_type, int x, int y, int z, int yaw );
void Cmd_GuardianQuest_f( gentity_t *ent ) {

	if (zyk_allow_guardian_quest.integer != 1)
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Guardian Quest: ^7this quest is not allowed in this server\n\"") );
		return;
	}

	if (level.guardian_quest_timer > level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Guardian Quest: ^7wait %d seconds and try again\n\"", (level.guardian_quest_timer - level.time)/1000) );
		return;
	}

	if (level.guardian_quest == 0)
	{
		int i = 0, j = 0, num_spawn_points = 0, chosen_spawn_point = -1;
		gentity_t *this_ent = NULL;
		gentity_t *npc_ent = NULL;
		vec3_t npc_origin, npc_angles;

		// zyk: player cant spawn if someone is fighting a guardian
		for (i = 0; i < level.maxclients; i++)
		{
			this_ent = &g_entities[i];
			if (this_ent && this_ent->client && this_ent->client->sess.amrpgmode == 2 && this_ent->client->pers.guardian_mode > 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"You can't start this quest while a player is fighting a guardian.\n\"" );
				return;
			}
		}

		for (i = 0; i < level.num_entities; i++)
		{
			this_ent = &g_entities[i];
			if (this_ent && Q_stricmp(this_ent->classname, "info_player_deathmatch") == 0)
			{
				num_spawn_points++;
			}
		}

		chosen_spawn_point = Q_irand(1,num_spawn_points) - 1;

		// zyk: finds the chosen spawn point entity and gets its origin, which will be the guardian origin
		for (i = 0; i < level.num_entities; i++)
		{
			this_ent = &g_entities[i];
			if (this_ent && Q_stricmp(this_ent->classname, "info_player_deathmatch") == 0)
			{
				if (chosen_spawn_point == j)
				{
					VectorCopy(this_ent->s.origin, npc_origin);
					VectorCopy(this_ent->s.angles, npc_angles);
					break;
				}

				j++;
			}
		}

		npc_ent = Zyk_NPC_SpawnType("map_guardian",(int)npc_origin[0],(int)npc_origin[1],(int)npc_origin[2],(int)npc_angles[1]);

		if (npc_ent)
		{
			npc_ent->client->pers.hunter_quest_messages = 0;
			npc_ent->client->pers.hunter_quest_timer = level.time + 5000;
			npc_ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
			level.initial_map_guardian_weapons = npc_ent->client->ps.stats[STAT_WEAPONS];
			level.guardian_quest = npc_ent->s.number;
		}

		zyk_start_boss_battle_music(ent);

		trap->SendServerCommand( -1, va("chat \"^3Guardian Quest: ^7The ^3Guardian Quest ^7is activated!\"") );
	}
	else
	{
		trap->SendServerCommand( ent->s.number, va("print \"Guardian Quest is already active.\n\"") );
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

		for (j = 0; j < MAX_CLIENTS; j++)
		{ // zyk: if race already started, this player cant join anymore
			this_ent = &g_entities[j];
			if (this_ent && this_ent->client && this_ent->inuse && this_ent->health > 0 && this_ent->client->sess.amrpgmode == 2 && this_ent->client->pers.guardian_mode > 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"You can't start a race while someone is in a Guardian Battle!\n\"");
				return;
			}
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

			// zyk: if the player is a Bounty Hunter and the item is a sentry gun, must decrease the sentry gun counter
			if (item->giTag == HI_SENTRY_GUN && ent->client->sess.amrpgmode == 2 && ent->client->pers.rpg_class == 2 && ent->client->pers.bounty_hunter_sentries > 0)
			{
				ent->client->pers.bounty_hunter_sentries--;
			}

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
		(ent->client->sess.amrpgmode < 2 || ent->client->pers.skill_levels[34] > 0) && 
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
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( number_of_args < 3)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the old shader and new shader. Ex: ^3/remap models/weapons2/heavy_repeater/heavy_repeater_w.glm models/items/bacta^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	AddRemap(G_NewString(arg1), G_NewString(arg2), f);
	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	trap->SendServerCommand( ent-g_entities, "print \"Shader remapped\n\"" );
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
		strcpy(message_content[0],va("^3  %d ^7- NPC: ^2yes\n",ADM_NPC));
	}
	else
	{
		strcpy(message_content[0],va("^3  %d ^7- NPC: ^1no\n",ADM_NPC));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_NOCLIP))) 
	{
		strcpy(message_content[1],va("^3  %d ^7- NoClip: ^2yes\n",ADM_NOCLIP));
	}
	else
	{
		strcpy(message_content[1],va("^3  %d ^7- NoClip: ^1no\n",ADM_NOCLIP));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_GIVEADM))) 
	{
		strcpy(message_content[2],va("^3  %d ^7- GiveAdmin: ^2yes\n",ADM_GIVEADM));
	}
	else
	{
		strcpy(message_content[2],va("^3  %d ^7- GiveAdmin: ^1no\n",ADM_GIVEADM));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_TELE))) 
	{
		strcpy(message_content[3],va("^3  %d ^7- Teleport: ^2yes\n",ADM_TELE));
	}
	else
	{
		strcpy(message_content[3],va("^3  %d ^7- Teleport: ^1no\n",ADM_TELE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_ADMPROTECT))) 
	{
		strcpy(message_content[4],va("^3  %d ^7- AdminProtect: ^2yes\n",ADM_ADMPROTECT));
	}
	else
	{
		strcpy(message_content[4],va("^3  %d ^7- AdminProtect: ^1no\n",ADM_ADMPROTECT));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM))) 
	{
		strcpy(message_content[5],va("^3  %d ^7- EntitySystem: ^2yes\n",ADM_ENTITYSYSTEM));
	}
	else
	{
		strcpy(message_content[5],va("^3  %d ^7- EntitySystem: ^1no\n",ADM_ENTITYSYSTEM));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_SILENCE))) 
	{
		strcpy(message_content[6],va("^3  %d ^7- Silence: ^2yes\n",ADM_SILENCE));
	}
	else
	{
		strcpy(message_content[6],va("^3  %d ^7- Silence: ^1no\n",ADM_SILENCE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_CLIENTPRINT))) 
	{
		strcpy(message_content[7],va("^3  %d ^7- ClientPrint: ^2yes\n",ADM_CLIENTPRINT));
	}
	else
	{
		strcpy(message_content[7],va("^3  %d ^7- ClientPrint: ^1no\n",ADM_CLIENTPRINT));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_RPMODE))) 
	{
		strcpy(message_content[8],va("^3  %d ^7- RP Mode: ^2yes\n",ADM_RPMODE));
	}
	else
	{
		strcpy(message_content[8],va("^3  %d ^7- RP Mode: ^1no\n",ADM_RPMODE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_KICK))) 
	{
		strcpy(message_content[9],va("^3  %d ^7- Kick: ^2yes\n",ADM_KICK));
	}
	else
	{
		strcpy(message_content[9],va("^3  %d ^7- Kick: ^1no\n",ADM_KICK));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_PARALYZE))) 
	{
		strcpy(message_content[10],va("^3 %d ^7- Paralyze: ^2yes\n",ADM_PARALYZE));
	}
	else
	{
		strcpy(message_content[10],va("^3 %d ^7- Paralyze: ^1no\n",ADM_PARALYZE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_GIVE))) 
	{
		strcpy(message_content[11],va("^3 %d ^7- Give: ^2yes\n",ADM_GIVE));
	}
	else
	{
		strcpy(message_content[11],va("^3 %d ^7- Give: ^1no\n",ADM_GIVE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_SCALE))) 
	{
		strcpy(message_content[12],va("^3 %d ^7- Scale: ^2yes\n",ADM_SCALE));
	}
	else
	{
		strcpy(message_content[12],va("^3 %d ^7- Scale: ^1no\n",ADM_SCALE));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_PLAYERS))) 
	{
		strcpy(message_content[13],va("^3 %d ^7- Players: ^2yes\n",ADM_PLAYERS));
	}
	else
	{
		strcpy(message_content[13],va("^3 %d ^7- Players: ^1no\n",ADM_PLAYERS));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_DUELARENA)))
	{
		strcpy(message_content[14], va("^3 %d ^7- DuelArena: ^2yes\n", ADM_DUELARENA));
	}
	else
	{
		strcpy(message_content[14], va("^3 %d ^7- DuelArena: ^1no\n", ADM_DUELARENA));
	}

	if ((ent->client->pers.bitvalue & (1 << ADM_CUSTOMQUEST)))
	{
		strcpy(message_content[15], va("^3 %d ^7- Custom Quest: ^2yes\n", ADM_CUSTOMQUEST));
	}
	else
	{
		strcpy(message_content[15], va("^3 %d ^7- Custom Quest: ^1no\n", ADM_CUSTOMQUEST));
	}

	for (i = 0; i < ADM_NUM_CMDS; i++)
	{
		strcpy(message,va("%s%s",message,message_content[i]));
	}

	trap->SendServerCommand( target_ent-g_entities, va("print \"\n%s^7\n%s\n^7Use ^3/adminlist <number> ^7to see command info\n\n\"", ent->client->pers.netname, message) );
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
		else if (command_number == ADM_RPMODE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/rpmode ^7to make all players not able to use ^3/rpgclass ^7and level up. Admins can give levels by using ^3/levelgive <player name or ID>^7\n\n\"" );
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
Cmd_RpMode_f
==================
*/
void Cmd_RpMode_f( gentity_t *ent ) {
	if (!(ent->client->pers.bitvalue & (1 << ADM_RPMODE)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if (zyk_rp_mode.integer == 1)
	{
		int i = 0;

		trap->Cvar_Set( "zyk_rp_mode", "0" );

		for (i = 0; i < MAX_CLIENTS; i++)
		{ // zyk: logout everyone in RPG Mode so they must reload their accounts
			gentity_t *this_player = &g_entities[i];

			if (this_player && this_player->client && this_player->client->sess.amrpgmode == 2)
				this_player->client->sess.amrpgmode = 0;
		}

		trap->SendServerCommand( ent-g_entities, "print \"^3RPG System: ^7RP Mode ^1OFF^7\n\"" );
	}
	else
	{
		trap->Cvar_Set( "zyk_rp_mode", "1" );
		trap->SendServerCommand( ent-g_entities, "print \"^3RPG System: ^7RP Mode ^2ON^7\n\"" );
	}
}

/*
==================
Cmd_RpModeClass_f
==================
*/
void Cmd_RpModeClass_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_RPMODE)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() != 3 )
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name or ID and the class number.\n\"" ); 
		return; 
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse ); 
				
	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode != 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not in RPG Mode\n\"") );
		return;
	}

	do_change_class(&g_entities[client_id], atoi(arg2));
	trap->SendServerCommand( ent-g_entities, va("print \"Changed target player class\n\"") );
}

/*
==================
Cmd_RpModeUp_f
==================
*/
void Cmd_RpModeUp_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_RPMODE)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() != 3 )
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name or ID and the skill number.\n\"" ); 
		return; 
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse ); 
				
	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode != 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not in RPG Mode\n\"") );
		return;
	}

	do_upgrade_skill(&g_entities[client_id], atoi(arg2), qfalse);
	trap->SendServerCommand( ent-g_entities, va("print \"Upgraded target player skill\n\"") );
}

/*
==================
Cmd_RpModeDown_f
==================
*/
void Cmd_RpModeDown_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_RPMODE)))
	{ // zyk: admin command
		trap->SendServerCommand( ent-g_entities, "print \"You don't have this admin command.\n\"" );
		return;
	}

	if ( trap->Argc() != 3 )
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name or ID and the skill number.\n\"" ); 
		return; 
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse ); 
				
	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode != 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not in RPG Mode\n\"") );
		return;
	}

	do_downgrade_skill(&g_entities[client_id], atoi(arg2));
	trap->SendServerCommand( ent-g_entities, va("print \"Downgraded target player skill\n\"") );
}

/*
==================
Cmd_LevelGive_f
==================
*/
void Cmd_LevelGive_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!(ent->client->pers.bitvalue & (1 << ADM_RPMODE)))
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

	if (zyk_rp_mode.integer != 1)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"The server is not at RP Mode\n\"") );
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2)
	{
		if (g_entities[client_id].client->pers.level < zyk_rpg_max_level.integer)
		{
			g_entities[client_id].client->pers.score_modifier = g_entities[client_id].client->pers.level;
			g_entities[client_id].client->pers.credits_modifier = -10;
			rpg_score(&g_entities[client_id], qtrue);

			trap->SendServerCommand( ent-g_entities, va("print \"Target player leveled up\n\"") );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is at the max rpg level\n\"") );
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"The player must be in RPG Mode\n\"") );
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

			if (Q_stricmp(arg2, "force") == 0 || Q_stricmp(arg2, "weapons") == 0 || Q_stricmp(arg2, "other") == 0 || 
				Q_stricmp(arg2, "ammo") == 0 || Q_stricmp(arg2, "items") == 0)
			{ // zyk: show skills of the player
				zyk_list_player_skills(player_ent, ent, G_NewString(arg2));
			}
			else if (Q_stricmp(arg2, "stuff") == 0)
			{ // zyk: lists stuff bought by the player
				zyk_list_stuff(player_ent, ent);
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

	if (zyk_allow_saber_command.integer > 1 && ent->client->sess.amrpgmode == 2 && ent->client->pers.guardian_mode > 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use this command in boss battles.\n\"" );
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

qboolean zyk_can_deflect_shots(gentity_t *ent)
{
	if (ent->client && ent->client->sess.amrpgmode == 2 &&
		((ent->client->pers.rpg_class == 3 && ent->client->pers.secrets_found & (1 << 16)) || // zyk: Armored Soldier Upgrade has chance to deflect shots
		(ent->client->pers.rpg_class == 9 && ent->client->pers.player_statuses & (1 << 21)) // zyk: Force Armor unique ability has chance to deflect shots
		))
	{
		return qtrue;
	}

	return qfalse;
}

qboolean zyk_can_use_unique(gentity_t *ent)
{
	if (ent->health < 1)
	{ // zyk: must be alive to use unique skills or unique abilities
		return qfalse;
	}

	if ((ent->client->ps.forceHandExtend != HANDEXTEND_NONE && ent->client->ps.forceHandExtend != HANDEXTEND_FORCE_HOLD) ||
		 ent->client->pers.quest_power_status & (1 << 2))
	{ // zyk: using emotes/anims, special moves, and hit by Time Power. Cannot use unique ability
		return qfalse;
	}

	return qtrue;
}

// zyk: Unique Boost, makes unique skill cooldown time lower
void zyk_unique_boost(gentity_t *ent)
{
	if (ent->client->pers.unique_skill_timer > level.time && ent->client->pers.universe_quest_progress == NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES &&
		ent->client->pers.universe_quest_counter & (1 << 2) && !(ent->client->sess.magic_more_disabled_powers & (1 << 1)))
	{
		ent->client->pers.unique_skill_timer -= ((ent->client->pers.unique_skill_timer - level.time) / 5);
	}
}

/*
==================
Cmd_Unique_f
==================
*/
extern void Jedi_Cloak(gentity_t *self);
extern void WP_AddAsMindtricked(forcedata_t *fd, int entNum);
extern qboolean G_InGetUpAnim(playerState_t *ps);
extern void zyk_WP_FireBryarPistol(gentity_t *ent);
extern void zyk_WP_FireRocket(gentity_t *ent);
extern gentity_t *zyk_WP_FireThermalDetonator(gentity_t *ent, int yaw);
extern void zyk_add_bomb_model(gentity_t *ent);
extern void elemental_attack(gentity_t *ent);
extern void zyk_no_attack(gentity_t *ent);
extern void zyk_super_beam(gentity_t *ent, int angle_yaw);
extern void force_scream(gentity_t *ent);
extern void zyk_force_storm(gentity_t *ent);
extern qboolean zyk_unique_ability_can_hit_target(gentity_t *attacker, gentity_t *target);
extern void zyk_ice_bomb(gentity_t *ent);
extern void zyk_force_dash(gentity_t *ent);
void Cmd_Unique_f(gentity_t *ent) {
	if (ent->client->pers.secrets_found & (1 << 2))
	{ // zyk: Unique Ability 1
		if (ent->client->pers.rpg_class == 3 && ent->client->ps.powerups[PW_SHIELDHIT] > level.time)
		{ // zyk: releasing the small lightning dome
			ent->client->ps.powerups[PW_SHIELDHIT] = 0;

			ent->client->ps.powerups[PW_NEUTRALFLAG] = 0;
			ent->client->pers.unique_skill_duration = 0;

			lightning_dome(ent, 45);

			return;
		}

		if (zyk_can_use_unique(ent) == qfalse)
		{
			trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7cannot use Unique Ability now\"");
			return;
		}

		if (ent->client->pers.unique_skill_timer < level.time)
		{
			if (ent->client->pers.rpg_class == 0)
			{ // zyk: Free Warrior Mimic Damage. Makes the enemy receive back part of the damage he did
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4) && ent->client->pers.magic_power >= 25)
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);
					ent->client->pers.magic_power -= 25;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 8000;
					ent->client->pers.unique_skill_duration = level.time + 8000;

					ent->client->pers.player_statuses |= (1 << 21);

					send_rpg_events(2000);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force and 25 mp to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 1)
			{ // zyk: Force User Force Maelstrom
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					int i = 0;

					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					for (i = 0; i < level.num_entities; i++)
					{
						gentity_t *player_ent = &g_entities[i];

						if (player_ent && player_ent->client && ent != player_ent && zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue &&
							Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 300)
						{
							G_Damage(player_ent, ent, ent, NULL, NULL, 70, 0, MOD_FORCE_DARK);

							//Must play custom sounds on the actual entity. Don't use G_Sound (it creates a temp entity for the sound)
							G_EntitySound(player_ent, CHAN_VOICE, G_SoundIndex(va("*choke%d.wav", Q_irand(1, 3))));

							player_ent->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
							player_ent->client->ps.forceHandExtendTime = level.time + 4000;

							player_ent->client->ps.fd.forceGripBeingGripped = level.time + 4000;
						}
					}

					// zyk: activating Force Lightning
					ent->client->ps.fd.forcePowersActive |= (1 << FP_LIGHTNING);
					ent->client->ps.fd.forcePowerDuration[FP_LIGHTNING] = level.time + 4000;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 4000;
					ent->client->pers.unique_skill_duration = level.time + 4000;

					ent->client->pers.player_statuses |= (1 << 21);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 2)
			{ // zyk: Bounty Hunter Homing Rocket. Shoots a powerful rocket that automatically goes after someone
				if (ent->client->ps.ammo[AMMO_ROCKETS] >= 2 && ent->client->ps.ammo[AMMO_POWERCELL] >= 2)
				{
					int i = 0;
					int min_dist = 1000;
					gentity_t *chosen_ent = NULL;

					ent->client->ps.ammo[AMMO_ROCKETS] -= 2;
					ent->client->ps.ammo[AMMO_POWERCELL] -= 2;

					for (i = 0; i < level.num_entities; i++)
					{
						gentity_t *player_ent = &g_entities[i];

						if (player_ent && player_ent->client && ent != player_ent && player_ent->health > 0 && 
							zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue)
						{
							int player_dist = Distance(ent->client->ps.origin, player_ent->client->ps.origin);

							if (player_dist < min_dist)
							{
								min_dist = player_dist;

								chosen_ent = player_ent;
							}
						}
					}

					if (chosen_ent)
					{ // zyk: if we have a target, shoot a rocket at him
						ent->client->ps.rocketLockIndex = chosen_ent->s.number;
						ent->client->ps.rocketLockTime = level.time;
					}

					zyk_WP_FireRocket(ent);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 500;

					ent->client->pers.player_statuses |= (1 << 21);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 35000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 2 rockets and 2 powercell ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 3)
			{ // zyk: Armored Soldier Lightning Shield. Decreases damage and releases a small lightning dome when /unique is used again
				if (ent->client->ps.ammo[AMMO_POWERCELL] >= 5)
				{
					ent->client->ps.ammo[AMMO_POWERCELL] -= 5;

					ent->client->ps.powerups[PW_SHIELDHIT] = level.time + 8000;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 8000;
					ent->client->pers.unique_skill_duration = level.time + 8000;

					ent->client->pers.lightning_shield_timer = level.time + 200;

					ent->client->pers.player_statuses |= (1 << 21);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 30000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 5 power cell ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 4)
			{ // zyk: Monk Meditation Strength. Setting the meditate taunt and the duration of the ability
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer/4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer/4);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_MEDITATE;
					ent->client->ps.forceHandExtendTime = level.time + 3500;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 3500;
					ent->client->pers.unique_skill_duration = level.time + 3500;

					ent->client->pers.player_statuses |= (1 << 21);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 30000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 5)
			{ // zyk: Stealth Attacker Ultra Cloak. Uses Mind Trick code to make it work
				if (ent->client->ps.ammo[AMMO_POWERCELL] >= 5)
				{
					int i = 0;

					ent->client->ps.ammo[AMMO_POWERCELL] -= 5;

					for (i = 0; i < level.maxclients; i++)
					{
						gentity_t *player_ent = &g_entities[i];

						if (player_ent && player_ent->client && ent->s.number != i)
						{
							WP_AddAsMindtricked(&ent->client->ps.fd, i);
						}
					}

					ent->client->ps.fd.forcePowersActive |= (1 << FP_TELEPATHY);

					ent->client->ps.fd.forcePowerDuration[FP_TELEPATHY] = level.time + 9000;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 9000;

					Jedi_Cloak(ent);

					ent->client->pers.player_statuses |= (1 << 21);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 45000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 5 power cell ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 6)
			{ // zyk: Duelist Impale Stab
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_PULL_IMPALE_STAB, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2500;
					ent->client->pers.unique_skill_duration = level.time + 2500;

					ent->client->ps.weaponTime = 1500;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 45000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 7)
			{ // zyk: Force Gunner Thermal Throw. Throws a high damage thermal
				if (ent->client->ps.ammo[AMMO_POWERCELL] >= 3 && ent->client->ps.ammo[AMMO_THERMAL] >= 3)
				{
					int zyk_yaw = ent->client->ps.viewangles[YAW];

					ent->client->ps.ammo[AMMO_POWERCELL] -= 3;
					ent->client->ps.ammo[AMMO_THERMAL] -= 3;

					zyk_WP_FireThermalDetonator(ent, zyk_yaw);

					zyk_yaw -= 20;
					zyk_WP_FireThermalDetonator(ent, zyk_yaw);

					zyk_yaw += 40;
					zyk_WP_FireThermalDetonator(ent, zyk_yaw);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_K1_S1_BL;
					ent->client->ps.forceHandExtendTime = level.time + 500;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 40000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 3 thermals and 3 power cell ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 8)
			{ // zyk: Magic Master Faster Bolts activation
				if (ent->client->pers.magic_power >= 2)
				{
					ent->client->pers.magic_power -= 2;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 15000;
					ent->client->pers.unique_skill_duration = level.time + 15000;

					ent->client->pers.player_statuses |= (1 << 21);

					send_rpg_events(2000);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs at least 2 MP to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 9)
			{ // zyk: Force Guardian Force Armor. Increases resistance, resists force powers and has shield flag
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 10000;
					ent->client->pers.unique_skill_duration = level.time + 10000;

					ent->client->pers.player_statuses |= (1 << 21);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}

			zyk_unique_boost(ent);
			Cmd_ZykMod_f(ent);
		}
		else
		{
			trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7%d seconds left\"", ((ent->client->pers.unique_skill_timer - level.time) / 1000)));
		}
	}
	else if (ent->client->pers.secrets_found & (1 << 3))
	{ // zyk: Unique Ability 2
		if (zyk_can_use_unique(ent) == qfalse)
		{
			trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7cannot use Unique Ability now\"");
			return;
		}

		if (ent->client->pers.rpg_class == 2 && ent->client->pers.player_statuses & (1 << 22))
		{ // zyk: Bounty Hunter Wrist Shot ability
			if (ent->client->ps.ammo[AMMO_BLASTER] >= 5 && ent->client->pers.wrist_shot_counter > 0)
			{
				ent->client->ps.ammo[AMMO_BLASTER] -= 5;

				ent->client->pers.wrist_shot_counter--;

				zyk_WP_FireBryarPistol(ent);

				ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
				ent->client->ps.forceDodgeAnim = BOTH_FORCELIGHTNING_START;
				ent->client->ps.forceHandExtendTime = level.time + 500;

				G_Sound(ent, CHAN_WEAPON, G_SoundIndex("sound/weapons/bryar/alt_fire.mp3"));
			}

			return;
		}

		if (ent->client->pers.unique_skill_timer < level.time)
		{
			if (ent->client->pers.rpg_class == 0)
			{ // zyk: Free Warrior Super Beam
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 2) && ent->client->pers.magic_power >= 25)
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 2);
					ent->client->pers.magic_power -= 25;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2000;
					ent->client->pers.unique_skill_duration = level.time + 2000;

					ent->client->pers.player_statuses |= (1 << 22);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
					ent->client->ps.forceHandExtendTime = level.time + 2000;

					zyk_super_beam(ent, ent->client->ps.viewangles[1]);

					send_rpg_events(2000);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force and 25 mp to use it\"", (zyk_max_force_power.integer / 2)));
				}
			}
			else if (ent->client->pers.rpg_class == 1)
			{ // zyk: Force User Force Repulse
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					int i = 0;
					int push_scale = 700;

					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					for (i = 0; i < level.num_entities; i++)
					{
						gentity_t *player_ent = &g_entities[i];

						if (player_ent && player_ent->client && ent != player_ent && 
							zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue &&
							Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 350)
						{
							vec3_t dir;

							VectorSubtract(player_ent->client->ps.origin, ent->client->ps.origin, dir);
							VectorNormalize(dir);

							// zyk: if using Meditate taunt, remove it
							if (player_ent->client->ps.legsAnim == BOTH_MEDITATE && player_ent->client->ps.torsoAnim == BOTH_MEDITATE)
							{
								player_ent->client->ps.legsAnim = player_ent->client->ps.torsoAnim = BOTH_MEDITATE_END;
							}

							player_ent->client->ps.velocity[0] = dir[0] * push_scale;
							player_ent->client->ps.velocity[1] = dir[1] * push_scale;
							player_ent->client->ps.velocity[2] = 250;

							player_ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
							player_ent->client->ps.forceHandExtendTime = level.time + 1000;
							player_ent->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
							player_ent->client->ps.quickerGetup = qtrue;

							G_Damage(player_ent, ent, ent, NULL, NULL, 40, 0, MOD_UNKNOWN);
						}
					}

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 500;

					G_Sound(ent, CHAN_BODY, G_SoundIndex("sound/weapons/force/push.wav"));
					if (ent->client->ps.forceHandExtend == HANDEXTEND_NONE)
					{
						ent->client->ps.forceHandExtend = HANDEXTEND_FORCEPUSH;
						ent->client->ps.forceHandExtendTime = level.time + 1000;
					}
					else if (ent->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN && G_InGetUpAnim(&ent->client->ps))
					{
						if (ent->client->ps.forceDodgeAnim > 4)
						{
							ent->client->ps.forceDodgeAnim -= 8;
						}
						ent->client->ps.forceDodgeAnim += 8; //special case, play push on upper torso, but keep playing current knockdown anim on legs
					}
					ent->client->ps.powerups[PW_DISINT_4] = level.time + 1100;
					ent->client->ps.powerups[PW_PULL] = 0;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 2)
			{ // zyk: Bounty Hunter Wrist Shot
				if (ent->client->ps.ammo[AMMO_BLASTER] >= 5)
				{
					ent->client->ps.ammo[AMMO_BLASTER] -= 5;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 25000;

					ent->client->pers.player_statuses |= (1 << 22);

					// zyk: can shoot 5 times
					ent->client->pers.wrist_shot_counter = 5;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 35000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 5 blaster pack ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 3)
			{ // zyk: Armored Soldier Shield to Ammo. Recovers ammo by spending his shield
				if (ent->client->ps.stats[STAT_ARMOR] >= 20)
				{
					ent->client->ps.stats[STAT_ARMOR] -= 20;

					Add_Ammo(ent, AMMO_BLASTER, 200);

					Add_Ammo(ent, AMMO_POWERCELL, 200);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 30000;

					G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs 20 shield to use it\""));
				}
			}
			else if (ent->client->pers.rpg_class == 4)
			{ // zyk: Monk Spin Kick ability. Kicks everyone around the Monk with very high damage
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_A7_KICK_S;
					ent->client->ps.forceHandExtendTime = level.time + 1500;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 500;

					ent->client->pers.player_statuses |= (1 << 22);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 30000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 5)
			{ // zyk: Stealth Attacker Timed Bomb
				if (ent->client->ps.ammo[AMMO_POWERCELL] >= 5 && ent->client->ps.ammo[AMMO_METAL_BOLTS] >= 5)
				{
					ent->client->ps.ammo[AMMO_POWERCELL] -= 5;
					ent->client->ps.ammo[AMMO_METAL_BOLTS] -= 5;

					zyk_add_bomb_model(ent);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 45000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 5 power cell ammo and 5 metal bolts ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 6)
			{ // zyk: Duelist Vertical DFA
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_FORCELEAP2_T__B_, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);

					ent->client->ps.velocity[2] = 350;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 2800;
					ent->client->pers.unique_skill_duration = level.time + 2800;

					ent->client->ps.weaponTime = 1800;

					ent->client->pers.vertical_dfa_timer = level.time + 1000;

					ent->client->pers.player_statuses |= (1 << 22);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 45000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 7)
			{ // zyk: Force Gunner No Attack
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					zyk_no_attack(ent);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 40000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 8)
			{ // zyk: Magic Master Elemental Attack
				if (ent->client->pers.magic_power >= 20 && ent->client->pers.quest_power_usage_timer < level.time)
				{
					ent->client->pers.magic_power -= 20;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 500;

					ent->client->pers.player_statuses |= (1 << 22);

					elemental_attack(ent);

					send_rpg_events(2000);

					ent->client->pers.quest_power_usage_timer = level.time + 10000;

					display_yellow_bar(ent, (ent->client->pers.quest_power_usage_timer - level.time));

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs at least 20 MP to use it and wait some seconds after last magic used\"");
				}
			}
			else if (ent->client->pers.rpg_class == 9)
			{ // zyk: Force Guardian Force Scream
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					force_scream(ent);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 6000;
					ent->client->pers.unique_skill_duration = level.time + 6000;

					ent->client->pers.player_statuses |= (1 << 22);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}

			zyk_unique_boost(ent);
			Cmd_ZykMod_f(ent);
		}
		else
		{
			trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7%d seconds left\"", ((ent->client->pers.unique_skill_timer - level.time) / 1000)));
		}
	}
	else if (ent->client->pers.secrets_found & (1 << 4))
	{ // zyk: Unique Ability 3
		if (zyk_can_use_unique(ent) == qfalse)
		{
			trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7cannot use Unique Ability now\"");
			return;
		}

		if (ent->client->pers.rpg_class == 0 && ent->client->pers.player_statuses & (1 << 23))
		{ // zyk: Free Warrior used Flee to Safety already, find the place to be transported to
			int i = 0;
			gentity_t *effect_ent = NULL;

			for (i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
			{
				effect_ent = &g_entities[i];

				if (effect_ent && effect_ent->parent == ent && Q_stricmp(effect_ent->targetname, "zyk_flee_to_safety") == 0)
				{ // zyk: found the effect entity
					break;
				}
			}

			for (i = 0; i < level.num_entities; i++)
			{
				gentity_t *player_ent = &g_entities[i];

				if (player_ent && player_ent->client && player_ent->health > 0 && 
					Distance(effect_ent->s.origin, player_ent->client->ps.origin) < 50)
				{ // zyk: do not teleport if there is someone near the tele point
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7someone near target point\"");
					return;
				}
			}

			// zyk: transport the player to the effect place
			if (effect_ent)
			{
				zyk_TeleportPlayer(ent, effect_ent->s.origin, ent->client->ps.viewangles);

				level.special_power_effects_timer[effect_ent->s.number] = level.time + 500;
			}

			// zyk: set timers to finish the unique and clear the effect entity
			ent->client->ps.powerups[PW_NEUTRALFLAG] = 0;
			ent->client->pers.unique_skill_duration = 0;
			
			return;
		}
		else if (ent->client->pers.rpg_class == 2 && ent->client->pers.player_statuses & (1 << 23) && ent->client->pers.ice_bomb_counter == 1)
		{ // zyk: Ice Bomb detonation
			ent->client->pers.ice_bomb_counter = 2;

			G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/effects/cloth1.mp3"));

			return;
		}
		else if (ent->client->pers.rpg_class == 6 && ent->client->pers.player_statuses & (1 << 23))
		{ // zyk: Spin Throw second move, the throw
			if (ent->client->ps.torsoAnim == BOTH_ALORA_SPIN_SLASH)
			{
				ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 3800;
				ent->client->pers.unique_skill_duration = level.time + 3800;

				ent->client->ps.weaponTime = 2800;

				G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_ALORA_SPIN_THROW, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);
			}

			return;
		}

		if (ent->client->pers.unique_skill_timer < level.time)
		{
			if (ent->client->pers.rpg_class == 0)
			{ // zyk: Free Warrior Flee to Safety
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4) && ent->client->pers.magic_power >= 20)
				{
					gentity_t *new_ent = G_Spawn();
					int flee_to_safety_duration = 45000;

					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);
					ent->client->pers.magic_power -= 20;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + flee_to_safety_duration;
					ent->client->pers.unique_skill_duration = level.time + flee_to_safety_duration;

					ent->client->pers.player_statuses |= (1 << 23);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_FORCE_DRAIN_START;
					ent->client->ps.forceHandExtendTime = level.time + 700;

					zyk_set_entity_field(new_ent, "classname", "fx_runner");
					zyk_set_entity_field(new_ent, "spawnflags", "0");
					zyk_set_entity_field(new_ent, "targetname", "zyk_flee_to_safety");

					zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)ent->client->ps.origin[0], (int)ent->client->ps.origin[1], (int)ent->client->ps.origin[2]));

					new_ent->s.modelindex = G_EffectIndex("env/btend");

					new_ent->parent = ent;

					zyk_spawn_entity(new_ent);

					level.special_power_effects[new_ent->s.number] = ent->s.number;
					level.special_power_effects_timer[new_ent->s.number] = level.time + flee_to_safety_duration;

					send_rpg_events(2000);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force and 20 mp to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 1)
			{ // zyk: Force User Force Storm
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 3000;
					ent->client->pers.unique_skill_duration = level.time + 3000;

					ent->client->pers.player_statuses |= (1 << 23);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_FORCE_RAGE;
					ent->client->ps.forceHandExtendTime = level.time + 3000;

					zyk_force_storm(ent);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 2)
			{ // zyk: Bounty Hunter Ice Bomb
				if (ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7you must be on the ground to use it\"");
				}
				else if (ent->client->ps.ammo[AMMO_POWERCELL] >= 10 && ent->client->ps.ammo[AMMO_DETPACK] >= 1)
				{
					ent->client->ps.ammo[AMMO_POWERCELL] -= 10;
					ent->client->ps.ammo[AMMO_DETPACK] -= 1;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 25000;

					ent->client->pers.player_statuses |= (1 << 23);

					ent->client->pers.ice_bomb_counter = 1;

					zyk_ice_bomb(ent);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 35000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 1 det pack ammo and 10 power cell ammo to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 3)
			{ // zyk: Armored Soldier Faster E11. Improves E11 Blaster Rifle alt fire rate
				if (ent->client->ps.ammo[AMMO_BLASTER] >= 5)
				{
					ent->client->ps.ammo[AMMO_BLASTER] -= 5;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 8000;

					ent->client->pers.player_statuses |= (1 << 23);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 30000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs 5 blaster pack ammo to use it\""));
				}
			}
			else if (ent->client->pers.rpg_class == 4)
			{ // zyk: Monk Meditation Drain
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = BOTH_MEDITATE;
					ent->client->ps.forceHandExtendTime = level.time + 3000;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 3000;
					ent->client->pers.unique_skill_duration = level.time + 3000;

					ent->client->pers.player_statuses |= (1 << 23);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 30000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 5)
			{ // zyk: Stealth Attacker Aimed Shot
				if (ent->client->ps.ammo[AMMO_POWERCELL] >= 30 && ent->client->ps.weapon == WP_DISRUPTOR)
				{
					int i = 0;
					int min_dist = 900;
					gentity_t *chosen_ent = NULL;

					ent->client->ps.ammo[AMMO_POWERCELL] -= 30;

					for (i = 0; i < level.num_entities; i++)
					{
						gentity_t *player_ent = &g_entities[i];

						if (player_ent && player_ent->client && ent != player_ent && player_ent->health > 0 &&
							zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue)
						{
							int player_dist = Distance(ent->client->ps.origin, player_ent->client->ps.origin);

							if (player_dist < min_dist)
							{
								min_dist = player_dist;

								chosen_ent = player_ent;
							}
						}
					}

					if (chosen_ent)
					{ // zyk: if we have a target, shoot at him
						ent->client->pers.unique_skill_user_id = chosen_ent->s.number;
					}
					else
					{
						ent->client->pers.unique_skill_user_id = -1;
					}

					ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
					ent->client->ps.forceDodgeAnim = TORSO_WEAPONREADY4;
					ent->client->ps.forceHandExtendTime = level.time + 1500;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 1500;
					ent->client->pers.unique_skill_duration = level.time + 1500;

					ent->client->pers.player_statuses |= (1 << 23);

					ent->client->pers.monk_unique_timer = level.time + 750;

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 45000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs 30 power cell ammo and disruptor rifle to use it\"");
				}
			}
			else if (ent->client->pers.rpg_class == 6)
			{ // zyk: Duelist Spin Throw
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 3800;
					ent->client->pers.unique_skill_duration = level.time + 3800;

					ent->client->ps.weaponTime = 2800;

					G_SetAnim(ent, NULL, SETANIM_BOTH, BOTH_ALORA_SPIN_SLASH, SETANIM_FLAG_OVERRIDE | SETANIM_FLAG_HOLD, 0);

					ent->client->pers.player_statuses |= (1 << 23);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 45000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 7)
			{ // zyk: Force Gunner Fast Dash
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4) && ent->client->pers.magic_power >= 10)
				{
					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);
					ent->client->pers.magic_power -= 10;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 1700;
					ent->client->pers.unique_skill_duration = level.time + 1700;

					ent->client->pers.player_statuses |= (1 << 23);

					zyk_force_dash(ent);

					send_rpg_events(2000);

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 40000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force and 10 mp to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}
			else if (ent->client->pers.rpg_class == 8)
			{ // zyk: Magic Master Healing Improvement
				if (ent->client->pers.magic_power >= 15 && ent->client->pers.quest_power_usage_timer < level.time)
				{
					ent->client->pers.magic_power -= 15;

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 8500;
					ent->client->pers.unique_skill_duration = level.time + 8500;

					ent->client->pers.player_statuses |= (1 << 23);

					healing_area(ent, 3, 8000);

					send_rpg_events(2000);

					rpg_skill_counter(ent, 200);

					ent->client->pers.quest_power_usage_timer = level.time + 5000;

					display_yellow_bar(ent, (ent->client->pers.quest_power_usage_timer - level.time));

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, "chat \"^3Unique Ability: ^7needs at least 15 MP to use it and wait some seconds after last magic used\"");
				}
			}
			else if (ent->client->pers.rpg_class == 9)
			{ // zyk: Force Guardian Force Attraction
				if (ent->client->ps.fd.forcePower >= (zyk_max_force_power.integer / 4))
				{
					int i = 0;
					int push_scale = 700;

					ent->client->ps.fd.forcePower -= (zyk_max_force_power.integer / 4);

					for (i = 0; i < level.num_entities; i++)
					{
						gentity_t *player_ent = &g_entities[i];

						if (player_ent && player_ent->client && ent != player_ent &&
							zyk_unique_ability_can_hit_target(ent, player_ent) == qtrue &&
							Distance(ent->client->ps.origin, player_ent->client->ps.origin) < 400)
						{
							vec3_t dir;

							VectorSubtract(ent->client->ps.origin, player_ent->client->ps.origin, dir);
							VectorNormalize(dir);

							// zyk: if using Meditate taunt, remove it
							if (player_ent->client->ps.legsAnim == BOTH_MEDITATE && player_ent->client->ps.torsoAnim == BOTH_MEDITATE)
							{
								player_ent->client->ps.legsAnim = player_ent->client->ps.torsoAnim = BOTH_MEDITATE_END;
							}

							player_ent->client->ps.velocity[0] = dir[0] * push_scale;
							player_ent->client->ps.velocity[1] = dir[1] * push_scale;
							player_ent->client->ps.velocity[2] = 250;

							player_ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
							player_ent->client->ps.forceHandExtendTime = level.time + 1000;
							player_ent->client->ps.forceDodgeAnim = 0; //this toggles between 1 and 0, when it's 1 we should play the get up anim
							player_ent->client->ps.quickerGetup = qtrue;

							G_Damage(player_ent, ent, ent, NULL, NULL, 20, 0, MOD_UNKNOWN);
						}
					}

					ent->client->ps.powerups[PW_NEUTRALFLAG] = level.time + 500;
					ent->client->pers.unique_skill_duration = level.time + 500;

					ent->client->pers.player_statuses |= (1 << 23);

					G_Sound(ent, CHAN_BODY, G_SoundIndex("sound/weapons/force/pull.wav"));
					if (ent->client->ps.forceHandExtend == HANDEXTEND_NONE)
					{
						ent->client->ps.forceHandExtend = HANDEXTEND_FORCEPULL;
						ent->client->ps.forceHandExtendTime = level.time + 400;
					}
					ent->client->ps.powerups[PW_DISINT_4] = ent->client->ps.forceHandExtendTime + 200;
					ent->client->ps.powerups[PW_PULL] = ent->client->ps.powerups[PW_DISINT_4];

					rpg_skill_counter(ent, 200);

					ent->client->pers.unique_skill_timer = level.time + 50000;
				}
				else
				{
					trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7needs %d force to use it\"", (zyk_max_force_power.integer / 4)));
				}
			}

			zyk_unique_boost(ent);
			Cmd_ZykMod_f(ent);
		}
		else
		{
			trap->SendServerCommand(ent->s.number, va("chat \"^3Unique Ability: ^7%d seconds left\"", ((ent->client->pers.unique_skill_timer - level.time) / 1000)));
		}
	}
	else
	{
		trap->SendServerCommand(ent - g_entities, "print \"You have no Unique Abilities to use this command\n\"");
		return;
	}
}

/*
==================
Cmd_Magic_f
==================
*/
void Cmd_Magic_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent->s.number, va("print \" 0 Magic Sense - %s^7      31 Ultimate - %s^7        32 Final - %s^7\n 1 ^4Healing Water - %s^7     2 ^4Water Splash - %s^7     3 ^4Water Attack - %s^7\n 4 ^3Earthquake - %s^7        5 ^3Rockfall - %s^7         6 ^3Shifting Sand - %s^7\n 7 ^2Sleeping Flowers - %s^7  8 ^2Poison Mushrooms - %s^7 9 ^2Tree of Life - %s^7\n10 ^5Magic Shield - %s^7     11 ^5Dome of Damage - %s^7  12 ^5Magic Disable - %s^7\n13 ^6Ultra Speed - %s^7      14 ^6Slow Motion - %s^7     15 ^6Fast and Slow - %s^7\n16 ^1Flame Burst - %s^7      17 ^1Ultra Flame - %s^7     18 ^1Flaming Area - %s^7\n19 Blowing Wind - %s^7     20 Hurricane - %s^7       21 Reverse Wind - %s^7\n22 ^3Ultra Resistance - %s^7 23 ^3Ultra Strength - %s^7  24 ^3Enemy Weakening - %s^7\n25 ^5Ice Stalagmite - %s^7   26 ^5Ice Boulder - %s^7     27 ^5Ice Block - %s^7\n28 Healing Area - %s^7     29 Magic Explosion - %s^7 30 Lightning Dome - %s^7\n\"", 
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_MAGIC_SENSE)) ? "^2yes" : "^1no ", 
			!(ent->client->sess.magic_more_disabled_powers & (1 << 0)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_more_disabled_powers & (1 << 1)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_HEALING_WATER)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_WATER_SPLASH)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_WATER_ATTACK)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_EARTHQUAKE)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ROCKFALL)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_SHIFTING_SAND)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_SLEEPING_FLOWERS)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_POISON_MUSHROOMS)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_TREE_OF_LIFE)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_MAGIC_SHIELD)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_DOME_OF_DAMAGE)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_MAGIC_DISABLE)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ULTRA_SPEED)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_SLOW_MOTION)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_FAST_AND_SLOW)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_FLAME_BURST)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ULTRA_FLAME)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_FLAMING_AREA)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_BLOWING_WIND)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_HURRICANE)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_REVERSE_WIND)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ULTRA_RESISTANCE)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ULTRA_STRENGTH)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ENEMY_WEAKENING)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ICE_STALAGMITE)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ICE_BOULDER)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_ICE_BLOCK)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_HEALING_AREA)) ? "^2yes" : "^1no ", !(ent->client->sess.magic_disabled_powers & (1 << MAGIC_MAGIC_EXPLOSION)) ? "^2yes" : "^1no ",
			!(ent->client->sess.magic_disabled_powers & (1 << MAGIC_LIGHTNING_DOME)) ? "^2yes" : "^1no ") );
	}
	else
	{
		int magic_power = 0;

		trap->Argv( 1, arg1, sizeof( arg1 ) );

		magic_power = atoi(arg1);

		if (magic_power < MAGIC_MAGIC_SENSE || magic_power >= (MAX_MAGIC_POWERS + 2))
		{
			trap->SendServerCommand(ent->s.number, "print \"Invalid magic power.\n\"" );
			return;
		}

		if (magic_power >= 31)
		{
			magic_power -= 31;

			if (ent->client->sess.magic_more_disabled_powers & (1 << magic_power))
			{
				ent->client->sess.magic_more_disabled_powers &= ~(1 << magic_power);
				save_account(ent, qtrue);
				trap->SendServerCommand(ent->s.number, "print \"Enabled a magic power.\n\"");
			}
			else
			{
				ent->client->sess.magic_more_disabled_powers |= (1 << magic_power);
				save_account(ent, qtrue);
				trap->SendServerCommand(ent->s.number, "print \"Disabled a magic power.\n\"");
			}
		}
		else if (ent->client->sess.magic_disabled_powers & (1 << magic_power))
		{
			ent->client->sess.magic_disabled_powers &= ~(1 << magic_power);
			save_account(ent, qtrue);
			trap->SendServerCommand(ent->s.number, "print \"Enabled a magic power.\n\"" );
		}
		else
		{
			ent->client->sess.magic_disabled_powers |= (1 << magic_power);
			save_account(ent, qtrue);
			trap->SendServerCommand(ent->s.number, "print \"Disabled a magic power.\n\"" );
		}
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

	if (ent->client->sess.amrpgmode == 2 && ent->client->pers.guardian_mode > 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot play RPG LMS while in boss battles\n\"");
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
Cmd_QuestSkip_f
==================
*/
void Cmd_QuestSkip_f(gentity_t *ent) {
	if (ent->client->pers.can_play_quest == 1)
	{
		if (ent->client->pers.universe_quest_progress == 1 && ent->client->pers.universe_quest_messages > 4)
		{
			ent->client->pers.universe_quest_messages = 35;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 3 && ent->client->pers.universe_quest_messages > 0)
		{
			ent->client->pers.universe_quest_messages = 16;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 4 && ent->client->pers.universe_quest_messages > 5)
		{
			ent->client->pers.universe_quest_messages = 18;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 5 && number_of_amulets(ent) == 3 && ent->client->pers.universe_quest_messages > 205)
		{
			ent->client->pers.universe_quest_messages = 223;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 8 && ent->client->pers.universe_quest_messages > 1)
		{
			ent->client->pers.universe_quest_messages = 20;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 10 && ent->client->pers.universe_quest_messages > 1)
		{
			ent->client->pers.universe_quest_messages = 18;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 12 && ent->client->pers.universe_quest_messages > 1)
		{
			ent->client->pers.universe_quest_messages = 24;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 13 && ent->client->pers.universe_quest_messages > 1)
		{
			ent->client->pers.universe_quest_messages = 27;
			ent->client->pers.universe_quest_timer = level.time;
		}
		else if (ent->client->pers.universe_quest_progress == 15)
		{
			if (ent->client->pers.universe_quest_counter & (1 << 0) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Sages Sequel
				ent->client->pers.universe_quest_messages = 30;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 1) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Guardians Sequel
				ent->client->pers.universe_quest_messages = 19;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 2) && ent->client->pers.universe_quest_messages >= 24)
			{ // zyk: Thor Sequel
				ent->client->pers.universe_quest_messages = 34;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 3) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Time Sequel
				ent->client->pers.universe_quest_messages = 35;
				ent->client->pers.universe_quest_timer = level.time;
			}
		}
		else if (ent->client->pers.universe_quest_progress == 17)
		{
			if (ent->client->pers.universe_quest_counter & (1 << 0) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Sages Sequel
				ent->client->pers.universe_quest_messages = 28;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 1) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Guardians Sequel
				ent->client->pers.universe_quest_messages = 9;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 2) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Thor Sequel
				ent->client->pers.universe_quest_messages = 15;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 3) && ent->client->pers.universe_quest_messages >= 14)
			{ // zyk: Time Sequel
				ent->client->pers.universe_quest_messages = 21;
				ent->client->pers.universe_quest_timer = level.time;
			}
		}
		else if (ent->client->pers.universe_quest_progress == 19)
		{
			if (ent->client->pers.universe_quest_counter & (1 << 0) && ent->client->pers.universe_quest_messages >= 10)
			{ // zyk: Sages Sequel
				ent->client->pers.universe_quest_messages = 26;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 1) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Guardians Sequel
				ent->client->pers.universe_quest_messages = 8;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 2) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Thor Sequel
				ent->client->pers.universe_quest_messages = 13;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 3) && ent->client->pers.universe_quest_messages >= 53)
			{ // zyk: Time Sequel
				ent->client->pers.universe_quest_messages = 64;
				ent->client->pers.universe_quest_timer = level.time;
			}
		}
		else if (ent->client->pers.universe_quest_progress == 21)
		{
			if (ent->client->pers.universe_quest_counter & (1 << 0) && ent->client->pers.universe_quest_messages >= 10)
			{ // zyk: Sages Sequel
				ent->client->pers.universe_quest_messages = 30;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 1) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Guardians Sequel
				ent->client->pers.universe_quest_messages = 12;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 2) && ent->client->pers.universe_quest_messages >= 1)
			{ // zyk: Thor Sequel
				ent->client->pers.universe_quest_messages = 10;
				ent->client->pers.universe_quest_timer = level.time;
			}
			else if (ent->client->pers.universe_quest_counter & (1 << 3) && ent->client->pers.universe_quest_messages >= 53)
			{ // zyk: Time Sequel
				ent->client->pers.universe_quest_messages = 70;
				ent->client->pers.universe_quest_timer = level.time;
			}
		}
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "chat \"^3Quest System: ^7Must be in a quest to use this command\"");
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
		trap->SendServerCommand(ent->s.number, va("print \"\n^7Using %s\n\n^7%s\n^3/rpgchar new <charname>: ^7creates a new char\n^3/rpgchar rename <new name>: ^7renames current char\n^3/rpgchar duplicate: ^7creates a copy of the current char in use\n^3/rpgchar use <charname>: ^7uses this char\n^3/rpgchar delete <charname>: ^7removes this char\n^3/rpgchar migrate <charname> <login> <password>: ^7moves char to account with this login and password\n\"", ent->client->sess.rpgchar, zyk_get_rpg_chars(ent, "\n")));
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char arg2[MAX_STRING_CHARS];
		char arg3[MAX_STRING_CHARS];
		char arg4[MAX_STRING_CHARS];

		trap->Argv(1, arg1, sizeof(arg1));

		if (argc == 2 && Q_stricmp(arg1, "duplicate") != 0)
		{
			trap->SendServerCommand(ent->s.number, "print \"This command requires at least one more argument\n\"");
			return;
		}

		if (Q_stricmp(arg1, "duplicate") != 0)
		{
			trap->Argv(2, arg2, sizeof(arg2));
		}
		else
		{
			strcpy(arg2, "");
		}

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
		else if (Q_stricmp(arg1, "duplicate") == 0)
		{
			char new_char_name[32];

			if (zyk_char_count(ent) >= MAX_RPG_CHARS)
			{
				trap->SendServerCommand(ent->s.number, "print \"Reached the max limit of chars\n\"");
				return;
			}

			if (strlen(ent->client->sess.rpgchar) < MAX_ACC_NAME_SIZE - 4)
			{
				strcpy(new_char_name, va("dup_%s", ent->client->sess.rpgchar));
			}
			else
			{ // zyk: cannot go over the max, so remove the last chars
				strcpy(new_char_name, va("%s", ent->client->sess.rpgchar));

				new_char_name[0] = 'd';
				new_char_name[1] = 'u';
				new_char_name[2] = 'p';
				new_char_name[3] = '_';
			}

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, new_char_name), "r");
			if (chars_file != NULL)
			{
				fclose(chars_file);
				trap->SendServerCommand(ent->s.number, "print \"Cannot overwrite existing duplicate\n\"");
				return;
			}

#if defined(__linux__)
			system(va("cp zykmod/accounts/%s_%s.txt zykmod/accounts/%s_%s.txt", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, new_char_name));
#else
			system(va("cd \"zykmod/accounts\" & COPY %s_%s.txt %s_%s.txt", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->sess.filename, new_char_name));
#endif

			trap->SendServerCommand(ent->s.number, va("print \"Char %s ^7duplicated!\n\"", ent->client->sess.rpgchar));
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
		else if (Q_stricmp(arg1, "migrate") == 0)
		{
			if (argc < 5)
			{
				trap->SendServerCommand(ent->s.number, "print \"This command requires more arguments\n\"");
				return;
			}

			trap->Argv(3, arg3, sizeof(arg3));
			trap->Argv(4, arg4, sizeof(arg4));

			if (zyk_check_user_input(arg2, strlen(arg2)) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid charname. Only letters and numbers allowed.\n\"");
				return;
			}

			if (zyk_check_user_input(arg3, strlen(arg3)) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"Invalid charname. Only letters and numbers allowed.\n\"");
				return;
			}

			chars_file = fopen(va("zykmod/accounts/%s.txt", arg3), "r");
			if (chars_file == NULL)
			{
				trap->SendServerCommand(ent->s.number, "print \"This account does not exist\n\"");
				return;
			}

			// zyk: getting the password
			fscanf(chars_file, "%s", content);
			fclose(chars_file);

			if (strlen(content) != strlen(arg4) || Q_strncmp(content, arg4, strlen(content)) != 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"The password is incorrect\n\"");
				return;
			}

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2), "r");
			if (chars_file == NULL)
			{
				trap->SendServerCommand(ent->s.number, "print \"This char does not exist\n\"");
				return;
			}
			fclose(chars_file);

			chars_file = fopen(va("zykmod/accounts/%s_%s.txt", arg3, arg2), "r");
			if (chars_file)
			{
				fclose(chars_file);
				trap->SendServerCommand(ent->s.number, "print \"Cannot overwrite existing char of that account\n\"");
				return;
			}

			if (Q_stricmp(arg2, arg3) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot overwrite the default char of that account\n\"");
				return;
			}

			if (Q_stricmp(arg2, ent->client->sess.filename) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot migrate the default char\n\"");
				return;
			}

			if (Q_stricmp(arg2, ent->client->sess.rpgchar) == 0)
			{
				trap->SendServerCommand(ent->s.number, "print \"Cannot migrate char you are using now\n\"");
				return;
			}

#if defined(__linux__)
			system(va("mv zykmod/accounts/%s_%s.txt zykmod/accounts/%s_%s.txt", ent->client->sess.filename, arg2, arg3, arg2));
#else
			system(va("cd \"zykmod/accounts\" & MOVE %s_%s.txt %s_%s.txt", ent->client->sess.filename, arg2, arg3, arg2));
#endif

			trap->SendServerCommand(ent->s.number, va("print \"Char %s ^7moved!\n\"", arg2));
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
	zyk_npc->client->sess.selected_left_special_power = 0;

	// zyk: ultimate magic powers and quest powers
	zyk_npc->client->sess.selected_right_special_power = 0;

	// zyk: unique abilities
	zyk_npc->client->sess.selected_special_power = 0;

	// zyk: setting the timers
	zyk_npc->client->pers.light_quest_messages = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcfirsttimer%d", level.zyk_custom_quest_counter)));
	zyk_npc->client->pers.hunter_quest_messages = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcsecondtimer%d", level.zyk_custom_quest_counter)));
	zyk_npc->client->pers.universe_quest_messages = atoi(zyk_get_mission_value(level.custom_quest_map, level.zyk_custom_quest_current_mission, va("npcthirdtimer%d", level.zyk_custom_quest_counter)));

	// zyk: setting default values of these timers
	if (zyk_npc->client->pers.light_quest_messages <= 0)
	{
		zyk_npc->client->pers.light_quest_messages = 5000;
	}

	if (zyk_npc->client->pers.hunter_quest_messages <= 0)
	{
		zyk_npc->client->pers.hunter_quest_messages = 7000;
	}

	if (zyk_npc->client->pers.universe_quest_messages <= 0)
	{
		zyk_npc->client->pers.universe_quest_messages = 9000;
	}

	zyk_npc->client->pers.light_quest_timer = level.time + zyk_npc->client->pers.light_quest_messages;
	zyk_npc->client->pers.hunter_quest_timer = level.time + zyk_npc->client->pers.hunter_quest_messages;
	zyk_npc->client->pers.universe_quest_timer = level.time + zyk_npc->client->pers.universe_quest_messages;

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
				zyk_npc->client->sess.selected_left_special_power |= (1 << zyk_power);
			}
			else if (zyk_power < MAX_MAGIC_POWERS + 8)
			{
				zyk_npc->client->sess.selected_right_special_power |= (1 << (zyk_power - MAX_MAGIC_POWERS));
			}
			else if (zyk_power < MAX_MAGIC_POWERS + 13)
			{
				zyk_npc->client->sess.selected_special_power |= (1 << (zyk_power - MAX_MAGIC_POWERS - 8));
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
	{ "guardianquest",		Cmd_GuardianQuest_f,		CMD_ALIVE|CMD_RPG|CMD_NOINTERMISSION },
	{ "ignore",				Cmd_Ignore_f,				CMD_NOINTERMISSION },
	{ "ignorelist",			Cmd_IgnoreList_f,			CMD_NOINTERMISSION },
	{ "jetpack",			Cmd_Jetpack_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "kill",				Cmd_Kill_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "killother",			Cmd_KillOther_f,			CMD_CHEAT|CMD_NOINTERMISSION },
//	{ "kylesmash",			TryGrapple,					0 },
	{ "levelgive",			Cmd_LevelGive_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "levelshot",			Cmd_LevelShot_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "list",				Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "listaccount",		Cmd_ListAccount_f,			CMD_NOINTERMISSION },
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
	{ "questskip",			Cmd_QuestSkip_f,			CMD_RPG|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "racemode",			Cmd_RaceMode_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "remap",				Cmd_Remap_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remapdeletefile",	Cmd_RemapDeleteFile_f,		CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remaplist",			Cmd_RemapList_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remapload",			Cmd_RemapLoad_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "remapsave",			Cmd_RemapSave_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "resetaccount",		Cmd_ResetAccount_f,			CMD_RPG|CMD_NOINTERMISSION },
	{ "rpgchar",			Cmd_RpgChar_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "rpgclass",			Cmd_RpgClass_f,				CMD_RPG|CMD_NOINTERMISSION },
	{ "rpglmsmode",			Cmd_RpgLmsMode_f,			CMD_RPG|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "rpglmstable",		Cmd_RpgLmsTable_f,			CMD_NOINTERMISSION },
	{ "rpmode",				Cmd_RpMode_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "rpmodeclass",		Cmd_RpModeClass_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "rpmodedown",			Cmd_RpModeDown_f,			CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "rpmodeup",			Cmd_RpModeUp_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
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
	{ "unique",				Cmd_Unique_f,				CMD_RPG | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "up",					Cmd_UpSkill_f,				CMD_RPG|CMD_NOINTERMISSION },
	{ "voice_cmd",			Cmd_VoiceCommand_f,			CMD_NOINTERMISSION },
	{ "vote",				Cmd_Vote_f,					CMD_NOINTERMISSION },
	{ "where",				Cmd_Where_f,				CMD_NOINTERMISSION },
	{ "zykfile",			Cmd_ZykFile_f,				CMD_NOINTERMISSION },
	{ "zykchars",			Cmd_ZykChars_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "zykmod",				Cmd_ZykMod_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
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
