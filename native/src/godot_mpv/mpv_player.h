#ifndef MPV_PLAYER_H
#define MPV_PLAYER_H

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/texture_rect.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "enum/debug_flags.h"

#include <thread>
#include <atomic>
#include <mutex>

// EGL includes
#ifdef _WIN32
#include <EGL/egl.h>
#include <glad/gles2.h>
#elif defined(__linux__)
#include <EGL/egl.h>
#include <glad/gles2.h>
#elif defined(__ANDROID__)
#include <EGL/egl.h>
#include <glad/gles2.h>
#endif

using namespace godot;

class MPVPlayer : public Node {
    GDCLASS(MPVPlayer, Node);
    
private:

    // MPV instance
    mpv_handle* mpv = nullptr;
    mpv_render_context* mpv_ctx = nullptr;
    
    // OpenGL resources
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;
    GLuint fbo = 0;
    GLuint texture = 0;
    
    // Godot resources
    Ref<Image> frame_image;
    Ref<ImageTexture> frame_texture;
    TextureRect* target_texture_rect = nullptr;

    
    // Thread management
    std::thread render_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> frame_available{false};
    std::atomic<bool> texture_needs_update{false};
    std::atomic<bool> has_new_frame{false};
    std::mutex frame_mutex;
    
    // Frame data
    PackedByteArray pixel_data;
    PackedByteArray pending_frame_data;
    int width;
    int height;
    
    // Streaming support
    bool is_streaming = false;
    int frame_count = 0;
    int stream_frame_threshold = 30; // Allow up to 30 black frames for streaming
    bool had_visible_content = false; // Track if we've seen non-black content
    
protected:
    static void _bind_methods();
    virtual void _notification(int p_what);
    
public:
    unsigned int debug_level;

    MPVPlayer();
    ~MPVPlayer();
    
    // Override Node methods
    virtual void _process(double delta) override;
    virtual void _ready() override;
    
    // Initialize the MPV player
    bool initialize();
    
    // Load and play a video file
    void load_file(const String& path);
    void set_resolution(int new_width, int new_height);

    // Get track information
    Array get_audio_tracks();
    Array get_subtitle_tracks();

    // Set the target TextureRect
    void set_target_texture_rect(TextureRect* rect);
    
    double get_content_aspect_ratio();
    void play();
    void set_volume(String value);
    double get_volume() const;
    void set_aspect_ratio(String ratio);
    void restart();
    void set_audio_track(String id);
    void set_subtitle_track(String id);
    void set_playback_speed(String speed);
    void set_repeat_file(String value);
    void set_time_pos(double pos);
    void seek_content_pos(String pos);
    void seek(String seconds, bool relative);
    void seek_to_percentage(String pos);
    void pause();
    void stop();

    // Playback state queries
    bool is_playing() const;
    bool is_paused() const;
    double get_time_pos() const;
    double get_duration() const;
    double get_percentage_pos() const;
    
    // Get the current frame texture
    Ref<Texture2D> get_texture() const;
    
    // Get video dimensions
    int get_width() const { return width; }
    int get_height() const { return height; }

    unsigned int get_debug_level();
    void set_debug_level(unsigned int);

    // Static methods for enum godot compatibility
    static int get_debug_none() {return DEBUG_NONE;}
    static int get_debug_simple() {return DEBUG_SIMPLE;}
    static int get_debug_full() {return DEBUG_FULL;}
    
private:
    // Initialize OpenGL for rendering
    bool initialize_gl();
    
    // Update the texture with the latest frame data
    void update_texture();
    
    // Update the texture on the main thread
    void _update_texture_internal();
    
    // Update a 3D material's texture
    void update_3d_material_texture(Ref<Material> p_material, Ref<Texture2D> p_texture);
    
    // MPV render update callback
    static void on_mpv_render_update(void* ctx);

    // Helper methods for MPV property access
    double get_property_double(const char* name, double default_value = 0.0) const;
    int64_t get_property_int(const char* name, int64_t default_value = 0) const;
    String get_property_string(const char* name, const String& default_value = "") const;
    bool get_property_bool(const char* name, bool default_value = false) const;
    
    // Render loop (runs in a separate thread)
    void render_loop();
    
    // Function to get OpenGL function pointers for MPV
    static void* get_proc_address_mpv(void* ctx, const char* name) {
        return (void*)eglGetProcAddress(name);
    }
};

#endif // MPV_PLAYER_H