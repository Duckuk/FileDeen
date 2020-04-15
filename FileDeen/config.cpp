#include <filesystem>
#include <fstream>
#include <string>
#include "config.h"
using namespace FileDeen;

const std::string sUseRealNames = "bUseRealNames";
const std::string sVerboseLogging = "bVerboseLogging";

Config::Config( std::filesystem::path path ) {
	_boolMap = {
		{"bUseRealNames",false},
		{"bVerboseLogging",false}
	};
	_filePath = path;
	if ( !std::filesystem::exists( _filePath ) ) {
		this->write();
	}
	this->read();
}

bool Config::getKey( std::string key ) {
	if ( _boolMap.find( key ) != _boolMap.end() ) {
		return _boolMap[key];
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
		std::string subBuffer = buffer.substr( 0, buffer.find_first_of( '\x0' ) );

		if ( _boolMap.find( subBuffer ) != _boolMap.end() ) {
			char cBuffer = configFile.get();
			switch ( cBuffer ) {
				case '0':
					_boolMap[subBuffer] = false;
					break;
				case '1':
					_boolMap[subBuffer] = true;
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
	for ( const auto& pair : _boolMap ) {
		std::string line = pair.first + '=' + std::to_string( pair.second ) + '\n';
		configFile.write( line.c_str(), line.size() );
	}
	configFile.close();
}