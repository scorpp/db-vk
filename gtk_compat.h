#if !GTK_CHECK_VERSION(2,14,0)
#define gtk_dialog_get_content_area(dialog) (dialog->vbox)
#endif
