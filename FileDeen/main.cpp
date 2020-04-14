#define CRCPP_USE_CPP11

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <windows.h>
#include "CRC.h"
#include "config.h"
#include "filedeen.h"
using namespace std;
namespace fs = filesystem;

FileDeen::Config CONFIG( "FileDeen.ini" );

const bool useRealNames = CONFIG.useRealNames();
const bool verboseLogging = CONFIG.verboseLogging();

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
						fs::path relativePath = fs::relative( dirEntry.path(), filePath.parent_path() );
						if ( verboseLogging ) wprintf( L"%ls: Creating entry...", relativePath.wstring().c_str() );
						FileDeen::FeD_Entry entry;

						entry.setIndex( i );
						entry.setPath( relativePath );

						fstream inputFile( dirEntry.path(), ios::in | ios::binary | ios::ate );
						size_t length = inputFile.tellg();
						inputFile.seekg( 0 );

						std::string dataBuffer( length, 0x00 );
						inputFile.read( &dataBuffer[0], length );
						inputFile.close();
						entry.moveData( dataBuffer );

						fedFile.moveEntry( entry );

						if ( verboseLogging ) printf( "Done!\n" );
						i++;
					}
				}
			}
			else {
				if ( verboseLogging ) wprintf( L"%ls: Creating entry...", filePath.filename().wstring().c_str() );
				FileDeen::FeD_Entry entry;

				entry.setIndex( i );
				entry.setPath( fs::relative( filePath, filePath.parent_path() ) );

				fstream inputFile( filePath, ios::in | ios::binary | ios::ate );
				size_t length = inputFile.tellg();
				inputFile.seekg( 0 );

				std::string dataBuffer( length, 0x00 );
				inputFile.read( &dataBuffer[0], length );
				inputFile.close();
				entry.moveData( dataBuffer );

				fedFile.moveEntry( entry );

				if ( verboseLogging ) printf( "Done!\n" );
				i++;
			}
		}
	}

	if ( !verboseLogging ) printf( "Done!\n" );

	if ( !verboseLogging ) wprintf( L"Writing to \'%ls\'...", outputFileName.c_str() );
	//Generate dictionary
	if ( verboseLogging ) printf( "Generating dictionary..." );
	fedFile.generateDictionary();
	if ( verboseLogging ) printf( "Done!\n" );

	//Clean dictionary
	if ( verboseLogging ) printf( "Cleaning dictionary..." );
	fedFile.cleanDictionary();
	if ( verboseLogging ) printf( "Done!\n" );

	//Calculate dictionary size
	if ( verboseLogging ) printf( "Calculating dictionary size..." );
	fedFile.calculateDictSize();
	if ( verboseLogging ) printf( "Done!\n" );

	//Randomize dictionary
	if ( verboseLogging ) printf( "Randomizing dictionary..." );
	fedFile.randomizeDictionary();
	if ( verboseLogging ) printf( "Done!\n" );

	//Translate data
	if ( verboseLogging ) printf( "Translating data..." );
	fedFile.translateEntries( fedFile.dictionary() );
	if ( verboseLogging ) printf( "Done!\n" );

	//Generate checksums
	if ( verboseLogging ) printf( "Generating checksums..." );
	for ( int i = 0; i<fedFile.numEntries(); i++ ) {
		FileDeen::FeD_Entry& entry = fedFile.entry( i );
		unsigned int checksum = entry.calculateChecksum();
		entry.setChecksum( checksum );
	}
	if ( verboseLogging ) printf( "Done!\n" );

	//Write data
	if ( verboseLogging ) wprintf( L"Writing to \'%ls\'...", outputFileName.c_str() );
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

	//Read dictionary size
	short dictionarySize;
	if ( verboseLogging ) printf( "Reading dictionary size..." );
	inputFile.read( (char*)&dictionarySize, sizeof( dictionarySize ) );
	if ( verboseLogging ) printf( "Done!: %hd\n", dictionarySize );

	//Read dictionary
	string dictionary( FileDeen::maxDictSize/2, 0x00 );
	buffer.resize( 2 );
	if ( verboseLogging ) printf( "Reading dictionary..." );
	for ( short i = 0; i<dictionarySize/2; i++ ) {
		inputFile.read( &buffer[0], 2 );
		dictionary[(unsigned char)buffer[0]] = buffer[1];
	}
	fedFile.setDictionary( &dictionary[0], dictionary.size() );
	if ( verboseLogging ) printf( "Done!\n" );

	while ( true ) {
		FileDeen::FeD_Entry entry;

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

		//Read path
		buffer.resize( FileDeen::pathSize );
		if ( verboseLogging ) printf( "%.3u: Reading path...", entry.index() );
		inputFile.read( &buffer[0], buffer.size() );
		entry.setPath( &buffer[0], buffer.size() );
		if ( verboseLogging ) printf( "Done!\n" );

		//Read data length
		size_t dataLength;
		if ( verboseLogging ) printf( "%.3u: Reading data length...", entry.index() );
		inputFile.read( (char*)&dataLength, sizeof( dataLength ) );
		if ( verboseLogging ) printf( "Done!: %zu\n", dataLength );

		//Read data
		buffer.resize( dataLength );
		if ( verboseLogging ) printf( "%.3u: Reading data...", entry.index() );
		inputFile.read( &buffer[0], buffer.size() );
		entry.moveData( buffer );
		if ( verboseLogging ) printf( "Done!\n" );

		//Check checksum
		buffer.resize( sizeof( entry.checksum() ) );
		if ( verboseLogging ) printf( "%.3u: Checking checksum...", entry.index() );
		unsigned int checksum = entry.calculateChecksum();
		inputFile.read( &buffer[0], buffer.size() );
		entry.setChecksum( &buffer[0], buffer.size() );
		if ( entry.checksum() != checksum ) {
			printf( "Error: Checksum mismatch, skipping %.3u\n", entry.index() );
			continue;
		}
		if ( verboseLogging ) printf( "Done!\n" );

		//Translate extension
		if ( verboseLogging ) printf( "%.3u: Translating path...", entry.index() );
		entry.translatePath( fedFile.dictionary() );
		if ( verboseLogging ) printf( "Done!: \'%ls\'\n", entry.path().c_str() );

		//Translate data
		if ( verboseLogging ) printf( "%.3u: Translating data...", entry.index() );
		entry.translateData( fedFile.dictionary() );
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

	SetConsoleTitleW( L"FileDeen | Encoding Scheme: v4" );

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
	int sizeUsed;
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