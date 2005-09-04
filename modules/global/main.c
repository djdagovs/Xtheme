/*
 * Copyright (c) 2005 Atheme Development Group
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the main() routine.
 *
 * $Id: main.c 2125 2005-09-04 23:34:32Z nenolod $
 */

#include "atheme.h"

DECLARE_MODULE_V1
(
	"global/main", FALSE, _modinit, _moddeinit,
	"$Id: main.c 2125 2005-09-04 23:34:32Z nenolod $",
	"Atheme Development Group <http://www.atheme.org>"
);

list_t gs_cmdtree;
list_t *os_cmdtree;

static void gs_cmd_global(char *origin);
static void gs_cmd_help(char *origin);

/* help commands we understand */
static struct help_command_ gs_help_commands[] = {
  { "HELP",     AC_IRCOP, "help/help"              },
  { "GLOBAL",   AC_IRCOP, "help/gservice/global"   },
  { NULL, 0, NULL }
};

command_t gs_help = { "HELP", "Displays contextual help information.",
                        AC_IRCOP, gs_cmd_help };
command_t gs_global = { "GLOBAL", "Sends a global notice.",
                        AC_IRCOP, gs_cmd_global };

/* *INDENT-ON* */

/* HELP <command> [params] */
static void gs_cmd_help(char *origin)
{
	char *command = strtok(NULL, "");
	char buf[BUFSIZE];
	struct help_command_ *c;
	FILE *help_file;

	if (!command)
	{
		command_help(globsvs.nick, origin, &gs_cmdtree);
		notice(globsvs.nick, origin, "For more specific help use \2HELP \37command\37\2.");

		return;
	}

	/* take the command through the hash table */
	if ((c = help_cmd_find(globsvs.nick, origin, command, gs_help_commands)))
	{
		if (c->file)
		{
			help_file = fopen(c->file, "r");

			if (!help_file)
			{
				notice(globsvs.nick, origin, "No help available for \2%s\2.", command);
				return;
			}

			while (fgets(buf, BUFSIZE, help_file))
			{
				strip(buf);

				replace(buf, sizeof(buf), "&nick&", globsvs.disp);

				if (buf[0])
					notice(globsvs.nick, origin, "%s", buf);
				else
					notice(globsvs.nick, origin, " ");
			}

			fclose(help_file);
		}
		else
			notice(globsvs.nick, origin, "No help available for \2%s\2.", command);
	}
}

/* GLOBAL <parameters>|SEND|CLEAR */
static void gs_cmd_global(char *origin)
{
	static BlockHeap *glob_heap = NULL;
	struct global_ *global;
	static list_t globlist;
	node_t *n, *n2, *tn;
	tld_t *tld;
	char *params = strtok(NULL, "");
	static char *sender = NULL;

	if (!params)
	{
		notice(globsvs.nick, origin, "Insufficient parameters for \2GLOBAL\2.");
		notice(globsvs.nick, origin, "Syntax: GLOBAL <parameters>|SEND|CLEAR");
		return;
	}

	if (!strcasecmp("CLEAR", params))
	{
		if (!globlist.count)
		{
			notice(globsvs.nick, origin, "No message to clear.");
			return;
		}

		/* destroy the list we made */
		LIST_FOREACH_SAFE(n, tn, globlist.head)
		{
			global = (struct global_ *)n->data;
			node_del(n, &globlist);
			node_free(n);
			free(global->text);
			BlockHeapFree(glob_heap, global);
		}

		BlockHeapDestroy(glob_heap);
		glob_heap = NULL;
		free(sender);
		sender = NULL;

		notice(globsvs.nick, origin, "The pending message has been deleted.");

		return;
	}

	if (!strcasecmp("SEND", params))
	{
		if (!globlist.count)
		{
			notice(globsvs.nick, origin, "No message to send.");
			return;
		}

		LIST_FOREACH(n, globlist.head)
		{
			global = (struct global_ *)n->data;

			/* send to every tld */
			LIST_FOREACH(n2, tldlist.head)
			{
				tld = (tld_t *)n2->data;

				sts(":%s NOTICE %s*%s :[Network Notice] %s", globsvs.nick, ircd->tldprefix, tld->name, global->text);
			}
		}

		/* destroy the list we made */
		LIST_FOREACH_SAFE(n, tn, globlist.head)
		{
			global = (struct global_ *)n->data;
			node_del(n, &globlist);
			node_free(n);
			free(global->text);
			BlockHeapFree(glob_heap, global);
		}

		BlockHeapDestroy(glob_heap);
		glob_heap = NULL;
		free(sender);
		sender = NULL;

		snoop("GLOBAL: \2%s\2", origin);

		notice(globsvs.nick, origin, "The global notice has been sent.");

		return;
	}

	if (!glob_heap)
		glob_heap = BlockHeapCreate(sizeof(struct global_), 5);

	if (!sender)
		sender = sstrdup(origin);

	if (irccasecmp(sender, origin))
	{
		notice(globsvs.nick, origin, "There is already a GLOBAL in progress by \2%s\2.", sender);
		return;
	}

	global = BlockHeapAlloc(glob_heap);

	global->text = sstrdup(params);

	n = node_create();
	node_add(global, n, &globlist);

	notice(globsvs.nick, origin,
	       "Stored text to be sent as line %d. Use \2GLOBAL SEND\2 "
	       "to send message, \2GLOBAL CLEAR\2 to delete the pending message, " "or \2GLOBAL\2 to store additional lines.", globlist.count);
}

/* main services client routine */
void gservice(char *origin, uint8_t parc, char *parv[])
{
	char *cmd, *s;
	char orig[BUFSIZE];

        if (!origin)
        {
                slog(LG_DEBUG, "services(): recieved a request with no origin!");
                return;
        }

	/* this should never happen */
	if (parv[0][0] == '&')
	{
		slog(LG_ERROR, "services(): got parv with local channel: %s", parv[0]);
		return;
	}

	/* make a copy of the original for debugging */
	strlcpy(orig, parv[parc - 1], BUFSIZE);

	/* lets go through this to get the command */
	cmd = strtok(parv[parc - 1], " ");

	if (!cmd)
		return;

	/* ctcp? case-sensitive as per rfc */
	if (!strcmp(cmd, "\001PING"))
	{
		if (!(s = strtok(NULL, " ")))
			s = " 0 ";

		strip(s);
		notice(globsvs.nick, origin, "\001PING %s\001", s);
		return;
	}
	else if (!strcmp(cmd, "\001VERSION\001"))
	{
		notice(globsvs.nick, origin,
		       "\001VERSION atheme-%s. %s %s %s%s%s%s%s%s%s%s%s TS5ow\001",
		       version, revision, me.name,
		       (match_mapping) ? "A" : "",
		       (me.loglevel & LG_DEBUG) ? "d" : "",
		       (me.auth) ? "e" : "",
		       (config_options.flood_msgs) ? "F" : "",
		       (config_options.leave_chans) ? "l" : "",
		       (config_options.join_chans) ? "j" : "",
		       (config_options.leave_chans) ? "l" : "", (config_options.join_chans) ? "j" : "", (!match_mapping) ? "R" : "", (config_options.raw) ? "r" : "", (runflags & RF_LIVE) ? "n" : "");

		return;
	}
	else if (!strcmp(cmd, "\001CLIENTINFO\001"))
	{
		/* easter egg :X */
		notice(globsvs.nick, origin, "\001CLIENTINFO 114 97 107 97 117 114\001");
		return;
	}

	/* ctcps we don't care about are ignored */
	else if (*cmd == '\001')
		return;

	command_exec(globsvs.disp, origin, cmd, &gs_cmdtree);
}

static void global_config_ready(void *unused)
{
        if (globsvs.me)
                del_service(globsvs.me);

        globsvs.me = add_service(globsvs.nick, globsvs.user,
                                 globsvs.host, globsvs.real, gservice);
        globsvs.disp = globsvs.me->disp;

        hook_del_hook("config_ready", global_config_ready);
}

void _modinit(module_t *m)
{
        hook_add_event("config_ready");
        hook_add_hook("config_ready", global_config_ready);

        if (!cold_start)
        {
                globsvs.me = add_service(globsvs.nick, globsvs.user,
                        globsvs.host, globsvs.real, gservice);
                globsvs.disp = globsvs.me->disp;
        }

	os_cmdtree = module_locate_symbol("operserv/main", "os_cmdtree");

        command_add(&gs_global, &gs_cmdtree);

	if (os_cmdtree)
		command_add(&gs_global, os_cmdtree);

        command_add(&gs_help, &gs_cmdtree);
}

void _moddeinit(void)
{
	if (globsvs.me)
		del_service(globsvs.me);

	command_delete(&gs_global, &gs_cmdtree);

	if (os_cmdtree)
		command_delete(&gs_global, os_cmdtree);

        command_delete(&gs_help, &gs_cmdtree);
}

