#pragma once

namespace FileDeen {
	
	const int signSize = 8,
		versionSize = 1,
		dictLengthSize = sizeof( short ),
		maxDictSize = 512,
		maxMetadataSize = signSize+versionSize+dictLengthSize+maxDictSize;

	const int indexSize = sizeof( int ),
		dataLengthSize = sizeof( size_t ),
		pathSize = 256*sizeof( wchar_t ),
		checksumSize = sizeof( unsigned int ),
		entryMetadataSize = indexSize+pathSize+dataLengthSize+checksumSize;

	class FeD_Entry {
	public:
		FeD_Entry();

		void setIndex( char*, size_t );
		void setIndex( unsigned int );
		unsigned int index() const { return _index; };

		void translatePath( std::string );
		void setPath( char*, size_t );
		void setPath( std::wstring );
		std::filesystem::path path() const { return _path; };

		size_t dataLength() const { return _dataLength; };

		void translateData( std::string );
		void setData( char*, size_t );
		void moveData( std::string& );
		std::string data() const { return _data; };

		unsigned int calculateChecksum();
		void setChecksum( char*, size_t );
		void setChecksum( unsigned int );
		unsigned int checksum() const { return _checksum; };

		void writeToFile( std::filesystem::path );

	private:
		friend class FeD;
		unsigned int _index, _checksum;
		std::wstring _path;
		size_t _dataLength;
		std::string _data;
	};

	class FeD {
	public:
		FeD();

		void setSignature(char*, size_t);
		std::string signature() const { return _signature; };

		const unsigned char version() const { return _versionByte; };

		void cleanDictionary();
		void generateDictionary();
		void randomizeDictionary();
		void setDictionary( char*, size_t );
		std::string dictionary() const { return _dictionary; };
		
		void calculateDictSize();
		short dictSize() const { return _dictionarySize; };

		void addEntry( FeD_Entry );
		void moveEntry( FeD_Entry& );
		void delEntry( int );
		void translateEntries( std::string );
		FeD_Entry& entry( int );
		std::vector<FeD_Entry> entries() const { return _entries; };
		size_t numEntries() const { return _entries.size(); };

		void writeToFile( std::filesystem::path );

	private:
		const unsigned char _versionByte = 0x04;
		std::string _signature, _dictionary;
		short _dictionarySize;
		std::vector<FeD_Entry> _entries;
		bool _omittedBytes[256];
	};
}