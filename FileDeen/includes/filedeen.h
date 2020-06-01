#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace FileDeen {
	
	const int blockSize = 64;

	const unsigned char SIGN[8] = { 0x53, 0x30, 0x53, 0x30, 0x72, 0x7F, 0x0D, 0x54 };
	//                               83    48    83    48   114   127    13    84

	const int signSize = sizeof( SIGN ),
		versionSize = 1,
		metadataSize = signSize+versionSize;

	const int indexSize = sizeof( int ),
		initVectorSize = blockSize,
		dataLengthSize = sizeof( size_t ),
		paddingLengthSize = sizeof( short )*2,
		pathMaxSize = 256*sizeof( wchar_t ),
		checksumSize = sizeof( unsigned int ),
		entryMaxMetadataSize = indexSize+initVectorSize+pathMaxSize+dataLengthSize+checksumSize;


	unsigned short CBCEncrypt( std::string& data, std::string key, std::vector<char> initVector );
	void CBCDecrypt( std::string& data, std::string key, std::vector<char> initVector);

	class FeD_Entry {
	public:
		FeD_Entry( std::seed_seq initVectorSeed );

		void setIndex( char* data, size_t length);
		void setIndex( unsigned int i );
		unsigned int index() const { return _index; };

		const std::vector<char> initVector() const { return _initVector; };
		void setInitVector( std::vector<char> initVector );
		void regenerateInitVector( std::seed_seq seed );

		void setPath( char* data, size_t length );
		void setPath( std::wstring path );
		void setPathPadLength( unsigned short length );
		std::filesystem::path path() const { return _path; };

		size_t dataLength() const { return _dataLength; };

		void setData( char* data, size_t length );
		void setDataPadLength( unsigned short length );
		void moveData( std::string& data );
		std::string data() const { return _data; };

		void writeDataToFile( std::filesystem::path filePath );
		void writeToFile( std::filesystem::path rootFolder, bool useRealNames, bool verboseLogging );

	private:
		friend class FeD;
		unsigned int _index, _checksum;
		std::vector<char> _initVector;
		unsigned short _pathLength, _pathPadLength;
		std::wstring _path;
		size_t _dataLength;
		unsigned short _dataPadLength;
		std::string _data;
	};

	class FeD {
	public:
		FeD();

		void setSignature(char* data, size_t length );
		std::string signature() const { return _signature; };

		const unsigned char version() const { return _versionByte; };

		void addEntry( FeD_Entry entry );
		void moveEntry( FeD_Entry& entry );
		void delEntry( int index );
		FeD_Entry& entry( int index );
		std::vector<FeD_Entry> entries() const { return _entries; };
		size_t numEntries() const { return _entries.size(); };

		int readFromFile( std::filesystem::path path, std::string key, bool verboseLogging );
		void writeToFile( std::filesystem::path pathToFile );

	private:
		const unsigned char _versionByte = 0x05;
		std::string _signature;
		std::vector<FeD_Entry> _entries;
		bool _omittedBytes[256];
	};
}