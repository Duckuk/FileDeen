#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include "filedeen.h"
using namespace FileDeen;

unsigned short FileDeen::CBCEncrypt( std::string& data, std::string key, std::vector<char> initVector ) {
	if ( key.length() == 0 ) {
		key = "DEFAULT";
	}
	//Generate potentially cryptographically insecure pseudorandom 512-bit key from given key
	std::seed_seq seed( key.begin(), key.end() );
	std::mt19937_64 rng( seed );
	std::uniform_int_distribution<short> dist( 0x00u, 0xFFu );
	std::vector<char> randKey( blockSize );
	for ( auto& c : randKey ) {
		c = dist( rng );
	}

	unsigned short paddingLength = data.length() % blockSize;
	data.append( paddingLength, rng() );
	std::string prevBlock( blockSize, 0x00 );
	for ( size_t blockStart = 0; blockStart < data.length(); blockStart += blockSize ) {
		std::string origPlain( data.substr( blockStart, blockSize ) );
		for ( size_t blockPos = 0; blockPos < blockSize; blockPos++ ) {
			if ( blockStart + blockPos >= data.length() ) {
				break;
			}
			data[blockStart+blockPos] = ((blockStart == 0 ? initVector[blockPos] : prevBlock[blockPos]) ^ data[blockStart+blockPos]) ^ randKey[blockPos];
			prevBlock[blockPos] = data[blockStart+blockPos] ^ origPlain[blockPos];
		}
	}
	return paddingLength;
}

void FileDeen::CBCDecrypt( std::string& data, std::string key, std::vector<char> initVector ) {
	if ( key.length() == 0 ) {
		key = "DEFAULT";
	}
	//Generate potentially cryptographically insecure pseudorandom 512-bit key from given key
	std::seed_seq seed( key.begin(), key.end() );
	std::mt19937_64 rng( seed );
	std::uniform_int_distribution<short> dist( 0x00u, 0xFFu );
	std::vector<char> randKey( blockSize );
	for ( auto& c : randKey ) {
		c = dist( rng );
	}

	std::string prevBlock( blockSize, 0x00 );
	for ( size_t blockStart = 0; blockStart < data.length(); blockStart += blockSize ) {
		std::string origCiph( data.substr( blockStart, blockSize ) );
		for ( size_t blockPos = 0; blockPos < blockSize; blockPos++ ) {
			if ( blockStart + blockPos >= data.length() ) {
				break;
			}
			data[blockStart+blockPos] = (data[blockStart+blockPos] ^ randKey[blockPos]) ^ (blockStart == 0 ? initVector[blockPos] : prevBlock[blockPos]);
			prevBlock[blockPos] = data[blockStart+blockPos] ^ origCiph[blockPos];
		}
	}
}



FeD_Entry::FeD_Entry( std::seed_seq initVectorSeed ) : _initVector( blockSize ) {
	_index = NULL;
	_pathLength = pathMaxSize;
	_pathPadLength = 0;
	_path.resize( pathMaxSize/2 );
	_dataLength = 0;
	_dataPadLength = 0;
	_checksum = NULL;
	std::mt19937_64 rng( initVectorSeed );
	std::uniform_int_distribution<short> dist( 0x00u, 0xFFu );
	for ( auto& c : _initVector ) {
		c = dist( rng );
	}
}

void FeD_Entry::setIndex( char* c, size_t length ) {
	memcpy( &_index, c, length );
}

void FeD_Entry::setIndex( unsigned int i ) {
	_index = i;
}

//Size of given vector should equal blockSize
void FeD_Entry::setInitVector( std::vector<char> v ) {
	_initVector = v;
}

void FeD_Entry::regenerateInitVector( std::seed_seq seed ) {
	std::mt19937_64 rng( seed );
	std::uniform_int_distribution<short> dist( 0x00u, 0xFFu );
	for ( auto& c : _initVector ) {
		c = dist( rng );
	}
}

void FeD_Entry::setPath( char* c, size_t length ) {
	memcpy( &_path[0], c, length );
	_pathLength = length;
}

void FeD_Entry::setPath( std::wstring s ) {
	_path = s;
	_pathLength = s.length()*2;
}

void FeD_Entry::setPathPadLength( unsigned short i ) {
	_pathPadLength = i;
}

void FeD_Entry::setData( char* c, size_t length ) {
	_data.resize( length );
	_dataLength = length;
	memcpy( &_data[0], c, length );
}

void FeD_Entry::setDataPadLength( unsigned short i ) {
	_dataPadLength = i;
}

//Uses move semantics to account for larger file sizes
void FeD_Entry::moveData( std::string& s ) {
	_data = std::move( s );
	_dataLength = _data.size();
}

void FeD_Entry::writeToFile( std::filesystem::path filePath ) {
	std::fstream outputFile( filePath, std::ios::out | std::ios::binary | std::ios::trunc );
	outputFile.write( &_data[0], _dataLength );
	outputFile.close();
}

//'Ez write' function
void FeD_Entry::writeToFile( std::filesystem::path rootFolder, bool useRealNames, bool verboseLogging ) {
	std::filesystem::path outputFileName;
	if ( useRealNames ) {
		outputFileName = rootFolder.wstring() + L"\\" + _path;
	}
	else {
		outputFileName = rootFolder.wstring() + L"\\" + this->path().parent_path().wstring() + L"\\" + std::to_wstring( _index ) + this->path().extension().wstring();
	}
	std::filesystem::create_directories( outputFileName.parent_path() );
	if ( verboseLogging ) wprintf( L"%.3u: Writing to \'%ls\'...", _index, outputFileName.c_str() );
	std::fstream outputFile( outputFileName, std::ios::out | std::ios::binary | std::ios::trunc );
	outputFile.write( &_data[0], _dataLength );
	outputFile.close();
	if ( verboseLogging ) printf( "Done!\n" );
}
FeD_Entry& FeD::entry( int index ) {
	return _entries.at( index );
}



FeD::FeD() : _entries(), _signature( signSize, 0x00 ) {
	memset( _omittedBytes, true, sizeof( _omittedBytes ) );
}


void FeD::setSignature( char* cSign, size_t length ) {
	memcpy( &_signature[0], cSign, length );
};

void FeD::addEntry( FeD_Entry e ) {
	_entries.push_back( e );
}

//Uses move semantics to account for larger file sizes
void FeD::moveEntry( FeD_Entry& e ) {
	_entries.push_back( std::move( e ) );
}

//Delete entry at index
void FeD::delEntry( int index ) {
	_entries.erase( _entries.begin()+index );
}

//'Ez read' function
int FeD::readFromFile( std::filesystem::path fileName, std::string key, bool verboseLogging ) {
	std::fstream inputFile( fileName, std::ios::in | std::ios::binary );
	std::string buffer;

	//Check signature
	if ( verboseLogging ) printf( "Checking signature..." );
	buffer.resize( signSize );
	inputFile.read( &buffer[0], signSize );
	if ( memcmp( &buffer[0], SIGN, signSize ) != NULL ) {
		printf( "Error: File is not a FeD file\n" );
		return 1;
	}
	_signature = buffer;
	if ( verboseLogging ) printf( "Done!\n" );

	//Check version
	if ( verboseLogging ) printf( "Checking version..." );
	unsigned char versionByte = inputFile.peek();
	if ( versionByte != _versionByte ) {
		short nextTwoBytes;
		inputFile.read( (char*)&nextTwoBytes, sizeof( nextTwoBytes ) );
		inputFile.seekg( -(signed)sizeof( nextTwoBytes ), std::ios::cur );
		if ( nextTwoBytes % 2 == 0 && nextTwoBytes <= 512 ) {
			printf( "Error:	FeD file was encoded before version checking was added.\nComplete decoding is not guaranteed\n" );
		}
		else if ( versionByte < _versionByte ) {
			printf( "Error: FeD file was encoded with past encoding scheme \'v%u\', whereas the current decoding scheme is \'v%u\'.\nComplete decoding is not guaranteed\n", versionByte, _versionByte );
			inputFile.seekg( 1, std::ios::cur );
		}
		else if ( versionByte > _versionByte ) {
			printf( "Error: FeD file was encoded with future encoding scheme \'v%u\', whereas the current decoding scheme is \'v%u\'.\nComplete decoding is not guaranteed.\n", versionByte, _versionByte );
			inputFile.seekg( 1, std::ios::cur );
		}
		printf( "Do you wish to continue? (y/n): " );
		if ( getchar() == 'n' ) {
			std::cin.ignore();
			return 1;
		}
		else {
			std::cin.ignore();
		}
	}
	else {
		if ( verboseLogging ) printf( "Done!: \'v%u\'\n", versionByte );
		inputFile.seekg( 1, std::ios::cur );
	}

	while ( true ) {
		FileDeen::FeD_Entry entry( { (unsigned)time( NULL ) } );

		//Read index
		if ( verboseLogging ) printf( "Reading index..." );
		buffer.resize( sizeof( entry.index() ) );
		inputFile.read( &buffer[0], buffer.size() );
		entry.setIndex( &buffer[0], buffer.size() );

		if ( entry.index() == 0xFFFFFFFF ) {
			if ( verboseLogging ) printf( "End of file found\n" );
			inputFile.close();
			break;
		}
		else if ( verboseLogging ) {
			printf( "Done!: %.3u\n", entry.index() );
		}

		//Read initialization vector
		if ( verboseLogging ) printf( "Reading initialization vector..." );
		std::vector<char> initVector( blockSize );
		inputFile.read( &initVector[0], initVector.size() );
		entry.setInitVector( initVector );
		if ( verboseLogging ) printf( "Done!\n" );

		//Read path size
		if ( verboseLogging ) printf( "%.3u: Reading path length...", entry.index() );
		unsigned short pathLength;
		inputFile.read( (char*)&pathLength, sizeof( pathLength ) );
		if ( verboseLogging ) printf( "Done!: %hu\n", pathLength );

		//Read path padding size
		if ( verboseLogging ) printf( "%.3u: Reading path padding length...", entry.index() );
		unsigned short pathPadLength;
		inputFile.read( (char*)&pathPadLength, sizeof( pathPadLength ) );
		if ( verboseLogging ) printf( "Done!: %hu\n", pathPadLength );

		//Read path
		if ( verboseLogging ) printf( "%.3u: Reading path...", entry.index() );
		buffer.resize( pathLength );
		inputFile.read( &buffer[0], buffer.size() );
		CBCDecrypt( buffer, key, entry.initVector() );
		buffer.resize( buffer.size()-pathPadLength );
		entry.setPath( &buffer[0], buffer.size() );
		if ( verboseLogging ) printf( "Done!\n" );

		//Read data length
		if ( verboseLogging ) printf( "%.3u: Reading data length...", entry.index() );
		size_t dataLength;
		inputFile.read( (char*)&dataLength, sizeof( dataLength ) );
		if ( verboseLogging ) printf( "Done!: %zu\n", dataLength );

		//Read data padding size
		if ( verboseLogging ) printf( "%.3u: Reading data padding length...", entry.index() );
		unsigned short dataPadLength;
		inputFile.read( (char*)&dataPadLength, sizeof( dataPadLength ) );
		if ( verboseLogging ) printf( "Done!: %hu\n", dataPadLength );

		//Read data
		if ( verboseLogging ) printf( "%.3u: Reading data...", entry.index() );
		buffer.resize( dataLength );
		inputFile.read( &buffer[0], buffer.size() );
		CBCDecrypt( buffer, key, entry.initVector() );
		buffer.resize( buffer.size()-dataPadLength );
		entry.moveData( buffer );
		if ( verboseLogging ) printf( "Done!\n" );

		this->moveEntry( entry );
	}
	return 0;
}

//Write FeD class to file
void FeD::writeToFile( std::filesystem::path fileName ) {
	std::fstream outputFile( fileName, std::ios::out | std::ios::binary | std::ios::trunc );
	
	outputFile.write( &_signature[0], _signature.size() );  //Write signature
	outputFile.put( _versionByte );  //Put version byte
	for ( const auto& entry : _entries ) {  //Iterate through entry vector and write each in sequence
		outputFile.write( (const char*)&entry._index, sizeof( entry._index ) );
		outputFile.write( (const char*)&entry._initVector[0], blockSize );
		outputFile.write( (const char*)&entry._pathLength, sizeof( entry._pathLength ) );
		outputFile.write( (const char*)&entry._pathPadLength, sizeof( entry._pathPadLength ) );
		outputFile.write( (const char*)&entry._path[0], entry._pathLength );
		outputFile.write( (const char*)&entry._dataLength, sizeof( entry._dataLength ) );
		outputFile.write( (const char*)&entry._dataPadLength, sizeof( entry._dataPadLength ) );
		outputFile.write( (const char*)&entry._data[0], entry._dataLength );
	}
	char endOfFile[4];
	memset( endOfFile, 0xFF, sizeof( endOfFile ) );
	outputFile.write( endOfFile, sizeof( endOfFile ) );  //Write end of data byte sequence
	outputFile.close();
}