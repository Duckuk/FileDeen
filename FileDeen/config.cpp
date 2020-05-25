#include <fstream>
#include "config.h"
using namespace FileDeen;

Config::Config( std::wstring fileName ) {
	_stringOptions = {
		{"sKey",""}
	};
	_boolOptions = {
		{"bKeyEnabled",false},
		{"bKeyUncensored",false},
		{"bOnlyIncludeFolderContents",false},
		{"bUseRealNames",false},
		{"bVerboseLogging",false}
	};
	wchar_t* buffer;
	_get_wpgmptr( &buffer );
	_filePath = std::filesystem::path( buffer ).parent_path().wstring() + L"\\" + fileName;
	if ( !std::filesystem::exists( _filePath ) ) {
		this->write();
	}
	this->read();
}

std::string Config::getString( std::string key ) {
	if ( _stringOptions.find( key ) != _stringOptions.end() ) {
		return _stringOptions[key];
	}
	else {
		return NULL;
	}
}

bool Config::getBool( std::string key ) {
	if ( _boolOptions.find( key ) != _boolOptions.end() ) {
		return _boolOptions[key];
	}
	else {
		return NULL;
	}
}

void Config::read() {
	std::string buffer( 256, 0x00 );
	std::fstream configFile( _filePath, std::ios::in );
	while ( configFile ) {
		configFile.getline( &buffer[0], buffer.size(), '=' );
		std::string subBuffer = buffer.c_str();

		if ( _stringOptions.find( subBuffer ) != _stringOptions.end() ) {
			configFile.ignore( std::numeric_limits<std::streamsize>::max(), '"' );
			configFile.getline( &buffer[0], buffer.size(), '"' );
			_stringOptions[subBuffer] = buffer.c_str();
		}
		else if ( _boolOptions.find( subBuffer ) != _boolOptions.end() ) {
			char cBuffer = configFile.get();
			switch ( cBuffer ) {
				case '0':
					_boolOptions[subBuffer] = false;
					break;
				case '1':
					_boolOptions[subBuffer] = true;
					break;
				default:
					printf( "Invalid value for %s: \'%c\'\n", buffer.c_str(), cBuffer );
					break;
			}
		}
		else {
			if ( strlen( &buffer[0] ) > 0 ) {
				printf( "Invalid key: \'%s\'\n", buffer.c_str() );
			}
		}
		configFile.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
	}
	configFile.close();
}

void Config::write() {
	std::fstream configFile( _filePath, std::ios::out | std::ios::trunc );
	for ( const auto& pair : _stringOptions ) {
		std::string line = pair.first + "=\"" + pair.second + "\"\n";
		configFile.write( line.c_str(), line.size() );
	}
	for ( const auto& pair : _boolOptions ) {
		std::string line = pair.first + '=' + std::to_string( pair.second ) + '\n';
		configFile.write( line.c_str(), line.size() );
	}
	configFile.close();
}