#include <chrono>
#include <iostream>
#include <thread>
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

#include "HTTPRequest.hpp"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define EXPORT __attribute__((visibility("default")))
#define APPLICATION_ID "484736379171897344"
#define MUSICBRAINZ_UA "Application audacious+fork.1/4.3.1 ( kenny.mh.hui@outlook.com )"

static const char *SETTING_ALBUM_BUTTON = "show_album_button";
static const char *SETTING_SHOW_COVER_ART = "show_cover_art";
static const char *SETTING_USE_PLAYING = "use_playing";
static const char *SETTING_USE_THREADING = "use_threading";

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
std::string albumKeyword;
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
                str.append(">");
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

std::string encode_url(std::string input) {
    for(int i=0 ; i<input.length() ; i++) {
        if(input[i] == ' ') input.replace(i, 1, "%20");
    }
    return input;
}

const void update_album_info(String album, String albumArtist) {
    http::Request request{"http://musicbrainz.org/ws/2/release?query=artist:" + encode_url(std::string(albumArtist).append(" AND ").append(album)) + "&fmt=json&limit=1"};
    try {
        const auto response = request.send("GET", "", {{"User-Agent", MUSICBRAINZ_UA}});
        std::string resp = std::string{response.body.begin(), response.body.end()};

        rapidjson::Document d;
        d.Parse(resp.c_str());

        const rapidjson::Value& releases = d["releases"];
        if(releases.Size() == 0) {
            presence.largeImageKey = "logo"; // No release found
            presence.button2text = "";
            presence.button2url = "";
        } else {
            std::string mbid = releases[0]["id"].GetString();

            if(aud_get_bool(SETTING_SHOW_COVER_ART)) {
                std::string coverUrl = std::string("http://coverartarchive.org/release/").append(mbid);
                http::Request coverReq{coverUrl};
                const auto coverResp = coverReq.send("GET", "", {{"User-Agent", MUSICBRAINZ_UA}});

                if(coverResp.status.code > 307) {
                    presence.largeImageKey = "logo"; // Can't get cover art
                } else {
                    presence.largeImageKey = strdup(coverUrl.append("/front").c_str());
                }
            } else {
                presence.largeImageKey = "logo";
            }

            if(aud_get_bool(SETTING_ALBUM_BUTTON)) {
                presence.button2text = "View Album";
                presence.button2url = strdup(std::string("https://musicbrainz.org/release/").append(mbid).c_str());
            } else {
                presence.button2text = "";
                presence.button2url = "";
            }
        }
    }
    catch ( const std::system_error& e ) { // catch the error if there's no internet connection
        std::cout << "Caught system_error in update_album_info() with code "
        "[" << e.code() << "] meaning "
        "[" << e.what() << "]\n";
        presence.button2text = "";
        presence.button2url = "";
        presence.largeImageKey = "logo";
    }
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

void update_rpc() {
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
        
        if(album && albumArtist) {
            std::string newAlbumKeyword = std::string(album).append(";").append(albumArtist);
            
            if(albumKeyword != newAlbumKeyword && (aud_get_bool(SETTING_SHOW_COVER_ART) || aud_get_bool(SETTING_ALBUM_BUTTON))) {
                albumKeyword = newAlbumKeyword;
                update_album_info(album, albumArtist);
            }
        }
    } else {
        Discord_ClearPresence();
    }

    playingStatus = playingStatus.substr(0, 127);

    presence.state = playingStatus.c_str();
    update_presence();
}

void update_title_presence(void*, void*) {
    bool useThreading = aud_get_bool(SETTING_USE_THREADING);
    
    if(useThreading) {
        std::thread* t = new std::thread(update_rpc); // I saw something something about not thread-safe in the audacious src code, let's hope for the best ^_^;
        t->detach();
        delete t;
    } else {
        update_rpc();
    }
}

/* Listening require an extra playback bar that needs to be constantly updated */
void update_ascii_player() {
    while(true) {
        bool usePlaying = aud_get_bool(SETTING_USE_PLAYING);
        
        if(!usePlaying) { 
            update_rpc();
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void open_github() {
   system("xdg-open https://github.com/Kenny-Hui/audacious-plugin-rpc");
}

bool RPCPlugin::init() {
    init_discord();
    init_presence();
    hook_associate("playback ready", update_title_presence, nullptr);
    hook_associate("playback end", update_title_presence, nullptr);
    hook_associate("playback stop", update_title_presence, nullptr);
    hook_associate("playback pause", update_title_presence, nullptr);
    hook_associate("playback unpause", update_title_presence, nullptr);
    hook_associate("playback seek", update_title_presence, nullptr);
    hook_associate("title change", update_title_presence, nullptr);
    std::thread t(update_ascii_player);
    t.detach();
    return true;
}

void RPCPlugin::cleanup() {
    hook_dissociate("playback ready", update_title_presence);
    hook_dissociate("playback end", update_title_presence);
    hook_dissociate("playback stop", update_title_presence);
    hook_dissociate("playback pause", update_title_presence);
    hook_dissociate("playback unpause", update_title_presence);
    hook_dissociate("playback seek", update_title_presence);
    hook_dissociate("title change", update_title_presence);
    cleanup_discord();
}

const char RPCPlugin::about[] = N_("Discord RPC music status plugin\n\nWritten by: Derzsi Daniel <daniel@tohka.us>\nForked by: LX86 <lx86@lx862.com>");

const PreferencesWidget RPCPlugin::widgets[] =
{
  WidgetCheck(
      N_("Show \"View Album\" button (Musicbrainz)"),
      WidgetBool(0, SETTING_ALBUM_BUTTON)
  ),
  WidgetCheck(
      N_("Show Cover Art (Musicbrainz)"),
      WidgetBool(0, SETTING_SHOW_COVER_ART)
  ),
  WidgetCheck(
      N_("Use \"Playing\" status instead of \"Listening to\":"),
      WidgetBool(0, SETTING_USE_PLAYING)
  ),
  WidgetCheck(
      N_("Use threading when fetching song info"),
      WidgetBool(0, SETTING_USE_THREADING)
  ),
  WidgetButton(
      N_("Fork on GitHub"),
      {open_github}
  )
};

const PluginPreferences RPCPlugin::prefs = {{ widgets }};
