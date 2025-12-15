#ifndef MPV_PLAYER_H
#define MPV_PLAYER_H

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/mesh_instance2d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/canvas_item_material.hpp>
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
    MeshInstance2D* video_mesh = nullptr;
    MeshInstance3D* video_mesh_3d = nullptr;
    
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
    int width = 1280;
    int height = 720;
    
    // Streaming support
    bool is_streaming = false;
    int frame_count = 0;
    int stream_frame_threshold = 30;
    bool had_visible_content = false;
    
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
    
    // Basic playback controls
    void load_file(const String& path);
    void play();
    void pause();
    void stop();
    void toggle_pause();
    
    // Seeking controls
    void seek(double seconds, bool relative = true);
    void seek_to_percentage(double percentage);
    
    // Playback speed
    void set_speed(double speed);
    double get_speed() const;
    
    // Volume and audio controls
    void set_volume(double volume); // 0-100
    double get_volume() const;
    void set_mute(bool muted);
    bool is_muted() const;
    
    // Playback state queries
    bool is_playing() const;
    bool is_paused() const;
    double get_time_pos() const;
    double get_duration() const;
    double get_percentage_pos() const;
    
    // Looping controls
    void set_loop(bool enable);
    void set_loop_file(const String& mode); // "inf", "no", or number
    bool get_loop() const;
    
    // Chapter controls
    int get_chapter_count() const;
    int get_current_chapter() const;
    void set_chapter(int chapter);
    void next_chapter();
    void previous_chapter();
    
    // Track selection (audio/subtitle)
    int get_audio_track_count() const;
    int get_current_audio_track() const;
    void set_audio_track(int track_id);
    
    int get_subtitle_track_count() const;
    int get_current_subtitle_track() const;
    void set_subtitle_track(int track_id);
    void toggle_subtitles();
    
    // Video properties
    String get_media_title() const;
    int get_video_width() const;
    int get_video_height() const;
    double get_fps() const;
    String get_video_codec() const;
    String get_audio_codec() const;
    
    // Screenshot
    void screenshot(const String& filename = "", bool subtitles = true);

    // Create a video mesh for 2D display
    MeshInstance2D* create_video_mesh_2d();
    
    // Create a video mesh for 3D display
    MeshInstance3D* create_video_mesh_3d();
    
    // Apply video texture to an existing 3D mesh
    void apply_to_mesh_3d(MeshInstance3D* mesh_instance);
    
    // Apply video texture to a SubViewport
    void apply_to_viewport(SubViewport* viewport);
    
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
    
    // Render loop (runs in a separate thread)
    void render_loop();
    
    // Function to get OpenGL function pointers for MPV
    static void* get_proc_address_mpv(void* ctx, const char* name) {
        return (void*)eglGetProcAddress(name);
    }
    
    // Helper methods for MPV property access
    double get_property_double(const char* name, double default_value = 0.0) const;
    int64_t get_property_int(const char* name, int64_t default_value = 0) const;
    String get_property_string(const char* name, const String& default_value = "") const;
    bool get_property_bool(const char* name, bool default_value = false) const;
    
    void set_property_double(const char* name, double value);
    void set_property_int(const char* name, int64_t value);
    void set_property_string(const char* name, const String& value);
    void set_property_bool(const char* name, bool value);
};

#endif // MPV_PLAYER_H