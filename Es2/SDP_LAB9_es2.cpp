/**
* Name:    Francesco
* Surname: Longo
* ID:      223428
* Lab:     9
* Ex:      2
*
* Copy a directory tree and modify each source file while copying it.
*
* A C program is run with two parameters
* name1 name2
* where name1 and name2 are C strings which indicates relative
* or absolute paths of two directory trees.
*
* The program must copy the directory name1 into an isomorphic
* directory name2.
* For each file copied from the first to the second directory tree, the
* program has to add two data fields on top of the destination file
* specifying:
* - the first one, the name of the file (C string)
* - the second one, the size of the source (original) file
*   (a 32-bit or 64-bit integer value at choice).
*
**/

#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif // !UNICODE

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif // !_CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <assert.h>

// file types
#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3

// prototypes
VOID addDataFieldsToFile(LPTSTR path1, LPTSTR path2);
VOID copyDirectoryAndDo(LPTSTR path1, LPTSTR path2, DWORD level, VOID(*whatToDo)(LPTSTR, LPTSTR));
static DWORD FileType(LPWIN32_FIND_DATA pFileData);
VOID cleanPath(LPTSTR);
int Return(int seconds, int value);
LPWSTR getErrorMessageAsString(DWORD errorCode);

// main
INT _tmain(INT argc, LPTSTR argv[]) {
	if (argc != 3) {
		_tprintf(_T("Usage: %s name1 name2\n"), argv[0]);
		return 1;
	}

	cleanPath(argv[1]);
	cleanPath(argv[2]);

	// start recursive visit (both argv[1] and argv[2] can be relative or absolute
	copyDirectoryAndDo(argv[1], argv[2], 0, addDataFieldsToFile);

	Return(10, 0);
}

VOID copyDirectoryAndDo(LPTSTR path1, LPTSTR path2, DWORD level, VOID(*whatToDo)(LPTSTR, LPTSTR)) {
	WIN32_FIND_DATA findFileData;
	HANDLE hFind;
	TCHAR searchPath[MAX_PATH];
	TCHAR newPath1[MAX_PATH], newPath2[MAX_PATH];

	// build the searchPath string, to be able to search inside path1: searchPath = path1\*
	_sntprintf(searchPath, MAX_PATH - 1, _T("%s\\*"), path1);
	searchPath[MAX_PATH - 1] = 0;

	// create a corresponding folder in the destination subtree
	if (!CreateDirectory(path2, NULL)) {
		_ftprintf(stderr, _T("Error creating directory %s. Error: %x\n"), path2, GetLastError());
		return;
	}

	// search inside path1
	hFind = FindFirstFile(searchPath, &findFileData);

	// check if handle is correctly open
	if (hFind == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("FindFirstFile failed. Error: %x\n"), GetLastError());
		return;
	}

	do {
		// generate a new path by appending to path1 the name of the found entry
		_sntprintf(newPath1, MAX_PATH, _T("%s\\%s"), path1, findFileData.cFileName);

		// generate a new path by appending to path2 the name of the found entry
		_sntprintf(newPath2, MAX_PATH, _T("%s\\%s"), path2, findFileData.cFileName);

		// check type of file
		DWORD fType = FileType(&findFileData);

		if (fType == TYPE_FILE) {
			// this is a file
			_tprintf(_T("FILE %s "), path1);

			// call the function to copy with modifications
			whatToDo(newPath1, newPath2);
		}

		if (fType == TYPE_DIR) {
			// this is a directory
			_tprintf(_T("DIR %s\n"), path1);

			// recursive call to the new paths
			copyDirectoryAndDo(newPath1, newPath2, level + 1, whatToDo);
		}

	} while (FindNextFile(hFind, &findFileData));

	FindClose(hFind);
	return;
}

static DWORD FileType(LPWIN32_FIND_DATA pFileData) {
	BOOL IsDir;
	DWORD FType;

	FType = TYPE_FILE;
	IsDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

	if (IsDir) {
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0 || lstrcmp(pFileData->cFileName, _T("..")) == 0) {
			FType = TYPE_DOT;
		} else {
			FType = TYPE_DIR;
		}
	}

	return FType;
}

VOID addDataFieldsToFile(LPTSTR path1, LPTSTR path2) {
	HANDLE hIn, hOut;
	LARGE_INTEGER fileSize;
	DWORD nRead, nWrote;
	DWORD fileNameLength;
	LPCH buffer;
	TCHAR buf[sizeof(DWORD)];

	// calculate the string length of the path1
	fileNameLength = (DWORD)_tcsnlen(path1, MAX_PATH);

	// open path1 to read the file
	hIn = CreateFile(path1, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Impossible to open file %s. Error: %x"), path1, GetLastError());
		return;
	}

	// open path2 to write the file
	hOut = CreateFile(path2, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Impossible to open file %s. Error: %x"), path2, GetLastError());
		return;
	}

	// read the file size (64bits)
	if (!GetFileSizeEx(hIn, &fileSize)) {
		_ftprintf(stderr, _T("Impossible to read the size of file %s"), path1);
		return;
	}

	// assumption: file is less tha 4GB (only 32 low bits of fileSize are used
	assert(fileSize.HighPart == 0);

	// allocate a buffer (who cares of the type of data)
	buffer = (LPCH)malloc(fileSize.LowPart);
	if (buffer == NULL) {
		_ftprintf(stderr, _T("Error Allocating memory for file %s. %x"), path1, GetLastError());
		return;
	}

	// read the content of file1 (on path1) and store into the buffer
	if (!ReadFile(hIn, buffer, fileSize.LowPart, &nRead, NULL) || nRead != fileSize.LowPart) {
		_ftprintf(stderr, _T("Error reading file %s. %x"), path1, GetLastError());
		return;
	}

	// write the filename of the original file to the file copied
	if (!WriteFile(hOut, path1, fileNameLength * sizeof(TCHAR), &nWrote, NULL) || nWrote != fileNameLength * sizeof(TCHAR)) {
		_ftprintf(stderr, _T("Error writing filenaname to file %s. %x"), path2, GetLastError());
		return;
	}

	_sntprintf(buf, sizeof(DWORD), _T("%d"), fileSize.LowPart);

	// write the file size of the original file in output
	if (!WriteFile(hOut, &buf, sizeof(DWORD), &nWrote, NULL) || nWrote != sizeof(DWORD)) {
		_ftprintf(stderr, _T("Error writing filenaname to file %s. %x"), path2, GetLastError());
		return;
	}

	// write the content of the original file to the copied file
	if (!WriteFile(hOut, buffer, fileSize.LowPart, &nWrote, NULL) || nWrote != fileSize.LowPart) {
		_ftprintf(stderr, _T("Error writing content to file %s. %x"), path2, GetLastError());
		return;
	}

	_tprintf(_T("written to %s\n"), path2);

	free(buffer);

	CloseHandle(hIn);
	CloseHandle(hOut);

	return;
}

// removes (if present) the trailing '\'
VOID cleanPath(LPTSTR path) {
	size_t strLen = _tcsclen(path);

	if (path[strLen] == '\\') {
		path[strLen] = '\0';
	}
}

int Return(int seconds, int value) {
	Sleep(seconds * 1000);
	return value;
}

LPWSTR getErrorMessageAsString(DWORD errorCode) {
	LPWSTR errString = NULL;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		0,
		errorCode,
		0,
		(LPWSTR)&errString,
		0,
		0);

	return errString;
}
