// BD2.DATTool.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include "filef.h"
#include <memory>
#include <string>
#include "zlib\zlib.h"
#include <filesystem>
#pragma comment (lib, "zlib/zdll.lib" )


enum eModes {
	MODE_EXTRACT = 1,
	MODE_CREATE
};

#pragma pack (push,1)
struct GOGHeader{
	char  header[32] = {}; // GaveOverGames' Resource File
	char  pad[6] = { 3,0,0,1,0,0 };
	int   files;
	char  gameName[214] = {};
};
#pragma (pop)

#pragma pack (push,1)
struct GOGEntry {
	int   startOffset; 
	int   value; //  0x001420A0, might be compression type?
	int   endOffset;
	int   rawSize;
	int   size;
	short pad;

};
#pragma (pop)


int main(int argc, char* argv[])
{
	if (argc < 3) {
		std::cout << "Usage: dattool <params> <file/folder>\n"
			<< "    -c  Creates archive from a folder\n"
			<< "    -e  Extracts archive\n";
		return 1;
	}

	int mode = 0;

	// params
	for (int i = 1; i < argc -1; i++)
	{
		if (argv[i][0] != '-' || strlen(argv[i]) != 2) {
			return 1;
		}
		switch (argv[i][1])
		{
		case 'e': mode = MODE_EXTRACT;
			break;
		case 'c': mode = MODE_CREATE;
			break;
		default:
			std::cout << "ERROR: Param does not exist: " << argv[i] << std::endl;
			break;
		}
	}

	if (mode == MODE_EXTRACT)
	{
		std::ifstream pFile(argv[argc - 1], std::ifstream::binary);

		if (!pFile)
		{
			std::cout << "ERROR: Could not open " << argv[argc - 1] << "!" << std::endl;
			return 1;
		}

		if (pFile)
		{

			GOGHeader gog;
			pFile.read((char*)&gog, sizeof(GOGHeader));

			
			if (strcmp(gog.header, "GaveOverGames' Resource File") != 0) {
				std::cout << "ERROR: " << argv[argc - 1] << "is not a valid GOG Archive!" << std::endl;
				return 1;
			}

			// get files 
			for (int i = 0; i < gog.files; i++) {
				GOGEntry ent;
				pFile.read((char*)&ent, sizeof(GOGEntry));
				std::string name;
				std::getline(pFile, name, '\0');

				std::cout << "Processing: " << splitString(name, true) << std::endl;
				
				// read data
				std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(ent.size);
				pFile.read(dataBuff.get(), ent.size);

				
				// skip 0x00
				pFile.seekg(sizeof(char), pFile.cur);

				// decompress data
				std::unique_ptr<char[]> uncompressedBuffer = std::make_unique<char[]>(ent.rawSize);
				unsigned long uncompressedSize = ent.rawSize;
				int zlib_output = uncompress((Bytef*)uncompressedBuffer.get(), &uncompressedSize,
					(Bytef*)dataBuff.get(), ent.size);


				if (zlib_output == Z_MEM_ERROR) {
					std::cout << "ERROR: ZLIB: Out of memory!" << std::endl;
					return 1;
				}

				// output file

				if (checkSlash(name))
					std::experimental::filesystem::create_directories(splitString(name, false));

				std::ofstream oFile(name, std::ofstream::binary);
				oFile.write(uncompressedBuffer.get(), ent.rawSize);

			}

		}
	}
	if (mode == MODE_CREATE)
	{
		std::experimental::filesystem::path folder(argv[argc - 1]);
		if (!std::experimental::filesystem::exists(folder))
		{
			std::cout << "ERROR: Could not open directory: " << argv[argc - 1] << "!" << std::endl;
			return 1;
		}

		if (std::experimental::filesystem::exists(folder))
		{

			int filesFound = 0;
			int foldersFound = 0;
			std::cout << "Processing folder..." << std::endl;
			// get files number
			for (const auto & file : std::experimental::filesystem::recursive_directory_iterator(folder))
			{
                 filesFound++; 
				 if (std::experimental::filesystem::is_directory(file)) foldersFound++;

			}
			filesFound -= foldersFound;


			// get stuff
			std::unique_ptr<std::string[]> filePaths = std::make_unique<std::string[]>(filesFound);
			std::unique_ptr<int[]> sizes = std::make_unique<int[]>(filesFound);
			int i = 0;
			for (const auto & file : std::experimental::filesystem::recursive_directory_iterator(folder))
			{
					if (!std::experimental::filesystem::is_directory(file))
					{
						filePaths[i] = file.path().string();
						std::ifstream tFile(filePaths[i], std::ifstream::binary);
						if (tFile)
							sizes[i] = (int)getSizeToEnd(tFile);
						i++;

					}
			}

			// create header
			GOGHeader gog;
			gog.files = filesFound;
			sprintf(gog.gameName, "Created by ermaccer's dattool");
			sprintf(gog.header, "GaveOverGames' Resource File");

			// create output
			std::string output = argv[argc - 1];
			output += ".dat";
			std::ofstream oFile(output, std::ofstream::binary);

			// write header
			oFile.write((char*)&gog, sizeof(GOGHeader));

			int currentPos = (int)oFile.tellp();
			// write files!

			for (int i = 0; i < gog.files; i++)
			{
				std::ifstream pFile(filePaths[i], std::ifstream::binary);

				if (!pFile) {
					std::cout << "ERROR: Could not open: " << filePaths[i] << std::endl;
					return 1;
				}

				if (pFile)
				{
					GOGEntry ent;
					// add ints + pad
					ent.startOffset = currentPos + 22;
					// uncompressed size
					ent.rawSize = (int)getSizeToEnd(pFile);

					std::cout << "Processing: " << filePaths[i] << std::endl;

					// get raw buff
					std::unique_ptr<char[]> dataBuff = std::make_unique<char[]>(ent.rawSize);
					pFile.read(dataBuff.get(), ent.rawSize);

					// create compression buff (+20%)
					unsigned long compSize = ent.rawSize * 1.2 + 12;
					std::unique_ptr<char[]> compBuff = std::make_unique<char[]>(compSize);

					int zlib_output = compress((Bytef*)compBuff.get(), &compSize, (Bytef*)dataBuff.get(), ent.rawSize);

					if (zlib_output == Z_MEM_ERROR) {
						std::cout << "ERROR: ZLIB: Out of memory!" << std::endl;
						return 1;
					}

					std::cout << "Compressing... " << std::endl;


					ent.size = compSize;
					ent.value = 0x001420A0;
					ent.endOffset = ent.startOffset + ent.size + filePaths[i].length() + 2;
					ent.pad = 0;
					
					// write entry
					oFile.write((char*)&ent, sizeof(GOGEntry));
					// write filepath
					oFile.write(filePaths[i].c_str(), filePaths[i].length() + 1);
					// write compressed data
					oFile.write(compBuff.get(), ent.size);
					// write 0x00
					char pad = 0;
					oFile.write((char*)&pad, sizeof(char));
					// update pos
					currentPos = ent.endOffset;
				}
			}
			std::cout << "Finished." << std::endl;

		}
	}

    return 0;
}

