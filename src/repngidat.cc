/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2002, 2003, 2005 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "portable.h"

#include "pngex.h"
#include "file.h"
#include "compress.h"
#include "siglock.h"

#include "lib/endianrw.h"

#include <iostream>
#include <iomanip>

using namespace std;

shrink_t opt_level;
bool opt_quiet;
bool opt_force;
bool opt_crc;

adv_error recompress_png_idat(adv_fz* f_in, adv_fz *f_out) {
  unsigned char *data = NULL;
  unsigned int type = 0, size = 0, noZlibHeader = 0;
  unsigned int pixel = 1, width = 1, width_align = 1, height = 1, depth = 1, depth_bytes = 1, color_type = 0;

  unsigned char *decompressed_IDAT = NULL;

  if (adv_png_read_signature(f_in)         != 0) { goto err; }
  if (adv_png_write_signature(f_out, NULL) != 0) { goto err; }

  do {
    if (adv_png_read_chunk(f_in, &data, &size, &type) != 0) { goto err; }

  reloopWithChunk:
    switch (type) {
      case ADV_PNG_CN_IDAT:
        {
          unsigned int decompressed_IDAT_size = (height * (width_align * pixel + 1));
          if((decompressed_IDAT = (unsigned char *)malloc(decompressed_IDAT_size)) == NULL) { goto err_data; }

          int r;
          z_stream z;
          memset(&z, 0, sizeof(z_stream));
                      
          z.zalloc = NULL;
          z.zfree = NULL;
          z.next_out = (Bytef *)decompressed_IDAT;
          z.avail_out = decompressed_IDAT_size;
                      
          r = inflateInit2(&z, noZlibHeader ? -15 : 15); // A negative value is used to specify a "raw" (no header, footer, CRC info) zlib stream which is used by the iPhone .png format.
                      
          while (r == Z_OK && type == ADV_PNG_CN_IDAT) {
            z.next_in  = data;
            z.avail_in = size;
            r = inflate(&z, Z_NO_FLUSH);
            free(data); data = NULL;
            if (adv_png_read_chunk(f_in, &data, &size, &type) != 0) { inflateEnd(&z); goto err_data; }
          }

          inflateEnd(&z);

          if (r != Z_STREAM_END) { error_set("Unable to decompress data"); goto err_data; }

          unsigned int compressed_IDAT_size = oversize_zlib(z.total_out) + 1024u; // For extremely small file corner cases.
          unsigned char *compressed_IDAT = NULL;

          if((compressed_IDAT = (unsigned char *)malloc(compressed_IDAT_size)) == NULL) { error_set("Unable to allocate memory"); goto err_data; }
          
          if(!compress_zlib(shrink_extreme, compressed_IDAT, compressed_IDAT_size, decompressed_IDAT, decompressed_IDAT_size, noZlibHeader ? 0 : 1)) { error_set("Recompression failed"); goto err_data; }
          if (adv_png_write_chunk(f_out, ADV_PNG_CN_IDAT, compressed_IDAT, compressed_IDAT_size, NULL) != 0) { goto err_data; }
          free(compressed_IDAT); compressed_IDAT = NULL;
          goto reloopWithChunk;
        }
        break;


      case ADV_PNG_CN_CgBI:
        noZlibHeader = 1;
        goto writeChunk;
        break;

      case ADV_PNG_CN_IHDR:
        width       = be_uint32_read(data + 0);
        height      = be_uint32_read(data + 4);
        depth       = data[8];
        depth_bytes = (depth < 8) ? 1 : depth / 8;
        color_type  = data[9];

        switch(color_type) {
          case 0: /* Grey  */ pixel = 1 * depth_bytes; if(!((depth == 1) || (depth == 2) || (depth == 4) || (depth == 8) || (depth == 16))) { error_unsupported_set("Unsupported bit depth/color type, %d/%d", depth, color_type); goto err_data; } break;
          case 2: /* RGB   */ pixel = 3 * depth_bytes; if(!(                                                (depth == 8) || (depth == 16))) { error_unsupported_set("Unsupported bit depth/color type, %d/%d", depth, color_type); goto err_data; } break;
          case 3: /* Index */ pixel = 1 * depth_bytes; if(!((depth == 1) || (depth == 2) || (depth == 4) || (depth == 8)                 )) { error_unsupported_set("Unsupported bit depth/color type, %d/%d", depth, color_type); goto err_data; } break;
          case 4: /* GreyA */ pixel = 2 * depth_bytes; if(!(                                                (depth == 8) || (depth == 16))) { error_unsupported_set("Unsupported bit depth/color type, %d/%d", depth, color_type); goto err_data; } break;
          case 6: /* RGBA  */ pixel = 4 * depth_bytes; if(!(                                                (depth == 8) || (depth == 16))) { error_unsupported_set("Unsupported bit depth/color type, %d/%d", depth, color_type); goto err_data; } break;
          default: error_unsupported_set("Unsupported color type, %d", color_type); goto err_data; break;
        }

        width_align = (width + 7) & ~7; // worst case.

        goto writeChunk;
        break;


      default :
      writeChunk:
        if (adv_png_write_chunk(f_out, type, data, size, NULL) != 0) { goto err_data; }
        free(data); data = NULL;
        if(type == ADV_PNG_CN_IEND) {
          if(decompressed_IDAT != NULL) { free(decompressed_IDAT); decompressed_IDAT = NULL; }
          return(0);
        }
        break;
    }

    free(data); data = NULL;
  } while (type != ADV_PNG_CN_IEND);

  error_set("Invalid PNG file");
  return -1;

 err_data:
  if(data != NULL) { free(data); }
 err:
  if(decompressed_IDAT != NULL) { free(decompressed_IDAT); decompressed_IDAT = NULL; }
  return -1;
}

void convert_inplace(const string& path)
{
	adv_fz* f_in;
	adv_fz* f_out;

	// temp name of the saved file
	string path_dst = file_basepath(path) + ".tmp";

	f_in = fzopen(path.c_str(), "rb");
	if (!f_in) {
		throw error() << "Failed open for reading " << path;
	}

	f_out = fzopen(path_dst.c_str(), "wb");
	if (!f_out) {
		fzclose(f_in);
		throw error() << "Failed open for writing " << path_dst;
	}

	try {
		if(recompress_png_idat(f_in, f_out) != 0) { throw_png_error(); }
	} catch (...) {
		fzclose(f_in);
		fzclose(f_out);
		remove(path_dst.c_str());
		throw;
	}

	fzclose(f_in);
	fzclose(f_out);

	unsigned dst_size = file_size(path_dst);
	if (!opt_force && file_size(path) < dst_size) {
		// delete the new file
		remove(path_dst.c_str());
		throw error_unsupported() << "Bigger " << dst_size;
	} else {
		// prevent external signal
		sig_auto_lock sal;

		// delete the old file
		if (remove(path.c_str()) != 0) {
			remove(path_dst.c_str());
			throw error() << "Failed delete of " << path;
		}

		// rename the new version with the correct name
		if (::rename(path_dst.c_str(), path.c_str()) != 0) {
			throw error() << "Failed rename of " << path_dst << " to " << path;
		}
	}
}

void png_print(const string& path)
{
	unsigned type;
	unsigned size;
	adv_fz* f_in;

	f_in = fzopen(path.c_str(), "rb");
	if (!f_in) {
		throw error() << "Failed open for reading " << path;
	}

	try {
		if (adv_png_read_signature(f_in) != 0) {
			throw_png_error();
		}

		do {
			unsigned char* data;

			if (adv_png_read_chunk(f_in, &data, &size, &type) != 0) {
				throw_png_error();
			}

			if (opt_crc) {
				cout << hex << setw(8) << setfill('0') << crc32(0, data, size);
				cout << " ";
				cout << dec << setw(0) << setfill(' ') << size;
				cout << "\n";
			} else {
				png_print_chunk(type, data, size);
			}

			free(data);

		} while (type != ADV_PNG_CN_IEND);
	} catch (...) {
		fzclose(f_in);
		throw;
	}

	fzclose(f_in);
}

void rezip_single(const string& file, unsigned long long& total_0, unsigned long long& total_1)
{
	unsigned size_0;
	unsigned size_1;
	string desc;

	if (!file_exists(file)) {
		throw error() << "File " << file << " doesn't exist";
	}

	try {
		size_0 = file_size(file);

		try {
			convert_inplace(file);
		} catch (error_unsupported& e) {
			desc = e.desc_get();
		}

		size_1 = file_size(file);
	} catch (error& e) {
		throw e << " on " << file;
	}

	if (!opt_quiet) {
		cout << setw(12) << size_0 << setw(12) << size_1 << " ";
		if (size_0) {
			unsigned perc = size_1 * 100LL / size_0;
			cout << setw(3) << perc;
		} else {
			cout << "  0";
		}
		cout << "% " << file;
		if (desc.length())
			cout << " (" << desc << ")";
		cout << endl;
	}

	total_0 += size_0;
	total_1 += size_1;
}

void rezip_all(int argc, char* argv[])
{
	unsigned long long total_0 = 0;
	unsigned long long total_1 = 0;

	for(int i=0;i<argc;++i)
		rezip_single(argv[i], total_0, total_1);

	if (!opt_quiet) {
		cout << setw(12) << total_0 << setw(12) << total_1 << " ";
		if (total_0) {
			unsigned perc = total_1 * 100LL / total_0;
			cout << setw(3) << perc;
		} else {
			cout << "  0";
		}
		cout << "%" << endl;
	}
}

void list_all(int argc, char* argv[])
{
	for(int i=0;i<argc;++i) {
		if (argc > 1 && !opt_crc)
			cout << "File: " << argv[i] << endl;
		png_print(argv[i]);
	}
}

#if HAVE_GETOPT_LONG
struct option long_options[] = {
	{"recompress", 0, 0, 'z'},
	{"list", 0, 0, 'l'},
	{"list-crc", 0, 0, 'L'},

	{"shrink-store", 0, 0, '0'},
	{"shrink-fast", 0, 0, '1'},
	{"shrink-normal", 0, 0, '2'},
	{"shrink-extra", 0, 0, '3'},
	{"shrink-insane", 0, 0, '4'},

	{"quiet", 0, 0, 'q'},
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'V'},
	{0, 0, 0, 0}
};
#endif

#define OPTIONS "zlL01234fqhV"

void version()
{
	cout << PACKAGE " v" VERSION " by Andrea Mazzoleni" << endl;
}

void usage()
{
	version();

	cout << "Usage: advpngidat [options] [FILES...]" << endl;
	cout << endl;
	cout << "Modes:" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-l, --list          ", "-l") "  List the content of the files" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-z, --recompress    ", "-z") "  Recompress the specified files" << endl;
	cout << "Options:" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-0, --shrink-store  ", "-0") "  Don't compress" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-1, --shrink-fast   ", "-1") "  Compress fast" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-2, --shrink-normal ", "-2") "  Compress normal" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-3, --shrink-extra  ", "-3") "  Compress extra" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-4, --shrink-insane ", "-4") "  Compress extreme" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-f, --force         ", "-f") "  Force the new file also if it's bigger" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-q, --quiet         ", "-q") "  Don't print on the console" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-h, --help          ", "-h") "  Help of the program" << endl;
	cout << "  " SWITCH_GETOPT_LONG("-V, --version       ", "-V") "  Version of the program" << endl;
}

void process(int argc, char* argv[])
{
	enum cmd_t {
		cmd_unset, cmd_recompress, cmd_list
	} cmd = cmd_unset;

	opt_quiet = false;
	opt_level = shrink_normal;
	opt_force = false;
	opt_crc = false;

	if (argc <= 1) {
		usage();
		return;
	}

	int c = 0;

	opterr = 0; // don't print errors

	while ((c =
#if HAVE_GETOPT_LONG
		getopt_long(argc, argv, OPTIONS, long_options, 0))
#else
		getopt(argc, argv, OPTIONS))
#endif
	!= EOF) {
		switch (c) {
		case 'z' :
			if (cmd != cmd_unset)
				throw error() << "Too many commands";
			cmd = cmd_recompress;
			break;
		case 'l' :
			if (cmd != cmd_unset)
				throw error() << "Too many commands";
			cmd = cmd_list;
			break;
		case 'L' :
			if (cmd != cmd_unset)
				throw error() << "Too many commands";
			cmd = cmd_list;
			opt_crc = true;
			break;
		case '0' :
			opt_level = shrink_none;
			opt_force = true;
			break;
		case '1' :
			opt_level = shrink_fast;
			break;
		case '2' :
			opt_level = shrink_normal;
			break;
		case '3' :
			opt_level = shrink_extra;
			break;
		case '4' :
			opt_level = shrink_extreme;
			break;
		case 'f' :
			opt_force = true;
			break;
		case 'q' :
			opt_quiet = true;
			break;
		case 'h' :
			usage();
			return;
		case 'V' :
			version();
			return;
		default: {
			// not optimal code for g++ 2.95.3
			string opt;
			opt = (char)optopt;
			throw error() << "Unknown option `" << opt << "'";
			}
		} 
	}

	switch (cmd) {
	case cmd_recompress :
		rezip_all(argc - optind, argv + optind);
		break;
	case cmd_list :
		list_all(argc - optind, argv + optind);
		break;
	case cmd_unset :
		throw error() << "No command specified";
	}
}

int main(int argc, char* argv[])
{
	try {
		process(argc, argv);
	} catch (error& e) {
		cerr << e << endl;
		exit(EXIT_FAILURE);
	} catch (std::bad_alloc) {
		cerr << "Low memory" << endl;
		exit(EXIT_FAILURE);
	} catch (...) {
		cerr << "Unknown error" << endl;
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}

