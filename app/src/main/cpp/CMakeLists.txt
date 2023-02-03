cmake_minimum_required(VERSION 3.18.1)
project("doom")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -ffunction-sections -fdata-sections -std=c99 -pedantic -D_BSD_SOURCE")

add_library(native_app_glue OBJECT
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(main SHARED
        i_main.c
        am_map.c
        doomstat.c
        dstrings.c
        d_event.c
        d_items.c
        d_iwad.c
        d_loop.c
        d_main.c
        d_mode.c
        d_net.c
        f_finale.c
        f_wipe.c
        g_game.c
        hu_lib.c
        hu_stuff.c
        info.c
        i_sound.c
        i_system.c
        i_timer.c
        m_argv.c
        m_bbox.c
        m_cheat.c
        m_config.c
        m_controls.c
        m_fixed.c
        m_menu.c
        m_misc.c
        m_random.c
        p_ceilng.c
        p_doors.c
        p_enemy.c
        p_floor.c
        p_inter.c
        p_lights.c
        p_map.c
        p_maputl.c
        p_mobj.c
        p_plats.c
        p_pspr.c
        p_saveg.c
        p_setup.c
        p_sight.c
        p_spec.c
        p_switch.c
        p_telept.c
        p_tick.c
        p_user.c
        r_bsp.c
        r_data.c
        r_draw.c
        r_main.c
        r_plane.c
        r_segs.c
        r_sky.c
        r_things.c
        sha1.c
        sounds.c
        st_lib.c
        st_stuff.c
        s_sound.c
        tables.c
        v_video.c
        wi_stuff.c
        w_checksum.c
        w_file.c
        w_main.c
        w_wad.c
        z_zone.c
        i_input.c
        i_video.c
        doomgeneric.c
        doomgeneric_android.c
        Draw.c
        AndroidDriver.c)

target_include_directories(main PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(main
        android
        native_app_glue
        EGL
        GLESv2
        m
        log)
