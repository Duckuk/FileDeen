#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <windows.h>
#include "config.h"
#include "filedeen.h"
using namespace std;
namespace fs = filesystem;

FileDeen::Config CONFIG( L"FileDeen.ini" );

const string key = CONFIG.getString( "sKey" );
const bool keyEnabled = CONFIG.getBool( "bKeyEnabled" );
const bool keyUncensored = CONFIG.getBool( "bKeyUncensored" );
const bool onlyIncludeFolderContents = CONFIG.getBool( "bOnlyIncludeFolderContents" );
const bool useRealNames = CONFIG.getBool( "bUseRealNames" );
const bool verboseLogging = CONFIG.getBool( "bVerboseLogging" );

const unsigned char
SIGN[8] = { 0x53, 0x30, 0x53, 0x30, 0x72, 0x7F, 0x0D, 0x54 };
//			 83	   48	 83	   48	 114   127	 13	   84

mt19937_64 RNG;

void EncodeFile( vector<fs::path> filePaths ) {

	//Generate 3 random letters between a - z
	uniform_int_distribution<short> dis( 0x61, 0x7A ); //a to z
	wstring randomLetters( 3, 0x43 );
	for ( int i = 0; i<3; i++ ) {
		randomLetters[i] = dis( RNG );
	}

	wstring outputFileName = to_wstring( unsigned long long( time( nullptr ) ) ) + L"_" + randomLetters + L".fed";

	FileDeen::FeD fedFile;
	fedFile.setSignature( (char*)SIGN, 8 );

	if ( !verboseLogging ) printf( "Reading files.." );

	//Create FeD entries
	{
		int i = 0;
		for ( const auto& filePath : filePaths ) {
			if ( fs::is_directory( filePath ) ) {
				for ( const auto& dirEntry : fs::recursive_directory_iterator( filePath ) ) {
					if ( dirEntry.is_regular_file() ) {
						fs::path relativePath = fs::relative( dirEntry.path(), onlyIncludeFolderContents ? filePath : filePath.parent_path() );
						if ( verboseLogging ) wprintf( L"%ls: Creating entry...", relativePath.wstring().c_str() );
						FileDeen::FeD_Entry entry( { (unsigned)time( NULL ) } );
						
						entry.setIndex( i );
						
						string sRelativePath( relativePath.wstring().length()*2, 0x00 );
						memcpy( &sRelativePath[0], &relativePath.wstring()[0], relativePath.wstring().length()*2 );
						entry.setPathPadLength( FileDeen::CBCEncrypt( sRelativePath, keyEnabled ? key : "", entry.initVector() ) );
						entry.setPath( &sRelativePath[0], sRelativePath.length() );

						fstream inputFile( dirEntry.path(), ios::in | ios::binary | ios::ate );
						size_t length = inputFile.tellg();
						inputFile.seekg( 0 );

						std::string dataBuffer( length, 0x00 );
						inputFile.read( &dataBuffer[0], length );
						inputFile.close();
						entry.setDataPadLength( FileDeen::CBCEncrypt( dataBuffer, keyEnabled ? key : "", entry.initVector() ) );
						entry.moveData( dataBuffer );

						fedFile.moveEntry( entry );

						if ( verboseLogging ) printf( "Done!\n" );
						i++;
					}
				}
			}
			else {
				if ( verboseLogging ) wprintf( L"%ls: Creating entry...", filePath.filename().wstring().c_str() );
				FileDeen::FeD_Entry entry( { (unsigned)time( NULL ) } );

				entry.setIndex( i );

				fs::path relativePath = fs::relative( filePath, filePath.parent_path() ).string();
				string sRelativePath( relativePath.wstring().length()*2, 0x00 );
				memcpy( &sRelativePath[0], &relativePath.wstring()[0], relativePath.wstring().length()*2 );
				entry.setPathPadLength( FileDeen::CBCEncrypt( sRelativePath, keyEnabled ? key : "", entry.initVector() ) );
				entry.setPath( &sRelativePath[0], sRelativePath.length() );

				fstream inputFile( filePath, ios::in | ios::binary | ios::ate );
				size_t length = inputFile.tellg();
				inputFile.seekg( 0 );

				std::string dataBuffer( length, 0x00 );
				inputFile.read( &dataBuffer[0], length );
				inputFile.close();
				entry.setDataPadLength( FileDeen::CBCEncrypt( dataBuffer, keyEnabled ? key : "", entry.initVector() ) );
				entry.moveData( dataBuffer );

				fedFile.moveEntry( entry );

				if ( verboseLogging ) printf( "Done!\n" );
				i++;
			}
		}
	}

	if ( !verboseLogging ) printf( "Done!\n" );

	//Write data
	wprintf( L"Writing to \'%ls\'...", outputFileName.c_str() );
	fedFile.writeToFile( outputFileName );
	printf( "Done!\n" );

	return;
}

void DecodeFile( fs::path filePath ) {

	FileDeen::FeD fedFile;

	fstream inputFile;
	inputFile.open( filePath, fstream::in | fstream::binary );

	string buffer;

	//Check signature
	buffer.resize( sizeof( SIGN ) );
	if ( verboseLogging ) printf( "Checking signature..." );
	inputFile.read( &buffer[0], sizeof( SIGN ) );
	if ( memcmp( &buffer[0], SIGN, sizeof( SIGN ) ) != NULL ) {
		printf( "Error: File is not a FeD file\n" );
		return;
	}
	fedFile.setSignature( &buffer[0], sizeof( SIGN ) );
	if ( verboseLogging ) printf( "Done!\n" );

	//Check version
	if ( verboseLogging ) printf( "Checking version..." );
	unsigned char versionByte = inputFile.peek();
	if ( versionByte != fedFile.version() ) {
		short nextTwoBytes;
		inputFile.read( (char*)&nextTwoBytes, sizeof( nextTwoBytes ) );
		inputFile.seekg( -(signed)sizeof( nextTwoBytes ), ios::cur );
		if ( nextTwoBytes % 2 == 0 && nextTwoBytes <= 512 ) {
			printf( "Error:	FeD file was encoded before version checking was added.\nComplete decoding is not guaranteed\n" );
		}
		else if ( versionByte < fedFile.version() ) {
			printf( "Error: FeD file was encoded with past encoding scheme \'v%u\', whereas the current decoding scheme is \'v%u\'.\nComplete decoding is not guaranteed\n", versionByte, fedFile.version() );
			inputFile.seekg( 1, ios::cur );
		}
		else if ( versionByte > fedFile.version() ) {
			printf( "Error: FeD file was encoded with future encoding scheme \'v%u\', whereas the current decoding scheme is \'v%u\'.\nComplete decoding is not guaranteed.\n", versionByte, fedFile.version() );
			inputFile.seekg( 1, ios::cur );
		}
		printf( "Do you wish to continue? (y/n): " );
		if ( getchar() == 'n' ) {
			cin.ignore();
			return;
		}
		else {
			cin.ignore();
		}
	}
	else {
		if ( verboseLogging ) printf( "Done!: \'v%u\'\n", versionByte );
		inputFile.seekg( 1, ios::cur );
	}

	while ( true ) {
		FileDeen::FeD_Entry entry( { (unsigned)time( NULL ) } );

		//Read index
		buffer.resize( sizeof( entry.index() ) );
		if ( verboseLogging ) printf( "Reading index..." );
		inputFile.read( &buffer[0], buffer.size() );
		entry.setIndex( &buffer[0], buffer.size() );
		if ( verboseLogging && entry.index() != 0xFFFFFFFF ) printf( "Done!: %.3u\n", entry.index() );

		if ( entry.index() == 0xFFFFFFFF ) {
			if ( verboseLogging ) printf( "End of file found\n" );
			inputFile.close();
			break;
		}

		//Read initialization vector
		std::vector<char> initVector( FileDeen::blockSize );
		if ( verboseLogging ) printf( "Reading initialization vector..." );
		inputFile.read( &initVector[0], initVector.size() );
		entry.setInitVector( initVector );
		if ( verboseLogging ) printf( "Done!\n" );

		//Read path size
		unsigned short pathLength;
		if ( verboseLogging ) printf( "%.3u: Reading path length...", entry.index() );
		inputFile.read( (char*)&pathLength, sizeof( pathLength ) );
		if ( verboseLogging ) printf( "Done!: %hu\n", pathLength );

		//Read path padding size
		unsigned short pathPadLength;
		if ( verboseLogging ) printf( "%.3u: Reading path padding length...", entry.index() );
		inputFile.read( (char*)&pathPadLength, sizeof( pathPadLength ) );
		if ( verboseLogging ) printf( "Done!: %hu\n", pathPadLength );

		//Read path
		buffer.resize( pathLength );
		if ( verboseLogging ) printf( "%.3u: Reading path...", entry.index() );
		inputFile.read( &buffer[0], buffer.size() );
		FileDeen::CBCDecrypt( buffer, key, entry.initVector() );
		buffer.resize( buffer.size()-pathPadLength );
		entry.setPath( &buffer[0], buffer.size() );
		if ( verboseLogging ) printf( "Done!\n" );

		//Read data length
		size_t dataLength;
		if ( verboseLogging ) printf( "%.3u: Reading data length...", entry.index() );
		inputFile.read( (char*)&dataLength, sizeof( dataLength ) );
		if ( verboseLogging ) printf( "Done!: %zu\n", dataLength );

		//Read data padding size
		unsigned short dataPadLength;
		if ( verboseLogging ) printf( "%.3u: Reading data padding length...", entry.index() );
		inputFile.read( (char*)&dataPadLength, sizeof( dataPadLength ) );
		if ( verboseLogging ) printf( "Done!: %hu\n", dataPadLength );

		//Read data
		buffer.resize( dataLength );
		if ( verboseLogging ) printf( "%.3u: Reading data...", entry.index() );
		inputFile.read( &buffer[0], buffer.size() );
		FileDeen::CBCDecrypt( buffer, key, entry.initVector() );
		buffer.resize( buffer.size()-dataPadLength );
		entry.moveData( buffer );
		if ( verboseLogging ) printf( "Done!\n" );

		//Write data
		fs::path outputFileName;
		if ( useRealNames ) {
			outputFileName = filePath.stem().wstring() + L"\\" + entry.path().wstring();
		}
		else {
			outputFileName = filePath.stem().wstring() + L"\\" + entry.path().parent_path().wstring() + L"\\" + to_wstring( entry.index() ) + entry.path().extension().wstring();
		}
		fs::create_directories( outputFileName.parent_path() );
		wprintf( L"%.3u: Writing to \'%ls\'...", entry.index(), outputFileName.c_str() );
		entry.writeToFile( outputFileName );
		printf( "Done!\n" );
	}
	return;
}

int wmain( int argc, wchar_t* argv[] ) {

	SetConsoleTitleW( L"FileDeen | Encoding Scheme: v5" );

	RNG.seed( (unsigned)time( NULL ) );

	vector<fs::path> filePaths;
	if ( argc < 2 ) {
		fs::path filePath;
		while ( true ) {
			string buffer;
			cout << "Input full path to file: ";
			getline( cin, buffer );
			filePath = buffer;
			if ( fs::is_regular_file( filePath ) || fs::is_directory( filePath ) ) {
				break;
			}
			cout << "Error: File does not exist or is unsupported" << endl << endl;
		}
		filePaths.push_back( filePath );
	}
	else {
		for ( int i = 1; i<argc; i++ ) {
			filePaths.push_back( argv[i] );
		}
	}
	
	bool extensionWarning = false;
	size_t approximateSize = FileDeen::maxMetadataSize;
	for ( auto it = filePaths.begin(); it!=filePaths.end(); ) {
		fs::path path = *it;
		if ( !fs::is_regular_file( path ) && !fs::is_directory( path ) ) {
			cout << "Error: " << path << " does not exist or is unsupported. It will not be encoded." << endl;
			it = filePaths.erase( it );
		}
		else if ( fs::is_directory( path ) ) {
			for ( const auto& entry : fs::recursive_directory_iterator( path ) ) {
				if ( entry.is_regular_file() ) {
					approximateSize += fs::file_size( entry ) + FileDeen::entryMetadataSize;
				}
			}
			it++;
		}
		else {
			approximateSize += fs::file_size( path ) + FileDeen::entryMetadataSize;
			it++;
		}
		if ( !extensionWarning ) {
			if ( path.extension().wstring().length() > 4 ) {
				extensionWarning = true;
			}
		}
	}

	double approximateSizeConverted = (double)approximateSize;
	int sizeUsed = 0;
	string sizes[4] = { " bytes", "KB", "MB", "GB" };
	for ( double i = 0; i<4; i++ ) {
		double kbSize = pow( 1024., i+1 );
		if ( approximateSize<kbSize ) {
			for ( int x = 0; x<i; x++ ) {
				approximateSizeConverted /= 1024.;
			}
			sizeUsed = i;
			break;
		}
	}

	if ( extensionWarning ) {
		cout << "WARNING: One or more file extensions are longer than three characters, which encoding does not support. Encode at your own risk." << endl;
	}

	if ( keyEnabled && key.length() > 0 ) {
		if ( keyUncensored ) {
			printf( "Password: \"%s\"\n", key.c_str() );
		}
		else {
			if ( key.length() < 3 ) {
				printf( "Password: \"%s\"\n", string( key.size(), '*' ).c_str() );
			}
			else {
				printf( "Password: \"%c%s%c\"\n", key[0], string( key.size()-2, '*' ).c_str(), key.back() );
			}
		}
	}

	printf( "Pick Mode:\n"
		" (E) Encode [Approximate File Size: %.2f%s]\n"
		" (D) Decode\n",
		approximateSizeConverted, sizes[sizeUsed].c_str());

	switch ( tolower( getchar() ) ) {
		case 'e':
			cin.ignore();
			EncodeFile( filePaths );
			break;
		case 'd':
			cin.ignore();
			DecodeFile( filePaths[0] );
			break;
	}
	printf( "All done!\n" );

	if ( verboseLogging ) {
		printf( "Press any key to exit\n" );
		getchar();
	}
	else {
		this_thread::sleep_for( 1.5s );
	}
	return 0;
}