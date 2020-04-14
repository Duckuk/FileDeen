#pragma once

namespace FileDeen {
	class Config {
	public:
		Config( std::filesystem::path );

		void read();

		bool useRealNames() const { return _bUseRealNames; };
		bool verboseLogging() const { return _bVerboseLogging; };
	private:
		void write();
		bool _bUseRealNames, _bVerboseLogging;
		std::filesystem::path _filePath;
	};
}