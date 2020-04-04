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
using namespace std;

const bool
DEBUG_MODE = false;

const unsigned char
SIGN[8] = { 0x53, 0x30, 0x53, 0x30, 0x72, 0x7F, 0x0D, 0x54 };
//			 83	   48	 83	   48	 114   127	 13	   84

unsigned char
DICT[256],
FILE_EXTENSION[8],
CHECKSUM[4];

uint16_t
DICT_SIZE = sizeof( DICT )*2;

uint32_t
INDEX = 0;

size_t
DATA_LENGTH,
METADATA_LENGTH = sizeof( INDEX )+sizeof( FILE_EXTENSION )+sizeof( DATA_LENGTH );

mt19937_64 rng;

void refreshMetadataLength() {
	METADATA_LENGTH = sizeof( INDEX )+sizeof( FILE_EXTENSION )+sizeof( DATA_LENGTH );
}

void encodeData( char* buffer, size_t bufferSize ) {
	for ( size_t i = 0; i<bufferSize; i++ ) {
		buffer[i] = DICT[(unsigned char)buffer[i]];
	}
}

void encodeFile( vector<filesystem::path> filePaths ) {
	wstring outputFileName = to_wstring( unsigned long long( time( NULL ) ) ) + L".fed";
	fstream outputFile( outputFileName, ios::out | ios::in | ios::binary | ios::trunc );

	//Generate dictionary
	if ( DEBUG_MODE ) printf( "Generating dictionary..." );
	for ( unsigned int i = 0; i<sizeof( DICT ); i++ ) {
		DICT[i] = i;
	}
	if ( DEBUG_MODE ) printf( "Done!\n" );

	bool existingBytes[sizeof( DICT )];
	memset( existingBytes, false, sizeof( existingBytes ) );
	fstream inputFile;
	string data;

	//Clean dictionary
	if ( DEBUG_MODE ) printf( "Cleaning dictionary.." );
	for ( const auto& filePath : filePaths ) {
		inputFile.open( filePath, ios::in | ios::binary | ios::ate );

		memcpy( FILE_EXTENSION, filePath.extension().c_str(), sizeof( FILE_EXTENSION ) );

		DATA_LENGTH = inputFile.tellg();
		data.reserve( DATA_LENGTH );

		inputFile.seekg( 0 );
		inputFile.read( (char*)&data[0], DATA_LENGTH );
		inputFile.close();

		for ( size_t i = 0; i<DATA_LENGTH; i++ ) {
			existingBytes[uint8_t( data[i] )] = true;
		}
		for ( size_t i = 0; i<sizeof( FILE_EXTENSION ); i++ ) {
			existingBytes[FILE_EXTENSION[i]] = true;
		}
		
		data.clear();
		inputFile.clear();
	}
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Write signature chunk
	if ( !DEBUG_MODE ) wprintf( L"Writing to \'%ls\'...", outputFileName.c_str() );
	if ( DEBUG_MODE ) printf( "Writing signature.." );
	outputFile.seekp( 0 );
	outputFile.write( (char*)SIGN, sizeof( SIGN ) );
	outputFile.flush();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Calculate dictionary size
	if ( DEBUG_MODE ) printf( "Calculating dictionary size..." );
	for ( int i = 0; i<sizeof( existingBytes ); i++ ) {
		if ( !existingBytes[i] ) {
			DICT_SIZE--; DICT_SIZE--;
		}
	}
	refreshMetadataLength();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Write dictionary size
	if ( DEBUG_MODE ) printf( "Writing dictionary size..." );
	outputFile.write( (char*)&DICT_SIZE, sizeof( DICT_SIZE ) );
	outputFile.flush();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	//Randomize dictionary and write
	shuffle( begin( DICT ), end( DICT ), rng );
	if ( DEBUG_MODE ) printf( "Writing dictionary..." );
	for ( int i = 0; i<sizeof( DICT ); i++ ) {
		if ( existingBytes[i] ) {
			outputFile.put( DICT[i] );
			outputFile.put( i );
		}
	}
	outputFile.flush();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	for ( const auto& filePath : filePaths ) {

		inputFile.open( filePath, ios::in | ios::binary | ios::ate );

		DATA_LENGTH = inputFile.tellg();
		data.reserve( DATA_LENGTH );

		memcpy( FILE_EXTENSION, filePath.extension().c_str(), sizeof( FILE_EXTENSION ) );

		//Store input data in buffer and close
		inputFile.seekg( 0 );
		inputFile.read( (char*)&data[0], DATA_LENGTH );
		inputFile.close();

		//Write index
		if ( DEBUG_MODE ) wprintf( L"%ls: Writing index...", filePath.filename().c_str() );
		outputFile.write( (char*)&INDEX, sizeof( INDEX ) );
		outputFile.flush();
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Write data length
		if ( DEBUG_MODE ) wprintf( L"%ls: Writing data length...", filePath.filename().c_str() );
		outputFile.write( (char*)&DATA_LENGTH, sizeof( DATA_LENGTH ) );
		outputFile.flush();
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Modify file extension with randomized dictionary
		encodeData( (char*)FILE_EXTENSION, sizeof( FILE_EXTENSION ) );

		//Write file extension
		if ( DEBUG_MODE ) wprintf( L"%ls: Writing file extension...", filePath.filename().c_str() );
		outputFile.write( (char*)FILE_EXTENSION, sizeof( FILE_EXTENSION ) );
		outputFile.flush();
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Modify data with randomized dictionary
		encodeData( (char*)&data[0], DATA_LENGTH );

		//Write data
		if ( DEBUG_MODE ) wprintf( L"%ls: Writing data...", filePath.filename().c_str() );
		outputFile.write( (char*)&data[0], DATA_LENGTH );
		outputFile.flush();
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Write checksum and close
		if ( DEBUG_MODE ) wprintf( L"%ls: Calculating checksum...", filePath.filename().c_str() );
		unsigned long long prevPos = outputFile.tellg();
		outputFile.seekg( prevPos-METADATA_LENGTH-DATA_LENGTH );
		string buffer( METADATA_LENGTH+DATA_LENGTH, 0x00 );
		outputFile.read( &buffer[0], METADATA_LENGTH+DATA_LENGTH );
		uint32_t checksum = CRC::Calculate( buffer.c_str(), buffer.length(), CRC::CRC_32() );
		memcpy( CHECKSUM, &checksum, sizeof( CHECKSUM ) );
		if ( DEBUG_MODE ) printf( "Done!\n" );

		if ( DEBUG_MODE ) wprintf( L"%ls: Writing checksum...", filePath.filename().c_str() );
		outputFile.seekg( prevPos );
		outputFile.write( (char*)CHECKSUM, sizeof( CHECKSUM ) );
		if ( DEBUG_MODE ) printf( "Done!\n" );

		data.clear();
		inputFile.clear();
		INDEX++;
	}
	if ( DEBUG_MODE ) printf( "Writing end of file..." );
	char endOfFile[8];
	memset( endOfFile, 0xFF, sizeof( endOfFile ) );
	outputFile.write( endOfFile, sizeof( INDEX ) );
	printf( "Done!\n" );
	printf( "Closing output file..." );
	outputFile.close();
	printf( "Done!\n" );
	return;
}

void decodeFile( filesystem::path filePath ) {
	fstream inputFile;
	inputFile.open( filePath, fstream::in | fstream::binary | fstream::ate );
	char buffer[1024];
	memset( buffer, 0x00, sizeof( buffer ) );

	//Check signature
	if ( !DEBUG_MODE ) printf( "Reading data..." );
	if ( DEBUG_MODE ) printf( "Checking signature..." );
	inputFile.seekg( 0 );
	inputFile.read( buffer, sizeof( SIGN ) );
	if ( memcmp( buffer, SIGN, sizeof( SIGN ) ) != 0 ) {
		printf( "Error: File is not a FED file\n" );
		return;
	}
	memset( buffer, 0x00, sizeof( buffer ) );
	printf( "Done!\n" );

	//Get size of dictionary
	if ( DEBUG_MODE ) printf( "Reading dictionary size..." );
	inputFile.seekg( sizeof( SIGN ) );
	inputFile.read( buffer, sizeof( DICT_SIZE ) );
	memcpy( &DICT_SIZE, buffer, sizeof( DICT_SIZE ) );
	memset( buffer, 0x00, sizeof( buffer ) );
	if ( DEBUG_MODE ) printf( "Done!: %hu\n", DICT_SIZE );

	//Get dictionary
	if ( DEBUG_MODE ) printf( "Reading dictionary..." );
	for ( uint16_t i = 0; i<DICT_SIZE/2; i++ ) {
		inputFile.read( buffer, 2 );
		DICT[(unsigned char)buffer[0]] = buffer[1];
		memset( buffer, 0x00, sizeof( buffer ) );
	}
	refreshMetadataLength();
	if ( DEBUG_MODE ) printf( "Done!\n" );

	for ( int i = 0; ; i++ ) {
		//Get index
		if ( DEBUG_MODE ) printf( "Reading index..." );
		inputFile.read( buffer, sizeof( INDEX ) );
		memcpy( &INDEX, buffer, sizeof( INDEX ) );
		memset( buffer, 0x00, sizeof( buffer ) );
		if ( DEBUG_MODE ) printf( "Done!: %u\n", INDEX );

		if ( INDEX == 0xFFFFFFFF ) {
			inputFile.close();
			break;
		}

		//Get length of data
		if ( DEBUG_MODE ) printf( "%u: Reading length of data...", INDEX );
		inputFile.read( buffer, sizeof( DATA_LENGTH ) );
		memcpy( &DATA_LENGTH, buffer, sizeof( DATA_LENGTH ) );
		memset( buffer, 0x00, sizeof( buffer ) );
		if ( DEBUG_MODE ) printf( "Done!: %zu\n", DATA_LENGTH );

		//Check checksum
		if ( DEBUG_MODE ) printf( "%u: Checking checksum...", INDEX );
		string checksumData( METADATA_LENGTH+DATA_LENGTH, 0x00 );
		unsigned long long prevPos = inputFile.tellg();
		inputFile.seekg( prevPos-sizeof( INDEX )-sizeof( DATA_LENGTH ) );
		inputFile.read( &checksumData[0], METADATA_LENGTH+DATA_LENGTH );
		uint32_t checksum = CRC::Calculate( checksumData.c_str(), checksumData.length(), CRC::CRC_32() );

		inputFile.read( buffer, sizeof( CHECKSUM ) );
		if ( memcmp( buffer, &checksum, sizeof( checksum ) ) != 0 ) {
			printf( "Error: Checksum mismatch" );
			return;
		}
		inputFile.seekg( prevPos );
		if ( DEBUG_MODE ) printf( "Done!\n" );

		//Decode and get file extension
		if ( DEBUG_MODE ) printf( "%u: Reading file extension...", INDEX );
		inputFile.read( buffer, sizeof( FILE_EXTENSION ) );
		encodeData( buffer, sizeof( FILE_EXTENSION ) );
		memcpy( FILE_EXTENSION, buffer, sizeof( FILE_EXTENSION ) );
		memset( buffer, 0x00, sizeof( buffer ) );
		if ( DEBUG_MODE ) printf( "Done!: \'%ls\'\n", FILE_EXTENSION );

		//Decode and write data
		if ( DEBUG_MODE ) printf( "%u: Reading data...", INDEX );
		filesystem::path outputFileName = filePath.stem().wstring() + L"_" + to_wstring( INDEX );
		for ( int i = 0; i<sizeof( FILE_EXTENSION ); i = i+2 ) {
			outputFileName += FILE_EXTENSION[i];
		}
		string data(DATA_LENGTH, 0x00);
		inputFile.read( (char*)&data[0], DATA_LENGTH );

		encodeData( (char*)&data[0], DATA_LENGTH );

		wprintf( L"%u: Writing to \'%ls\'...", INDEX, outputFileName.c_str() );
		fstream outputFile( outputFileName, ios::out | ios::binary );
		outputFile.write( (char*)&data[0], DATA_LENGTH );
		printf( "Done!\n" );
		outputFile.close();
		outputFile.clear();

		inputFile.seekg( inputFile.tellg()+(streampos)4 );
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
			encodeFile( filePaths );
			break;
		case 'd':
			decodeFile( filePaths[0] );
			break;
	}
	if ( DEBUG_MODE ) {
		cin.ignore();
		getchar();
	}
	else {
		this_thread::sleep_for( 3s );
	}
	return 0;
}