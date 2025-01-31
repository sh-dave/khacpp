#include <hx/CFFI.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef EPPC
#include <sys/types.h>
#include <sys/stat.h>
#endif

#pragma warning(disable : 4996)

#ifdef __clang__
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif

int __sys_prims() { return 0; }

#if defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
#	include <windows.h>
#	include <direct.h>
#	include <conio.h>
#	include <locale.h>
#	include "Shlwapi.h"
#else
#	include <errno.h>
#if !defined(EPPC) && !defined(KORE_CONSOLE)
#	include <unistd.h>
#	include <dirent.h>
#	include <termios.h>
#	include <sys/time.h>
#	include <sys/times.h>
#endif
#	include <limits.h>
#ifndef ANDROID
#	include <locale.h>
#if !defined(BLACKBERRY) && !defined(EPPC) && !defined(GCW0) && !defined(__GLIBC__) && !defined(KORE_CONSOLE)
#	include <xlocale.h>
#endif
#endif
#endif

#ifdef EMSCRIPTEN
#include <sys/wait.h>
#endif

#if !defined(IPHONE) && !defined(APPLETV)
#ifdef NEKO_MAC
#	include <sys/syslimits.h>
#	include <limits.h>
#	include <mach-o/dyld.h>
#endif
#endif

#if defined(HX_WINRT) && !defined(_XBOX_ONE)
#include <string>
#endif

#ifndef CLK_TCK
#	define CLK_TCK	100
#endif

/**
	<doc>
	<h1>System</h1>
	<p>
	Interactions with the operating system.
	</p>
	</doc>
**/

/**
	get_env : string -> string?
	<doc>Get some environment variable if exists</doc>
**/
static value get_env( value v ) {
	char *s = 0;
	val_check(v,string);
   #if !defined(HX_WINRT) && !defined(KORE_CONSOLE)
	s = getenv(val_string(v));
   #endif
	if( s == NULL )
		return val_null;
	return alloc_string(s);
}

/**
	put_env : var:string -> val:string -> void
	<doc>Set some environment variable value</doc>
**/
static value put_env( value e, value v ) {
#if defined(HX_WINRT) || defined(KORE_CONSOLE)
   // Do nothing
	return alloc_null();
#elif defined(NEKO_WINDOWS)
	val_check(e,string);
	val_check(v,string);
	buffer b = alloc_buffer(0);
	val_buffer(b,e);
	buffer_append_sub(b,"=",1);
	val_buffer(b,v);
	if( putenv(buffer_data(b)) != 0 )
		return alloc_null();
#else
	val_check(e,string);
	val_check(v,string);
	if( setenv(val_string(e),val_string(v),1) != 0 )
		return alloc_null();
#endif
	return alloc_bool(true);
}

/**
	sys_sleep : number -> void
	<doc>Sleep a given number of seconds</doc>
**/

static value sys_sleep( value f ) {
	val_check(f,number);
	gc_enter_blocking();
#if defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
	Sleep((DWORD)(val_number(f) * 1000));
#elif defined(EPPC) || defined(KORE_CONSOLE)
//TODO: Implement sys_sleep for EPPC
#else
	{
		struct timespec t;
		struct timespec tmp;
		t.tv_sec = (int)val_number(f);
		t.tv_nsec = (int)((val_number(f) - t.tv_sec) * 1e9);
		while( nanosleep(&t,&tmp) == -1 ) {
			if( errno != EINTR ) {
				gc_exit_blocking();
				return alloc_null();
         }
			t = tmp;
		}
	}
#endif
	gc_exit_blocking();
	return alloc_bool(true);
}

/**
	set_time_locale : string -> bool
	<doc>Set the locale for LC_TIME, returns true on success</doc>
**/
static value set_time_locale( value l ) {
#if defined(ANDROID) || defined(GCW0) || defined(KORE_CONSOLE)
        return alloc_null();
#else

#ifdef NEKO_POSIX
	locale_t lc, old;
	val_check(l,string);
	lc = newlocale(LC_TIME_MASK,val_string(l),NULL);
	if( lc == NULL ) return alloc_bool(false);
	old = uselocale(lc);
	if( old == NULL ) {
		freelocale(lc);
		return alloc_bool(false);
	}
	if( old != LC_GLOBAL_LOCALE )
		freelocale(old);
	return alloc_bool(true);
#else
	val_check(l,string);
	return alloc_bool(setlocale(LC_TIME,val_string(l)) != NULL);
#endif

#endif // !Android
}

/**
	get_cwd : void -> string
	<doc>Return current working directory</doc>
**/
static value get_cwd() {
   #ifdef HX_WINRT
   return alloc_string("ms-appdata:///local/");
   #elif defined(EPPC) || defined(KORE_CONSOLE)
   return alloc_null();
   #else
	#ifdef NEKO_WINDOWS
	wchar_t buf[256];
	int l;
	if( GetCurrentDirectoryW(256,buf) == NULL )
		return alloc_null();
	l = (int)wcslen(buf);
	if( buf[l-1] != '/' && buf[l-1] != '\\' ) {
		buf[l] = '/';
		buf[l+1] = 0;
	}
	return alloc_wstring(buf);
	#else
	char buf[256];
	int l;
	if( getcwd(buf,256) == NULL )
		return alloc_null();
	l = (int)strlen(buf);
	if( buf[l-1] != '/' && buf[l-1] != '\\' ) {
		buf[l] = '/';
		buf[l+1] = 0;
	}
	return alloc_string(buf);
	#endif
   #endif
}

/**
	set_cwd : string -> void
	<doc>Set current working directory</doc>
**/
static value set_cwd( value d ) {
   #if !defined(HX_WINRT) && !defined(EPPC) && !defined(KORE_CONSOLE)
	val_check(d,string);
	#ifdef NEKO_WINDOWS
	if( SetCurrentDirectoryW(val_wstring(d)) )
		return alloc_null();
	#else
	if( chdir(val_string(d)) )
		return alloc_null();
	#endif
   #endif
	return alloc_bool(true);
}


/**
	sys_string : void -> string
	<doc>
	Return the local system string. The current value are possible :
	<ul>
	<li>[Windows]</li>
	<li>[Linux]</li>
	<li>[BSD]</li>
	<li>[Mac]</li>
	</ul>
	</doc>
**/
static value sys_string() {
#if defined(KORE_CONSOLE)
	return alloc_string("Kore Console");
#elif defined(HX_WINRT)
	return alloc_string("WinRT");
#elif defined(NEKO_WINDOWS)
	return alloc_string("Windows");
#elif defined(NEKO_GNUKBSD)
	return alloc_string("GNU/kFreeBSD");
#elif defined(NEKO_LINUX)
	return alloc_string("Linux");
#elif defined(NEKO_BSD)
	return alloc_string("BSD");
#elif defined(NEKO_MAC)
	return alloc_string("Mac");
#elif defined(ANDROID)
	return alloc_string("Android");
#elif defined(BLACKBERRY)
	return alloc_string("BlackBerry");
#elif defined(EMSCRIPTEN)
	return alloc_string("Emscripten");
#elif defined(EPPC)
	return alloc_string("EPPC");
#else
#error Unknow system string
#endif
}

/**
	sys_is64 : void -> bool
	<doc>
	Returns true if we are on a 64-bit system
	</doc>
**/
static value sys_is64() {
#ifdef NEKO_64BITS
	return alloc_bool(true);
#else
	return alloc_bool(false);
#endif
}

/**
	sys_command : string -> int
	<doc>Run the shell command and return exit code</doc>
**/
static value sys_command( value cmd ) {
   #if defined(HX_WINRT) || defined(EMSCRIPTEN) || defined(EPPC) || defined(IPHONE) || defined(APPLETV) || defined(HX_APPLEWATCH) || defined(KORE_CONSOLE)
	return alloc_int( -1 );
   #else
	val_check(cmd,string);
	if( val_strlen(cmd) == 0 )
		return alloc_int(-1);
	#ifdef NEKO_WINDOWS
	const wchar_t* _cmd = val_wstring(cmd);
	gc_enter_blocking();
	int result =  _wsystem(_cmd);
	#else
	gc_enter_blocking();
	int result =  system(val_string(cmd));
	#endif
	gc_exit_blocking();
   #if !defined(NEKO_WINDOWS) && !defined(ANDROID)
   result = WEXITSTATUS(result) | (WTERMSIG(result) << 8);
   #endif
	return alloc_int( result );
   #endif
}


/**
	sys_exit : int -> void
	<doc>Exit with the given errorcode. Never returns.</doc>
**/
static value sys_exit( value ecode ) {
	val_check(ecode,int);
	exit(val_int(ecode));
	return alloc_bool(true);
}

/**
	sys_exists : string -> bool
	<doc>Returns true if the file or directory exists.</doc>
**/
static value sys_exists( value path ) {
	#if defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_bool(true);
	#else
	val_check(path,string);
	#ifdef NEKO_WINDOWS
	const wchar_t* _path = val_wstring(path);
	gc_enter_blocking();
	bool result =  GetFileAttributesW(_path) != INVALID_FILE_ATTRIBUTES;
	#else
	struct stat st;
	gc_enter_blocking();
	bool result =  stat(val_string(path),&st) == 0;
	#endif
	gc_exit_blocking();
	return alloc_bool(result);
	#endif
}

/**
	file_exists : string -> bool
	<doc>Deprecated : use sys_exists instead.</doc>
**/
static value file_exists( value path ) {
	gc_enter_blocking();
	bool result =  sys_exists(path);
	gc_exit_blocking();
	return alloc_bool(result);
}

/**
	file_delete : string -> void
	<doc>Delete the file. Exception on error.</doc>
**/
static value file_delete( value path ) {
	#if defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_bool(true);
	#else
	val_check(path,string);
	#ifdef NEKO_WINDOWS
	const wchar_t* _path = val_wstring(path);
	gc_enter_blocking();
	if( DeleteFileW(_path) != 0 )
	#else
	gc_enter_blocking();
	if( unlink(val_string(path)) != 0 )
	#endif
	{
		gc_exit_blocking();
		return alloc_null();
	}
	gc_exit_blocking();
	return alloc_bool(true);
	#endif
}

/**
	sys_rename : from:string -> to:string -> void
	<doc>Rename the file or directory. Exception on error.</doc>
**/
static value sys_rename( value path, value newname ) {
	val_check(path,string);
	val_check(newname,string);
	
	#if defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
	const wchar_t* _path = val_wstring(path);
	const wchar_t* _newname = val_wstring(newname);
	gc_enter_blocking();
	if( MoveFileExW(_path,_newname,MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH) != 0 )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	#elif (!defined(KORE_CONSOLE))
	gc_enter_blocking();
	if( rename(val_string(path),val_string(newname)) != 0 )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	#endif
	gc_exit_blocking();
	return alloc_bool(true);
}

#define STATF(f) alloc_field(o,val_id(#f),alloc_int(s.st_##f))
#define STATF32(f) alloc_field(o,val_id(#f),alloc_int32(s.st_##f))

/**
	sys_stat : string -> {
		gid => int,
		uid => int,
		atime => 'int32,
		mtime => 'int32,
		ctime => 'int32,
		dev => int,
		ino => int,
		nlink => int,
		rdev => int,
		mode => int,
		size => int
	}
	<doc>Run the [stat] command on the given file or directory.</doc>
**/
static value sys_stat( value path ) {
	#if defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_null();
	#else
	value o;
	val_check(path,string);
	
	#if defined(NEKO_WINDOWS) && !defined(KORE_WINDOWSAPP) && !defined(KORE_XBOX_ONE)
	const wchar_t* _path = val_wstring(path);
	gc_enter_blocking();
	WIN32_FILE_ATTRIBUTE_DATA data;
	if( !GetFileAttributesExW(_path,GetFileExInfoStandard,&data) )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	gc_exit_blocking();
	wchar_t fullPath[MAX_PATH+1];
	GetFullPathNameW(_path,MAX_PATH+1,fullPath,NULL);
	int dev = PathGetDriveNumberW(fullPath);
	#define EPOCH_DIFF	(134774*24*60*60.0)
	ULARGE_INTEGER ui;
	o = alloc_empty_object( );
	alloc_field(o,val_id("gid"),alloc_int(0));
	alloc_field(o,val_id("uid"),alloc_int(0));
	ui.LowPart = data.ftLastAccessTime.dwLowDateTime;
	ui.HighPart = data.ftLastAccessTime.dwHighDateTime;
	alloc_field(o,val_id("atime"),alloc_int32((int)(((double)ui.QuadPart) / 10000000.0 - EPOCH_DIFF)));
	ui.LowPart = data.ftLastWriteTime.dwLowDateTime;
	ui.HighPart = data.ftLastWriteTime.dwHighDateTime;
	alloc_field(o,val_id("mtime"),alloc_int32((int)(((double)ui.QuadPart) / 10000000.0 - EPOCH_DIFF)));
	ui.LowPart = data.ftCreationTime.dwLowDateTime;
	ui.HighPart = data.ftCreationTime.dwHighDateTime;
	alloc_field(o,val_id("ctime"),alloc_int32((int)(((double)ui.QuadPart) / 10000000.0 - EPOCH_DIFF)));
	alloc_field(o,val_id("dev"),alloc_int(dev));
	alloc_field(o,val_id("ino"),alloc_int(0));
	int mode = 0;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) mode |= _S_IFDIR;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) == 0) mode |= _S_IFREG;
	mode |= _S_IREAD;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == 0) mode |= _S_IWRITE;
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) mode |= _S_IEXEC;
	alloc_field(o,val_id("mode"),alloc_int(mode));
	alloc_field(o,val_id("nlink"),alloc_int(1));
	alloc_field(o,val_id("rdev"),alloc_int(dev));
	alloc_field(o,val_id("size"),alloc_int32(data.nFileSizeLow));
	#else
	gc_enter_blocking();
	struct stat s;
	if( stat(val_string(path),&s) != 0 )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	gc_exit_blocking();
	o = alloc_empty_object( );
	STATF(gid);
	STATF(uid);
	STATF32(atime);
	STATF32(mtime);
	STATF32(ctime);
	STATF(dev);
	STATF(ino);
	STATF(mode);
	STATF(nlink);
	STATF(rdev);
	STATF(size);
	#endif
	return o;
	#endif
}

/**
	sys_file_type : string -> string
	<doc>
	Return the type of the file. The current values are possible :
	<ul>
	<li>[file]</li>
	<li>[dir]</li>
	<li>[symlink]</li>
	<li>[sock]</li>
	<li>[char]</li>
	<li>[block]</li>
	<li>[fifo]</li>
	</ul>
	</doc>
**/
static value sys_file_type( value path ) {
	#if defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_null();
	#else
	val_check(path,string);
	#ifdef NEKO_WINDOWS
	const wchar_t* _path = val_wstring(path);
	gc_enter_blocking();
	WIN32_FILE_ATTRIBUTE_DATA data;
	if( !GetFileAttributesExW(_path,GetFileExInfoStandard,&data) )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	gc_exit_blocking();
	if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		return alloc_string("dir");
	}
	else
	{
		return alloc_string("file");
	}
	#else
	struct stat s;
	gc_enter_blocking();
	if( stat(val_string(path),&s) != 0 )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	gc_exit_blocking();
	if( s.st_mode & S_IFREG )
		return alloc_string("file");
	if( s.st_mode & S_IFDIR )
		return alloc_string("dir");
	if( s.st_mode & S_IFCHR )
		return alloc_string("char");
#ifndef NEKO_WINDOWS
	if( s.st_mode & S_IFLNK )
		return alloc_string("symlink");
	if( s.st_mode & S_IFBLK )
		return alloc_string("block");
	if( s.st_mode & S_IFIFO )
		return alloc_string("fifo");
	if( s.st_mode & S_IFSOCK )
		return alloc_string("sock");
#endif
	#endif
	return alloc_null();
	#endif
}

/**
	sys_create_dir : string -> mode:int -> void
	<doc>Create a directory with the specified rights</doc>
**/
static value sys_create_dir( value path, value mode ) {
	#if defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_bool(true);
	#else
	val_check(path,string);
	val_check(mode,int);
	#ifdef NEKO_WINDOWS
	const wchar_t* _path = val_wstring(path);
	gc_enter_blocking();
	if( _wmkdir(_path) != 0 )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	#else
	const char* _path = val_string(path);
	gc_enter_blocking();
	if( mkdir(val_string(path),val_int(mode)) != 0 )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	#endif
	gc_exit_blocking();
	return alloc_bool(true);
	#endif
}

/**
	sys_remove_dir : string -> void
	<doc>Remove a directory. Exception on error</doc>
**/
static value sys_remove_dir( value path ) {
	#if defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_bool(true);
	#else
	val_check(path,string);
	#ifdef NEKO_WINDOWS
	const wchar_t* _path = val_wstring(path);
	gc_enter_blocking();
	bool ok = _wrmdir(_path) != 0;
	#else
	const char* _path = val_string(path);
	gc_enter_blocking();
	bool ok = rmdir(_path) != 0;
	#endif
	gc_exit_blocking();
	return alloc_bool(ok);
	#endif
}

/**
	sys_time : void -> float
	<doc>Return an accurate local time stamp in seconds since Jan 1 1970</doc>
**/
static value sys_time() {
#if defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
#define EPOCH_DIFF	(134774*24*60*60.0)
	SYSTEMTIME t;
	FILETIME ft;
    ULARGE_INTEGER ui;
	GetSystemTime(&t);
	if( !SystemTimeToFileTime(&t,&ft) )
		return alloc_null();
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
	return alloc_float( ((double)ui.QuadPart) / 10000000.0 - EPOCH_DIFF );
#elif defined(EPPC) || defined(KORE_CONSOLE)
	time_t tod;
	time(&tod);
	return alloc_float ((double)tod);
#else
	struct timeval tv;
	if( gettimeofday(&tv,NULL) != 0 )
		return alloc_null();
	return alloc_float( tv.tv_sec + ((double)tv.tv_usec) / 1000000.0 );
#endif
}

/**
	sys_cpu_time : void -> float
	<doc>Return the most accurate CPU time spent since the process started (in seconds)</doc>
**/
static value sys_cpu_time() {
#if defined(HX_WINRT) && !defined(_XBOX_ONE)
    return alloc_float ((double)GetTickCount64()/1000.0);
#elif defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
	FILETIME unused;
	FILETIME stime;
	FILETIME utime;
	if( !GetProcessTimes(GetCurrentProcess(),&unused,&unused,&stime,&utime) )
		return alloc_null();
	return alloc_float( ((double)(utime.dwHighDateTime+stime.dwHighDateTime)) * 65.536 * 6.5536 + (((double)utime.dwLowDateTime + (double)stime.dwLowDateTime) / 10000000) );
#elif defined(EPPC) || defined(KORE_CONSOLE)
    return alloc_float ((double)clock()/(double)CLOCKS_PER_SEC);
#else
	struct tms t;
	times(&t);
	return alloc_float( ((double)(t.tms_utime + t.tms_stime)) / CLK_TCK );
#endif
}

/**
	sys_read_dir : string -> string list
	<doc>Return the content of a directory</doc>
**/
static value sys_read_dir( value p) {
	val_check(p,string);

        value result = alloc_array(0);
#if defined(HX_WINRT) && defined(__cplusplus_winrt)
   auto folder = (Windows::Storage::StorageFolder::GetFolderFromPathAsync( ref new Platform::String(val_wstring(p)) ))->GetResults();
   auto results = folder->GetFilesAsync(Windows::Storage::Search::CommonFileQuery::DefaultQuery)->GetResults();
   for(int i=0;i<results->Size;i++)
      val_array_push(result,alloc_wstring(results->GetAt(i)->Path->Data()));
#elif defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
	const wchar_t *path = val_wstring(p);
	size_t len = wcslen(path);
   if (len>MAX_PATH)
      return alloc_null();

	WIN32_FIND_DATAW d;
	HANDLE handle;
  #if defined(HX_WINRT) && !defined(_XBOX_ONE)
	std::wstring tempWStr(path);
	std::string searchPath(tempWStr.begin(), tempWStr.end());
  #else
   wchar_t searchPath[ MAX_PATH + 4 ];
   memcpy(searchPath,path, len*sizeof(wchar_t));
  #endif


	if( len && path[len-1] != '/' && path[len-1] != '\\' )
      searchPath[len++] = '/';
   searchPath[len++] = '*';
   searchPath[len++] = '.';
   searchPath[len++] = '*';
   searchPath[len] = '\0';

	gc_enter_blocking();
  #if defined(HX_WINRT) && !defined(_XBOX_ONE)
	handle = FindFirstFileEx(searchPath.c_str(), FindExInfoStandard, &d, FindExSearchNameMatch, NULL, 0);
  #else
	handle = FindFirstFileW(searchPath,&d);
  #endif
	if( handle == INVALID_HANDLE_VALUE )
	{
		gc_exit_blocking();
		return alloc_null();
	}
	while( true ) {
		// skip magic dirs
		if( d.cFileName[0] != '.' || (d.cFileName[1] != 0 && (d.cFileName[1] != '.' || d.cFileName[2] != 0)) ) {
		   gc_exit_blocking();
			val_array_push(result,alloc_wstring(d.cFileName));
			gc_enter_blocking();
		}
		if( !FindNextFileW(handle,&d) )
			break;
	}
	FindClose(handle);
#elif !defined(EPPC) && !defined(KORE_CONSOLE)
	DIR *d;
	struct dirent *e;
   const char *name = val_string(p);
	gc_enter_blocking();
	d = opendir(name);
	if( d == NULL )
	{
		gc_exit_blocking();
      val_throw(alloc_string("Invalid directory"));
	}
	while( true ) {
		e = readdir(d);
		if( e == NULL )
			break;
		// skip magic dirs
		if( e->d_name[0] == '.' && (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0)) )
			continue;
		gc_exit_blocking();
		val_array_push(result, alloc_string(e->d_name) );
		gc_enter_blocking();
	}
	closedir(d);
#endif
	gc_exit_blocking();
	return result;
}

/**
	file_full_path : string -> string
	<doc>Return an absolute path from a relative one. The file or directory must exists</doc>
**/
static value file_full_path( value path ) {
#if defined(HX_WINRT) || defined(KORE_CONSOLE)
	return path;
#elif defined(NEKO_WINDOWS)
	wchar_t buf[MAX_PATH+1];
	val_check(path,string);
	if( GetFullPathNameW(val_wstring(path),MAX_PATH+1,buf,NULL) == 0 )
		return alloc_null();
	return alloc_wstring(buf);
#elif defined(EPPC)
	return path;
#else
	char buf[PATH_MAX];
	val_check(path,string);
	if( realpath(val_string(path),buf) == NULL )
		return alloc_null();
	return alloc_string(buf);
#endif
}

/**
	sys_exe_path : void -> string
	<doc>Return the path of the executable</doc>
**/
static value sys_exe_path() {
#if defined(HX_WINRT) && defined(__cplusplus_winrt)
   Windows::ApplicationModel::Package^ package = Windows::ApplicationModel::Package::Current;
   Windows::Storage::StorageFolder^ installedLocation = package->InstalledLocation;
   return(alloc_wstring(installedLocation->Path->Data()));
#elif defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
	wchar_t path[MAX_PATH+1];
	if( GetModuleFileNameW(NULL,path,MAX_PATH+1) == 0 )
		return alloc_null();
	return alloc_wstring(path);
#elif defined(NEKO_MAC) && !defined(IPHONE) && !defined(APPLETV)
	char path[PATH_MAX+1];
	uint32_t path_len = PATH_MAX;
	if( _NSGetExecutablePath(path, &path_len) )
		return alloc_null();
	return alloc_string(path);
#elif defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_string("");
#else
	const char *p = getenv("_");
	if( p != NULL )
		return alloc_string(p);
	{
		char path[PATH_MAX];
		int length = readlink("/proc/self/exe", path, sizeof(path));
		if( length < 0 )
			return alloc_null();
	    path[length] = '\0';
		return alloc_string(path);
	}
#endif
}

#if !defined(IPHONE) && !defined(APPLETV)
#ifdef NEKO_MAC
#include <crt_externs.h>
#	define environ (*_NSGetEnviron())
#endif
#endif

#ifndef NEKO_WINDOWS
extern char **environ;
#endif

/**
	sys_env : void -> #list
	<doc>Return all the (key,value) pairs in the environment as a chained list</doc>
**/
static value sys_env() {
   value result = alloc_array(0);
   #if !defined(HX_WINRT) && !defined(KORE_CONSOLE)
	char **e = environ;
	while( *e ) {
		char *x = strchr(*e,'=');
		if( x == NULL ) {
			e++;
			continue;
		}
                val_array_push(result,alloc_string_len(*e,(int)(x-*e)));
                val_array_push(result,alloc_string(x+1));
		e++;
	}
   #endif
	return result;
}

#ifdef HX_ANDROID
   #define tcsetattr(fd,opt,s) ioctl(fd,opt,s)
   #define tcgetattr(fd,s) ioctl(fd,TCGETS,s)

   static __inline__ void inline_cfmakeraw(struct termios *s)
   {
       s->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
       s->c_oflag &= ~OPOST;
       s->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
       s->c_cflag &= ~(CSIZE|PARENB);
       s->c_cflag |= CS8;
   }

   #define cfmakeraw inline_cfmakeraw

#endif

/**
	sys_getch : bool -> int
	<doc>Read a character from stdin with or without echo</doc>
**/
static value sys_getch( value b ) {
#if defined(HX_WINRT) || defined(EMSCRIPTEN) || defined(EPPC) || defined(KORE_CONSOLE)
   return alloc_null();
#elif defined(NEKO_WINDOWS)
	val_check(b,bool);
	gc_enter_blocking();
	int result = val_bool(b)?getche():getch();
	gc_exit_blocking();
	return alloc_int( result );
#else
	// took some time to figure out how to do that
	// without relying on ncurses, which clear the
	// terminal on initscr()
	int c;
	struct termios term, old;
	val_check(b,bool);
	gc_enter_blocking();
	tcgetattr(fileno(stdin), &old);
	term = old;
	cfmakeraw(&term);
	tcsetattr(fileno(stdin), 0, &term);
	c = getchar();
	tcsetattr(fileno(stdin), 0, &old);
	if( val_bool(b) ) fputc(c,stdout);
	gc_exit_blocking();
	return alloc_int(c);
#	endif
}

/**
	sys_get_pid : void -> int
	<doc>Returns the current process identifier</doc>
**/
static value sys_get_pid() {
#if defined(NEKO_WINDOWS) && !defined(KORE_CONSOLE)
	return alloc_int(GetCurrentProcessId());
#elif defined(EPPC) || defined(KORE_CONSOLE)
	return alloc_int(1);
#	else
	return alloc_int(getpid());
#	endif
}

DEFINE_PRIM(get_env,1);
DEFINE_PRIM(put_env,2);
DEFINE_PRIM(set_time_locale,1);
DEFINE_PRIM(get_cwd,0);
DEFINE_PRIM(set_cwd,1);
DEFINE_PRIM(sys_sleep,1);
DEFINE_PRIM(sys_command,1);
DEFINE_PRIM(sys_exit,1);
DEFINE_PRIM(sys_string,0);
DEFINE_PRIM(sys_is64,0);
DEFINE_PRIM(sys_stat,1);
DEFINE_PRIM(sys_time,0);
DEFINE_PRIM(sys_cpu_time,0);
DEFINE_PRIM(sys_env,0);
DEFINE_PRIM(sys_create_dir,2);
DEFINE_PRIM(sys_remove_dir,1);
DEFINE_PRIM(sys_read_dir,1);
DEFINE_PRIM(file_full_path,1);
DEFINE_PRIM(file_exists,1);
DEFINE_PRIM(sys_exists,1);
DEFINE_PRIM(file_delete,1);
DEFINE_PRIM(sys_rename,2);
DEFINE_PRIM(sys_exe_path,0);
DEFINE_PRIM(sys_file_type,1);
DEFINE_PRIM(sys_getch,1);
DEFINE_PRIM(sys_get_pid,0);

/* ************************************************************************ */
