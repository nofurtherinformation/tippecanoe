#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include "pmtiles.hpp"
#include <protozero/varint.hpp>
#include <iostream>
#include <sstream>
#include "mvt.hpp"

pmtilesv3 *pmtilesv3_open(const char *filename, char **argv, int force) {
	pmtilesv3 *outfile = new pmtilesv3;

	struct stat st;
	if (force) {
		unlink(filename);
	} else {
		if (stat(filename, &st) == 0) {
			fprintf(stderr, "%s: %s: file exists\n", argv[0], filename);
			exit(EXIT_FAILURE);
		}
	}
	outfile->ostream.open(filename,std::ios::out | std::ios::binary);
	outfile->offset = 512000;
	for (uint64_t i = 0; i < outfile->offset; ++i) {
		char zero = 0;
		outfile->ostream.write(&zero,sizeof(char));
	}
	return outfile;
}

void serialize_string(const std::string &str, std::stringstream &ss) {
	size_t MAX_SIZE = 10;
	uint8_t slen;
	slen = str.size();
	if (slen > MAX_SIZE) {
		fprintf(stderr, "%s: header value exceeds limit\n", str.data());
		exit(EXIT_FAILURE);
	}
	ss.write((char *)&slen,1);
	ss.write(str.data(),slen);

	char zero = 0;
	for (size_t i = 0; i < MAX_SIZE-slen; i++) {
    ss.write(&zero,sizeof(char));
	}
}

// TODO: endianness check
std::string pmtilesv3_header::serialize() {
	std::stringstream ss;
	uint16_t MAGIC = 0x4d50;
	ss.write((char *)&MAGIC,2);
	uint16_t version = 2;
	ss.write((char *)&version,2);

	ss.write((char *)&root_dir_bytes,4);
	ss.write((char *)&json_metadata_bytes,4);
	ss.write((char *)&leaf_dirs_bytes,8);
	ss.write((char *)&leaf_dirs_offset,8);
	ss.write((char *)&tile_data_offset,8);
	ss.write((char *)&addressed_tiles_count,8);
	ss.write((char *)&tile_entries_count,8);
	ss.write((char *)&unique_tile_contents_count,8);

	uint8_t clustered_val = 0x0;
	if (clustered) {
		clustered_val = 0x1;
	}
	ss.write((char *)&clustered_val,1);

	serialize_string(directory_compression,ss);
	serialize_string(tile_compression,ss);
	serialize_string(tile_format,ss);

	ss.write((char *)&min_zoom,1);
	ss.write((char *)&max_zoom,1);
	ss.write((char *)&min_lon,4);
	ss.write((char *)&min_lat,4);
	ss.write((char *)&max_lon,4);
	ss.write((char *)&max_lat,4);
	ss.write((char *)&center_zoom,1);
	ss.write((char *)&center_lon,4);
	ss.write((char *)&center_lat,4);

	return ss.str();
}

void pmtilesv3_write_tile(pmtilesv3 *outfile, int z, int tx, int ty, const char *data, int size) {
	outfile->entries.emplace_back(pmtilesv3_entry(zxy_to_tileid(z,tx,ty),outfile->offset,size,1));
	outfile->ostream.write(data,size);
	outfile->offset += size;
}

void pmtilesv3_write_metadata(pmtilesv3 *outfile, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description, bool do_tilestats, std::map<std::string, std::string> const &attribute_descriptions, std::string const &program, std::string const &commandline) {
	fprintf(stderr, "not yet implemented\n");
}

void pmtilesv3_finalize(pmtilesv3 *outfile) {
	fprintf(stderr, "offset: %llu\n", outfile->offset);
	fprintf(stderr, "entries: %lu\n", outfile->entries.size());

	std::string directory = serialize_entries(outfile->entries);

	fprintf(stderr, "directory size: %lu\n", directory.size());

	pmtilesv3_header header;

	header.min_zoom = 0;
	header.max_zoom = 10;
	header.min_lon = -180.0;
	header.max_lon = 180.0;
	header.min_lat = -90.0;
	header.max_lat = 90.0;
	header.center_zoom = 0;
	header.center_lon = 0.0;
	header.center_lat = 0.0;
	header.tile_format = "pbf";
	header.tile_compression = "gzip";
	header.directory_compression = "gzip";
	header.clustered = false;
	header.unique_tile_contents_count = 0;
	header.tile_entries_count = 0;
	header.addressed_tiles_count = 0;
	header.leaf_dirs_offset = 0;
	header.tile_data_offset = 0;
	header.leaf_dirs_bytes = 0;
	header.json_metadata_bytes = 0;
	header.root_dir_bytes = 0;

	std::string serialized_header = header.serialize();

	// write header
	// write root directory
	// write json
	// write leaf dirs
	// write tile data

	std::cout << serialized_header.size() << std::endl;

	outfile->ostream.close();
	delete outfile;
};

inline void rotate(int64_t n, int64_t &x, int64_t &y, int64_t rx, int64_t ry) {
  if (ry == 0) {
    if (rx == 1) {
        x = n-1 - x;
        y = n-1 - y;
    }
    int64_t t = x;
    x = y;
    y = t;
  }
}

pmtiles_zxy t_on_level(uint8_t z, uint64_t pos) {
    int64_t n = 1 << z;
    int64_t rx, ry, s, t = pos;
    int64_t tx = 0;
    int64_t ty = 0;

    for (s=1; s<n; s*=2) {
      rx = 1 & (t/2);
      ry = 1 & (t ^ rx);
      rotate(s, tx, ty, rx, ry);
      tx += s * rx;
      ty += s * ry;
      t /= 4;
    }
    return pmtiles_zxy(z,tx,ty);
}

pmtiles_zxy tileid_to_zxy(uint64_t tile_id) {
	uint64_t acc = 0;
  uint8_t t_z = 0;
  while(true) {
    uint64_t num_tiles = (1 << t_z) * (1 << t_z);
    if (acc + num_tiles > tile_id) {
      return t_on_level(t_z, tile_id - acc);
    }
    acc += num_tiles;
    t_z++;
  }
}

uint64_t zxy_to_tileid(uint8_t z, uint32_t x, uint32_t y) {
	uint64_t acc = 0;
  for (uint8_t t_z = 0; t_z < z; t_z++) acc += (0x1 << t_z) * (0x1 << t_z);
  int64_t n = 1 << z;
  int64_t rx, ry, s, d=0;
  int64_t tx = x;
  int64_t ty = y;
  for (s=n/2; s>0; s/=2) {
    rx = (tx & s) > 0;
    ry = (ty & s) > 0;
    d += s * s * ((3 * rx) ^ ry);
    rotate(s, tx, ty, rx, ry);
  }
  return acc + d;
}

// precondition: entries is sorted by tile_id
std::string serialize_entries(const std::vector<pmtilesv3_entry>& entries) {
	std::string data;

	protozero::write_varint(std::back_inserter(data), entries.size());

	uint64_t last_id = 0;
	for (auto const &entry : entries) {
		protozero::write_varint(std::back_inserter(data), entry.tile_id - last_id);
		last_id = entry.tile_id;
	}

	for (auto const &entry : entries) {
		protozero::write_varint(std::back_inserter(data), entry.run_length);
	}

	for (auto const &entry : entries) {
		protozero::write_varint(std::back_inserter(data), entry.length);
	}

	for (size_t i = 0; i < entries.size(); i++) {
		if (i > 0 && entries[i].offset == entries[i-1].offset + entries[i-1].length) {
			protozero::write_varint(std::back_inserter(data), 0);
		} else {
			protozero::write_varint(std::back_inserter(data), entries[i].offset+1);
		}
	}

	std::string compressed;
	compress(data,compressed);

	return compressed;
}

std::vector<pmtilesv3_entry> deserialize_entries(const std::string &data) {
	std::string decompressed;
	decompress(data,decompressed);

	const char *t = decompressed.data();
	const char *end = t + decompressed.size();

	uint64_t num_entries = protozero::decode_varint(&t,end);

	std::vector<pmtilesv3_entry> result;
	result.resize(num_entries);

	uint64_t last_id = 0;
	for (size_t i = 0; i < num_entries; i++) {
		uint64_t tile_id = last_id + protozero::decode_varint(&t,end);
		result[i].tile_id = tile_id;
		last_id = tile_id;
	}

	for (size_t i = 0; i < num_entries; i++) {
		result[i].run_length = protozero::decode_varint(&t,end);
	}

	for (size_t i = 0; i < num_entries; i++) {
		result[i].length = protozero::decode_varint(&t,end);
	}

	for (size_t i = 0; i < num_entries; i++) {
		uint64_t tmp = protozero::decode_varint(&t,end);

		if (i > 0 && tmp == 0) {
			result[i].offset = result[i-1].offset + result[i-1].length;
		} else {
			result[i].offset = tmp - 1;
		}
	}

	// assert the directory has been fully consumed
	if (t != end) {
		fprintf(stderr, "Error: malformed pmtiles directory\n");
		exit(EXIT_FAILURE);
  }

	return result;
}