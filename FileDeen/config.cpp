#include <filesystem>
#include <fstream>
#include "config.h"
using namespace FileDeen;

const std::string sUseRealNames = "bUseRealNames";
const std::string sVerboseLogging = "bVerboseLogging";

Config::Config( std::filesystem::path path ) {
	_bUseRealNames = false;
	_bVerboseLogging = false;
	_filePath = path;
	if ( !std::filesystem::exists( _filePath ) ) {
		this->write();
	}
	this->read();
}

void Config::read() {
	char cBuffer;
	std::string buffer( 256, 0x00 );
	std::fstream configFile( _filePath, std::ios::in );
	while ( configFile ) {
		configFile.getline( &buffer[0], buffer.size(), '=' );

		if ( !memcmp( &buffer[0], &sUseRealNames[0], sUseRealNames.size() ) ) {
			cBuffer = configFile.get();
			if ( cBuffer == '0' ) {
				_bUseRealNames = false;
			}
			else if ( cBuffer == '1' ) {
				_bUseRealNames = true;
			}
			else {
				printf( "Invalid value for %s: %c\n", buffer.c_str(), cBuffer );
			}
		}
		else if ( !memcmp( &buffer[0], &sVerboseLogging[0], sVerboseLogging.size() ) ) {
			cBuffer = configFile.get();
			if ( cBuffer == '0' ) {
				_bVerboseLogging = false;
			}
			else if ( cBuffer == '1' ) {
				_bVerboseLogging = true;
			}
			else {
				printf( "Invalid value for %s: %c\n", buffer.c_str(), cBuffer );
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

	configFile.write( sUseRealNames.c_str(), sUseRealNames.size() );
	configFile.write( "=0\n", 3 );

	configFile.write( sVerboseLogging.c_str(), sVerboseLogging.size() );
	configFile.write( "=0\n", 3 );

	configFile.close();
}