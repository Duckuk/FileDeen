#pragma once
#include <filesystem>
#include <map>
#include <string>

namespace FileDeen {
	class Config {
	public:
		Config( std::wstring );

		std::string getString( std::string );
		bool getBool( std::string );

		void read();
	private:
		void write();
		std::map<std::string, bool> _boolOptions;
		std::map<std::string, std::string> _stringOptions;
		std::filesystem::path _filePath;
	};
}