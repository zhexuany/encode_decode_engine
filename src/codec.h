
#ifndef __CODEC_H__
#define __CODEC_H__

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/stat.h>
typedef char* string;
/* Returns 1 if 'val' is a valid char under the encoding scheme */
int is_valid_char(uint8_t val);

/* encode 3x 8-bit binary bytes as 4x '6-bit' characters */
size_t encode_block(uint8_t *inp, uint8_t *out, int len);

/* decode 4x '6-bit' characters into 3x 8-bit binary bytes */
size_t decode_block(uint8_t *inp, uint8_t *out);

/*this fucntion will travel the working directory and encode file and
  recursively travel directory
 */
size_t encodeDir(const string path_in, const string path_out);

/*encode file*/
size_t encode(FILE *infile, FILE *outfile);

/*this fucntion will travel the working directory and decode file and
  recursively travel directory
*/
size_t decodeDir(const string path_in, const string path_out);

/*decode file*/
size_t decode(FILE *infile, FILE *outfile);

/*print summary into report file*/
size_t reportFile(const string path_in, const string file_in, const string path_out, const string file_out, const string filename, const char flag);

/*get file szie accoring the full path*/
off_t getFilesize(const string f_in);

/*get file number accoring to full path*/
ino_t getFileNumber(const string path);

/*if the path is a sym link return true, oterwise return false*/
size_t softLinkStatus(const string path);


/*return the number of hard links*/
nlink_t hardLinkStatus(const string path);

/*return true if the file is a directory*/
size_t isDirectory(const string path);

/*create a directory */
void makeDir(const string path_in, const string path_out);

/*call system call sort to sort report file*/
void sort(const string path_in, const string path_out);

/*delete the temperatory reprot file*/
void delete(const string path_in, const string path_out);
#endif /* __CODEC_H__ */
