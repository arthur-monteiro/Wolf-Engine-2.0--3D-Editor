#include <iostream>
#include <string>

enum class BrowseToFileOption { FILE_OPEN, FILE_SAVE };
enum class BrowseToFileFilter { SAVE, OBJ, DAE, IMG };

#if defined(WIN32)
#include <shtypes.h>
#include <ShlObj_core.h>
#include <codecvt>
#undef ERROR
void BrowseToFile(std::string& filename, BrowseToFileOption option, BrowseToFileFilter filter)
{
	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	char szFile[MAX_PATH];
	ofn.lpstrFile = szFile;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	switch (filter)
	{
	case BrowseToFileFilter::SAVE:
		ofn.lpstrFilter = "Wolf Editor Save (JSON)\0*.json\0";
		break;
	case BrowseToFileFilter::OBJ:
		ofn.lpstrFilter = "Object (OBJ)\0*.obj\0";
		break;
	case BrowseToFileFilter::DAE:
		ofn.lpstrFilter = "Animated model (DAE)\0*.dae\0";
		break;
	case BrowseToFileFilter::IMG:
		ofn.lpstrFilter = "Image\0*.jpg;*.png;*.tga;*.dds;*.hdr;*.cube\0";
		break;
	}
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if ((option == BrowseToFileOption::FILE_SAVE && GetSaveFileNameA(&ofn)) ||
		(option == BrowseToFileOption::FILE_OPEN && GetOpenFileNameA(&ofn)))
	{
		filename = szFile;
	}
}
#elif __linux__
#include <gtk/gtk.h>

void BrowseToFile(std::string& filename, BrowseToFileOption option, BrowseToFileFilter filter)
{
    if (!gtk_init_check(NULL, NULL))
    {
    	std::cout << "Unable to initialize Gtk.\n";
        return;
    }

    GtkFileChooserAction action = (option == BrowseToFileOption::FILE_SAVE) ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN;

    const char* title = (option == BrowseToFileOption::FILE_SAVE) ? "Save file" : "Open file";
    const char* button_text = (option == BrowseToFileOption::FILE_SAVE) ? "_Save" : "_Open";

    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        title,
        NULL, // no parent window
        action,
        button_text,
        "_Cancel"
    );
	gtk_native_dialog_set_modal(GTK_NATIVE_DIALOG(native), TRUE);

    // Create and add filters
    GtkFileFilter *gtk_filter = gtk_file_filter_new();
    switch (filter)
    {
    case BrowseToFileFilter::SAVE:
        gtk_file_filter_set_name(gtk_filter, "Wolf Editor Save (JSON)");
        gtk_file_filter_add_pattern(gtk_filter, "*.json");
        break;
    case BrowseToFileFilter::OBJ:
        gtk_file_filter_set_name(gtk_filter, "Object (OBJ)");
        gtk_file_filter_add_pattern(gtk_filter, "*.obj");
        break;
    case BrowseToFileFilter::DAE:
        gtk_file_filter_set_name(gtk_filter, "Animated model (DAE)");
        gtk_file_filter_add_pattern(gtk_filter, "*.dae");
        break;
    case BrowseToFileFilter::IMG:
        gtk_file_filter_set_name(gtk_filter, "Image");
        gtk_file_filter_add_pattern(gtk_filter, "*.jpg");
        gtk_file_filter_add_pattern(gtk_filter, "*.png");
        gtk_file_filter_add_pattern(gtk_filter, "*.tga");
        gtk_file_filter_add_pattern(gtk_filter, "*.dds");
        gtk_file_filter_add_pattern(gtk_filter, "*.hdr");
        gtk_file_filter_add_pattern(gtk_filter, "*.cube");
        break;
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(native), gtk_filter);

    gint res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
        filename = path;
        g_free(path);
    }

    g_object_unref(native);

    // flush events to ensure the window closes properly
    while (gtk_events_pending()) gtk_main_iteration();
}
#endif

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cout << "Not enough arguments\n";
		return EXIT_FAILURE;
	}

	const std::string inputOption = argv[1];
	const std::string inputFilter = argv[2];
	const std::string folderRestriction = argv[3];

	BrowseToFileOption browseToFileOption;
	if (inputOption == "open")
		browseToFileOption = BrowseToFileOption::FILE_OPEN;
	else if (inputOption == "save")
		browseToFileOption = BrowseToFileOption::FILE_SAVE;
	else
	{
		std::cout << "Unsupported pick file option \"" + inputOption + "\"\n";
		return EXIT_FAILURE;
	}

	BrowseToFileFilter browseToFileFilter;
	if (inputFilter == "obj")
		browseToFileFilter = BrowseToFileFilter::OBJ;
	else if (inputFilter == "save")
		browseToFileFilter = BrowseToFileFilter::SAVE;
	else if (inputFilter == "dae")
		browseToFileFilter = BrowseToFileFilter::DAE;
	else if (inputFilter == "img")
		browseToFileFilter = BrowseToFileFilter::IMG;
	else
	{
		std::cout << "Unsupported pick file filter \"" + inputFilter + "\"\n";
		return EXIT_FAILURE;
	}

	std::string filename;
	while (filename.empty() || filename.substr(0, folderRestriction.size()) != folderRestriction)
		BrowseToFile(filename, browseToFileOption, browseToFileFilter);

	std::cout << filename << "\n";
}
