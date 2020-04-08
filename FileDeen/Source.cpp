#define CRCPP_USE_CPP11

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#include <string>
#include <windows.h>
#include "CRC.h"
#include "classes.h"
using namespace std;

const bool
DEBUG_MODE = false;

const unsigned char
SIGN[8] = { 0x53, 0x30, 0x53, 0x30, 0x72, 0x7F, 0x0D, 0x54 };
//			 83	   48	 83	   48	 114   127	 13	   84

mt19937_64 rng;

void EncodeFile( vector<filesystem::path> filePaths ) {

	//Generate 3 random letters between a - z
	uniform_int_distribution<short> dis( 0x61, 0x7A ); //a to z
	wstring randomLetters( 3, 0x43 );
	for ( int i = 0; i<3; i++ )
		randomLetters[i] = dis(rng);

	wstring outputFileName = to_wstring( unsigned long long( time( nullptr ) ) ) + L"_" + randomLetters + L".fed";

	FileDeen::FED fedFile;
	fedFile.setSignature( (char*)SIGN, 8 );

	if ( !DEBUG_MODE ) printf( "Reading files.." );

	//Create FED entries
	{
		int i = 0;
		for ( const auto& filePath : filePaths ) {
			if ( DEBUG_MODE ) wprintf( L"%ls: Creating entry...", filePath.filename().wstring().c_str() );
			FileDeen::FED_Entry entry;

			entry.setIndex( i );
			entry.setFileExtension( filePath.extension() );

			fstream inputFile( filePath, ios::in | ios::binary | ios::ate );
			size_t length = inputFile.tellg();
			inputFile.seekg( 0 );

			std::string dataBuffer( length, 0x00 );
			inputFile.read( &dataBuffer[0], length );
			inputFile.close();
			entry.moveData( dataBuffer );
			
			fedFile.moveEntry( entry );

			i++;
			if ( DEBUG_MODE ) printf( "Done!\n" );
		}
	}

	if ( !DEBUG_MODE ) printf( "Done!\n" );

	//Generate dictionary
	if ( DEBUG_MODE ) printf( "Generating dictionary..." );
	fedFile.generateDictionary();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Clean dictionary
	if ( DEBUG_MODE ) printf( "Cleaning dictionary..." );
	fedFile.cleanDictionary();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Calculate dictionary size
	if ( DEBUG_MODE ) printf( "Calculating dictionary size..." );
	fedFile.calculateDictSize();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Randomize dictionary
	if ( DEBUG_MODE ) printf( "Randomizing dictionary..." );
	fedFile.randomizeDictionary();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Translate data
	if ( DEBUG_MODE ) printf( "Translating data..." );
	fedFile.translateEntries( fedFile.dictionary() );
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Generate checksums
	if ( DEBUG_MODE ) printf( "Generating checksums..." );
	for ( int i = 0; i<fedFile.numEntries(); i++ ) {
		FileDeen::FED_Entry& entry = fedFile.entry( i );
		unsigned int checksum = entry.calculateChecksum();
		entry.setChecksum( checksum );
	}
	printf( "Done!\n" );

	//Write data
	wprintf( L"Writing to \'%ls\'...", outputFileName.c_str() );
	fedFile.writeToFile( outputFileName );
	printf( "Done!\n" );

	return;
}

void DecodeFile( filesystem::path filePath ) {

	FileDeen::FED fedFile;

	fstream inputFile;
	inputFile.open( filePath, fstream::in | fstream::binary );

	string buffer;

	//Check signature
	buffer.resize( sizeof( SIGN ) );
	if ( DEBUG_MODE ) printf( "Checking signature..." );
	inputFile.read( &buffer[0], sizeof( SIGN ) );
	if ( memcmp( &buffer[0], SIGN, sizeof( SIGN ) ) != NULL ) {
		printf( "Error: File is not a FED file\n" );
		return;
	}
	fedFile.setSignature( &buffer[0], sizeof( SIGN ) );
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Read dictionary size
	short dictionarySize;
	if ( DEBUG_MODE ) printf( "Reading dictionary size..." );
	inputFile.read( (char*)&dictionarySize, sizeof( dictionarySize ) );
	if ( DEBUG_MODE ) printf( "Done!: %hd\n", dictionarySize );

	//Read dictionary
	string dictionary( dictionarySize/2, 0x00 );
	buffer.resize( 2 );
	if ( DEBUG_MODE ) printf( "Reading dictionary..." );
	for ( short i = 0; i<dictionarySize/2; i++ ) {
		inputFile.read( &buffer[0], 2 );
		dictionary[(unsigned char)buffer[0]] = buffer[1];
	}
	fedFile.setDictionary( &dictionary[0], dictionary.size() );
	if ( DEBUG_MODE ) printf( "Done!\n" );

	while ( true ) {
		FileDeen::FED_Entry entry;

		//Read index
		buffer.resize( sizeof( entry.index() ) );
		if ( DEBUG_MODE ) printf( "Reading index..." );
		inputFile.read( &buffer[0], buffer.size() );
		entry.setIndex( &buffer[0], buffer.size() );
		if ( DEBUG_MODE && entry.index() != 0xFFFFFFFF ) printf( "Done!: %u\n", entry.index() );

		if ( entry.index() == 0xFFFFFFFF ) {
			if ( DEBUG_MODE ) printf( "End of file found\n" );
			inputFile.close();
			break;
		}
		
		//Read data length
		size_t dataLength;
		if ( DEBUG_MODE ) printf( "%u: Reading data length...", entry.index() );
		inputFile.read( (char*)&dataLength, sizeof( dataLength ) );
		if ( DEBUG_MODE ) printf( "Done!: %zu\n", dataLength );

		//Read file extension
		buffer.resize( entry.fileExtension().size()*sizeof( wchar_t ) );
		if ( DEBUG_MODE ) printf( "%u: Reading file extension...", entry.index() );
		inputFile.read( &buffer[0], buffer.size() );
		entry.setFileExtension( &buffer[0], buffer.size() );
		if ( DEBUG_MODE ) printf( "Done!\n");

		//Read data
		buffer.resize( dataLength );
		if ( DEBUG_MODE ) printf( "%u: Reading data...", entry.index() );
		inputFile.read( &buffer[0], buffer.size() );
		entry.moveData( buffer );
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Check checksum
		buffer.resize( sizeof( entry.checksum() ) );
		if ( DEBUG_MODE ) printf( "%u: Checking checksum...", entry.index() );
		unsigned int checksum = entry.calculateChecksum();
		inputFile.read( &buffer[0], buffer.size() );
		entry.setChecksum( &buffer[0], buffer.size() );
		if ( entry.checksum() != checksum ) {
			printf( "Error: Checksum mismatch, skipping\n" );
			continue;
		}
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Translate extension
		if ( DEBUG_MODE ) printf( "%u: Translating extension...", entry.index() );
		entry.translateExtension( fedFile.dictionary() );
		if ( DEBUG_MODE ) printf( "Done!: \'%ls\'\n", entry.fileExtension().c_str() );

		//Translate data
		if ( DEBUG_MODE ) printf( "%u: Translating data...", entry.index() );
		entry.translateData( fedFile.dictionary() );
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Write data
		filesystem::path outputFileName = filePath.stem().wstring() + L"_" + to_wstring(entry.index()) + entry.fileExtension();
		wprintf( L"%u: Writing to \'%ls\'...", entry.index(), outputFileName.c_str() );
		entry.writeToFile( outputFileName );
		printf( "Done!\n" );
	}
	return;
}

int wmain( int argc, wchar_t* argv[] ) {

	SetConsoleTitleW( L"FileDeen" );

	rng.seed( (unsigned)time( NULL ) );

	vector<filesystem::path> filePaths;
	filesystem::path filePath;
	if ( argc < 2 ) {
		string buffer;
		while ( true ) {
			cout << "Input full path to file: ";
			getline( cin, buffer );
			filePath = buffer;
			if ( filesystem::exists( filePath ) && !filesystem::is_directory( filePath ) ) {
				break;
			}
			cout << "Error: File does not exist" << endl << endl;
		}
		filePaths.push_back( filePath );
	}
	else {
		for ( int i = 1; i<argc; i++ ) {
			filePaths.push_back( argv[i] );
		}
	}

	bool fail = false;
	bool extensionWarning = false;
	for ( const auto &filePath : filePaths ) {
		if ( !filesystem::exists( filePath ) || filesystem::is_directory( filePath ) ) {
			cout << "Error: " << filePath << " does not exist" << endl;
			fail = true;
		}
		if ( !extensionWarning )
			if ( filePath.extension().wstring().length() > 4 )
				extensionWarning = true;
	}

	if ( fail ) {
		this_thread::sleep_for( 2s );
		return 1;
	}
	if ( extensionWarning )
		cout << "WARNING: One or more file extensions are longer than three characters, which encoding does not support. Encode at your own risk." << endl;

	cout << "Pick Mode:" << endl <<
		" (E) Encode" << endl <<
		" (D) Decode" << endl;

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
	if ( DEBUG_MODE ) {
		getchar();
	}
	else {
		this_thread::sleep_for( 3s );
	}
	return 0;
}