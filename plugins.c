#include "uwsgi.h"

extern struct uwsgi_server uwsgi;

static int plugin_already_loaded(const char *plugin) {
	int i;

	for (i = 0; i < 0xFF; i++) {
		if (uwsgi.p[i]->name) {
			if (!strcmp(plugin, uwsgi.p[i]->name)) {
#ifdef UWSGI_DEBUG
				uwsgi_log("%s plugin already available\n", plugin);
#endif
				return 1;
			}	
		}
		if (uwsgi.p[i]->alias) {
			if (!strcmp(plugin, uwsgi.p[i]->alias)) {
#ifdef UWSGI_DEBUG
				uwsgi_log("%s plugin already available\n", plugin);
#endif
				return 1;
			}	
		}
	}

	for(i=0;i<uwsgi.gp_cnt;i++) {

		if (uwsgi.gp[i]->name) {
                        if (!strcmp(plugin, uwsgi.gp[i]->name)) {
#ifdef UWSGI_DEBUG
                                uwsgi_log("%s plugin already available\n", plugin);
#endif
                                return 1;
                        }       
                }
                if (uwsgi.gp[i]->alias) {
                        if (!strcmp(plugin, uwsgi.gp[i]->alias)) {
#ifdef UWSGI_DEBUG
                                uwsgi_log("%s plugin already available\n", plugin);
#endif
                                return 1;
                        }
                }
        }

	return 0;
}

void *uwsgi_load_plugin(int modifier, char *plugin, char *has_option) {

	void *plugin_handle = NULL;

	int need_free = 0;
	char *plugin_name = plugin;
	char *plugin_symbol_name_start = plugin;


	struct uwsgi_plugin *up;
	char linkpath_buf[1024], linkpath[1024];
	int linkpath_size;

	char *colon = strchr(plugin_name, ':');
	if (colon) {
		colon[0] = 0;
		modifier = atoi(plugin_name);
		plugin_name = colon+1;
		colon[0] = ':';
	}

	if (!uwsgi_endswith(plugin_name, "_plugin.so")) {
		plugin_name = uwsgi_concat2(plugin_name, "_plugin.so");
		need_free = 1;
	}

	plugin_symbol_name_start = plugin_name;

	// step 1: check for absolute plugin (stop if it fails)
	if (strchr(plugin_name, '/')) {
		plugin_handle = dlopen(plugin_name, RTLD_NOW | RTLD_GLOBAL);
		if (!plugin_handle) {
			uwsgi_log( "%s\n", dlerror());
			goto end;
		}
		plugin_symbol_name_start = uwsgi_get_last_char(plugin_name, '/');
		plugin_symbol_name_start++;
	}

	// step dir, check for user-supplied plugins directory
	struct uwsgi_string_list *pdir = uwsgi.plugins_dir;
	while(pdir) {
		char *plugin_filename = uwsgi_concat3(pdir->value, "/", plugin_name);
		plugin_handle = dlopen(plugin_filename, RTLD_NOW | RTLD_GLOBAL);
		if (plugin_handle) {
			free(plugin_filename);
			break;
		}
		free(plugin_filename);
		pdir = pdir->next;
	}

	// last step: search in compile-time plugin_dir
	if (!plugin_handle) {
		char *plugin_filename = uwsgi_concat3(UWSGI_PLUGIN_DIR, "/", plugin_name);
		plugin_handle = dlopen(plugin_filename, RTLD_NOW | RTLD_GLOBAL);
		free(plugin_filename);
	}

        if (!plugin_handle) {
                uwsgi_log( "%s\n", dlerror());
        }
        else {
		char *plugin_entry_symbol = uwsgi_concat2n(plugin_symbol_name_start, strlen(plugin_symbol_name_start)-3, "", 0);
                up = dlsym(plugin_handle, plugin_entry_symbol);
		if (!up) {
			// is it a link ?
			memset(linkpath_buf, 0, 1024);
			memset(linkpath, 0, 1024);
			if ((linkpath_size = readlink(plugin_name, linkpath_buf, 1023)) > 0) {
				do {
					linkpath_buf[linkpath_size] = '\0';
					strcpy(linkpath, linkpath_buf);
				} while ((linkpath_size = readlink(linkpath, linkpath_buf, 1023)) > 0);
#ifdef UWSGI_DEBUG
				uwsgi_log("%s\n", linkpath);
#endif
				free(plugin_entry_symbol);
				up = dlsym(plugin_handle, plugin_entry_symbol);
				char *slash = uwsgi_get_last_char(linkpath, '/');
				if (!slash) {
					slash = linkpath;
				}
				else {
					slash++;
				}
				plugin_entry_symbol = uwsgi_concat2n(slash, strlen(slash)-3, "",0);
				up = dlsym(plugin_handle, plugin_entry_symbol);
			}
		}
                if (up) {
			if (!up->name) {
				uwsgi_log("the loaded plugin (%s) has no .name attribute\n", plugin_name);
				if (dlclose(plugin_handle)) {
                                	uwsgi_error("dlclose()");
                                }
				if (need_free)
					free(plugin_name);
				free(plugin_entry_symbol);
				return NULL;
			}
			if (plugin_already_loaded(up->name)) {
				if (dlclose(plugin_handle)) {
                                	uwsgi_error("dlclose()");
                                }
				if (need_free)
					free(plugin_name);
				free(plugin_entry_symbol);
				return NULL;
			}
			if (has_option) {
				struct uwsgi_option *op = uwsgi.options;
				int found = 0;
				while (op && op->name) {
					if (!strcmp(has_option, op->name)) {
						found = 1;
						break;
					}	
                			op++;
				}
				if (!found) {
					if (dlclose(plugin_handle)) {
						uwsgi_error("dlclose()");
					}
					if (need_free)
						free(plugin_name);
					free(plugin_entry_symbol);
					return NULL;
				}
				
			}
			if (modifier != -1) {
				fill_plugin_table(modifier, up);			
			}
			else {
				fill_plugin_table(up->modifier1, up);			
			}
			if (need_free)
				free(plugin_name);
			free(plugin_entry_symbol);

			if (up->on_load)
				up->on_load();
			return plugin_handle;
                }
                uwsgi_log( "%s\n", dlerror());
        }

end:
	if (need_free)
		free(plugin_name);

	return NULL;
}
