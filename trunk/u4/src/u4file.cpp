/*
 * $Id$
 */

#include <cctype>

#include "u4file.h"
#include "unzip.h"
#include "debug.h"

using std::string;
using std::vector;

/**
 * A specialization of U4FILE that uses C stdio internally.
 */
class U4FILE_stdio : public U4FILE {
public:
    static U4FILE *create(const string &fname);

    virtual void close();
    virtual int seek(long offset, int whence);
    virtual size_t read(void *ptr, size_t size, size_t nmemb);
    virtual int getc();
    virtual int putc(int c);
    virtual long length();

private:
    FILE *file;
};

/**
 * A specialization of U4FILE that reads files out of zip archives
 * automatically.
 */
class U4FILE_zip : public U4FILE {
public:
    static U4FILE *create(const string &fname, const string &zipfile, const string &zippath, int translate);

    virtual void close();
    virtual int seek(long offset, int whence);
    virtual size_t read(void *ptr, size_t size, size_t nmemb);
    virtual int getc();
    virtual int putc(int c);
    virtual long length();

private:
    unzFile zfile;
};

extern bool verbose;

/* these are for figuring out where to find files */
int u4zipExists = 0;
int u4upgradeZipExists = 0;
int u4upgradeExists = 0;
int u4upgradeInstalled = 0;

/* the possible paths where u4 for DOS can be installed */
static const char * const paths[] = {
    "./",
    "./ultima4/",
    "/usr/lib/u4/ultima4/",
    "/usr/local/lib/u4/ultima4/"
};

/* the possible paths where the u4 zipfiles can be installed */
static const char * const zip_paths[] = {
    "./",
    "/usr/lib/u4/",
    "/usr/local/lib/u4/"
};

/* the possible paths where the u4 music files can be installed */
static const char * const music_paths[] = {
    "./",
    "./ultima4/",
    "./mid/",
    "../mid/",
    "/usr/lib/u4/music/",
    "/usr/local/lib/u4/music/"
};

/* the possible paths where the u4 sound files can be installed */
static const char * const sound_paths[] = {
    "./",
    "./ultima4/",
    "./sound/",
    "../sound/",
    "/usr/lib/u4/sound/",
    "/usr/local/lib/u4/sound/"
};

/* the possible paths where the u4 config files can be installed */
static const char * const conf_paths[] = {
    "./",
    "./conf/",
    "../conf/",
    "/usr/lib/u4/",
    "/usr/local/lib/u4/"
};

/* the possible paths where the u4 graphics files can be installed */
static const char * const graphics_paths[] = {
    "./",
    "./ultima4/",
    "./graphics/",
    "../graphics/",
    "/usr/lib/u4/graphics/",
    "/usr/local/lib/u4/graphics/"
};


/**
 * Returns true if the upgrade is not only present, but is installed
 * (switch.bat or setup.bat has been run)
 */
int u4isUpgradeInstalled(void) {
    U4FILE *u4f = NULL;
    long int filelength;
    int result = 0;

    /* FIXME: Is there a better way to determine this? */
    u4f = u4fopen("ega.drv");
    if (u4f) {

        filelength = u4f->length();
        u4fclose(u4f);

        /* see if (ega.drv > 5k).  If so, the upgrade is installed */
        if (filelength > (5 * 1024))
            result = 1;
    }

    if (verbose)
        printf("u4isUpgradeInstalled %d\n", result);

    return result;
}

int U4FILE::getshort() {
    int byteLow = getc();
    return byteLow | (getc() << 8);
}

U4FILE *U4FILE_stdio::create(const string &fname) {
    U4FILE_stdio *u4f;
    FILE *f;

    f = fopen(fname.c_str(), "rb");
    if (!f)
        return NULL;

    u4f = new U4FILE_stdio;
    u4f->file = f;
    
    return u4f;
}

void U4FILE_stdio::close() {
    fclose(file);
}

int U4FILE_stdio::seek(long offset, int whence) {
    return fseek(file, offset, whence);
}

size_t U4FILE_stdio::read(void *ptr, size_t size, size_t nmemb) {
    return fread(ptr, size, nmemb, file);
}

int U4FILE_stdio::getc() {
    return fgetc(file);
}

int U4FILE_stdio::putc(int c) {
    return fputc(c, file);
}

long U4FILE_stdio::length() {
    long curr, len;

    curr = ftell(file);
    fseek(file, 0L, SEEK_END);
    len = ftell(file);
    fseek(file, curr, SEEK_SET);

    return len;
}

/**
 * Opens a file from within a zip archive.  fname is the filename
 * being searched for, zipfile is the name of the zip archive, zippath
 * is a path prefix prepended to each filename.  If translate is
 * non-zero, some special case translation is done to the filenames to
 * match up with the names in u4upgrad.zip.
 */
U4FILE *U4FILE_zip::create(const string &fname, const string &zipfile, const string &zippath, int translate) {
    U4FILE_zip *u4f;
    unzFile f;

    f = unzOpen(zipfile.c_str());
    if (!f)
        return NULL;

    string pathname(zippath);
    if (translate)
        pathname += u4upgrade_translate_filename(fname);
    else
        pathname += fname;

    if (unzLocateFile(f, pathname.c_str(), 2) == UNZ_END_OF_LIST_OF_FILE) {
        unzClose(f);
        return NULL;
    }
    unzOpenCurrentFile(f);

    u4f = new U4FILE_zip;
    u4f->zfile = f;

    return u4f;
}

void U4FILE_zip::close() {
    unzClose(zfile);
}

int U4FILE_zip::seek(long offset, int whence) {
    char *buf;
    long pos;

    ASSERT(whence != SEEK_END, "seeking with whence == SEEK_END not allowed with zipfiles");
    pos = unztell(zfile);
    if (whence == SEEK_CUR)
        offset = pos + offset;
    if (offset == pos)
        return 0;
    if (offset < pos) {
        unzCloseCurrentFile(zfile);
        unzOpenCurrentFile(zfile);
        pos = 0;
    }
    ASSERT(offset - pos > 0, "error in U4FILE_zip::seek");
    buf = new char[offset - pos];
    unzReadCurrentFile(zfile, buf, offset - pos);
    delete buf;
    return 0;
}

size_t U4FILE_zip::read(void *ptr, size_t size, size_t nmemb) {
    size_t retval = unzReadCurrentFile(zfile, ptr, size * nmemb);
    if (retval > 0)
        retval = retval / size;

    return retval;
}

int U4FILE_zip::getc() {
    int retval;
    unsigned char c;

    if (unzReadCurrentFile(zfile, &c, 1) > 0)
        retval = c;
    else
        retval = EOF;

    return retval;
}

int U4FILE_zip::putc(int c) {
    ASSERT(0, "zipfiles must be read-only!");
    return c;
}

long U4FILE_zip::length() {
    unz_file_info fileinfo;

    unzGetCurrentFileInfo(zfile, &fileinfo,
                          NULL, 0,
                          NULL, 0,
                          NULL, 0);
    return fileinfo.uncompressed_size;
}

/**
 * Open a data file from the Ultima 4 for DOS installation.  This
 * function checks the various places where it can be installed, and
 * maps the filenames to uppercase if necessary.  The files are always
 * opened for reading only.
 *
 * First, it looks in the zipfiles.  Next, it tries FILENAME, Filename
 * and filename in up to four paths, meaning up to twelve or more
 * opens per file.  Seems to be ok for performance, but could be
 * getting excessive.  The presence of the zipfiles should probably be
 * cached.
 */
U4FILE *u4fopen(const string &fname) {
    U4FILE *u4f = NULL;
    unsigned int i;

    if (verbose)
        printf("looking for %s\n", fname.c_str());

    /**
     * search for file within ultima4.zip or u4upgrad.zip
     */
    string pathname(u4find_path("ultima4.zip", zip_paths, sizeof(zip_paths) / sizeof(zip_paths[0])));
    if (!pathname.empty()) {
        /* original u4 zip is present */
        u4zipExists = 1;
        
        string upg_pathname(u4find_path("u4upgrad.zip", zip_paths, sizeof(zip_paths) / sizeof(zip_paths[0])));
        /* both zip files are present */
        if (!upg_pathname.empty())
            u4upgradeZipExists = 1;        

        /* look for the file in ultima4.zip */
        u4f = U4FILE_zip::create(fname, pathname, "ultima4/", 0);
        if (u4f)
            return u4f; /* file was found, return it! */

        /* look for the file in u4upgrad.zip */
        if (u4upgradeZipExists) {
            u4f = U4FILE_zip::create(fname, upg_pathname, "", 1);
            if (u4f)
                return u4f; /* file was found, return it! */ 
        }
    }

    /*
     * file not in a zipfile; check if it has been unzipped
     */
    string fname_copy(fname);

    pathname = u4find_path(fname_copy, paths, sizeof(paths) / sizeof(paths[0]));
    if (pathname.empty()) {
        using namespace std;
        if (islower(fname_copy[0])) {
            fname_copy[0] = toupper(fname_copy[0]);
            pathname = u4find_path(fname_copy, paths, sizeof(paths) / sizeof(paths[0]));
        }

        if (pathname.empty()) {
            for (i = 0; fname_copy[i] != '\0'; i++) {
                if (islower(fname_copy[i]))
                    fname_copy[i] = toupper(fname_copy[i]);
            }
            pathname = u4find_path(fname_copy, paths, sizeof(paths) / sizeof(paths[0]));
        }
    }

    if (!pathname.empty()) {
        u4f = U4FILE_stdio::create(pathname);
        if (verbose && u4f != NULL)
            printf("%s successfully opened\n", pathname.c_str());
    }

    return u4f;
}

/**
 * Opens a file with the standard C stdio facilities and wrap it in a
 * U4FILE struct.
 */
U4FILE *u4fopen_stdio(const string &fname) {
    return U4FILE_stdio::create(fname);
}

/**
 * Closes a data file from the Ultima 4 for DOS installation.
 */
void u4fclose(U4FILE *f) {
    f->close();
    delete f;
}

int u4fseek(U4FILE *f, long offset, int whence) {
    return f->seek(offset, whence);
}

size_t u4fread(void *ptr, size_t size, size_t nmemb, U4FILE *f) {
    return f->read(ptr, size, nmemb);
}

int u4fgetc(U4FILE *f) {
    return f->getc();
}

int u4fgetshort(U4FILE *f) {
    return f->getshort();
}

int u4fputc(int c, U4FILE *f) {
    return f->putc(c);
}

/**
 * Returns the length in bytes of a file.
 */
long u4flength(U4FILE *f) {
    return f->length();
}

/**
 * Read a series of zero terminated strings from a file.  The strings
 * are read from the given offset, or the current file position if
 * offset is -1.
 */
vector<string> u4read_stringtable(U4FILE *f, long offset, int nstrings) {
    string buffer;
    int i;
    vector<string> strs;

    ASSERT(offset < u4flength(f), "offset begins beyond end of file");

    if (offset != -1)
        f->seek(offset, SEEK_SET);
    for (i = 0; i < nstrings; i++) {
        char c;
        buffer.erase();

        while ((c = f->getc()) != '\0')
            buffer += c;
        
        strs.push_back(buffer);
    }

    return strs;
}

string u4find_path(const string &fname, const char * const *pathent, unsigned int npathents) {
    FILE *f = NULL;
    unsigned int i;
    char pathname[128];

    for (i = 0; i < npathents; i++) {
        snprintf(pathname, sizeof(pathname), "%s%s", pathent[i], fname.c_str());

        if (verbose)
            printf("trying to open %s\n", pathname);

        if ((f = fopen(pathname, "rb")) != NULL)
            break;
    }

    if (verbose) {
        if (f != NULL)
            printf("%s successfully found\n", pathname);
        else 
            printf("%s not found\n", fname.c_str());
    }

    if (f) {
        fclose(f);
        return pathname;
    } else
        return "";
}

string u4find_music(const string &fname) {
    return u4find_path(fname, music_paths, sizeof(music_paths) / sizeof(music_paths[0]));
}

string u4find_sound(const string &fname) {
    return u4find_path(fname, sound_paths, sizeof(sound_paths) / sizeof(sound_paths[0]));
}

string u4find_conf(const string &fname) {
    return u4find_path(fname, conf_paths, sizeof(conf_paths) / sizeof(conf_paths[0]));
}

string u4find_graphics(const string &fname) {
    return u4find_path(fname, graphics_paths, sizeof(graphics_paths) / sizeof(graphics_paths[0]));
}

string u4upgrade_translate_filename(const string &fname) {
    unsigned int i;
    const static struct {
        char *name;
        char *translation;
    } translations[] = {
        { "compassn.ega", "compassn.old" },
        { "courage.ega", "courage.old" },
        { "cove.tlk", "cove.old" },
        { "ega.drv", "ega.old" }, /* not actually used */
        { "honesty.ega", "honesty.old" },
        { "honor.ega", "honor.old" },
        { "humility.ega", "humility.old" },
        { "key7.ega", "key7.old" },
        { "lcb.tlk", "lcb.old" },
        { "love.ega", "love.old" },
        { "love.ega", "love.old" },
        { "minoc.tlk", "minoc.old" },
        { "rune_0.ega", "rune_0.old" },
        { "rune_1.ega", "rune_1.old" },
        { "rune_2.ega", "rune_2.old" },
        { "rune_3.ega", "rune_3.old" },
        { "rune_4.ega", "rune_4.old" },
        { "rune_5.ega", "rune_5.old" },
        { "sacrific.ega", "sacrific.old" },
        { "skara.tlk", "skara.old" },
        { "spirit.ega", "spirit.old" },
        { "start.ega", "start.old" },
        { "stoncrcl.ega", "stoncrcl.old" },
        { "truth.ega", "truth.old" },
        { "ultima.com", "ultima.old" }, /* not actually used */
        { "valor.ega", "valor.old" },
        { "yew.tlk", "yew.old" }
    };

    for (i = 0; i < sizeof(translations) / sizeof(translations[0]); i++) {
        if (strcasecmp(translations[i].name, fname.c_str()) == 0)
            return translations[i].translation;
    }
    return fname;
}
