#if !GTK_CHECK_VERSION(2, 14, 0)
#define gtk_dialog_get_content_area(dialog) (dialog->vbox)
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
#define gtk_box_new(orientation, spacing) (orientation == GTK_ORIENTATION_HORIZONTAL \
        ? gtk_hbox_new (FALSE, spacing)\
        : gtk_vbox_new (FALSE, spacing))
#endif
