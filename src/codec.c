#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
/*this header file was copied from textbook*/
#include "makeargv.h"
#include "alloc.h"
#include "codec.h"

/*define some strings for reportFile function*/
#define FILE_TYPE   "regular file"
#define DIRECTORY   "directory"
#define REPORT      "_report.txt"
#define TEMPREPORT  "temp.txt"
#define SOFT_LINK   "sym link"
#define HARD_LINK   "hard link"
#define SORT        "sort"
#define REMOVE      "rm"
#define FLAGS       O_WRONLY | O_CREAT | O_APPEND
/*define operation flag for reprotFile function*/
#define HARD        0x48
#define SOFT        0x53
#define DUPLICATE   0x44
#define DIRECT      0x4e
#define DEFAULT     0x00
/*define flag option for skiping currecnt directory
  parent direcotry
 */
#define CURRENT_DIR "."
#define PARENT_DIR  ".."
/*define contents may be written into report file */
#define COMMA       ", "
#define NEWLINE     0x0a
#define ZERO        0x30
#define EMPTY       " "
#define SLASH       "/"
#define MAX_BUFF    1024
#define CTIME_SIZE  26
#define MAX_FILE    64
#define WHITESPACE  64
#define EQUALS      65
#define INVALID     66
#define PERM        0755
#define INP_SIZE    3
#define OUT_SIZE    4
/* deinfe boolean algerba */
#define bool        int
#define true        1
#define false       0

/*define global variable for detecting duplicate hard link*/
ino_t in = 0;
static bool duplicate = false;

/* Encoding translation table */
static ino_t filenumber[MAX_FILE];
static const unsigned char enc64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Decoding translation table */
static const unsigned char dec64[] = {
    66,66,66,66,66,66,66,66,66,66,64,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,62,66,66,66,63,52,53,
    54,55,56,57,58,59,60,61,66,66,66,65,66,66,66, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,66,66,66,66,66,66,26,27,28,
    29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
    66,66,66,66,66,66
};

/*
 * @brief Checks the value passed is a valid encoded character or not
 *        Returns 1 if valid, 0 if invalid
 *
 * @param val value to be tested
 */
int is_valid_char(uint8_t val) {
    int flag = 0;
    unsigned char c = dec64[val];

    switch(c) {
        case WHITESPACE:
        case INVALID:
            flag = 0;
            break;
        case EQUALS:
        default:
            flag = 1;
    }
    return flag;
}

/* @brief encode 3x 8-bit binary bytes as 4x '6-bit' characters
 *        Returns length of encoded string
 *
 * @param inp input array (size 3)
 * @param out output array (size 4)
 * @param len length of input array
 */
size_t encode_block(uint8_t *inp, uint8_t *out, int len) {
    size_t output_len = (sizeof(uint8_t) * 4);
    memset(out, 0, output_len);
    out[0] = (uint8_t) enc64[(int)(inp[0] >> 2) ];
    out[1] = (uint8_t) enc64[(int)(((inp[0] & 0x03) << 4) |
            ((inp[1] & 0xf0) >> 4)) ];
    out[2] = (uint8_t) ((len > 1) ? (enc64[(int)(((inp[1] & 0x0f) << 2) |
                    ((inp[2] & 0xc0) >> 6)) ]) : '=');
    out[3] = (uint8_t) ((len > 2) ? (enc64[(int)(inp[2] & 0x3f) ]) : '=');
    return output_len;
}

/* @brief decode 4x '6-bit' characters into 3x 8-bit binary bytes
 *        Returns the length of the decoded block
 *
 * @param inp input array (size 4)
 * @param out output array (size 3)
 */
size_t decode_block(uint8_t *inp, uint8_t *out) {
    int j, iter = 0;
    unsigned char val;
    size_t buf = 0;
    size_t output_len = 0;

    memset(out, '\0', sizeof(uint8_t) * 3);
    for (j = 0; j < 4; j++) {
        if (inp[j] == '=')
            continue;
        val = dec64[inp[j]];
        buf = buf << 6 | val;
        ++iter;
    }
    if (iter == 4) {
        out[0] = (unsigned char) (buf >> 16) & 0xff;
        out[1] = (unsigned char) (buf >> 8) & 0xff;
        out[2] = (unsigned char) (buf & 0xff);
    } else if (iter == 3) {
        out[0] = (unsigned char) (buf >> 10) & 0xff;
        out[1] = (unsigned char) (buf >> 2) & 0xff;
    } else if (iter == 2) {
        out[0] = (unsigned char) (buf >> 4) & 0xff;
    }
    for (j = 0; j < (iter - 1); j++) {
        if (out[j] != 0)
            ++output_len;
    }
    return output_len;
}


/*used for encde file*/
size_t encode(FILE* infile, FILE* outfile) {
  uint8_t* in;
  uint8_t* out;
  int i, len = 0;
  in = MALLOC( INP_SIZE, uint8_t );
  out = MALLOC( OUT_SIZE, uint8_t );
  /*if the file is empty, we need add newline manually*/
  bool emptyFile = true;
  while( feof( infile ) == 0 ) {
    len = 0;
    /*read 3 bytes at a time*/
    for( i = 0; i < INP_SIZE; i++ ) {
      in[i] = (uint8_t) getc( infile );

      if( feof(infile) == 0 ) {
        len++;
      }
      else {
        in[i] = (uint8_t) 0;
      }
    }

    if ( len > 0 ) {
      emptyFile = false;
      encode_block( in, out, len );
      for( i = 0; i < OUT_SIZE; i++ ) {
        if( fputc( (int)(out[i]), outfile ) == 0 ){
          if( ferror( outfile ) != 0 )      {
            perror( "I/O error when reading" );
          }
          break;
        }
      }
    }
  }
  /*write newline when finished to enocde file if the original
   file is not empty */
  if (!emptyFile) {
    fputc( NEWLINE, outfile );
  }

  free(in);
  free(out);
  return 0;
  }


/*recursivly traver directory specified by path_in and crete a directory and its name is path_out
  In the process of travelling, it will call encode fucntion if the file is not soft link. In addition,
  it will skip duplicated hard link if we already encoded it.
*/
size_t encodeDir(const string path_in, const string path_out) {
  struct dirent *direntp;
  DIR *dirp;
  string currPath, outPath;
  FILE* infile;
  FILE* outfile;

  /*create output directory first*/
  makeDir(path_in, path_out);

  /*open a directory*/
  if ((dirp = opendir(path_in)) == NULL) {
    perror ("Failed to open directory");
  }

  /*use while loop and readdir to read a directory*/
  while ((direntp = readdir(dirp)) != NULL) {

    /*skip parent directory and current directory*/
    if (!strcmp(direntp -> d_name, CURRENT_DIR) ||
        !strcmp(direntp -> d_name, PARENT_DIR)) {
      continue;
    }

    /*allocate memory for current path and output path*/
    currPath = CALLOC(strlen(path_in) +
                     strlen(direntp -> d_name) + 1, char);
    outPath = CALLOC(strlen(path_out) +
                     strlen(direntp -> d_name) + 1, char);
    /*make path of current file*/
    strcat(currPath, path_in);
    strcat(currPath, SLASH);
    strcat(currPath, direntp -> d_name);

    /*make path of output file*/
    strcat(outPath, path_out);
    strcat(outPath, SLASH);
    strcat(outPath, currPath);

    /*if it is file call fopen and start to encode*/
    /*if it is dir, we need update direntp and redo the process*/
    if (!isDirectory(currPath)) {
      /*call makeDir to create a file in output dir*/
      if (!(infile  = fopen(currPath, "r"))) {
        perror("File opening failed");
        return EXIT_FAILURE;
      }

      if (!(outfile = fopen(outPath, "w"))) {
        perror("File opening failed");
        return EXIT_FAILURE;
      }

      /*start to encode file*/
      /*if the file is not soft link then encode the file*/
      if ( softLinkStatus(currPath) ) {
        reportFile(path_in, currPath, path_out, outPath,
                   direntp -> d_name, SOFT);
        fprintf( stderr, "The %s is sym link\n", direntp -> d_name);
        /*call reprot file and close file stream*/
      }
      else
        {
        if ( hardLinkStatus(currPath) == 1 ) {

          encode( infile, outfile );
          fclose(outfile);
          fclose(infile);
          reportFile(path_in, currPath, path_out, outPath,
                     direntp -> d_name, DEFAULT);
          printf( "Start to encode %s\n", direntp -> d_name );
        }
        else
          {
          // printf("%lld\n", getFileNumber(currPath));
          ino_t result = getFileNumber(currPath);
          filenumber[in] = result;

          ino_t i;
          for (i = 0; i < in; i++) {
            if (result == filenumber[i]) {
              printf( "Duplicated file founded %s\n", direntp -> d_name );
              duplicate = true;
              reportFile(path_in, currPath, path_out, outPath,
                         direntp -> d_name, DUPLICATE);
              break;
            }
          }
          /*use information from for loop*/
          if (!duplicate) {
            encode( infile, outfile );
            printf( "Start to encode %s\n", direntp -> d_name );
            fclose(outfile);
            fclose(infile);
            reportFile(path_in, currPath, path_out, outPath,
                       direntp -> d_name, DEFAULT);
          }
          /* update index if have another duplicated file*/
          in += 1;
        }
      }

    }
    else
      {
      /*recursively travel subdirectory if it is not soft link*/
      if ( !softLinkStatus (currPath) ) {
        encodeDir(currPath, path_out);
        reportFile(path_in, currPath, path_out, outPath,
                   direntp -> d_name, DIRECT);
        duplicate = true;
      }
      else
        {
        printf( "Soft link directry founded %s\n", direntp -> d_name );
        reportFile(path_in, currPath, path_out, outPath,
                   direntp -> d_name, SOFT);
      }
    }

    free(currPath);
    free(outPath);
    }

  while ((closedir(dirp) == -1) && (errno == EINTR)) ;
  return 0;
  }


/*used for decode file*/
size_t decode(FILE* infile, FILE* outfile) {
  uint8_t in[OUT_SIZE];
  uint8_t out[INP_SIZE];
  uint8_t v;
  int i, len;
  int character;
  while( feof( infile ) == 0 ) {
    for( len = 0, i = 0; i < OUT_SIZE; i++ ) {
      character = 0;
      while( feof( infile ) == 0 && character == 0 ) {
        v = getc( infile );
        character = is_valid_char(v);
      }
      if( feof( infile ) == 0 ) {
        len++;
        if( character != 0 ) {
          in[ i ] = (uint8_t) v;
        }
      }
      else {
        in[i] = 0;
      }
    }

    if( len > 0 ) {
      decode_block( in, out );
      /*change typo*/
      for( i = 0; i < INP_SIZE; i++ ) {
        if (out[i] > (uint8_t) 0) {
          if( fputc( (int) out[i], outfile ) == 0 ){
            if( ferror( outfile ) != 0 )      {
              perror("I/O error when reading");
            }

            break;
          }
        }

      }
    }
  }
  return 0;
}

/*recursivly traver directory specified by path_in and crete
  a directory and its name is path_out. In the process of
  travelling, it will call decode fucntion if the file is
  not soft link. In addition, it will skip duplicated hard
  link if we already decoded it.
 */
size_t decodeDir(const string path_in, const string path_out){
  struct dirent *direntp;
  DIR *dirp;
  string currPath, outPath;
  FILE* infile;
  FILE* outfile;

  /*create output directory first*/
  makeDir(path_in, path_out);

  /*open a directory*/
  if ((dirp = opendir(path_in)) == NULL) {
    perror ("Failed to open directory");
    return 1;
  }

  /*use while loop and readdir to read a directory*/
  while ((direntp = readdir(dirp)) != NULL) {

    /*skip parent directory and current directory*/
    if (!strcmp(direntp -> d_name, CURRENT_DIR) ||
        !strcmp(direntp -> d_name, PARENT_DIR)) {
      continue;
    }

    /*allocate memory for current path and output path*/
    currPath = CALLOC(strlen(path_in) +
                      strlen(direntp -> d_name) + 1, char);
    outPath = CALLOC(strlen(path_out) +
                     strlen(direntp -> d_name) + 1, char);
    /*make path of current file*/
    strcat(currPath, path_in);
    strcat(currPath, SLASH);
    strcat(currPath, direntp -> d_name);

    /*make path of output file*/
    strcat(outPath, path_out);
    strcat(outPath, SLASH);
    strcat(outPath, currPath);

    /*if it is file call fopen and start to decode*/
    /*if it is dir, we need update direntp and redo the process*/
    if (!isDirectory(currPath)) {
      /*call fopen for opening the file we need decode*/
      if (!(infile  = fopen(currPath, "r"))) {
        perror("File opening failed");
        return EXIT_FAILURE;
      }
      /*call fopen for creating the file we want to write the
        output*/
      if (!(outfile = fopen(outPath, "w"))) {
        perror("File opening failed");
        return EXIT_FAILURE;
      }

      /*start to decode file*/

      /*if the file is not soft link then decode the file*/
      if ( softLinkStatus(currPath) ) {
        reportFile(path_in, currPath, path_out, outPath, direntp -> d_name, SOFT);
        fprintf( stderr, "The %s is sym link\n", direntp -> d_name);
      }
      else
        {
        if ( hardLinkStatus(currPath) == 1 ) {
          decode( infile, outfile );
          fclose(outfile);
          fclose(infile);
          reportFile(path_in, currPath, path_out, outPath,
                     direntp -> d_name, DEFAULT);
          printf( "Start to decode %s\n", direntp -> d_name );
        } else {
          /*call getFileNumber for detecting the duplicated file*/
          ino_t result = getFileNumber(currPath);
          filenumber[in] = result;

          ino_t i;
          for (i = 0; i < in; i++) {
            if (result == filenumber[i]) {
              printf( "Duplicated file founded %s\n", direntp -> d_name );
              duplicate = true;
              reportFile( path_in, currPath, path_out, outPath,
                          direntp -> d_name, DUPLICATE );
              break;
            }
          }
          /*use information from for loop, if duplicated is false we decode the file*/
          if (!duplicate) {
            decode( infile, outfile );
            printf( "Start to decode %s\n", direntp -> d_name );
            fclose(outfile);
            fclose(infile);
            reportFile( path_in, currPath, path_out, outPath,
                        direntp -> d_name, DEFAULT );
          }
          /* update index if have another duplicated file*/
          in += 1;
        }
      }

    }else {
      /*recursively travel subdirectory if it is not soft link*/
      if ( !softLinkStatus (currPath) ) {
        decodeDir( currPath, path_out );
        reportFile( path_in, currPath, path_out, outPath,
                    direntp -> d_name, DIRECT );
        duplicate = true;
      }
      else
        {
        printf( "Soft link directry founded %s\n", direntp -> d_name );
        reportFile(path_in, currPath, path_out, outPath,
                   direntp -> d_name, SOFT);
      }
    }

    free(currPath);
    free(outPath);
    }

  while ((closedir(dirp) == -1) && (errno == EINTR)) ;
  return 0;
}


/*pass file name into this function and return the size of a file*/
off_t getFilesize(const string f_in) {
  struct stat st;
  if( stat(f_in, &st) != 0 ) {
    return 0;
  }
  return st.st_size;
}
/*get the file number of the file specified by path*/
ino_t getFileNumber(const string path) {
  struct stat st;
  if( stat(path, &st) != 0 ) {
    return 0;
  }
  return st.st_ino;
}

/*pass the path of a file or directory, if it is directry
  return nonzero. If it is file, then return 0*/
size_t isDirectory(const string path) {
  struct stat statbuf;
  if ( stat(path, &statbuf) == -1 )
    return 0;
  else
    return S_ISDIR(statbuf.st_mode);
}

void makeDir(const string path_in, const string path_out) {
  char path[MAX_BUFF];
  int errno;
  strcpy( path, path_out );
  if (path_in != NULL) {
    strcat( path, "/" );
    strcat( path, path_in );
  }
  /*ignore the file if it is existed*/
  if ( ( mkdir( path, PERM ) == -1 ) && errno != EEXIST ) {
    fprintf( stderr, "mkdir() failed: %s\n", strerror(errno) );
    exit(EXIT_FAILURE);
  }
}


size_t softLinkStatus( const string path ) {
  struct stat st;
  if(lstat(path, &st) == -1){
    return 0;
  }
  else
    {
    return S_ISLNK(st.st_mode);
  }
}


nlink_t hardLinkStatus(const string path) {
  struct stat st;
  if( stat(path, &st) == -1 ){
    return 0;
  }
  else{
    return st.st_nlink;
  }
}


size_t reportFile(const string path_in, const string file_in,
                  const string path_out, const string file_out,
                  const string filename, const char flag) {
  char new_path[MAX_BUFF];
  string* fixed_path;
  FILE* fp;
  off_t fileSize;
  makeargv( path_in, SLASH, &fixed_path );
  strcpy( new_path, path_out );
  strcat( new_path, SLASH );
  strcat( new_path, fixed_path[0] );
  strcat( new_path, TEMPREPORT );

  if ((fp = fopen(new_path, "a")) == NULL) {
    perror("Failed to create report file\n");
  }

  fwrite( filename, sizeof(char), strlen(filename), fp );
  fwrite( COMMA, sizeof(char), strlen(COMMA), fp );
  switch (flag) {
    /*if the input file is a directory*/
  case DIRECT: {
    fwrite(DIRECTORY, sizeof(char), strlen(DIRECTORY), fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fputc(ZERO, fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fputc(ZERO, fp);
    break;
  }
    /*if the input file is a soft link*/
  case SOFT: {
    fwrite(SOFT_LINK, sizeof(char), strlen(SOFT_LINK), fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fputc(ZERO, fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fputc(ZERO, fp);
    break;
  }
    /*if the file is a duplicate hard link*/
  case DUPLICATE: {
    fwrite(HARD_LINK, sizeof(char), strlen(HARD_LINK), fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fputc(ZERO, fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fputc(ZERO, fp);
    break;
  }
    /*normal mode*/
  default:
    fwrite(FILE_TYPE, sizeof(char), strlen(FILE_TYPE), fp);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    /*write filesize into report file*/
    fileSize = getFilesize(file_in);
    fprintf(fp, "%d", (int)fileSize);
    fwrite(COMMA, sizeof(char), strlen(COMMA), fp);
    fileSize = getFilesize(file_out);
    fprintf(fp, "%d", (int)fileSize);
    break;
  }

  if( ferror( fp ) != 0 )      {
    perror( "I/O error when writing:" );
  }

  fputc(NEWLINE, fp);
  fclose(fp);
  freemakeargv(fixed_path);
  return 0;
}

void sort(const string path_in, const string path_out) {
  pid_t childpid;
  string argument = MALLOC(MAX_BUFF, char);
  char newreport[MAX_BUFF];
  string* prog;
  int fd;
  strcpy(argument, SORT);
  strcat(argument, EMPTY);
  strcat(argument, path_out);
  strcpy(newreport, path_out);
  strcat(newreport, SLASH);
  strcat(argument, SLASH);
  strcat(newreport, path_in);
  strcat(argument, path_in);
  strcat(argument, TEMPREPORT);
  strcat(newreport, REPORT);

  if ((fd = open(newreport, FLAGS, PERM)) <= 0) {
    perror("Error on opening:");
    exit(0);
  }
  else
    {
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }

  makeargv(argument, EMPTY, &prog);
  childpid = fork();
  if ( childpid == 0) {
    execvp( prog[0], prog );
    perror( "failed to sort file:" );
  }

  /*block current process and wait for sort system call
    to be finished
   */
  wait(NULL);

  if (childpid == -1) {
    perror( "failed to sort file:" );
    exit(errno);
  }

  free(argument);
  freemakeargv(prog);
}

void delete(const string path_in, const string path_out) {
  pid_t childpid;
  string argument = MALLOC(MAX_BUFF, char);
  string* prog;
  strcpy(argument, REMOVE);
  strcat(argument, EMPTY);
  strcat(argument, path_out);
  strcat(argument, SLASH);
  strcat(argument, path_in);
  strcat(argument, TEMPREPORT);
  makeargv(argument, EMPTY, &prog);
  childpid = fork();
  if ( childpid == 0) {
    execvp( prog[0], prog );
    perror( "failed to delete file:" );
  }

  if (childpid == -1) {
    perror( "failed to delete file:" );
    exit(errno);
  }

  free(argument);
  freemakeargv(prog);
}
