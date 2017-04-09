#define _CRT_SECURE_NO_DEPRECATE

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include "include/png.h"
#include <cstdio>

//THIS FILE DOESN'T HAVE APPROPRIATE ERROR HANDLING FOR ITS METHODS.
//FOR CREATION WHEN SUPERSTRUCTURE IS KNOWN.

struct MyException
{
	MyException(std::string arg) : debuglog(arg) {}
	std::string debuglog = "";
};

struct Pixel {
	int r;
	int g;
	int b;
	int a;
	Pixel()
	{
		r = 0.0f;
		g = 0.0f;
		b = 0.0f;
		a = 0.0f;
	}

	Pixel(int red, int green, int blue, int alpha)
	{
		r = red;
		g = green;
		b = blue;
		a = alpha;
	}
};

struct Bitmap {
	bool* values;
	int width;
	int height;
};

struct image_data {
	int width;
	int height;
	//row_pointers is expected to be allocated using malloc.
	png_bytep* row_pointers = NULL;
	image_data() {}
	image_data(int w, int h, png_bytep* array) 
	{
		width = w;
		height = h;
		row_pointers = array;
	}
	~image_data() 
	{
		std::cout << "Image Data Destruction" << std::endl;
		//DON'T FORGET TO FREE DATA FOR ANY (m)ALLOCATED MEMORY
		if (row_pointers == NULL) 
		{
			return;
		}
		for (int y = 0; y < height; y++)
		{
			free(row_pointers[y]);
		}
		free(row_pointers);
	}
};

static bool boolAt(Bitmap* bitmap, int x, int y)
{
	return *(bitmap->values + bitmap->width * y + x);
}


void read_RGBA_data(char* filename, image_data& container)
{
		png_byte color_type;
		png_byte bit_depth;
		png_bytep *row_pointers;

		FILE *fp = fopen(filename, "rb");
		//Check filetype
		png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!png)
		{
			throw MyException("exit thrown on !png");
		}
		png_infop info = png_create_info_struct(png);
		if (!info)
		{
			throw MyException("exit thrown on !info");
		}
		if (setjmp(png_jmpbuf(png)))
		{
			throw MyException("exit thrown on setjmp(png_jmpbuf(png))");
		}

		png_init_io(png, fp);
		png_read_info(png, info);

		const int width = png_get_image_width(png, info);
		const int height = png_get_image_height(png, info);
		color_type = png_get_color_type(png, info);
		bit_depth = png_get_bit_depth(png, info);

		//Only accept RGBA, 16-bit depth is stripped down to 8bit.

		if (color_type == PNG_COLOR_TYPE_RGB)
		{
			throw MyException("RGB not accepted as it is not useful to program design");
		}
		else if (color_type != PNG_COLOR_TYPE_RGB_ALPHA)
		{
			throw MyException("exit thrown on color_type check");
		}
		else {
			//Set bit depth to 8 (strip from 16 if not already)
			if (bit_depth == 16) {
				png_set_strip_16(png);
			}
		}

		png_read_update_info(png, info);
		row_pointers = NULL;
		row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
		for (int row = 0; row < height; row++)
		{
			row_pointers[row] = (unsigned char *)png_malloc(png, png_get_rowbytes(png, info));
		}
		png_read_image(png, row_pointers);

		//deallocation memory of libpng structs.
		png_destroy_read_struct(&png, &info, NULL);
		png = NULL;
		info = NULL;
		fclose(fp);
		container.height = height;
		container.width = width;
		container.row_pointers = row_pointers;
}



void read_png_overwrite_map(char* filename)
{
	image_data image;
	read_RGBA_data(filename, image);
	
	//Interpret pixels alpha and create a new string of 0|1's to represent the tile. 
	//If the pixels are 32-bit RGBA, then the data is composed of interleaved and repeating R, G, B, and A samples. 
	//R comes first, then G, then B, then A.

	//Create new filename for accompanying bitmap.
	std::string map_filename = filename;
	int max = map_filename.size();
	//remove the previous extention and append.
	map_filename[max - 4] = '_';
	map_filename[max - 3] = '_';
	map_filename[max - 2] = 'm';
	map_filename[max - 1] = '.';
	map_filename.append("txt");
	
	//Convert the alpha information into a standard string format
	int num_of_values = image.width * 4;
	int current_value = 0;
	std::string charStream = "|";
	for (int Y = 0; Y < image.height; Y++)
	{
		png_byte* row = image.row_pointers[Y];
		for (int X = 3; X < num_of_values; X+=4)
		{
			current_value = row[X];
			if (current_value > 0) 
			{
				current_value = 1;
			}
			charStream += std::to_string(current_value) + ", ";
		}
		charStream += "|\n";
		if (Y != image.height - 1)
		{
			charStream += "|";
		}
	}
	//Create or edit&flush file with standardised string.
	const char* new_filename = map_filename.c_str();
	FILE *fp2 = fopen(new_filename, "w+");
	const char *cstr = charStream.c_str();
	fputs(cstr, fp2);
	fclose(fp2);
}


bool** create_2D_array_from_map(std::string *filename, int dimension)
{
	std::ifstream inf(*filename);

	//Return error if cannot open file
	if (!inf)
	{
		throw MyException("Cannot open file when creating 2D array from textmap");
	}

	//initialize bitmap array
	bool **bitmap = new bool*[dimension];
	for (int i = 0; i < dimension; i++) {
		bitmap[i] = new bool[dimension];
	}

	std::string buffer = "";
	char c;
	int row = 0;
	int column = 0;

	//add parsing details here.
	while (c = inf.get())
	{
		if (inf.eof())
		{
			break;
		}
		else if (c == '|')
		{
			//Increment row
			if (row == (dimension - 1))
			{
				break;
			}
			row++;
			column = 0;
		}
		else
		{
			//Add number to string
			column++;
			if (c == '0')
			{
				bitmap[row][column] = false;
			}
			else if (c == '1')
			{
				bitmap[row][column] = true;
			}
			else
			{
				//some error handling/throwing mechanism here.
				throw MyException("Unexpected formatting in text file");
			}
		}
	}

	inf.close();
	return bitmap;
}

//'Dimension' is the value of a single tile length (assuming square shape)
//This method only handles tilemaps with all tiles the same sq_dimension
void split_tilemap(char* filename, int dimension) 
{
	image_data tilemap_data;
	read_RGBA_data(filename, tilemap_data);
	int num_of_tiles = (tilemap_data.width / dimension) * (tilemap_data.height / dimension);
	int file_Y_pos = 0;
	int file_X_pos = 0;
	int file_number = 0;
	std::string new_filename = filename;
	new_filename.erase(new_filename.size() - 4, 4);
	int name_length = new_filename.size();

	png_bytep* row_pointers = NULL;
	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * dimension);
	for (int row = 0; row < dimension; row++)
	{
		row_pointers[row] = (png_byte*)malloc((png_uint_32) dimension * 4);
	}

	//Process the file in local chunks
	int X, Y = 0;
	while (file_number < num_of_tiles) {
		for (; Y < dimension; Y++)
		{
			for (; X < dimension; X++)
			{
				row_pointers[Y][X] = tilemap_data.row_pointers[dimension * file_Y_pos + Y][dimension * file_X_pos + X];
			}
		}
		new_filename.append("_tile" + std::to_string(file_number) + ".png");
		FILE *fp = fopen(new_filename.c_str(), "wb");
		png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop info = png_create_info_struct(png);
		if (!fp && !png && !info && !(setjmp(png_jmpbuf(png))))
		{	
			throw MyException("Issue encountered when creating new png files");
		}
		png_init_io(png, fp);

		//Output is 8-bit depth, RGBA format
		int width, height = dimension;
		png_set_IHDR(
			png,
			info,
			width, height,
			8,
			PNG_COLOR_TYPE_RGBA,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT
			);
		png_write_info(png, info);
		png_write_image(png, row_pointers);
		png_write_end(png, NULL);
		fclose(fp);
		//remove the previously added name ending for the next iteration
		new_filename.erase(name_length, new_filename.size() - name_length);

		//Iterate file_pos' to adjust where the next iteration of row_pointers is read from.
		file_X_pos++;
		if (file_X_pos == (tilemap_data.width / dimension))
		{
			file_X_pos = 0;
			file_Y_pos++;
		}
		file_number++;
		std::cout << "file_number: " + std::to_string(file_number) << std::endl;
	}

	//free memory.
	for (int y = 0; y < dimension; y++)
	{
		free(row_pointers[y]);
	}
	free(row_pointers);
}


//Add a third option to create a tilemap from png images in e.g. a folder or zip? - requires boost filesystem or other similar.
int main() 
{
	try 
	{
		std::cout << "Bitmap Creator"
			<< "\n==============\n"
			<< "Select the appropriate function:\n"
			<< "\t(1) Create png transparency boolean text file (RGBA only)\n"
			<< "\t(2) Split png tilemap into sq-dimension png files\n\n"
			<< "Selection: ";

		char c = '0';
		while (!(c == '1' | c == '2')) 
		{
			std::cin >> c;
		}
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		switch (c) 
		{
		case '1':
			std::cout << "Input the filename of (square dimension) png: ";
			char line[30];
			std::cin.get(line, 30);
			read_png_overwrite_map(line);
			break;
		case '2':
			std::cout << "Input the filename of the tilemap png (output to square dimension tiles) & dimension width of a single tile\nFilename: ";
			std::cin.get(line, 30);
			std::cout << "\nDimension: ";
			std::cin.clear();
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			int dimension;
			std::cin >> dimension;
			split_tilemap(line, dimension);
			break;
		}
	}
	catch (MyException& e)
	{
		std::cerr << "Error content: " << e.debuglog << std::endl;
		exit(0);
	}
	catch (...)
	{
		std::cerr << "Other Exception Caught" << std::endl;
		exit(0);
	}
	std::cout << "End of Program" << std::endl;
	return 0;
}