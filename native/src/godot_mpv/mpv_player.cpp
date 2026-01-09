#include "mpv_player.h"
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <iostream>

using namespace godot;

// Static member for callback context
static MPVPlayer* g_instance = nullptr;

void* load_func(const char* name) {
    return (void*)eglGetProcAddress(name);
}

void MPVPlayer::_bind_methods() {
    // Register methods
    ClassDB::bind_method(D_METHOD("initialize"), &MPVPlayer::initialize);
    ClassDB::bind_method(D_METHOD("load_file"), &MPVPlayer::load_file);
    ClassDB::bind_method(D_METHOD("play"), &MPVPlayer::play);
    ClassDB::bind_method(D_METHOD("set_volume", "value"), &MPVPlayer::set_volume);
    ClassDB::bind_method(D_METHOD("get_volume"), &MPVPlayer::get_volume);
    ClassDB::bind_method(D_METHOD("set_audio_track", "id"), &MPVPlayer::set_audio_track);
    ClassDB::bind_method(D_METHOD("set_subtitle_track", "id"), &MPVPlayer::set_subtitle_track);
    ClassDB::bind_method(D_METHOD("seek_content_pos", "pos"), &MPVPlayer::seek_content_pos);
    ClassDB::bind_method(D_METHOD("seek", "seconds", "relative"), &MPVPlayer::seek);
    ClassDB::bind_method(D_METHOD("seek_to_percentage", "pos"), &MPVPlayer::seek_to_percentage);
    ClassDB::bind_method(D_METHOD("add_subtitle_file", "path", "title", "lang"), &MPVPlayer::add_subtitle_file, DEFVAL(""), DEFVAL(""));

    ClassDB::bind_method(D_METHOD("set_time_pos", "pos"), &MPVPlayer::set_time_pos);
    ClassDB::bind_method(D_METHOD("set_resolution", "new_width", "new_height"), &MPVPlayer::set_resolution);
    ClassDB::bind_method(D_METHOD("set_target_texture_rect", "rect"), &MPVPlayer::set_target_texture_rect);
    ClassDB::bind_method(D_METHOD("get_audio_tracks"), &MPVPlayer::get_audio_tracks);
    ClassDB::bind_method(D_METHOD("get_subtitle_tracks"), &MPVPlayer::get_subtitle_tracks);
    ClassDB::bind_method(D_METHOD("set_aspect_ratio", "ratio"), &MPVPlayer::set_aspect_ratio);
    ClassDB::bind_method(D_METHOD("set_playback_speed", "speed"), &MPVPlayer::set_playback_speed);
    ClassDB::bind_method(D_METHOD("set_repeat_file", "value"), &MPVPlayer::set_repeat_file);
    ClassDB::bind_method(D_METHOD("set_native_subtitles_enabled", "enabled"), &MPVPlayer::set_native_subtitles_enabled);
    ClassDB::bind_method(D_METHOD("pause"), &MPVPlayer::pause);
    ClassDB::bind_method(D_METHOD("restart"), &MPVPlayer::pause); 
    ClassDB::bind_method(D_METHOD("stop"), &MPVPlayer::stop);


    // Playback state
    ClassDB::bind_method(D_METHOD("is_playing"), &MPVPlayer::is_playing);
    ClassDB::bind_method(D_METHOD("is_paused"), &MPVPlayer::is_paused);
    ClassDB::bind_method(D_METHOD("get_time_pos"), &MPVPlayer::get_time_pos);
    ClassDB::bind_method(D_METHOD("get_duration"), &MPVPlayer::get_duration);
    ClassDB::bind_method(D_METHOD("get_percentage_pos"), &MPVPlayer::get_percentage_pos);

    
    // Getters
    ClassDB::bind_method(D_METHOD("get_debug_level"), &MPVPlayer::get_debug_level);
    ClassDB::bind_method(D_METHOD("get_texture"), &MPVPlayer::get_texture);
    ClassDB::bind_method(D_METHOD("get_width"), &MPVPlayer::get_width);
    ClassDB::bind_method(D_METHOD("get_height"), &MPVPlayer::get_height);
    ClassDB::bind_method(D_METHOD("get_content_aspect_ratio"), &MPVPlayer::get_content_aspect_ratio);

    // Setters
    ClassDB::bind_method(D_METHOD("set_debug_level"), &MPVPlayer::set_debug_level);
    
    ClassDB::bind_method(D_METHOD("set_subtitle_delay", "seconds"), &MPVPlayer::set_subtitle_delay);
    ClassDB::bind_method(D_METHOD("get_subtitle_delay"), &MPVPlayer::get_subtitle_delay);


    // Register signals
    ADD_SIGNAL(MethodInfo("texture_updated", PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D")));
    ADD_SIGNAL(MethodInfo("time_changed", PropertyInfo(Variant::FLOAT, "time_pos")));

    ADD_SIGNAL(MethodInfo("buffering_started"));
    ADD_SIGNAL(MethodInfo("buffering_ended"));

    ADD_SIGNAL(MethodInfo("subtitle_changed", PropertyInfo(Variant::STRING, "text")));

    // Loading signals
    ADD_SIGNAL(MethodInfo("loading_started"));
    ADD_SIGNAL(MethodInfo("loading_finished"));
}

// ==================== Helper Methods ====================

double MPVPlayer::get_property_double(const char* name, double default_value) const {
    if (!mpv) return default_value;

    double value = default_value;
    if (mpv_get_property(mpv, name, MPV_FORMAT_DOUBLE, &value) < 0) {
        return default_value;
    }
    return value;
}


int64_t MPVPlayer::get_property_int(const char* name, int64_t default_value) const {
    if (!mpv) return default_value;

    int64_t value = default_value;
    if (mpv_get_property(mpv, name, MPV_FORMAT_INT64, &value) < 0) {
        return default_value;
    }
    return value;
}

String MPVPlayer::get_property_string(const char* name, const String& default_value) const {
    if (!mpv) return default_value;

    char* value = nullptr;
    if (mpv_get_property(mpv, name, MPV_FORMAT_STRING, &value) < 0 || !value) {
        return default_value;
    }

    String result = String::utf8(value);
    mpv_free(value);
    return result;
}

bool MPVPlayer::get_property_bool(const char* name, bool default_value) const {
    if (!mpv) return default_value;

    int value = default_value ? 1 : 0;
    if (mpv_get_property(mpv, name, MPV_FORMAT_FLAG, &value) < 0) {
        return default_value;
    }
    return value != 0;
}

// Static callback for MPV render updates
void MPVPlayer::on_mpv_render_update(void* ctx) {
    MPVPlayer* instance = static_cast<MPVPlayer*>(ctx);
    if (instance) {
        // Don't call any Godot functions from this callback
        // Just set the flag and let the main thread handle it
        instance->frame_available.store(true);
    }
}

unsigned int MPVPlayer::get_debug_level() {
    return debug_level;
}

void MPVPlayer::set_debug_level(unsigned int level) {
    debug_level = level;
}

MPVPlayer::MPVPlayer() : 
    debug_level(DEBUG_NONE),
    mpv(nullptr), 
    mpv_ctx(nullptr),
    fbo(0),
    texture(0),
    width(1920),
    height(1080),
    target_texture_rect(nullptr),
    running(false),
    frame_available(false),
    texture_needs_update(false),
    has_new_frame(false),
    is_streaming(false),
	native_subtitles_enabled(false),
	last_subtitle_text(""),
    frame_count(0),
    stream_frame_threshold(30), // Allow up to 30 black frames for streaming
    egl_display(EGL_NO_DISPLAY),
    egl_surface(EGL_NO_SURFACE),
    egl_context(EGL_NO_CONTEXT),
    is_buffering(false) {
    
    g_instance = this;
    
    // Initialize image with default data to avoid "empty image" errors
    frame_image.instantiate();
    frame_image->create(width, height, false, Image::FORMAT_RGBA8);
    
    // Fill with black pixels
    PackedByteArray initial_data;
    initial_data.resize(width * height * 4);
    for (int i = 0; i < initial_data.size(); i += 4) {
        initial_data.set(i, 0);     // R
        initial_data.set(i + 1, 0); // G
        initial_data.set(i + 2, 0); // B
        initial_data.set(i + 3, 255); // A (fully opaque)
    }
    frame_image->set_data(width, height, false, Image::FORMAT_RGBA8, initial_data);
    
    // Create texture from the initialized image
    frame_texture = ImageTexture::create_from_image(frame_image);
    
    // Initialize pixel data buffer
    pixel_data.resize(width * height * 4);
    pending_frame_data.resize(width * height * 4);
}

MPVPlayer::~MPVPlayer() {
    // Stop the render thread
    if (running.load()) {
        running.store(false);
        if (render_thread.joinable()) {
            render_thread.join();
        }
    }
    
    // Clean up MPV resources
    if (mpv_ctx) {
        mpv_render_context_free(mpv_ctx);
        mpv_ctx = nullptr;
    }
    
    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
    
    // Clean up OpenGL resources
    if (fbo != 0) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    
    // Reset static instance
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

void MPVPlayer::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_PREDELETE: {
            // Stop the render thread
            if (running.load()) {
                running.store(false);
                if (render_thread.joinable()) {
                    render_thread.join();
                }
            }
            
            // Clean up MPV resources
            if (mpv_ctx) {
                mpv_render_context_free(mpv_ctx);
                mpv_ctx = nullptr;
            }
            
            if (mpv) {
                mpv_terminate_destroy(mpv);
                mpv = nullptr;
            }
            
            // Clean up OpenGL resources
            if (egl_display != EGL_NO_DISPLAY) {
                if (egl_context != EGL_NO_CONTEXT) {
                    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroyContext(egl_display, egl_context);
                    egl_context = EGL_NO_CONTEXT;
                }
                
                if (egl_surface != EGL_NO_SURFACE) {
                    eglDestroySurface(egl_display, egl_surface);
                    egl_surface = EGL_NO_SURFACE;
                }
                
                eglTerminate(egl_display);
                egl_display = EGL_NO_DISPLAY;
            }
            
            // Clean up OpenGL resources
            if (fbo != 0) {
                glDeleteFramebuffers(1, &fbo);
                fbo = 0;
            }
            
            if (texture != 0) {
                glDeleteTextures(1, &texture);
                texture = 0;
            }
            
            break;
        }
    }
}

void MPVPlayer::set_resolution(int new_width, int new_height) {
    this->width = new_width;
    this->height = new_height;
}

void MPVPlayer::set_target_texture_rect(TextureRect* rect) {
    target_texture_rect = rect;

    // If we already have a texture, apply it immediately
    if (target_texture_rect && frame_texture.is_valid()) {
        target_texture_rect->set_texture(frame_texture);
    }
}

bool MPVPlayer::initialize() {
    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
    UtilityFunctions::print("Starting MPV player initialization");
    
    // Create MPV instance
    mpv = mpv_create();
    if (!mpv) {
        UtilityFunctions::print("Failed to create MPV instance");
        return false;
    }
    
    // Set basic MPV options
    mpv_set_option_string(mpv, "vo", "libmpv");
    mpv_set_option_string(mpv, "hwdec", "auto-safe");
    mpv_set_option_string(mpv, "profile", "fast");
    mpv_set_option_string(mpv, "video-sync", "display");
    
    // Set network-related options for better HTTP streaming support
    mpv_set_option_string(mpv, "network-timeout", "15"); // 15 seconds timeout
    mpv_set_option_string(mpv, "user-agent", "Stremio");
    
    // Enable verbose logging in debug mode
    if (debug_level == DEBUG_FULL) {
        mpv_request_log_messages(mpv, "v"); // Verbose logging
        mpv_set_option_string(mpv, "msg-level", "all=v");
    }
    
    // Initialize MPV
    int init_result = mpv_initialize(mpv);
    if (init_result < 0) {
        UtilityFunctions::print("Failed to initialize MPV: ", mpv_error_string(init_result));
        return false;
    }
    
    // // Set up property observation for debugging
    // if (debug_level == DEBUG_FULL) {
    //     // Observe network-related properties
    //     mpv_observe_property(mpv, 0, "demuxer-cache-state", MPV_FORMAT_NODE);
    //     mpv_observe_property(mpv, 0, "paused-for-cache", MPV_FORMAT_FLAG);
    // }

    mpv_observe_property(mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 2, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 3, "core-idle", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 4, "sub-text", MPV_FORMAT_STRING);

    // Initialize OpenGL rendering
    if (!initialize_gl()) {
        UtilityFunctions::print("Failed to initialize OpenGL");
        return false;
    }
    
    // Set up MPV render context
    mpv_opengl_init_params gl_init_params = {
        .get_proc_address = MPVPlayer::get_proc_address_mpv,
        .get_proc_address_ctx = nullptr
    };
    
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    
    if (mpv_render_context_create(&mpv_ctx, mpv, params) < 0) {
        UtilityFunctions::print("Failed to create MPV render context");
        return false;
    }
    
    // Set up render update callback
    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
    UtilityFunctions::print("Setting up render update callback");
    mpv_render_context_set_update_callback(mpv_ctx, on_mpv_render_update, this);
    
    // We'll start the render thread in _ready to ensure all Godot objects are properly initialized
    // This helps avoid thread safety issues
    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
    UtilityFunctions::print("MPV player initialized");
    return true;
}

void MPVPlayer::_ready() {
    // Start render thread
    running.store(true);
    render_thread = std::thread(&MPVPlayer::render_loop, this);
}

bool MPVPlayer::initialize_gl() {
    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
    UtilityFunctions::print("Initializing OpenGL for MPV rendering");
    
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        UtilityFunctions::print("failed to init egl display");
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        UtilityFunctions::print("failed to init egl");
    }

    const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
        UtilityFunctions::print("failed to apply egl config");
    }
    
    const EGLint pbuffer_attribs[] = {
    EGL_WIDTH, 1,
    EGL_HEIGHT, 1,
    EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(display, config, pbuffer_attribs);

    const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);

    if (!eglMakeCurrent(display, surface, surface, context)) {
        UtilityFunctions::print("could not make egl context current");
    }
    
    if (!gladLoadGLES2((GLADloadfunc)load_func)) {
        UtilityFunctions::print("eglGetProcName failed");
    }

    // Create FBO for rendering
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    // Create texture for the FBO
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Allocate texture storage
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    // Attach texture to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    
    // Check FBO status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        UtilityFunctions::print("ERROR: Framebuffer is not complete: ", status);
        return false;
    }
    
    // Initialize pixel data buffer
    pixel_data.resize(width * height * 4);
    pending_frame_data.resize(width * height * 4);
    
    // Unbind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
    UtilityFunctions::print("OpenGL initialized successfully");
    return true;
}

void MPVPlayer::render_loop() {
    // This is a minimal render loop that only handles MPV render updates
    // It avoids any direct OpenGL operations that might cause thread safety issues
    
    while (running.load()) {
        if (frame_available.exchange(false)) {
            texture_needs_update.store(true);
        }
            
            // Sleep to avoid busy waiting
            // std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
}

void MPVPlayer::update_texture() {
    // This method is called from the main thread
    
    // Check if there's new frame data available
    if (texture_needs_update.load()) {
        _update_texture_internal();
    } else {
        // Make sure MPV updates its internal state
        if (mpv && mpv_ctx) {
            mpv_render_context_update(mpv_ctx);
        }
    }
}

void MPVPlayer::_update_texture_internal() {
    // This method runs on the main thread
    
    // Create a new local image and update it with the pixel data
    Ref<Image> new_image;
    new_image.instantiate();
    new_image->create(width, height, false, Image::FORMAT_RGBA8);
    
    {
        std::lock_guard<std::mutex> lock(frame_mutex);

        new_image->set_data(width, height, false, Image::FORMAT_RGBA8, pending_frame_data);
    }
    
    // Create a new texture from the image
    Ref<ImageTexture> new_texture = ImageTexture::create_from_image(new_image);
    
    // Only update the reference if we successfully created a new texture
    if (new_texture.is_valid()) {
        frame_image = new_image;
        frame_texture = new_texture;


        // Update the TextureRect if set
        if (target_texture_rect) {
            target_texture_rect->set_texture(frame_texture);
        }
        
        // Add debug info to verify texture content
        if(debug_level == DEBUG_FULL)
            UtilityFunctions::print("Created texture with size: ", width, "x", height);
        
        // Emit signal for texture update (useful for 3D and SubViewport usage)
        if(debug_level == DEBUG_FULL)
            UtilityFunctions::print("Emitting texture_updated signal");
        emit_signal("texture_updated", frame_texture);
    } else {
        UtilityFunctions::print("ERROR: Failed to create valid texture from image");
    }
}

void MPVPlayer::_process(double delta) {
    // This method runs on the main thread
    
    // Check if we need to update the texture
    if (texture_needs_update.load()) {
        // Reset the flag at the beginning to avoid missing frames
        texture_needs_update.store(false);
        
        // Perform all OpenGL operations on the main thread
        if (mpv_ctx && fbo) {
            // Bind our FBO for rendering
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            
            // Set up FBO for rendering
            mpv_opengl_fbo mpv_fbo = {
                .fbo = static_cast<int>(fbo),
                .w = width,
                .h = height,
                .internal_format = 0
            };
            
            // Rendering parameters
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
                {MPV_RENDER_PARAM_INVALID, nullptr}
            };
            
            // Render frame to FBO
            int render_result = mpv_render_context_render(mpv_ctx, params);
            
            // Read pixels from FBO
            {
                std::lock_guard<std::mutex> lock(frame_mutex);
                
                // Make sure we're in the correct framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                
                // Don't clear the buffer as it would erase the MPV rendering
                // glClear(GL_COLOR_BUFFER_BIT);
                
                // Read pixels - make sure we're reading RGBA data
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data.ptrw());
                
                uint8_t* data = (uint8_t*)pixel_data.ptrw();
                
                memcpy(pending_frame_data.ptrw(), pixel_data.ptr(), width * height * 4);
                has_new_frame.store(true);

            }
            
            // Unbind our FBO to restore the default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            // Now update the texture with the new frame data
            _update_texture_internal();
        }
    }
    
    // Handle any logging that was requested from the render thread
    if (has_new_frame.load()) {
        has_new_frame.store(false);
    }
    
    // Make sure MPV updates its internal state
    if (mpv) {
        mpv_event* event = mpv_wait_event(mpv, 0);
        double time_pos = 0.0;
        double duration = 0.0;
        while (event->event_id != MPV_EVENT_NONE) {
            // Process MPV events
            switch (event->event_id) {
                case MPV_EVENT_FILE_LOADED:
                    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
                        UtilityFunctions::print("File loaded successfully");
                    
                    // Reset frame counter and content flag when a new file is loaded
                    frame_count = 0;
                    had_visible_content = false;

                    emit_signal("loading_finished");
                    break;
                    
                case MPV_EVENT_PLAYBACK_RESTART:
                    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
                        UtilityFunctions::print("Playback restarted");
                    break;

                    
                case MPV_EVENT_END_FILE:
                    {
                        mpv_event_end_file* end_file = static_cast<mpv_event_end_file*>(event->data);
                        
                        // Always log errors
                        if (end_file->reason == MPV_END_FILE_REASON_ERROR) {
                            UtilityFunctions::print("ERROR: Playback failed with error code: ", end_file->error);
                            
                            // Get the error string
                            const char* err_str = mpv_error_string(end_file->error);
                            if (err_str) {
                                UtilityFunctions::print("MPV Error: ", err_str);
                            }
                            
                            // For streaming, try to provide more specific error information
                            if (is_streaming) {
                                UtilityFunctions::print("HTTP stream playback failed. Possible causes:");
                                UtilityFunctions::print("- Network connectivity issues");
                                UtilityFunctions::print("- Unsupported codec or format");
                                UtilityFunctions::print("- Invalid URL or stream");
                                
                                // Try to get more diagnostic information
                                char* media_title = nullptr;
                                if (mpv_get_property(mpv, "media-title", MPV_FORMAT_STRING, &media_title) >= 0 && media_title) {
                                    UtilityFunctions::print("Media title: ", media_title);
                                    mpv_free(media_title);
                                }
                            }
                        } else if (debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL) {
                            // Log normal end-of-file events only if debug is enabled
                            UtilityFunctions::print("Playback ended with reason: ", end_file->reason);
                        }
                    }
                    break;
                    
                case MPV_EVENT_LOG_MESSAGE:
                    if(debug_level == DEBUG_FULL) {
                        mpv_event_log_message* msg = static_cast<mpv_event_log_message*>(event->data);
                        UtilityFunctions::print("MPV Log [", msg->prefix, "]: ", msg->text);
                    }
                    break;
                    
                case MPV_EVENT_PROPERTY_CHANGE: {
                    mpv_event_property *prop = static_cast<mpv_event_property *>(event->data);
                    if (!prop || !prop->data) break;
                    switch (event->reply_userdata)
                    {
                    case 1:
                        time_pos = *static_cast<double *>(prop->data);
                        call_deferred("emit_signal", "time_changed", time_pos);
                        break;
                    case 2: {
                            bool paused_for_cache = *static_cast<int *>(prop->data) != 0;
                            if (paused_for_cache && !is_buffering) {
                                is_buffering = true;
                                call_deferred("emit_signal", "buffering_started");
                            } else if (!paused_for_cache && is_buffering) {
                                is_buffering = false;
                                call_deferred("emit_signal", "buffering_ended");
							}
                            break;
                    }
					case 3: {
                           bool core_idle = *static_cast<int*>(prop->data);
                           if (core_idle && !is_buffering) {
                               is_buffering = true;
                               call_deferred("emit_signal", "buffering_started");
                           }
                           else if (!core_idle && is_buffering) {
                               is_buffering = false;
                               call_deferred("emit_signal", "buffering_ended");
                           }
                           break;
                    }
                    case 4: {
                        if (prop->format == MPV_FORMAT_STRING && prop->data) {
                            char* sub_text = *static_cast<char**>(prop->data);
                            if (sub_text != nullptr) {
                                String subtitle_text = String::utf8(sub_text);
                                // Only emit if text changed to avoid spam
                                if (subtitle_text != last_subtitle_text) {
                                    last_subtitle_text = subtitle_text;
                                    call_deferred("emit_signal", "subtitle_changed", subtitle_text);
                                }
                            }
                        }
                        else {
                            // No subtitle or subtitle cleared
                            if (!last_subtitle_text.is_empty()) {
                                last_subtitle_text = "";
                                call_deferred("emit_signal", "subtitle_changed", String(""));
                            }
                        }
                          }
                    }
                    break;
                    }
            }
            
            event = mpv_wait_event(mpv, 0);
        }
    }
}

void MPVPlayer::load_file(const String& path) {
    UtilityFunctions::print("Loading video: ", path);
    
    // Check if MPV is initialized
    if (!mpv) {
        UtilityFunctions::print("MPV is not initialized");
        return;
    }
    
    // Check if this is a streaming URL
    is_streaming = path.begins_with("http://") || path.begins_with("https://");
    
    // Reset frame counter and content flag when loading a new file/stream
    frame_count = 0;
    had_visible_content = false;
    
    if (is_streaming) {
        UtilityFunctions::print("Detected HTTP stream, enabling streaming mode");
     

        mpv_set_option_string(mpv, "network-timeout", "60"); // 60 seconds timeout (default in mpv)
        mpv_set_option_string(mpv, "demuxer-readahead-secs", "20"); // Read ahead 20 seconds
        mpv_set_option_string(mpv, "cache", "yes"); // Enable cache
        mpv_set_option_string(mpv, "cache-secs", "15"); // Cache 30 seconds (more generous)
        mpv_set_option_string(mpv, "force-seekable", "yes"); // Try to make stream seekable
        mpv_set_option_string(mpv, "hr-seek", "yes");
        mpv_set_option_string(mpv, "hr-seek-demuxer-offset", "1.5");
        mpv_set_option_string(mpv, "stream-buffer-size", "10M");
        
        // Set these to match command-line behavior
        // mpv_set_option_string(mpv, "audio-file-auto", "no"); // Don't load external audio
        // mpv_set_option_string(mpv, "sub-auto", "no"); // Don't load subtitles

        // mpv_set_option_string(mpv, "stream-lavf-o", headers.utf8().get_data());
        
        UtilityFunctions::print("Applied streaming-specific MPV options");
    } else {
        // Local file options
        mpv_set_option_string(mpv, "cache", "auto");
    }
    
    // Convert Godot String to C string - we need to keep the CharString alive
    // until after the command is executed to prevent the pointer from becoming invalid
    CharString cs = path.utf8();
    const char* c_path = cs.get_data();
    
    if (c_path == nullptr || c_path[0] == '\0') {
        UtilityFunctions::print("ERROR: Invalid empty path");
        return;
    }
    
    UtilityFunctions::print("Loading path: '", c_path, "'");
    
    // For HTTP streams, use synchronous command to match command-line behavior
    // This ensures the command completes before continuing
    if (is_streaming) {
        const char* cmd[] = {"loadfile", c_path, nullptr};
        int result = mpv_command(mpv, cmd);
        if (result < 0) {
            UtilityFunctions::print("Error loading stream: ", mpv_error_string(result));
        } else {
            UtilityFunctions::print("Stream loaded successfully");
        }
    } else {
        // For local files, use async command as before
        const char* cmd[] = {"loadfile", c_path, nullptr};
        mpv_command_async(mpv, 0, cmd);
    }
}

void MPVPlayer::play() {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    if(debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
    UtilityFunctions::print("Starting playback");
    
    const char* cmd[] = {"set", "pause", "no", nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::set_volume(String value) {
        if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"set", "volume", value.utf8().get_data(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

double MPVPlayer::get_volume() const {
    return get_property_double("volume", 100.0);
}

void MPVPlayer::set_aspect_ratio(String ratio) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"set", "video-aspect-override", ratio.utf8().get_data(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::set_playback_speed(String speed) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"set", "speed", speed.utf8().get_data(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::set_repeat_file(String value) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"set", "loop-file", value.utf8().get_data(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::restart() {
        if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"seek", "0", "absolute", nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::set_audio_track(String id) {
        if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"set", "aid", id.utf8().get_data(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::set_subtitle_track(String id) {
        if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* cmd[] = {"set", "sid", id.utf8().get_data(), nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::add_subtitle_file(String path, String title, String lang) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }

    CharString cs = path.utf8();
    const char* c_path = cs.get_data();

    if (c_path == nullptr || c_path[0] == '\0') {
        UtilityFunctions::print("ERROR: Invalid empty subtitle path");
        return;
    }

    if (debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
        UtilityFunctions::print("Adding external subtitle file: ", path);


    CharString title_cs = title.utf8();
    CharString lang_cs = lang.utf8();
    const char* cmd[] = { "sub-add", c_path, "auto", title_cs.get_data(), lang_cs.get_data(), nullptr };
    int result = mpv_command(mpv, cmd);

    if (result < 0) {
        UtilityFunctions::print("Error loading subtitle file: ", mpv_error_string(result));
    }
    else {
        if (debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL)
            UtilityFunctions::print("Subtitle file loaded successfully");
    }
}


double MPVPlayer::get_content_aspect_ratio() {
    int width;
    int height;
    double aspect;

    mpv_get_property(mpv, "video-params/w", MPV_FORMAT_INT64, &width);
    mpv_get_property(mpv, "video-params/h", MPV_FORMAT_INT64, &height);

    return aspect = (double)width / (double)height;
}

void MPVPlayer::seek_content_pos(String pos) {
        if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* seek_cmd[] = {"seek", pos.utf8().get_data(), "absolute", nullptr};
    mpv_command(mpv, seek_cmd);
}


void MPVPlayer::seek(String seconds, bool relative) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }

    const char* seek_cmd[] = { "seek", seconds.utf8().get_data(),  relative ? "relative" : "absolute", nullptr };
    mpv_command(mpv, seek_cmd);
}

void MPVPlayer::seek_to_percentage(String pos) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    const char* seek_cmd[] = { "seek", pos.utf8().get_data(), "absolute-percent", nullptr };
    mpv_command(mpv, seek_cmd);
}

// ==================== Playback State ====================

bool MPVPlayer::is_playing() const {
    return !is_paused();
}

bool MPVPlayer::is_paused() const {
    return get_property_bool("pause", true);
}

double MPVPlayer::get_time_pos() const {
    return get_property_double("time-pos", 0.0);
}

double MPVPlayer::get_duration() const {
    return get_property_double("duration", 0.0);
}

double MPVPlayer::get_percentage_pos() const {
    return get_property_double("percent-pos", 0.0);
}

void MPVPlayer::set_time_pos(double pos) {
        if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    mpv_set_property_string(mpv, "pause", "yes");
    mpv_set_property_async(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE, &pos);
    mpv_set_property_string(mpv, "pause", "no");
}

void MPVPlayer::pause() {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    
    const char* cmd[] = {"set", "pause", "yes", nullptr};
    mpv_command_async(mpv, 0, cmd);
}

void MPVPlayer::stop() {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }
    
    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(mpv, 0, cmd);
}

Ref<Texture2D> MPVPlayer::get_texture() const {
    return frame_texture;
}

Array MPVPlayer::get_audio_tracks() {
    Array tracks;

    if (!mpv) {
        return tracks;
    }

    mpv_node track_list;
    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &track_list) < 0) {
        return tracks;
    }

    if (track_list.format != MPV_FORMAT_NODE_ARRAY) {
        mpv_free_node_contents(&track_list);
        return tracks;
    }

    for (int i = 0; i < track_list.u.list->num; i++) {
        mpv_node* track = &track_list.u.list->values[i];

        if (track->format != MPV_FORMAT_NODE_MAP) {
            continue;
        }

        Dictionary track_info;
        const char* type = nullptr;

        for (int j = 0; j < track->u.list->num; j++) {
            const char* key = track->u.list->keys[j];
            mpv_node* value = &track->u.list->values[j];

            if (strcmp(key, "type") == 0 && value->format == MPV_FORMAT_STRING) {
                type = value->u.string;
            }
            else if (strcmp(key, "id") == 0 && value->format == MPV_FORMAT_INT64) {
                track_info["id"] = (int)value->u.int64;
            }
            else if (strcmp(key, "lang") == 0 && value->format == MPV_FORMAT_STRING) {
                track_info["lang"] = String::utf8(value->u.string);
            }
            else if (strcmp(key, "title") == 0 && value->format == MPV_FORMAT_STRING) {
                track_info["title"] = String::utf8(value->u.string);
            }
            else if (strcmp(key, "selected") == 0 && value->format == MPV_FORMAT_FLAG) {
                track_info["selected"] = (bool)value->u.flag;
            }
        }

        if (type && strcmp(type, "audio") == 0) {
            tracks.append(track_info);
        }
    }

    mpv_free_node_contents(&track_list);
    return tracks;
}

Array MPVPlayer::get_subtitle_tracks() {
    Array tracks;

    if (!mpv) {
        return tracks;
    }

    mpv_node track_list;
    if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &track_list) < 0) {
        return tracks;
    }

    if (track_list.format != MPV_FORMAT_NODE_ARRAY) {
        mpv_free_node_contents(&track_list);
        return tracks;
    }

    for (int i = 0; i < track_list.u.list->num; i++) {
        mpv_node* track = &track_list.u.list->values[i];

        if (track->format != MPV_FORMAT_NODE_MAP) {
            continue;
        }

        Dictionary track_info;
        const char* type = nullptr;

        for (int j = 0; j < track->u.list->num; j++) {
            const char* key = track->u.list->keys[j];
            mpv_node* value = &track->u.list->values[j];

            if (strcmp(key, "type") == 0 && value->format == MPV_FORMAT_STRING) {
                type = value->u.string;
            }
            else if (strcmp(key, "id") == 0 && value->format == MPV_FORMAT_INT64) {
                track_info["id"] = (int)value->u.int64;
            }
            else if (strcmp(key, "lang") == 0 && value->format == MPV_FORMAT_STRING) {
                track_info["lang"] = String::utf8(value->u.string);
            }
            else if (strcmp(key, "title") == 0 && value->format == MPV_FORMAT_STRING) {
                track_info["title"] = String::utf8(value->u.string);
            }
            else if (strcmp(key, "selected") == 0 && value->format == MPV_FORMAT_FLAG) {
                track_info["selected"] = (bool)value->u.flag;
            }
        }

        if (type && strcmp(type, "sub") == 0) {
            tracks.append(track_info);
        }
    }

    mpv_free_node_contents(&track_list);
    return tracks;
}

void MPVPlayer::set_native_subtitles_enabled(bool enabled) {
    if (!mpv) {
        ERR_PRINT("MPV not initialized");
        return;
    }

    native_subtitles_enabled = enabled;

    if (enabled) {
        // Show native subtitles
        mpv_set_option_string(mpv, "sub-visibility", "yes");
    }
    else {
        // Hide native subtitles (but still emit subtitle_changed signal)
        mpv_set_option_string(mpv, "sub-visibility", "no");
    }
}

void MPVPlayer::set_subtitle_delay(String seconds) {
    if (!mpv) {
        UtilityFunctions::print("MPV not initialized");
        return;
    }

    //mpv_set_property_string(mpv, "sub-delay", seconds);

    const char* cmd[] = { "set", "sub-delay", seconds.utf8().get_data(), nullptr };
    mpv_command_async(mpv, 0, cmd);

    if (debug_level == DEBUG_SIMPLE || debug_level == DEBUG_FULL) {
        UtilityFunctions::print("Subtitle delay set to: ", seconds, " seconds");
    }
}

double MPVPlayer::get_subtitle_delay() const {
    return get_property_double("sub-delay", 0.0);
}
