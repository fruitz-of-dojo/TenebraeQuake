
bin_PROGRAMS = tenebrae.sdl

tenebrae_sdl_LDADD = @MATHLIB@ @INETLIB@ @OPENGLLIBS@

MASTER_DIR=..

# datadir is a variable defined by autoconf to @prefix@/share/ 
tenebrae_sdl_CFLAGS = @CFLAGS@ -DGLQUAKE -DBASEDIR=@GAMEDIR@ -I$(MASTER_DIR)


tenebrae_sdl_SOURCES =			\
	$(MASTER_DIR)/adivtab.h		\
	$(MASTER_DIR)/anorm_dots.h		\
	$(MASTER_DIR)/anorms.h		\
	$(MASTER_DIR)/asm_draw.h		\
	$(MASTER_DIR)/asm_i386.h		\
	$(MASTER_DIR)/block16.h		\
	$(MASTER_DIR)/block8.h		\
	$(MASTER_DIR)/bspfile.h		\
	$(MASTER_DIR)/cd_sdl.c		\
	$(MASTER_DIR)/cdaudio.h		\
	$(MASTER_DIR)/chase.c		\
	$(MASTER_DIR)/cl_demo.c		\
	$(MASTER_DIR)/cl_input.c		\
	$(MASTER_DIR)/cl_main.c		\
	$(MASTER_DIR)/cl_parse.c		\
	$(MASTER_DIR)/cl_tent.c		\
	$(MASTER_DIR)/client.h		\
	$(MASTER_DIR)/cmd.c			\
	$(MASTER_DIR)/cmd.h			\
	$(MASTER_DIR)/common.c		\
	$(MASTER_DIR)/common.h		\
	$(MASTER_DIR)/conproc.h		\
	$(MASTER_DIR)/console.c		\
	$(MASTER_DIR)/console.h		\
	$(MASTER_DIR)/crc.c			\
	$(MASTER_DIR)/crc.h			\
	$(MASTER_DIR)/cvar.c			\
	$(MASTER_DIR)/cvar.h			\
	$(MASTER_DIR)/draw.h			\
	$(MASTER_DIR)/host.c			\
	$(MASTER_DIR)/host_cmd.c		\
	$(MASTER_DIR)/input.h		\
	$(MASTER_DIR)/keys.c			\
	$(MASTER_DIR)/keys.h			\
	$(MASTER_DIR)/mathlib.c		\
	$(MASTER_DIR)/mathlib.h		\
	$(MASTER_DIR)/menu.c			\
	$(MASTER_DIR)/menu.h			\
	$(MASTER_DIR)/modelgen.h		\
	$(MASTER_DIR)/mpdosock.h		\
	$(MASTER_DIR)/net.h			\
	$(MASTER_DIR)/net_bsd.c		\
	$(MASTER_DIR)/net_bw.h		\
	$(MASTER_DIR)/net_dgrm.c		\
	$(MASTER_DIR)/net_dgrm.h		\
	$(MASTER_DIR)/net_loop.c		\
	$(MASTER_DIR)/net_loop.h		\
	$(MASTER_DIR)/net_main.c		\
	$(MASTER_DIR)/net_udp.c		\
	$(MASTER_DIR)/net_udp.h		\
	$(MASTER_DIR)/net_vcr.c		\
	$(MASTER_DIR)/net_vcr.h		\
	$(MASTER_DIR)/pr_cmds.c		\
	$(MASTER_DIR)/pr_comp.h		\
	$(MASTER_DIR)/pr_edict.c		\
	$(MASTER_DIR)/pr_exec.c		\
	$(MASTER_DIR)/progdefs.h		\
	$(MASTER_DIR)/progs.h			\
	$(MASTER_DIR)/protocol.h		\
	$(MASTER_DIR)/quakeasm.h		\
	$(MASTER_DIR)/quakedef.h		\
	$(MASTER_DIR)/r_part.c		\
	$(MASTER_DIR)/r_local.h		\
	$(MASTER_DIR)/r_shared.h		\
	$(MASTER_DIR)/render.h		\
	$(MASTER_DIR)/resource.h		\
	$(MASTER_DIR)/sbar.c			\
	$(MASTER_DIR)/sbar.h			\
	$(MASTER_DIR)/screen.h		\
	$(MASTER_DIR)/server.h		\
	$(MASTER_DIR)/snd_dma.c		\
	$(MASTER_DIR)/snd_mem.c		\
	$(MASTER_DIR)/snd_mix.c		\
	$(MASTER_DIR)/snd_sdl.c		\
	$(MASTER_DIR)/sound.h		\
	$(MASTER_DIR)/spritegn.h		\
	$(MASTER_DIR)/sv_main.c		\
	$(MASTER_DIR)/sv_move.c		\
	$(MASTER_DIR)/sv_phys.c		\
	$(MASTER_DIR)/sv_user.c		\
	$(MASTER_DIR)/sys.h			\
	$(MASTER_DIR)/sys_sdl.c		\
	$(MASTER_DIR)/sys_uxfindfirst.c	\
	$(MASTER_DIR)/te_scripts.c		\
	$(MASTER_DIR)/te_scripts.h		\
	$(MASTER_DIR)/vgamodes.h		\
	$(MASTER_DIR)/vid.h			\
	$(MASTER_DIR)/gl_vidsdl.c		\
	$(MASTER_DIR)/view.c			\
	$(MASTER_DIR)/view.h			\
	$(MASTER_DIR)/wad.c			\
	$(MASTER_DIR)/wad.h			\
	$(MASTER_DIR)/winquake.h		\
	$(MASTER_DIR)/world.c		\
	$(MASTER_DIR)/world.h		\
	$(MASTER_DIR)/zone.c			\
	$(MASTER_DIR)/zone.h			\
	$(GL_SRCS)			\
	$(GL_TENEBRAE_SRCS)		\
	$(LEX_TENEBRAE_SRCS)

QUAKE_SRCS =				\
	$(MASTER_DIR)/d_edge.c		\
	$(MASTER_DIR)/d_fill.c		\
	$(MASTER_DIR)/d_ifacea.h		\
	$(MASTER_DIR)/d_iface.h		\
	$(MASTER_DIR)/d_init.c		\
	$(MASTER_DIR)/d_part.c		\
	$(MASTER_DIR)/draw.c			\
	$(MASTER_DIR)/d_scan.c		\
	$(MASTER_DIR)/d_sky.c		\
	$(MASTER_DIR)/d_sprite.c		\
	$(MASTER_DIR)/d_surf.c		\
	$(MASTER_DIR)/r_aclip.c		\
	$(MASTER_DIR)/r_alias.c		\
	$(MASTER_DIR)/r_bsp.c			\
	$(MASTER_DIR)/r_draw.c		\
	$(MASTER_DIR)/r_edge.c		\
	$(MASTER_DIR)/r_efrag.c		\
	$(MASTER_DIR)/r_light.c		\
	$(MASTER_DIR)/r_main.c		\
	$(MASTER_DIR)/r_misc.c		\
	$(MASTER_DIR)/r_sky.c			\
	$(MASTER_DIR)/r_sprite.c		\
	$(MASTER_DIR)/r_surf.c		\
	$(MASTER_DIR)/r_varsa.s		\
	$(MASTER_DIR)/r_vars.c		\
	$(MASTER_DIR)/screen.c		


X86_SRCS =			\
	$(MASTER_DIR)/snd_mixa.s		\
	$(MASTER_DIR)/sys_dosa.s		\
	$(MASTER_DIR)/d_copy.s		\
	$(MASTER_DIR)/d_draw.s		\
	$(MASTER_DIR)/d_draw16.s		\
	$(MASTER_DIR)/d_parta.s		\
	$(MASTER_DIR)/d_polysa.s		\
	$(MASTER_DIR)/d_scana.s		\
	$(MASTER_DIR)/d_spr8.s		\
	$(MASTER_DIR)/d_varsa.s		\
	$(MASTER_DIR)/math.s			\
	$(MASTER_DIR)/r_aclipa.s		\
	$(MASTER_DIR)/r_aliasa.s		\
	$(MASTER_DIR)/r_drawa.s		\
	$(MASTER_DIR)/r_edgea.s		\
	$(MASTER_DIR)/surf16.s		\
	$(MASTER_DIR)/surf8.s			\
	$(MASTER_DIR)/worlda.s

NONX86_SRCS =			\
	$(MASTER_DIR)/d_vars.c		\
	$(MASTER_DIR)/nonintel.c


NULL_SRCS =			\
	$(MASTER_DIR)/cd_null.c		\
	$(MASTER_DIR)/in_null.c		\
	$(MASTER_DIR)/net_none.c		\
	$(MASTER_DIR)/snd_null.c		\
	$(MASTER_DIR)/sys_null.c		\
	$(MASTER_DIR)/vid_null.c

DOS_SRCS =			\
	$(MASTER_DIR)/cd_audio.c		\
	$(MASTER_DIR)/dos_v2.c		\
	$(MASTER_DIR)/in_dos.c		\
	$(MASTER_DIR)/mplib.c			\
	$(MASTER_DIR)/mplpc.c			\
	$(MASTER_DIR)/net_bw.c		\
	$(MASTER_DIR)/net_comx.c		\
	$(MASTER_DIR)/net_dos.c		\
	$(MASTER_DIR)/net_ipx.c		\
	$(MASTER_DIR)/net_ipx.h		\
	$(MASTER_DIR)/net_ser.c		\
	$(MASTER_DIR)/net_ser.h		\
	$(MASTER_DIR)/snd_dos.c		\
	$(MASTER_DIR)/snd_gus.c		\
	$(MASTER_DIR)/sys_dos.c		\
	$(MASTER_DIR)/vid_dos.c		\
	$(MASTER_DIR)/vid_dos.h		\
	$(MASTER_DIR)/vid_ext.c		\
	$(MASTER_DIR)/vid_vga.c		\
	$(MASTER_DIR)/vregset.c		\
	$(MASTER_DIR)/vregset.h

WIN_SRCS =			\
	$(MASTER_DIR)/cd_win.c		\
	$(MASTER_DIR)/conproc.c		\
	$(MASTER_DIR)/in_win.c		\
	$(MASTER_DIR)/net_mp.c		\
	$(MASTER_DIR)/net_mp.h		\
	$(MASTER_DIR)/net_win.c		\
	$(MASTER_DIR)/net_wins.c		\
	$(MASTER_DIR)/net_wins.h		\
	$(MASTER_DIR)/net_wipx.c		\
	$(MASTER_DIR)/net_wipx.h		\
	$(MASTER_DIR)/snd_win.c		\
	$(MASTER_DIR)/sys_win.c		\
	$(MASTER_DIR)/sys_wina.s		\
	$(MASTER_DIR)/sys_wind.c		\
	$(MASTER_DIR)/vid_win.c

LNX_SRCS =			\
	$(MASTER_DIR)/cd_linux.c		\
	$(MASTER_DIR)/snd_linux.c		\
	$(MASTER_DIR)/sys_linux.c		\
	$(MASTER_DIR)/vid_svgalib.c		\
	$(MASTER_DIR)/vid_x.c

GL_LNX_SRCS =			\
	$(MASTER_DIR)/gl_vidlinuxglx.c

GL_WIN_SRCS =			\
	$(MASTER_DIR)/gl_vidnt.c

SUN_SRCS =			\
	$(MASTER_DIR)/snd_sun.c		\
	$(MASTER_DIR)/in_sun.c		\
	$(MASTER_DIR)/sys_sun.c		\
	$(MASTER_DIR)/vid_sunx.c		\
	$(MASTER_DIR)/vid_sunxil.c

NEXT_SRCS =			\
	$(MASTER_DIR)/snd_next.c

GL_SRCS =			\
	$(MASTER_DIR)/gl_decals.c		\
	$(MASTER_DIR)/gl_draw.c		\
	$(MASTER_DIR)/gl_mesh.c		\
	$(MASTER_DIR)/gl_model.c		\
	$(MASTER_DIR)/gl_model.h		\
	$(MASTER_DIR)/gl_refrag.c		\
	$(MASTER_DIR)/gl_rlight.c		\
	$(MASTER_DIR)/gl_rmain.c		\
	$(MASTER_DIR)/gl_rmisc.c		\
	$(MASTER_DIR)/gl_rsurf.c		\
	$(MASTER_DIR)/gl_screen.c		\
	$(MASTER_DIR)/gl_test.c		\
	$(MASTER_DIR)/gl_warp.c		\
	$(MASTER_DIR)/gl_warp_sin.h		\
	$(MASTER_DIR)/glquake.h		\
	$(MASTER_DIR)/glquake2.h

GL_TENEBRAE_SRCS =		\
	$(MASTER_DIR)/glATI.h			\
	$(MASTER_DIR)/gl_aliasinstant.c	\
	$(MASTER_DIR)/gl_brushinstant.c	\
	$(MASTER_DIR)/gl_bumpdriver.c		\
	$(MASTER_DIR)/gl_bumparb.c		\
	$(MASTER_DIR)/gl_bumpgf.c		\
	$(MASTER_DIR)/gl_bumpmap.c		\
	$(MASTER_DIR)/gl_bumpparhelia.c		\
	$(MASTER_DIR)/gl_bumpradeon.c		\
	$(MASTER_DIR)/gl_common.c		\
	$(MASTER_DIR)/gl_glare.c		\
	$(MASTER_DIR)/gl_md3.c		\
	$(MASTER_DIR)/gl_screenrect.c		\
	$(MASTER_DIR)/gl_shadow.c		\
	$(MASTER_DIR)/gl_svbsp.c		

LEX_TENEBRAE_SRCS =				\
	$(MASTER_DIR)/lex_part.l		


EXTRA_DIST =			\
	$(MASTER_DIR)/sdl/configure.ac		\
	$(MASTER_DIR)/sdl/Makefile.am		\
	$(MASTER_DIR)/sdl/autogen.sh		\
	$(MASTER_DIR)/quake.ico		\
	$(MASTER_DIR)/winquake.rc		\
	$(QUAKE_SRCS)		\
	$(NULL_SRCS)		\
	$(DOS_SRCS)		\
	$(WIN_SRCS)		\
	$(LNX_SRCS)		\
	$(SUN_SRCS)		\
	$(NEXT_SRCS)		\
	$(GL_SRCS)		\
	$(GL_LNX_SRCS)		\
	$(GL_WIN_SRCS)

dist-hook:
	cp -rp $(MASTER_DIR)/win $(MASTER_DIR)/linux $(MASTER_DIR)/macosx $(MASTER_DIR)/dxsdk $(MASTER_DIR)/gas2masm $(distdir)/

