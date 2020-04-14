#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include "filedeen.h"
#include "CRC.h"
using namespace FileDeen;

FeD_Entry::FeD_Entry() {
	_index = NULL;
	_path.resize( pathSize/2 );
	_dataLength = 0;
	_checksum = NULL;
}

void FeD_Entry::setIndex( char* c, size_t length ) {
	memcpy( &_index, c, length );
}

void FeD_Entry::setIndex( unsigned int i ) {
	_index = i;
}

void FeD_Entry::translatePath( std::string dict ) {
	for ( size_t i = 0; i<_path.size(); i++ ) {
		_path[i] = dict[(unsigned char)_path[i]];
	}
}

void FeD_Entry::setPath( char* c, size_t length ) {
	memcpy( &_path[0], c, length );
}

void FeD_Entry::setPath( std::wstring s ) {
	if ( s.size() != pathSize/2 ) {
		s.resize( pathSize/2 );
	}
	_path = s;
}

void FeD_Entry::translateData( std::string dict ) {
	for ( size_t i = 0; i<_data.size(); i++ ) {
		_data[i] = dict[(unsigned char)_data[i]];
	}
}

void FeD_Entry::setData( char* c, size_t length ) {
	_data.resize( length );
	_dataLength = length;
	memcpy( &_data[0], c, length );
}

//Uses move semantics to account for larger file sizes
void FeD_Entry::moveData( std::string& s ) {
	_data = std::move( s );
	_dataLength = _data.size();
}

unsigned int FeD_Entry::calculateChecksum() {
	std::string checksumData;
	checksumData.append( (const char*)&_index, sizeof( _index ) );
	checksumData.append( (const char*)&_path[0], _path.size()*sizeof( wchar_t ) );
	checksumData.append( (const char*)&_dataLength, sizeof( _dataLength ) );
	unsigned int checksum = CRC::Calculate( &checksumData[0], checksumData.size(), CRC::CRC_32() );
	checksum = CRC::Calculate( &_data[0], _data.size(), CRC::CRC_32(), checksum );
	return checksum;
}

void FeD_Entry::setChecksum( char* c, size_t length ) {
	memcpy( &_checksum, c, length );
}

void FeD_Entry::setChecksum( unsigned int i ) {
	_checksum = i;
}

void FeD_Entry::writeToFile( std::filesystem::path filePath ) {
	std::fstream outputFile( filePath, std::ios::out | std::ios::binary | std::ios::trunc );
	outputFile.write( &_data[0], _dataLength );
	outputFile.close();
}

FeD_Entry& FeD::entry( int index ) {
	return _entries.at( index );
}



FeD::FeD() : _entries(), _signature( signSize, 0x00 ), _dictionary( maxDictSize/2, 0x00 ) {
	memset( _omittedBytes, true, sizeof( _omittedBytes ) );
	_dictionarySize = _dictionary.size()*2;
}

void FeD::setSignature( char* cSign, size_t length ) {
	memcpy( &_signature[0], cSign, length );
};

void FeD::cleanDictionary() {
	for ( auto& entry : _entries ) {
		std::string path;
		path.resize( entry._path.size()*sizeof( wchar_t ) );
		memcpy( &path[0], &entry._path[0], path.size() );
		for ( int i = 0; i<sizeof( _omittedBytes ); i++ ) {
			if ( memchr( &entry._data[0], i, entry._dataLength ) != NULL ) {
				_omittedBytes[i] = false;
			}
			else if ( memchr( &path[0], i, path.size() ) != NULL ) {
				_omittedBytes[i] = false;
			}
		}
	}
}

void FeD::generateDictionary() {
	for ( int i = 0; i<_dictionary.size(); i++ ) {
		_dictionary[i] = i;
	}
}

void FeD::randomizeDictionary() {
	std::mt19937_64 rng;
	rng.seed( (unsigned)time( NULL ) );
	std::shuffle( _dictionary.begin(), _dictionary.end(), rng );
}

void FeD::setDictionary( char* cDict, size_t length ) {
	memcpy( &_dictionary[0], cDict, length );
}


void FeD::calculateDictSize() {
	for ( int i = 0; i<sizeof( _omittedBytes ); i++ ) {
		if ( _omittedBytes[i] ) {
			_dictionarySize--; _dictionarySize--;
		}
	}
}

void FeD::addEntry( FeD_Entry e ) {
	_entries.push_back( e );
}

//Uses move semantics to account for larger file sizes
void FeD::moveEntry( FeD_Entry& e ) {
	_entries.push_back( std::move( e ) );
}

void FeD::delEntry( int index ) {
	_entries.erase( _entries.begin()+index );
}

void FeD::translateEntries( std::string dictionary ) {
	for ( auto& entry : _entries ) {
		entry.translatePath( dictionary );
		entry.translateData( dictionary );
	}
}

void FeD::writeToFile( std::filesystem::path fileName ) {
	std::fstream outputFile( fileName, std::ios::out | std::ios::binary | std::ios::trunc );

	outputFile.write( &_signature[0], _signature.size() );
	outputFile.put( _versionByte );
	outputFile.write( (const char*)&_dictionarySize, sizeof( _dictionarySize ) );
	for ( int i = 0; i<_dictionary.size(); i++ ) {
		if ( !_omittedBytes[i] ) {
			outputFile.put( _dictionary[i] );
			outputFile.put( i );
		}
	}
	for ( const auto& entry : _entries ) {
		outputFile.write( (const char*)&entry._index, sizeof( entry._index ) );
		outputFile.write( (const char*)&entry._path[0], entry._path.size()*2 );
		outputFile.write( (const char*)&entry._dataLength, sizeof( entry._dataLength ) );
		outputFile.write( (const char*)&entry._data[0], entry._dataLength );
		outputFile.write( (const char*)&entry._checksum, sizeof( entry._checksum ) );
	}
	char endOfFile[4];
	memset( endOfFile, 0xFF, sizeof( endOfFile ) );
	outputFile.write( endOfFile, sizeof( endOfFile ) );
	outputFile.close();
}