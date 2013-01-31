#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_dialog_get_content_area(dialog) (dialog->vbox)
#endif

#if !GTK_CHECK_VERSION(2, 24, 0)
#define GTK_COMBO_BOX_TEXT(o) GTK_COMBO_BOX (o)
#define gtk_combo_box_text_new()                        gtk_combo_box_new_text ()
#define gtk_combo_box_text_append_text(widget, text)    gtk_combo_box_append_text (widget, text)
#define gtk_combo_box_text_prepend_text(widget, text)   gtk_combo_box_prepend_text (widget, text)
#define gtk_combo_box_text_remove(widget, text)         gtk_combo_box_remove_text (widget, pos)
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
#define gtk_box_new(orientation, spacing) (orientation == GTK_ORIENTATION_HORIZONTAL \
        ? gtk_hbox_new (FALSE, spacing)\
        : gtk_vbox_new (FALSE, spacing))
#endif



#if !GLIB_CHECK_VERSION(2, 30, 0)
#define G_VALUE_INIT  { 0, { { 0 } } }
#endif
