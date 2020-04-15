#pragma once
#include <map>

namespace FileDeen {
	class Config {
	public:
		Config( std::filesystem::path );

		bool getKey( std::string );

		void read();
	private:
		void write();
		std::map<std::string,bool> _boolMap;
		std::filesystem::path _filePath;
	};
}