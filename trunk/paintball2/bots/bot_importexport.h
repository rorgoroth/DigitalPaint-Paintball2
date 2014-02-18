/*
Copyright (c) 2014 Nathan "jitspoe" Wulf, Digital Paint

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

#define BOT_API_VERSION 1

typedef struct edict_s edict_t;

// for use the game library (gamex86)
typedef struct
{
	int			apiversion;

	// The init function will only be called when a game starts,
	// not each time a level is loaded.  Persistant data for clients
	// and the server can be allocated in init.
	void		(*Init) (void);
	void		(*InitMap) (const char *mapname);
	void		(*ShutdownMap) (void);
	void		(*Shutdown) (void);
	void		(*GameEvent) (game_event_t event, edict_t *ent, void *data1, void *data2);
	void		(*RunFrame) (int msec, float game_time); // should be called each game frame
	qboolean	(*Command) (edict_t *ent, const char *cmd, const char *cmd2, const char *cmd3, const char *cmd4);
	void		(*ExitLevel) (void); // called when level ends
	void		(*SpawnEntities) (void); // called when level starts
	void		(*ObservePlayerInput) (unsigned int player_index, edict_t *ent, pmove_t *pm); // called for each pm packet received from a client

	// Block of unset data that will be zeroed out, in case of API changes, this will make new function pointers null,
	// so crashes will be more obvious.
	char		unset[64];
} bot_export_t;

typedef struct
{
	int			apiversion;

	void		(*bprintf) (int printlevel, char *fmt, ...);
	void		(*cprintf) (edict_t *ent, int printlevel, char *fmt, ...);
	void		(*dprintf) (char *fmt, ...);
	trace_t		(*trace) (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
	qboolean	(*inPVS) (vec3_t p1, vec3_t p2);
	qboolean	(*inPHS) (vec3_t p1, vec3_t p2);
	void		(*Pmove) (pmove_t *pmove);		// player movement code common with client prediction
	int			(*pointcontents) (vec3_t point);
	void		*(*TagMalloc) (int size, int tag);
	void		(*TagFree) (void *block);
	void		(*FreeTags) (int tag);
	cvar_t		*(*cvar) (char *var_name, char *value, int flags);

	void		(*ClientThink) (edict_t *ent, usercmd_t *ucmd);
	edict_t		*(*AddBotClient) (char *userinfo);
	const char	*(*GetClientName) (edict_t *ent);
	void		(*DisconnectBot) (edict_t *ent);

	char		unset[64];
} bot_import_t;


// for use with the rendering engine (just to pass rendering functions for debugging purposes)
typedef struct
{
	int			apiversion;

	int			(*DrawDebugLine) (const vec_t *start, const vec_t *end, float r, float g, float b, float time, int id);
	int			(*DrawDebugSphere) (const vec_t *pos, float radius, float r, float g, float b, float time, int id);

	char		unset[64];
} bot_render_import_t;


