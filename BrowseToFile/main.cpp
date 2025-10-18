#include <iostream>
#include <string>

enum class BrowseToFileOption { FILE_OPEN, FILE_SAVE };
enum class BrowseToFileFilter { SAVE, OBJ, DAE, IMG };
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
