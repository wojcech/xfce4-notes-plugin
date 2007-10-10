/*  $Id$
 *
 *  Copyright (c) 2006 Mike Massonnet <mmassonnet@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <libxfce4util/libxfce4util.h>

#include "notes.h"

#define PLUGIN_NAME "xfce4-notes-plugin"



static void             notes_window_menu_new           (NotesWindow *notes_window);

static void             notes_window_menu_popup         (NotesWindow *notes_window);

static void             notes_window_menu_position      (GtkMenu *menu,
                                                         gint *x,
                                                         gint *y,
                                                         gboolean *push_in,
                                                         gpointer user_data);
static void             notes_window_menu_detach        (NotesWindow *notes_window);

static void             notes_window_set_sos_always     (NotesWindow *notes_window);

static void             notes_window_set_sos_never      (NotesWindow *notes_window);

static void             notes_window_set_sos_last_state (NotesWindow *notes_window);

static void             notes_window_set_statusbar      (NotesWindow *notes_window);

static void             notes_window_set_above          (NotesWindow *notes_window);

static void             notes_window_set_sticky         (NotesWindow *notes_window);

static gboolean         notes_window_state_event        (NotesWindow *notes_window,
                                                         GdkEventWindowState *event);
static gboolean         notes_window_start_move         (NotesWindow *notes_window,
                                                         GdkEventButton *event);
static gboolean         notes_window_scroll_event       (NotesWindow *notes_window,
                                                         GdkEventScroll *event);
static void             notes_window_shade              (NotesWindow *notes_window);

static void             notes_window_unshade            (NotesWindow *notes_window);

static void             notes_window_rename_note_dialog (NotesWindow *notes_window);

static void             notes_window_rename_dialog      (NotesWindow *notes_window);

static void             notes_window_rename             (NotesWindow *notes_window,
                                                         const gchar *name);
static inline NotesNote * notes_window_get_current_note (NotesWindow *notes_window);

static void             notes_window_add_note           (NotesWindow *notes_window);

static void             notes_window_delete_note        (NotesWindow *notes_window);

static gboolean         notes_note_save_data            (NotesNote *notes_note);

static inline void      notes_note_sort_names           (NotesNote *notes_note);

static gint             notes_note_strcasecmp           (NotesNote *notes_note0,
                                                         NotesNote *notes_note1);
static void             notes_note_rename_dialog        (NotesNote *notes_note);

static void             notes_note_rename               (NotesNote *notes_note,
                                                         const gchar *name);
static void             notes_note_buffer_changed       (NotesNote *notes_note);

static gboolean         notes_note_key_pressed          (NotesNote *notes_note,
                                                         GdkEventKey *event);


const gchar *
notes_window_read_name (NotesPlugin *notes_plugin)
{
  static GDir          *dir = NULL;
  static const gchar   *window_name = NULL;

  if (G_UNLIKELY (dir == NULL))
    dir = g_dir_open (notes_plugin->notes_path, 0, NULL);

  window_name = g_dir_read_name (dir);
  TRACE ("window_name: %s", window_name);
  if (G_UNLIKELY (window_name == NULL))
    {
      g_dir_close (dir);
      dir = NULL;
    }

  return window_name;
}

NotesWindow *
notes_window_new (NotesPlugin *notes_plugin)
{
  /* Convenience function for signal callbacks */
  return notes_window_new_with_label (notes_plugin, NULL);
}

NotesWindow *
notes_window_new_with_label (NotesPlugin *notes_plugin,
                             const gchar *window_name)
{
  DBG ("New window: %s", window_name);

  NotesWindow          *notes_window;
  GtkWidget            *img_add, *img_del, *img_close, *arrow_menu;
  gchar                *window_name_tmp;
  gchar                *accel_name;

  notes_window = g_slice_new0 (NotesWindow);
  notes_window->notes_plugin = notes_plugin;
  notes_window->notes = NULL;
  notes_window->name = g_strdup (window_name);

  /* Window */
  notes_window->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_deletable (GTK_WINDOW (notes_window->window), FALSE);
  gtk_window_set_title (GTK_WINDOW (notes_window->window), window_name);
  gtk_window_set_default_size (GTK_WINDOW (notes_window->window), 375, 430);
  gtk_window_set_decorated (GTK_WINDOW (notes_window->window), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (notes_window->window), "xfce4-notes-plugin");
  gtk_widget_set_name (notes_window->window, PLUGIN_NAME);

  /* Frame */
  notes_window->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (notes_window->frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (notes_window->window),
                     notes_window->frame);
  gtk_widget_show (notes_window->frame);

  /* Vertical box */
  notes_window->vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_set_spacing (GTK_BOX (notes_window->vbox), 1);
  gtk_container_add (GTK_CONTAINER (notes_window->frame),
                     notes_window->vbox);
  gtk_widget_show (notes_window->vbox);

  /* Horizontal box */
  notes_window->hbox = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (notes_window->vbox),
                      notes_window->hbox,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show (notes_window->hbox);

  /* Add button */
  notes_window->btn_add = xfce_create_panel_button ();
  gtk_widget_set_size_request (notes_window->btn_add, 22, 22);
  img_add = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (notes_window->btn_add),
                     img_add);
  gtk_box_pack_start (GTK_BOX (notes_window->hbox),
                      notes_window->btn_add,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show_all (notes_window->btn_add);

  /* Remove button */
  notes_window->btn_del = xfce_create_panel_button ();
  gtk_widget_set_size_request (notes_window->btn_del, 22, 22);
  img_del = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (notes_window->btn_del),
                     img_del);
  gtk_box_pack_start (GTK_BOX (notes_window->hbox),
                      notes_window->btn_del,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show_all (notes_window->btn_del);

  /* Event box move */
  notes_window->eb_move = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (notes_window->eb_move), FALSE);
  gtk_box_pack_start (GTK_BOX (notes_window->hbox),
                      notes_window->eb_move,
                      TRUE,
                      TRUE,
                      0);
  gtk_widget_show (notes_window->eb_move);
  gtk_widget_realize (notes_window->eb_move);

  /* Title */
  window_name_tmp = g_strdup_printf ("<b>%s</b>", window_name);
  notes_window->title = gtk_label_new (window_name_tmp);
  g_free (window_name_tmp);
  gtk_label_set_use_markup (GTK_LABEL (notes_window->title), TRUE);
  gtk_container_add (GTK_CONTAINER (notes_window->eb_move),
                     notes_window->title);
  gtk_widget_show (notes_window->title);

  /* Menu button */
  notes_window->btn_menu = xfce_create_panel_toggle_button ();
  gtk_widget_set_size_request (notes_window->btn_menu, 22, 22);
  arrow_menu = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (notes_window->btn_menu),
                     arrow_menu);
  gtk_box_pack_start (GTK_BOX (notes_window->hbox),
                      notes_window->btn_menu,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show_all (notes_window->btn_menu);

  /* Close button */
  notes_window->btn_close = xfce_create_panel_button ();
  gtk_widget_set_size_request (notes_window->btn_close, 22, 22);
  img_close = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (notes_window->btn_close),
                     img_close);
  gtk_box_pack_start (GTK_BOX (notes_window->hbox),
                      notes_window->btn_close,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show_all (notes_window->btn_close);

  /* Notebook */
  notes_window->notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notes_window->notebook), FALSE);
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notes_window->notebook), GTK_POS_LEFT);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notes_window->notebook), TRUE);
  gtk_box_pack_start (GTK_BOX (notes_window->vbox),
                      notes_window->notebook,
                      TRUE,
                      TRUE,
                      0);
  gtk_widget_show (notes_window->notebook);

  /* Status bar */
  notes_window->statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (notes_window->vbox),
                      notes_window->statusbar,
                      FALSE,
                      FALSE,
                      0);

  /* Accel group */
  notes_window->accel_group = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW (notes_window->window),
                              notes_window->accel_group);
  gtk_widget_add_accelerator (notes_window->btn_add,
                              "clicked",
                              notes_window->accel_group,
                              'N',
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_MASK);
  gtk_widget_add_accelerator (notes_window->btn_del,
                              "clicked",
                              notes_window->accel_group,
                              'W',
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_MASK);
  gtk_widget_add_accelerator (notes_window->btn_menu,
                              "clicked",
                              notes_window->accel_group,
                              'M',
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_MASK);
  gtk_widget_add_accelerator (notes_window->btn_close,
                              "clicked",
                              notes_window->accel_group,
                              GDK_Escape,
                              0,
                              GTK_ACCEL_MASK);

  /* Tooltips */
  accel_name = gtk_accelerator_get_label ('N', GDK_CONTROL_MASK);
  gtk_tooltips_set_tip (GTK_TOOLTIPS (notes_window->notes_plugin->tooltips),
                        notes_window->btn_add,
                        accel_name,
                        NULL);
  g_free (accel_name);
  accel_name = gtk_accelerator_get_label ('W', GDK_CONTROL_MASK);
  gtk_tooltips_set_tip (GTK_TOOLTIPS (notes_window->notes_plugin->tooltips),
                        notes_window->btn_del,
                        accel_name,
                        NULL);
  g_free (accel_name);
  accel_name = gtk_accelerator_get_label ('M', GDK_CONTROL_MASK);
  gtk_tooltips_set_tip (GTK_TOOLTIPS (notes_window->notes_plugin->tooltips),
                        notes_window->btn_menu,
                        accel_name,
                        NULL);
  g_free (accel_name);
  accel_name = gtk_accelerator_get_label (GDK_Escape, 0);
  gtk_tooltips_set_tip (GTK_TOOLTIPS (notes_window->notes_plugin->tooltips),
                        notes_window->btn_close,
                        accel_name,
                        NULL);
  g_free (accel_name);

  /* Signals */
  g_signal_connect_swapped (notes_window->window,
                            "window-state-event",
                            G_CALLBACK (notes_window_state_event),
                            notes_window);
  g_signal_connect_swapped (notes_window->btn_add,
                            "clicked",
                            G_CALLBACK (notes_window_add_note),
                            notes_window);
  g_signal_connect_swapped (notes_window->btn_del,
                            "clicked",
                            G_CALLBACK (notes_window_delete_note),
                            notes_window);
  g_signal_connect_swapped (notes_window->eb_move,
                            "button-press-event",
                            G_CALLBACK (notes_window_start_move),
                            notes_window);
  g_signal_connect_swapped (notes_window->eb_move,
                            "scroll-event",
                            G_CALLBACK (notes_window_scroll_event),
                            notes_window);
  g_signal_connect_swapped (notes_window->btn_menu,
                            "clicked",
                            G_CALLBACK (notes_window_menu_popup),
                            notes_window);
  g_signal_connect_swapped (notes_window->window,
                            "delete-event",
                            G_CALLBACK (notes_window_hide),
                            notes_window);
  g_signal_connect_swapped (notes_window->btn_close,
                            "clicked",
                            G_CALLBACK (notes_window_hide),
                            notes_window);

  /* Load data */
  notes_window_load_data (notes_window);
  notes_window_menu_new (notes_window);
  notes_plugin->windows = g_slist_insert_sorted (notes_plugin->windows,
                                                 notes_window,
                                                 (GCompareFunc)notes_window_strcasecmp);

  /* Show the stuff, or not */
  if (G_UNLIKELY (notes_window->show_statusbar))
    gtk_widget_show (notes_window->statusbar);

  if (G_LIKELY (notes_window->show_on_startup == ALWAYS
                || (notes_window->visible
                    && notes_window->show_on_startup == LAST_STATE)))
    notes_window_show (notes_window);

  return notes_window;
}

void
notes_window_load_data (NotesWindow *notes_window)
{
  XfceRc               *rc;
  NotesNote            *notes_note;
  const gchar          *note_name;

  if (G_LIKELY (NULL == notes_window->name))
    {
      guint id = 1;
      gchar *window_path = NULL;
      do
        {
          g_free (notes_window->name);
          notes_window->name =
            (id == 1) ? g_strdup ("Notes") : g_strdup_printf ("Notes %d", id);
          id++;

          g_free (window_path);
          window_path = g_build_path (G_DIR_SEPARATOR_S,
                                      notes_window->notes_plugin->notes_path,
                                      notes_window->name,
                                      NULL);
        }
      while (G_UNLIKELY (g_file_test (window_path, G_FILE_TEST_EXISTS)));
      g_free (window_path);

      gchar *window_name;
      window_name = g_strdup_printf ("<b>%s</b>", notes_window->name);
      gtk_label_set_text (GTK_LABEL (notes_window->title), window_name);
      gtk_label_set_use_markup (GTK_LABEL (notes_window->title), TRUE);
      g_free (window_name);
    }

  rc = xfce_rc_simple_open (notes_window->notes_plugin->config_file, FALSE);
  xfce_rc_set_group (rc, notes_window->name);

  notes_window->x = xfce_rc_read_int_entry (rc, "PosX", -1);
  notes_window->y = xfce_rc_read_int_entry (rc, "PosY", -1);
  notes_window->w = xfce_rc_read_int_entry (rc, "Width", 375);
  notes_window->h = xfce_rc_read_int_entry (rc, "Height", 430);

  notes_window->above           = xfce_rc_read_bool_entry (rc, "Above", FALSE);
  notes_window->show_on_startup = xfce_rc_read_int_entry (rc, "ShowOnStartup", LAST_STATE);
  notes_window->show_statusbar  = xfce_rc_read_bool_entry (rc, "ShowStatusbar", FALSE);
  notes_window->sticky          = xfce_rc_read_bool_entry (rc, "Sticky", TRUE);
  notes_window->visible         = xfce_rc_read_bool_entry (rc, "Visible", TRUE);

  xfce_rc_close (rc);

  TRACE ("\nname: %s"
         "\ngeometry: %d/%d:%dx%d"
         "\nabove: %d"
         "\nshow_on_startup: %d"
         "\nshow_statusbar: %d"
         "\nsticky: %d"
         "\nvisible: %d",
         notes_window->name,
         notes_window->x, notes_window->y, notes_window->w, notes_window->h,
         notes_window->above,
         notes_window->show_on_startup,
         notes_window->show_statusbar,
         notes_window->sticky,
         notes_window->visible);

  /**
   * Make sure we have at least one note if note_name is NULL.  After that an
   * inital note is created and it must not be read again.
   **/
  note_name = notes_note_read_name (notes_window);
  do
    {
      notes_note = notes_note_new (notes_window, note_name);
      if (G_UNLIKELY (NULL != note_name))
        note_name = notes_note_read_name (notes_window);
    }
  while (G_LIKELY (NULL != note_name));
}

void
notes_window_save_data (NotesWindow *notes_window)
{
  XfceRc               *rc;

  rc = xfce_rc_simple_open (notes_window->notes_plugin->config_file, FALSE);
  g_return_if_fail (G_LIKELY (rc != NULL));
  xfce_rc_set_group (rc, notes_window->name);

  if (GTK_WIDGET_VISIBLE (notes_window->window))
    {
      gtk_window_get_position (GTK_WINDOW (notes_window->window),
                               &notes_window->x,
                               &notes_window->y);
      if (GTK_WIDGET_VISIBLE (notes_window->notebook))
        gtk_window_get_size (GTK_WINDOW (notes_window->window),
                             &notes_window->w,
                             &notes_window->h);
      else
        gtk_window_get_size (GTK_WINDOW (notes_window->window),
                             &notes_window->w,
                             NULL);
    }

  TRACE ("\nabove: %d"
         "\nshow_on_startup: %d"
         "\nshow_statusbar: %d"
         "\nsticky: %d"
         "\nvisible: %d",
         notes_window->above,
         notes_window->show_on_startup,
         notes_window->show_statusbar,
         notes_window->sticky,
         notes_window->visible);

  xfce_rc_write_int_entry (rc, "PosX", notes_window->x);
  xfce_rc_write_int_entry (rc, "PosY", notes_window->y);
  xfce_rc_write_int_entry (rc, "Width", notes_window->w);
  xfce_rc_write_int_entry (rc, "Height", notes_window->h);

  xfce_rc_write_bool_entry (rc, "Above",
                            notes_window->above);
  xfce_rc_write_int_entry  (rc, "ShowOnStartup",
                            notes_window->show_on_startup);
  xfce_rc_write_bool_entry (rc, "ShowStatusbar",
                            notes_window->show_statusbar);
  xfce_rc_write_bool_entry (rc, "Sticky",
                            notes_window->sticky);
  xfce_rc_write_bool_entry (rc, "Visible",
                            GTK_WIDGET_VISIBLE (notes_window->window));

  xfce_rc_close (rc);
}

void
notes_window_destroy (NotesWindow *notes_window)
{
  DBG ("Destroy window `%s' (%p)", notes_window->name, notes_window);

  GtkWidget *dialog =
    gtk_message_dialog_new (GTK_WINDOW (notes_window->window),
                            GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_MESSAGE_QUESTION,
                            GTK_BUTTONS_YES_NO,
                            _("Are you sure you want to delete this window?"));
  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  if (G_UNLIKELY (result != GTK_RESPONSE_YES))
    return;

  NotesPlugin *notes_plugin = notes_window->notes_plugin;

  /* Drop configuration data */
  XfceRc *rc = xfce_rc_simple_open (notes_window->notes_plugin->config_file, FALSE);
  g_return_if_fail (G_LIKELY (rc != NULL));
  xfce_rc_delete_group (rc, notes_window->name, FALSE);
  xfce_rc_close (rc);

  /* Destroy all NotesNote */
  g_slist_foreach (notes_window->notes, (GFunc)notes_note_destroy, NULL);
  g_slist_free (notes_window->notes);

  /* Remove directory */
  gchar *window_path = g_build_path (G_DIR_SEPARATOR_S,
                                     notes_window->notes_plugin->notes_path,
                                     notes_window->name,
                                     NULL);
  g_rmdir (window_path);
  g_free (window_path);

  /* Remove GSList entry */
  notes_plugin->windows = g_slist_remove (notes_plugin->windows, notes_window);

  /* Free data */
  g_free (notes_window->name);
  gtk_widget_destroy (notes_window->window);
  g_slice_free (NotesWindow, notes_window);

  /* Init a new window if we get too low */
  if (g_slist_length (notes_plugin->windows) == 0)
    notes_window_new (notes_plugin);
}

void
notes_window_menu_new (NotesWindow *notes_window)
{
  /* Menu */
  notes_window->menu = gtk_menu_new ();
  GtkWidget *mi_new_window     = gtk_menu_item_new_with_mnemonic (_("_New window"));
  GtkWidget *mi_destroy_window = gtk_menu_item_new_with_mnemonic (_("_Destroy window"));
  GtkWidget *mi_rename_window  = gtk_menu_item_new_with_mnemonic (_("_Rename window"));
  GtkWidget *mi_rename_note    = gtk_menu_item_new_with_mnemonic (_("R_ename note"));
  GtkWidget *mi_separator1     = gtk_separator_menu_item_new ();
  notes_window->mi_options     = gtk_menu_item_new_with_label (_("Options"));

  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu), mi_new_window);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu), mi_destroy_window);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu), mi_rename_window);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu), mi_rename_note);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu), mi_separator1);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu), notes_window->mi_options);
  gtk_menu_attach_to_widget (GTK_MENU (notes_window->menu), notes_window->btn_menu, NULL);

  /* Accel group */
  gtk_menu_set_accel_group (GTK_MENU (notes_window->menu),
                            notes_window->accel_group);
  gtk_widget_add_accelerator (mi_new_window,
                              "activate",
                              notes_window->accel_group,
                              'N',
                              GDK_SHIFT_MASK|GDK_CONTROL_MASK,
                              GTK_ACCEL_MASK);
  gtk_widget_add_accelerator (mi_destroy_window,
                              "activate",
                              notes_window->accel_group,
                              'Q',
                              GDK_CONTROL_MASK,
                              GTK_ACCEL_MASK);
  gtk_widget_add_accelerator (mi_rename_note,
                              "activate",
                              notes_window->accel_group,
                              GDK_F2,
                              0,
                              GTK_ACCEL_MASK);
  gtk_widget_add_accelerator (mi_rename_window,
                              "activate",
                              notes_window->accel_group,
                              GDK_F2,
                              GDK_SHIFT_MASK,
                              GTK_ACCEL_MASK);

  /* Signals */
  g_signal_connect_swapped (notes_window->menu,
                            "deactivate",
                            G_CALLBACK (notes_window_menu_detach),
                            notes_window);
  g_signal_connect_swapped (mi_new_window,
                            "activate",
                            G_CALLBACK (notes_window_new),
                            notes_window->notes_plugin);
  g_signal_connect_swapped (mi_destroy_window,
                            "activate",
                            G_CALLBACK (notes_window_destroy),
                            notes_window);
  g_signal_connect_swapped (mi_rename_note,
                            "activate",
                            G_CALLBACK (notes_window_rename_note_dialog),
                            notes_window);
  g_signal_connect_swapped (mi_rename_window,
                            "activate",
                            G_CALLBACK (notes_window_rename_dialog),
                            notes_window);

  /* Show the stuff */
  gtk_widget_show_all (notes_window->menu);
}

void
notes_window_menu_options_new (NotesWindow *notes_window)
{
  DBG ("Create menu options (%p)", notes_window);

  /* NotesWindow options menu */
  notes_window->menu_options = gtk_menu_new ();
  GtkWidget *mi_show_statusbar  = gtk_check_menu_item_new_with_label (_("Show statusbar"));
  GtkWidget *mi_above           = gtk_check_menu_item_new_with_label (_("Always on top"));
  GtkWidget *mi_sticky          = gtk_check_menu_item_new_with_label (_("Sticky window"));
  GtkWidget *mi_show_on_startup = gtk_menu_item_new_with_label (_("Show on startup"));

  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu_options), mi_show_statusbar);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu_options), mi_above);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu_options), mi_sticky);
  gtk_menu_shell_append (GTK_MENU_SHELL (notes_window->menu_options), mi_show_on_startup);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (notes_window->mi_options), notes_window->menu_options);

  /* Sub-menu "Show on startup" */
  GtkWidget *menu_show_on_startup = gtk_menu_new ();
  GSList *menu_group = NULL;
  GtkWidget *mi_sos_always      = gtk_radio_menu_item_new_with_label (menu_group, _("Always"));
  menu_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (mi_sos_always));
  GtkWidget *mi_sos_never       = gtk_radio_menu_item_new_with_label (menu_group, _("Never"));
  menu_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (mi_sos_never));
  GtkWidget *mi_sos_last_state  = gtk_radio_menu_item_new_with_label (menu_group, _("Last state"));
  menu_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (mi_sos_last_state));

  gtk_menu_shell_append (GTK_MENU_SHELL (menu_show_on_startup), mi_sos_always);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_show_on_startup), mi_sos_never);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_show_on_startup), mi_sos_last_state);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi_show_on_startup), menu_show_on_startup);

  /* Activate check menu items */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi_sos_always),
                                  (notes_window->show_on_startup == ALWAYS));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi_sos_never),
                                  (notes_window->show_on_startup == NEVER));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi_sos_last_state),
                                  (notes_window->show_on_startup == LAST_STATE));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi_show_statusbar),
                                  notes_window->show_statusbar);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi_above),
                                  notes_window->above);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi_sticky),
                                  notes_window->sticky);

  /* Signals */
  g_signal_connect_swapped (mi_sos_always,
                            "activate",
                            G_CALLBACK (notes_window_set_sos_always),
                            notes_window);
  g_signal_connect_swapped (mi_sos_never,
                            "activate",
                            G_CALLBACK (notes_window_set_sos_never),
                            notes_window);
  g_signal_connect_swapped (mi_sos_last_state,
                            "activate",
                            G_CALLBACK (notes_window_set_sos_last_state),
                            notes_window);
  g_signal_connect_swapped (mi_show_statusbar,
                            "activate",
                            G_CALLBACK (notes_window_set_statusbar),
                            notes_window);
  g_signal_connect_swapped (mi_above,
                            "activate",
                            G_CALLBACK (notes_window_set_above),
                            notes_window);
  g_signal_connect_swapped (mi_sticky,
                            "activate",
                            G_CALLBACK (notes_window_set_sticky),
                            notes_window);

  /* Show the stuff */
  gtk_widget_show_all (notes_window->menu_options);
}

static void
notes_window_menu_popup (NotesWindow *notes_window)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (notes_window->btn_menu)))
    {
      notes_window_menu_options_new (notes_window);
      gtk_menu_popup (GTK_MENU (notes_window->menu),
                      NULL,
                      NULL,
                      (GtkMenuPositionFunc) notes_window_menu_position,
                      NULL,
                      0,
                      gtk_get_current_event_time ());
    }
}

static void
notes_window_menu_position (GtkMenu *menu,
                            gint *x0,
                            gint *y0,
                            gboolean *push_in,
                            gpointer user_data)
{
  GdkWindow            *toplevel;
  gint                  x1, y1, width, height, depth;
  GtkWidget            *btn_menu;
  GtkRequisition        requisition0;

  g_return_if_fail (GTK_IS_MENU (menu));
  btn_menu = gtk_menu_get_attach_widget (menu);
  g_return_if_fail (GTK_IS_WIDGET (btn_menu));

  toplevel = gdk_window_get_toplevel (btn_menu->window);
  gdk_window_get_geometry (toplevel, &x1, &y1, &width, &height, &depth);
  gdk_window_get_origin (btn_menu->window, x0, y0);
  gtk_widget_size_request (GTK_WIDGET (menu), &requisition0);

  TRACE ("\nx0/y0: %d/%d"
         "\nx1/y1/width/height: %d/%d/%d/%d",
         *x0, *y0,
         x1, y1, width, height);

   if (*y0 + btn_menu->allocation.height + requisition0.height > gdk_screen_height())
    /* Show menu above button, since there is not enough space below */
    *y0 -= requisition0.height;
   else
    /* Show menu below button */
    *y0 += btn_menu->allocation.height;

   *x0 += width - requisition0.width;
   if (*x0 + requisition0.width > gdk_screen_width ())
     /* Adjust horizontal position */
     *x0 = gdk_screen_width () - requisition0.width;
}

static void
notes_window_menu_detach (NotesWindow *notes_window)
{
  DBG ("Dettach window menu options");
  gtk_menu_detach (GTK_MENU (notes_window->menu_options));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (notes_window->btn_menu), FALSE);
}

static void
notes_window_set_sos_always (NotesWindow *notes_window)
{
  notes_window->show_on_startup = ALWAYS;
}

static void
notes_window_set_sos_never (NotesWindow *notes_window)
{
  notes_window->show_on_startup = NEVER;
}

static void
notes_window_set_sos_last_state (NotesWindow *notes_window)
{
  notes_window->show_on_startup = LAST_STATE;
}

static void
notes_window_set_statusbar (NotesWindow *notes_window)
{
  if (!GTK_WIDGET_VISIBLE (notes_window->notebook))
    notes_window_unshade (notes_window);

  notes_window->show_statusbar = !notes_window->show_statusbar;
  if (notes_window->show_statusbar)
    gtk_widget_show (notes_window->statusbar);
  else
    gtk_widget_hide (notes_window->statusbar);
}

static void
notes_window_set_above (NotesWindow *notes_window)
{
  notes_window->above = !notes_window->above;
  gtk_window_set_keep_above (GTK_WINDOW (notes_window->window),
                             notes_window->above);
}

static void
notes_window_set_sticky (NotesWindow *notes_window)
{
  notes_window->sticky = !notes_window->sticky;
  if (notes_window->sticky)
    gtk_window_stick (GTK_WINDOW (notes_window->window));
  else
    gtk_window_unstick (GTK_WINDOW (notes_window->window));
}

static gboolean
notes_window_state_event (NotesWindow *notes_window,
                          GdkEventWindowState *event)
{
  if (G_UNLIKELY (event->type != GDK_WINDOW_STATE))
    return FALSE;

  if (event->changed_mask & GDK_WINDOW_STATE_ABOVE)
    {
      /* FIXME above state event isn't notified */
      notes_window->above = (gboolean) event->new_window_state & GDK_WINDOW_STATE_ABOVE;
      TRACE ("Window state above: %d", notes_window->above);
    }

  if (event->changed_mask & GDK_WINDOW_STATE_STICKY)
    {
      /**
       * Hiding the top level window will unstick it too, and send a
       * window-state-event signal, so here we take the value only if
       * the window is visible
       **/
      if (GTK_WIDGET_VISIBLE (notes_window->window))
        notes_window->sticky = (gboolean) event->new_window_state & GDK_WINDOW_STATE_STICKY;
      TRACE ("Window state sticky: %d", notes_window->sticky);
    }

  return FALSE;
}

void
notes_window_show (NotesWindow *notes_window)
{
  TRACE ("Show window: %p", notes_window);
  if (GTK_WIDGET_VISIBLE (notes_window->window))
    {
      if (!GTK_WIDGET_VISIBLE (notes_window->notebook))
        notes_window_unshade (notes_window);
      gtk_window_present (GTK_WINDOW (notes_window->window));
      return;
    }

  if (notes_window->x != -1 && notes_window->y != -1)
    gtk_window_move (GTK_WINDOW (notes_window->window),
                     notes_window->x,
                     notes_window->y);
  gtk_window_resize (GTK_WINDOW (notes_window->window),
                     notes_window->w,
                     notes_window->h);
  gtk_window_set_keep_above (GTK_WINDOW (notes_window->window),
                             notes_window->above);
  if (notes_window->sticky)
    gtk_window_stick (GTK_WINDOW (notes_window->window));
  else
    gtk_window_unstick (GTK_WINDOW (notes_window->window));

  gtk_window_set_skip_pager_hint (GTK_WINDOW (notes_window->window),
                                  TRUE);
  GTK_WIDGET_UNSET_FLAGS (notes_window->notebook,
                          GTK_CAN_FOCUS);
  gtk_widget_show (notes_window->window);
}

gboolean
notes_window_hide (NotesWindow *notes_window)
{
  TRACE ("Hide window: %p", notes_window);
  gtk_window_get_position (GTK_WINDOW (notes_window->window),
                           &notes_window->x,
                           &notes_window->y);
  if (GTK_WIDGET_VISIBLE (notes_window->notebook))
    gtk_window_get_size (GTK_WINDOW (notes_window->window),
                         &notes_window->w,
                         &notes_window->h);
  else
    gtk_window_get_size (GTK_WINDOW (notes_window->window),
                         &notes_window->w,
                         NULL);
  
  gtk_widget_hide (notes_window->window);
  notes_window_unshade (notes_window);

  return TRUE; /* Stop other handlers from being invoked (incl. ALT+F4) */
}

static gboolean
notes_window_start_move (NotesWindow *notes_window,
                         GdkEventButton *event)
{
  if (G_LIKELY (event->type == GDK_BUTTON_PRESS))
    {
      /* Send to foreground and move the window */
      if (event->button == 1)
        {
          gdk_window_show (notes_window->window->window);
          gtk_window_begin_move_drag (GTK_WINDOW (notes_window->window),
                                      event->button,
                                      event->x_root,
                                      event->y_root,
                                      event->time);
        }
      /* Send to background */
      else if (event->button == 2)
        gdk_window_lower (notes_window->window->window);
    }

  return FALSE;
}

static gboolean
notes_window_scroll_event (NotesWindow *notes_window,
                           GdkEventScroll *event)
{
  if (G_LIKELY (event->type == GDK_SCROLL))
    {
      if (event->direction == GDK_SCROLL_UP)
        notes_window_shade (notes_window);
      else if (event->direction == GDK_SCROLL_DOWN)
        notes_window_unshade (notes_window);
    }
  return FALSE;
}

static void
notes_window_shade (NotesWindow *notes_window)
{
  if (G_LIKELY (GTK_WIDGET_VISIBLE (notes_window->notebook)))
    gtk_window_get_size (GTK_WINDOW (notes_window->window),
                         &notes_window->w,
                         &notes_window->h);
  if (G_LIKELY (notes_window->show_statusbar))
    gtk_widget_hide (notes_window->statusbar);
  gtk_widget_hide (notes_window->notebook);
  gtk_window_resize (GTK_WINDOW (notes_window->window),
                     notes_window->w,
                     1);
}

static void
notes_window_unshade (NotesWindow *notes_window)
{
  gtk_widget_show (notes_window->notebook);
  GTK_WIDGET_UNSET_FLAGS (notes_window->notebook,
                          GTK_CAN_FOCUS);
  if (notes_window->show_statusbar)
    gtk_widget_show (notes_window->statusbar);

  if (GTK_WIDGET_VISIBLE (notes_window->window))
    {
      gtk_window_get_size (GTK_WINDOW (notes_window->window),
                           &notes_window->w,
                           NULL);
      gtk_window_resize (GTK_WINDOW (notes_window->window),
                         notes_window->w,
                         notes_window->h);
    }
}

static void
notes_window_rename_note_dialog (NotesWindow *notes_window)
{
  /* Rename a NotesNote through a NotesWindow (accelerator group) */
  gint current_page =
    gtk_notebook_get_current_page (GTK_NOTEBOOK (notes_window->notebook));
  NotesNote *notes_note =
    g_slist_nth_data (notes_window->notes, current_page);
  notes_note_rename_dialog (notes_note);
}

static void
notes_window_rename_dialog (NotesWindow *notes_window)
{
  /* Dialog */
  GtkWidget *dialog =
    gtk_dialog_new_with_buttons (_("Rename window"),
                                 GTK_WINDOW (notes_window->window),
                                 GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_STOCK_CANCEL,
                                 GTK_RESPONSE_CANCEL,
                                 GTK_STOCK_OK,
                                 GTK_RESPONSE_OK,
                                 NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_EDIT);

  /* Vbox */
  GtkWidget *vbox = GTK_DIALOG (dialog)->vbox;

  /* Entry */
  GtkWidget *entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), notes_window->name);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  /* Containers */
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (vbox), entry);
  gtk_widget_show_all (vbox);

  /* Run the dialog */
  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
  if (G_LIKELY (result == GTK_RESPONSE_OK))
    {
      const gchar *name = gtk_entry_get_text (GTK_ENTRY (entry));
      TRACE ("Rename %s to %s", notes_window->name, name);
      notes_window_rename (notes_window, name);
      notes_window_sort_names (notes_window);
    }
  gtk_widget_destroy (dialog);
}

static void
notes_window_rename (NotesWindow *notes_window,
                     const gchar *name)
{
  /* Move some directory */
  gchar *oldfilename = g_build_path (G_DIR_SEPARATOR_S,
                                     notes_window->notes_plugin->notes_path,
                                     notes_window->name,
                                     NULL);
  gchar *newfilename = g_build_path (G_DIR_SEPARATOR_S,
                                     notes_window->notes_plugin->notes_path,
                                     name,
                                     NULL);

  TRACE ("\nOld filename: `%s'\nNew filename: `%s'", oldfilename, newfilename);
  if (G_LIKELY (!g_rename (oldfilename, newfilename)))
    {
      XfceRc *rc = xfce_rc_simple_open (notes_window->notes_plugin->config_file, FALSE);
      g_return_if_fail (G_LIKELY (rc != NULL));
      xfce_rc_delete_group (rc, notes_window->name, FALSE);
      xfce_rc_close (rc);

      g_free (notes_window->name);
      notes_window->name = g_strdup (name);

      gtk_window_set_title (GTK_WINDOW (notes_window->window), name);
      gchar *name_tmp = g_strdup_printf ("<b>%s</b>", name);
      gtk_label_set_text (GTK_LABEL (notes_window->title), name_tmp);
      gtk_label_set_use_markup (GTK_LABEL (notes_window->title), TRUE);
      g_free (name_tmp);

      notes_window_save_data (notes_window);
    }

  g_free (oldfilename);
  g_free (newfilename);
}

inline void
notes_window_sort_names (NotesWindow *notes_window)
{
  notes_window->notes_plugin->windows
    = g_slist_sort (notes_window->notes_plugin->windows,
                    (GCompareFunc)notes_window_strcasecmp);
}

gint
notes_window_strcasecmp (NotesWindow *notes_window0,
                         NotesWindow *notes_window1)
{
  TRACE ("Compare `%s' with `%s'", notes_window0->name, notes_window1->name);
  gchar *name0 = g_utf8_collate_key_for_filename (notes_window0->name, -1);
  gchar *name1 = g_utf8_collate_key_for_filename (notes_window1->name, -1);
  gint res = g_ascii_strcasecmp (name0, name1);
  g_free (name0);
  g_free (name1);
  return res;
}

static inline NotesNote *
notes_window_get_current_note (NotesWindow *notes_window)
{
  gint id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notes_window->notebook));
  return (NotesNote *)g_slist_nth_data (notes_window->notes, id);
}

static void
notes_window_add_note (NotesWindow *notes_window)
{
  if (!GTK_WIDGET_VISIBLE (notes_window->notebook))
    notes_window_unshade (notes_window);

  notes_note_new (notes_window, NULL);
}

static void
notes_window_delete_note (NotesWindow *notes_window)
{
  if (!GTK_WIDGET_VISIBLE (notes_window->notebook))
    notes_window_unshade (notes_window);

  NotesNote *notes_note = notes_window_get_current_note (notes_window);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (notes_note->text_view));

  if (gtk_text_buffer_get_char_count (buffer) > 0)
    {
      GtkWidget *dialog =
        gtk_message_dialog_new (GTK_WINDOW (notes_window->window),
                                GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_YES_NO,
                                _("Are you sure you want to delete this note?"));
      gint result = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      if (G_UNLIKELY (result != GTK_RESPONSE_YES))
        return;
    }

  notes_note_destroy (notes_note);

  /* Create a new note if we reach the ground 0 */
  if (g_slist_length (notes_window->notes) == 0)
    notes_note_new (notes_window, NULL);
}



const gchar *
notes_note_read_name (NotesWindow *notes_window)
{
  static GDir          *dir = NULL;
  static gchar         *path = NULL;
  const gchar          *note_name = NULL;

  TRACE ("NotesWindow: %p", notes_window);
  if (G_UNLIKELY (dir == NULL))
    {
      path = g_build_path (G_DIR_SEPARATOR_S,
                           notes_window->notes_plugin->notes_path,
                           notes_window->name,
                           NULL);
      TRACE ("path: %s", path);
      if (G_UNLIKELY (!g_file_test (path, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))))
        g_mkdir (path, 0755);

      dir = g_dir_open (path, 0, NULL);
    }

  note_name = g_dir_read_name (dir);
  TRACE ("note_name: %s", note_name);
  if (G_UNLIKELY (note_name == NULL))
    {
      g_dir_close (dir);
      dir = NULL;
      g_free (path);
    }

  return note_name;
}

NotesNote *
notes_note_new (NotesWindow *notes_window,
                const gchar *note_name)
{
  NotesNote            *notes_note;
  gint                  id;
  GtkTextBuffer        *buffer;

  notes_note = g_slice_new0 (NotesNote);
  notes_note->notes_window = notes_window;
  notes_note->name = g_strdup (note_name);

  /* Label */
  GtkWidget *eb_border = gtk_event_box_new ();
  gtk_container_set_border_width (GTK_CONTAINER (eb_border), 3);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (eb_border), FALSE);
  notes_note->title = gtk_label_new (note_name);
  gtk_label_set_angle (GTK_LABEL (notes_note->title), 90);
  gtk_container_add (GTK_CONTAINER (eb_border),
                     notes_note->title);

  /* Scrolled window */
  notes_note->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (notes_note->scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  /* Text view */
  notes_note->text_view = gtk_text_view_new ();
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (notes_note->text_view));
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (notes_note->text_view), GTK_WRAP_WORD);
  gtk_container_add (GTK_CONTAINER (notes_note->scrolled_window),
                     notes_note->text_view);

  /* Signals */
  g_signal_connect_swapped (notes_note->text_view,
                            "key-press-event",
                            G_CALLBACK (notes_note_key_pressed),
                            notes_note);
  g_signal_connect_swapped (buffer,
                            "changed",
                            G_CALLBACK (notes_note_buffer_changed),
                            notes_note);

  /* Load data */
  notes_note_load_data (notes_note, buffer);
  notes_window->notes = g_slist_insert_sorted (notes_window->notes,
                                               notes_note,
                                               (GCompareFunc)notes_note_strcasecmp);

  /* Show the stuff */
  gtk_widget_show_all (eb_border);
  gtk_widget_show_all (notes_note->scrolled_window);
  id = g_slist_index (notes_window->notes, notes_note);
  gtk_notebook_insert_page (GTK_NOTEBOOK (notes_window->notebook),
                            notes_note->scrolled_window,
                            eb_border,
                            id);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notes_window->notebook), id);
  TRACE ("Put to front note `%s'", notes_note->name);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notes_window->notebook),
                              (g_slist_length (notes_window->notes) > 1));

  return notes_note;
}

void
notes_note_load_data (NotesNote *notes_note,
                      GtkTextBuffer *buffer)
{
  guint                 id = 1;
  gchar                *filename = NULL;
  gchar                *contents = NULL;

  if (G_LIKELY (NULL == notes_note->name))
    {
      do
        {
          g_free (notes_note->name);
          notes_note->name = g_strdup_printf ("%d", id);
          id++;

          g_free (filename);
          filename =
            g_build_path (G_DIR_SEPARATOR_S,
                          notes_note->notes_window->notes_plugin->notes_path,
                          notes_note->notes_window->name,
                          notes_note->name,
                          NULL);
        }
      while (G_UNLIKELY (g_file_test (filename, G_FILE_TEST_EXISTS)));
      g_file_set_contents (filename, "", -1, NULL);
      g_free (filename);

      gtk_label_set_text (GTK_LABEL (notes_note->title), notes_note->name);
    }

  filename = g_build_path (G_DIR_SEPARATOR_S,
                           notes_note->notes_window->notes_plugin->notes_path,
                           notes_note->notes_window->name,
                           notes_note->name,
                           NULL);
  DBG ("Read notes from `%s'", filename);

  if (G_LIKELY (g_file_get_contents (filename, &contents, NULL, NULL)))
    {
      TRACE ("Load data for notes `%s':\n%s", notes_note->name, contents);
      gtk_text_buffer_set_text (buffer, contents, -1);
      gtk_text_view_set_buffer (GTK_TEXT_VIEW (notes_note->text_view), buffer);
    }

  g_free (contents);
  g_free (filename);
}

static gboolean
notes_note_save_data (NotesNote *notes_note)
{
  DBG ("Save note `%s'", notes_note->name);

  GtkTextBuffer        *buffer;
  GtkTextIter           start, end;
  gchar                *filename, *contents;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (notes_note->text_view));

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  contents = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);

  filename = g_build_path (G_DIR_SEPARATOR_S,
                           notes_note->notes_window->notes_plugin->notes_path,
                           notes_note->notes_window->name,
                           notes_note->name,
                           NULL);
  g_file_set_contents (filename, contents, -1, NULL);

  g_free (filename);
  g_free (contents);

  return FALSE;
}

void
notes_note_destroy (NotesNote *notes_note)
{
  DBG ("Destroy note `%s' (%p)", notes_note->name, notes_note);

  gint                  id;
  gchar                *note_path;
  NotesWindow          *notes_window = notes_note->notes_window;

  /* Make sure we kill the timeout */
  if (notes_note->timeout != 0)
    g_source_remove (notes_note->timeout);

  /* Remove notebook page */
  id = gtk_notebook_get_current_page (GTK_NOTEBOOK (notes_window->notebook));
  gtk_notebook_remove_page (GTK_NOTEBOOK (notes_window->notebook), id);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notes_window->notebook),
                              ((g_slist_length (notes_window->notes) - 1) > 1));

  /* Remove file */
  note_path = g_build_path (G_DIR_SEPARATOR_S,
                            notes_window->notes_plugin->notes_path,
                            notes_window->name,
                            notes_note->name,
                            NULL);
  g_unlink (note_path);
  g_free (note_path);

  /* Remove GSList entry */
  notes_window->notes = g_slist_remove (notes_window->notes, notes_note);

  /* Free data */
  g_free (notes_note->name);
  g_slice_free (NotesNote, notes_note);
}

static void
notes_note_rename_dialog (NotesNote *notes_note)
{
  /* Dialog */
  GtkWidget *dialog =
    gtk_dialog_new_with_buttons (_("Rename note"),
                                 GTK_WINDOW (notes_note->notes_window->window),
                                 GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                 GTK_STOCK_CANCEL,
                                 GTK_RESPONSE_CANCEL,
                                 GTK_STOCK_OK,
                                 GTK_RESPONSE_OK,
                                 NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_EDIT);

  /* Vbox */
  GtkWidget *vbox = GTK_DIALOG (dialog)->vbox;

  /* Entry */
  GtkWidget *entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), notes_note->name);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  /* Containers */
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (vbox), entry);
  gtk_widget_show_all (vbox);

  /* Run the dialog */
  gint result = gtk_dialog_run (GTK_DIALOG (dialog));
  if (G_LIKELY (result == GTK_RESPONSE_OK))
    {
      const gchar *name = gtk_entry_get_text (GTK_ENTRY (entry));
      TRACE ("Rename %s to %s", notes_note->name, name);
      notes_note_rename (notes_note, name);
      notes_note_sort_names (notes_note);
    }
  gtk_widget_destroy (dialog);
}

static void
notes_note_rename (NotesNote *notes_note,
                   const gchar *name)
{
  /* Move some file */
  gchar *oldfilename = g_build_path (G_DIR_SEPARATOR_S,
                                     notes_note->notes_window->notes_plugin->notes_path,
                                     notes_note->notes_window->name,
                                     notes_note->name,
                                     NULL);
  gchar *newfilename = g_build_path (G_DIR_SEPARATOR_S,
                                     notes_note->notes_window->notes_plugin->notes_path,
                                     notes_note->notes_window->name,
                                     name,
                                     NULL);

  TRACE ("\nOld filename: `%s'\nNew filename: `%s'", oldfilename, newfilename);
  if (G_LIKELY (!g_rename (oldfilename, newfilename)))
    {
      g_free (notes_note->name);
      notes_note->name = g_strdup (name);
      gtk_label_set_text (GTK_LABEL (notes_note->title), name);
    }

  g_free (oldfilename);
  g_free (newfilename);
}

static void
notes_note_sort_names (NotesNote *notes_note)
{
  gint                  i = 0;
  NotesNote            *foo;

  notes_note->notes_window->notes
    = g_slist_sort (notes_note->notes_window->notes,
                    (GCompareFunc)notes_note_strcasecmp);

  while (NULL != (foo = (NotesNote *)g_slist_nth_data (notes_note->notes_window->notes, i++)))
    gtk_notebook_reorder_child (GTK_NOTEBOOK (foo->notes_window->notebook), foo->scrolled_window, i);
}

static gint
notes_note_strcasecmp (NotesNote *notes_note0,
                       NotesNote *notes_note1)
{
  TRACE ("Compare `%s' with `%s'", notes_note0->name, notes_note1->name);
  gchar *name0 = g_utf8_collate_key_for_filename (notes_note0->name, -1);
  gchar *name1 = g_utf8_collate_key_for_filename (notes_note1->name, -1);
  gint res = g_ascii_strcasecmp (name0, name1);
  g_free (name0);
  g_free (name1);
  return res;
}

static gboolean
notes_note_key_pressed (NotesNote *notes_note,
                        GdkEventKey *event)
{
  GtkWidget            *notebook = notes_note->notes_window->notebook;

  if (G_UNLIKELY (event->type != GDK_KEY_PRESS))
    return FALSE;

  if (event->state & GDK_CONTROL_MASK)
    {
      switch (event->keyval)
        {
        case GDK_Page_Down:
          gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
          break;

        case GDK_Page_Up:
          gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
          break;

        default:
          break;
        }

      return FALSE;
    }
  else if (event->state & GDK_MOD1_MASK)
    {
      gint page = -1;
      switch (event->keyval)
        {
        case GDK_0:
          page += 10;
        case GDK_1:
        case GDK_2:
        case GDK_3:
        case GDK_4:
        case GDK_5:
        case GDK_6:
        case GDK_7:
        case GDK_8:
        case GDK_9:
          page += (event->keyval - 0x30);
          gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
          break;

        default:
          break;
        }

      return FALSE;
    }

  return FALSE;
}

static void
notes_note_buffer_changed (NotesNote *notes_note)
{
  if (notes_note->timeout > 0)
    {
      g_source_remove (notes_note->timeout);
      notes_note->timeout = 0;
    }
  notes_note->timeout = g_timeout_add (60000, (GSourceFunc)notes_note_save_data, notes_note);
}

