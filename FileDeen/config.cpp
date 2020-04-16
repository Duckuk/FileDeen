#include <filesystem>
#include <fstream>
#include <string>
#include "config.h"
using namespace FileDeen;

Config::Config( std::wstring fileName ) {
	_boolOptions = {
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

bool Config::getKey( std::string key ) {
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
		std::string subBuffer = buffer.substr( 0, buffer.find_first_of( '\x0' ) );

		if ( _boolOptions.find( subBuffer ) != _boolOptions.end() ) {
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
	for ( const auto& pair : _boolOptions ) {
		std::string line = pair.first + '=' + std::to_string( pair.second ) + '\n';
		configFile.write( line.c_str(), line.size() );
	}
	configFile.close();
}