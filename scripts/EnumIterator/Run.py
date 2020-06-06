#!/usr/bin/python2

import os
import hashlib
import sys
import EnumIterator

rootFolder = "../../build/"
includeRoot = "../../include/"
srcRoot = "../../src/"
filesToParse = ["CmdLineParams.h"]

## Opens the given file, verifies if it has changed by comparing its md5 with our records
## If they're different, saves the fileContents. This allows us to avoid unnecessary
## recompilation in C/C++
## @param outputFolder
##		Folder where to store the file. e.g.
##		'/home/username/project/outfolder/'
##		'C:/project/outfolder/'
## @param fileName
##		Name of the file. e.g. 'MyHeaderEnum.h'
## @param fileContents
##		The contents of the file to store
def writeIfModified( outputFolder, fileName, fileContents ):
	finalMd5 = hashlib.md5( fileContents ).hexdigest()

	md5FileName = os.path.join( rootFolder, '.tmp', fileName + '.md5' )
	try:
		file = open( md5FileName, 'r+b' )
		oldMd5 = file.read()
	except:
		oldMd5 = ""

	outputPath = os.path.join( outputFolder, fileName )
	if finalMd5 != oldMd5 or not os.path.isfile( outputPath ):
		open( md5FileName, 'w+b' ).write( finalMd5 )
		open( outputPath, 'w+b' ).write( fileContents )

## Returns True if, according to our records, the modification time of the
## file has changed since the last time we've called this function.
## @param folderPath
##		Folder where the file is stored. e.g.
##		'/home/username/project/folder/'
##		'C:/project/folder/'
## @param fileName
##		Name of the file. e.g. 'MyHeader.h'
def fileWasModified( folderPath, fileName ):
	mtFileName = os.path.join( rootFolder, '.tmp', fileName + '.mt' )

	try:
		file = open( mtFileName, 'r+b' )
		oldMt = file.read()
	except:
		oldMt = -1

	fullPath = os.path.join( folderPath, fileName )
	try:
		mtime = str( os.path.getmtime( fullPath ) )
	except OSError:
		mtime = 0

	if oldMt != mtime:
		open( mtFileName, 'w+b' ).write( mtime )
		return True

	return False


if not os.path.exists( rootFolder + '.tmp' ):
	os.makedirs( rootFolder + '.tmp' )

needsRerun = False

for root, dir, files in os.walk(includeRoot):
	for fileName in files:
		fullPath = os.path.join( root, fileName )
		if fileName in filesToParse and fileWasModified( root, fileName ) \
		and not EnumIterator.wasFileGeneratedByUs( fullPath ):
			needsRerun = True
			break
	if needsRerun:
		break

if not needsRerun:
	print( "All files are up to date or none found" )
	exit(0)

outputCpp = EnumIterator.processEnumFileBegin()

print( "A file could be out of date. Updating enums..." )

for root, dir, files in os.walk(includeRoot):
	for fileName in files:
		splitFileName = os.path.splitext( fileName )
		fullPath = os.path.join( root, fileName )
		if fileName in filesToParse and not EnumIterator.wasFileGeneratedByUs( fullPath ):
			print( "****************" + fullPath + "****************" )
			print( EnumIterator.wasFileGeneratedByUs( fullPath ) )
			cppData, hppData = EnumIterator.processEnumFile( fullPath, fileName )

			outputCpp += cppData
			writeIfModified( root, splitFileName[0] + 'Enum.h', hppData )

writeIfModified( srcRoot, "AmalgamationEnum.cpp", outputCpp )
