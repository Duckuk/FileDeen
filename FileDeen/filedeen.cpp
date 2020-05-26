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
	for ( size_t x = 0; x < data.length(); x += blockSize ) {
		std::string origPlain( data.substr( x, blockSize ) );
		for ( size_t y = 0; y < blockSize; y++ ) {
			if ( x + y >= data.length() ) {
				break;
			}
			data[x+y] = ((x == 0 ? initVector[y] : prevBlock[y]) ^ data[x+y]) ^ randKey[y];
			prevBlock[y] = data[x+y] ^ origPlain[y];
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

	{
		std::string prevBlock( blockSize, 0x00 );
		for ( size_t x = 0; x < data.length(); x += blockSize ) {
			std::string origCiph( data.substr( x, blockSize ) );
			for ( size_t y = 0; y < blockSize; y++ ) {
				if ( x + y >= data.length() ) {
					break;
				}
				data[x+y] = (data[x+y] ^ randKey[y]) ^ (x == 0 ? initVector[y] : prevBlock[y]);
				prevBlock[y] = data[x+y] ^ origCiph[y];
			}
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