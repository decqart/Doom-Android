CC=gcc

CFLAGS=-Wall -Os
LDFLAGS=-Wl,--gc-sections -lm -lX11

OBJDIR=build
OUTPUT=doom

SRC_DOOM = i_main.o am_map.o doomstat.o dstrings.o d_event.o d_items.o d_iwad.o d_loop.o d_main.o d_mode.o d_net.o f_finale.o f_wipe.o g_game.o hu_lib.o hu_stuff.o info.o i_scale.o i_sound.o i_system.o i_timer.o m_argv.o m_bbox.o m_cheat.o m_config.o m_controls.o m_fixed.o m_menu.o m_misc.o m_random.o p_ceilng.o p_doors.o p_enemy.o p_floor.o p_inter.o p_lights.o p_map.o p_maputl.o p_mobj.o p_plats.o p_pspr.o p_saveg.o p_setup.o p_sight.o p_spec.o p_switch.o p_telept.o p_tick.o p_user.o r_bsp.o r_data.o r_draw.o r_main.o r_plane.o r_segs.o r_sky.o r_things.o sha1.o sounds.o st_lib.o st_stuff.o s_sound.o tables.o v_video.o wi_stuff.o w_checksum.o w_file.o w_main.o w_wad.o z_zone.o i_input.o i_video.o doomgeneric.o doomgeneric_xlib.o
OBJS += $(addprefix $(OBJDIR)/, $(SRC_DOOM))

all: doom

clean:
	rm -rf $(OBJDIR)
	rm -f $(OUTPUT)

doom: $(OBJS)
	@echo [Linking $@]
	$(CC) $(CFLAGS) $(OBJS) \
	-o $(OUTPUT) $(LDFLAGS)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o:	%.c
	@echo [Compiling $<]
	$(CC) $(CFLAGS) -c $< -o $@
