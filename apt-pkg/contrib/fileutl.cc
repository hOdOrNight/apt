// -*- mode: cpp; mode: fold -*-
// Description								/*{{{*/
// $Id: fileutl.cc,v 1.1 1998/07/02 02:58:13 jgg Exp $
/* ######################################################################
   
   File Utilities
   
   CopyFile - Buffered copy of a single file
   GetLock - dpkg compatible lock file manipulation (fcntl)
   
   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.
   
   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <fileutl.h>
#include <pkglib/error.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
									/*}}}*/

// CopyFile - Buffered copy of a file					/*{{{*/
// ---------------------------------------------------------------------
/* The caller is expected to set things so that failure causes erasure */
bool CopyFile(File From,File To)
{
   if (From.IsOpen() == false || To.IsOpen() == false)
      return false;
   
   // Buffered copy between fds
   unsigned char *Buf = new unsigned char[64000];
   long Size;
   while ((Size = read(From.Fd(),Buf,64000)) > 0)
   {
      if (To.Write(Buf,Size) == false)
      {
	 delete [] Buf;
	 return false;
      }
   }

   delete [] Buf;
   return true;   
}
									/*}}}*/
// GetLock - Gets a lock file						/*{{{*/
// ---------------------------------------------------------------------
/* This will create an empty file of the given name and lock it. Once this
   is done all other calls to GetLock in any other process will fail with
   -1. The return result is the fd of the file, the call should call
   close at some time. */
int GetLock(string File,bool Errors)
{
   int FD = open(File.c_str(),O_RDWR | O_CREAT | O_TRUNC,0640);
   if (FD < 0)
   {
      if (Errors == true)
	 _error->Errno("open","Could not open lock file %s",File.c_str());
      return -1;
   }
   
   // Aquire a write lock
   struct flock fl;
   fl.l_type= F_WRLCK;
   fl.l_whence= SEEK_SET;
   fl.l_start= 0;
   fl.l_len= 1;
   if (fcntl(FD,F_SETLK,&fl) == -1)
   {
      if (Errors == true)
	 _error->Errno("open","Could not get lock %s",File.c_str());
      close(FD);
      return -1;
   }

   return FD;
}
									/*}}}*/
// FileExists - Check if a file exists					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool FileExists(string File)
{
   struct stat Buf;
   if (stat(File.c_str(),&Buf) != 0)
      return false;
   return true;
}
									/*}}}*/
// SafeGetCWD - This is a safer getcwd that returns a dynamic string	/*{{{*/
// ---------------------------------------------------------------------
/* We return / on failure. */
string SafeGetCWD()
{
   // Stash the current dir.
   char S[300];
   S[0] = 0;
   if (getcwd(S,sizeof(S)) == 0)
      return "/";
   return S;
}
									/*}}}*/

// File::File - Open a file						/*{{{*/
// ---------------------------------------------------------------------
/* The most commonly used open mode combinations are given with Mode */
File::File(string FileName,OpenMode Mode, unsigned long Perms)
{
   Flags = 0;
   switch (Mode)
   {
      case ReadOnly:
      iFd = open(FileName.c_str(),O_RDONLY);
      break;
      
      case WriteEmpty:
      unlink(FileName.c_str());
      iFd = open(FileName.c_str(),O_RDWR | O_CREAT | O_EXCL,Perms);
      break;
      
      case WriteExists:
      iFd = open(FileName.c_str(),O_RDWR);
      break;
   }  

   if (iFd < 0)
      _error->Errno("open","Could not open file %s",FileName.c_str());
   else
      this->FileName = FileName;
}
									/*}}}*/
// File::~File - Closes the file					/*{{{*/
// ---------------------------------------------------------------------
/* If the proper modes are selected then we close the Fd and possibly
   unlink the file on error. */
File::~File()
{
   Close();
}
									/*}}}*/
// File::Read - Read a bit of the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool File::Read(void *To,unsigned long Size)
{
   if (read(iFd,To,Size) != (signed)Size)
   {
      Flags |= Fail;
      return _error->Errno("read","Read error");
   }   
      
   return true;
}
									/*}}}*/
// File::Write - Write to the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool File::Write(void *From,unsigned long Size)
{
   if (write(iFd,From,Size) != (signed)Size)
   {
      Flags |= Fail;
      return _error->Errno("write","Write error");
   }
   
   return true;
}
									/*}}}*/
// File::Seek - Seek in the file					/*{{{*/
// ---------------------------------------------------------------------
/* */
bool File::Seek(unsigned long To)
{
   if (lseek(iFd,To,SEEK_SET) != (signed)To)
   {
      Flags |= Fail;
      return _error->Error("Unable to seek to %u",To);
   }
   
   return true;
}
									/*}}}*/
// File::Size - Return the size of the file				/*{{{*/
// ---------------------------------------------------------------------
/* */
unsigned long File::Size()
{
   struct stat Buf;
   if (fstat(iFd,&Buf) != 0)
      return _error->Errno("fstat","Unable to determine the file size");
   return Buf.st_size;
}
									/*}}}*/
// File::Close - Close the file	if the close flag is set		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool File::Close()
{
   bool Res = true;
   if ((Flags & AutoClose) == AutoClose)
      if (close(iFd) != 0)
	 Res &= _error->Errno("close","Problem closing the file");
      
   if ((Flags & Fail) == Fail && (Flags & DelOnFail) == DelOnFail &&
       FileName.empty() == false)
      if (unlink(FileName.c_str()) != 0)
	 Res &= _error->Warning("unlnk","Problem unlinking the file");
   return Res;
}
									/*}}}*/
