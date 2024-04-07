#include <iostream>
#include <chrono>
#include <string.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/tuple.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include <discord_rpc.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define EXPORT __attribute__((visibility("default")))
#define APPLICATION_ID "484736379171897344"

static const char *SETTING_EXTRA_TEXT = "extra_text";
static const char *SETTING_USE_PLAYING = "use_playing";

class RPCPlugin : public GeneralPlugin {

public:
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Discord RPC"),
        "audacious-plugin-rpc",
        about,
        &prefs
    };

    constexpr RPCPlugin() : GeneralPlugin (info, false) {}

    bool init();
    void cleanup();
};

EXPORT RPCPlugin aud_plugin_instance;

DiscordEventHandlers handlers;
DiscordRichPresence presence;
std::string fullTitle;
std::string playingStatus;

std::string get_ascii_player_art() {
    double prg = (double)aud_drct_get_time() / (double)aud_drct_get_length();
    int sec = (int)(aud_drct_get_time() / 1000.0);
    int min = (int)(sec / 60);
    int fullSec = (int)(aud_drct_get_length() / 1000.0);
    int fullMin = (int)(fullSec / 60);
    sec = sec % 60;
    fullSec = fullSec % 60;
    
    std::string str = "";
    str.append(std::to_string(min));
    str.append(":");
    if(sec < 10) str.append("0");
    str.append(std::to_string(sec));
    str.append(" [");

    for(int i = 0; i < 19; i++) {
        if(i / 19.0 <= prg) {
            if((i+1) / 19.0 > prg) {
                str.append("●");
            } else {
                str.append("=");
            }
        } else {
            str.append("-");
        }
    }

    str.append("] ");
    str.append(std::to_string(fullMin));
    str.append(":");
    if(fullSec < 10) str.append("0");
    str.append(std::to_string(fullSec));

    return str;
}

void init_discord() {
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);
}

void update_presence() {
    Discord_UpdatePresence(&presence);
}

void init_presence() {
    memset(&presence, 0, sizeof(presence));
    presence.state = "Initialized";
    presence.details = "Waiting...";
    presence.largeImageKey = "logo";
    presence.smallImageKey = "stop";
    update_presence();
}

void cleanup_discord() {
    Discord_ClearPresence();
    Discord_Shutdown();
}

void title_changed() {
    if (!aud_drct_get_ready()) {
        return;
    }

    if (aud_drct_get_playing()) {
        bool paused = aud_drct_get_paused();
        bool usePlayingStatus = aud_get_bool(SETTING_USE_PLAYING);
        Tuple tuple = aud_drct_get_tuple();
        String artist = tuple.get_str(Tuple::Artist);
        String album = tuple.get_str(Tuple::Album);
        String albumArtist = tuple.get_str(Tuple::AlbumArtist);
        if(NULL == albumArtist) albumArtist = artist;
        
        std::string title(tuple.get_str(Tuple::Title));
        fullTitle = title.substr(0, 127);
        
        playingStatus = std::string("by ").append(artist);
        presence.type = usePlayingStatus ? 0 : 2;
        presence.details = fullTitle.c_str();
        presence.smallImageKey = paused ? "pause" : "play";
        presence.smallImageText = paused ? "Paused" : "Playing";
        presence.largeImageText = album;
        
        if(usePlayingStatus) {
            presence.button1text = "";
            presence.button1url = "";
            
            // Timestamp
            int remainingTimeMs = aud_drct_get_length() - aud_drct_get_time();
            using namespace std::chrono;
            uint64_t nowMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            if(!paused) {
                presence.startTimestamp = nowMs - aud_drct_get_time();
                presence.endTimestamp = nowMs + remainingTimeMs;
            } else {
                presence.startTimestamp = 0;
                presence.endTimestamp = 0;
            }
        } else {
            presence.button1text = strdup(get_ascii_player_art().c_str());
            presence.button1url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
        }
    } else {
        Discord_ClearPresence();
    }

    std::string extraText(aud_get_str("audacious-plugin-rpc", SETTING_EXTRA_TEXT));
    playingStatus = (playingStatus + " " + extraText).substr(0, 127);

    presence.state = playingStatus.c_str();
    update_presence();
}

void update_title_presence(void*, void*) {
    title_changed();
}

void open_github() {
   system("xdg-open https://github.com/darktohka/audacious-plugin-rpc");
}

bool RPCPlugin::init() {
    init_discord();
    init_presence();
    hook_associate("playback ready", update_title_presence, nullptr);
    hook_associate("playback end", update_title_presence, nullptr);
    hook_associate("playback stop", update_title_presence, nullptr);
    hook_associate("playback pause", update_title_presence, nullptr);
    hook_associate("playback unpause", update_title_presence, nullptr);
    hook_associate("title change", update_title_presence, nullptr);
    return true;
}

void RPCPlugin::cleanup() {
    hook_dissociate("playback ready", update_title_presence);
    hook_dissociate("playback end", update_title_presence);
    hook_dissociate("playback stop", update_title_presence);
    hook_dissociate("playback pause", update_title_presence);
    hook_dissociate("playback unpause", update_title_presence);
    hook_dissociate("title change", update_title_presence);
    cleanup_discord();
}

const char RPCPlugin::about[] = N_("Discord RPC music status plugin\n\nWritten by: Derzsi Daniel <daniel@tohka.us>");

const PreferencesWidget RPCPlugin::widgets[] =
{
  WidgetEntry(
      N_("Extra status text:"),
      WidgetString("audacious-plugin-rpc", SETTING_EXTRA_TEXT, title_changed)
  ),
  WidgetCheck(
      N_("Use \"Playing\" status instead of \"Listening to\":"),
      WidgetBool(0, SETTING_USE_PLAYING)
  ),
  WidgetButton(
      N_("Fork on GitHub"),
      {open_github}
  )
};

const PluginPreferences RPCPlugin::prefs = {{ widgets }};
