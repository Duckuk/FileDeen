#pragma once

namespace FileDeen {
	
	class FED_Entry {
	public:
		FED_Entry();

		void setIndex( char*, size_t );
		void setIndex( unsigned int );
		unsigned int index() const { return _index; };

		size_t dataLength() const { return _dataLength; };

		void translateExtension( std::string );
		void setFileExtension( char*, size_t );
		void setFileExtension( std::wstring );
		std::wstring fileExtension() const { return _fileExtension; };

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
		friend class FED;
		unsigned int _index, _checksum;
		size_t _dataLength;
		std::wstring _fileExtension;
		std::string _data;
	};

	class FED {
	public:
		FED();

		void setSignature(char*, size_t);
		std::string signature() const { return _signature; };

		void cleanDictionary();
		void generateDictionary();
		void randomizeDictionary();
		void setDictionary( char*, size_t );
		std::string dictionary() const { return _dictionary; };
		
		void calculateDictSize();
		short dictSize() const { return _dictionarySize; };

		void addEntry( FED_Entry );
		void moveEntry( FED_Entry& );
		void delEntry( int );
		void translateEntries( std::string );
		FED_Entry& entry( int );
		std::vector<FED_Entry> entries() const { return _entries; };
		int numEntries() const { return _entries.size(); };
		
		void writeToFile( std::filesystem::path );

	private:
		std::vector<FED_Entry> _entries;
		std::string _signature, _dictionary;
		short _dictionarySize;
		bool _omittedBytes[256];
	};
}