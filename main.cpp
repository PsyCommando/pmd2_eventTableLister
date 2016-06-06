
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <iterator>
#include <string>
#include <cstdint>
#include <cassert>
#include <map>
using namespace std;

const uint32_t Overlay_0011LoadOffset = 0x022DC240;
const uint32_t Arm9BinLoadOffset      = 0x02000000;

template<typename _intty>
    string NumberToHexString( _intty val )
{
    stringstream sstr; 
    sstr <<hex <<"0x" <<uppercase <<val;
    return std::move(sstr.str());
}

/*
    A little tool to gather statistics on values
*/
template<class T>
    struct LimitVal
{
    typedef T val_t;
    val_t min;
    val_t avg;
    val_t max;
    int   cntavg; //Counts nb of value samples
    int   accavg; //Accumulate values
    map<val_t,size_t> distribution; 

    LimitVal()
        :min(0), avg(0), max(0), cntavg(0), accavg(0)
    {}

    void Process(val_t anotherval )
    {
        if( anotherval < min )
            min = anotherval;
        else if( anotherval > max )
            max = anotherval;

        ++cntavg;
        accavg += anotherval;
        avg = static_cast<val_t>(accavg / cntavg);

        
        auto result = distribution.try_emplace( anotherval, 1 );
        if( !result.second && (result.first != distribution.end()) )
        {
            (result.first->second) += 1;
        }
    }

    std::string Print()const
    {
        std::stringstream sstr;
        sstr <<"(" << static_cast<int>( min ) <<" to " <<static_cast<int>( max ) <<" ) Avg : " <<avg <<"\n\tDistribution with more than one match:\n";
        for( const auto distent : distribution )
        {
            if( distent.second > 1 )
                sstr<<"\t\tVal: " <<setw(8) <<setfill(' ') <<distent.first <<" : " <<setw(8) <<setfill(' ') <<distent.second <<" times\n";
        }
        return std::move( sstr.str() );
    }
};


/*********************************************************************************************
    ReadIntFromBytes
        Tool to read integer values from a byte vector!
        ** The iterator's passed as input, has its position changed !!
*********************************************************************************************/
template<class T, class _init> 
    inline T ReadIntFromBytes( _init & itin, _init itend, bool basLittleEndian = true )
{
    static_assert( std::numeric_limits<T>::is_integer, "ReadIntFromBytes() : Type T is not an integer!" );
    T out_val = 0;

    if( basLittleEndian )
    {
        unsigned int i = 0;
        for( ; (itin != itend) && (i < sizeof(T)); ++i, ++itin )
        {
            T tmp = (*itin);
            out_val |= ( tmp << (i * 8) ) & ( 0xFF << (i*8) );
        }

        if( i != sizeof(T) )
        {
#ifdef _DEBUG
            assert(false);
#endif
            throw std::runtime_error( "ReadIntFromBytes(): Not enough bytes to read from the source container!" );
        }
    }
    else
    {
        int i = (sizeof(T)-1);
        for( ; (itin != itend) && (i >= 0); --i, ++itin )
        {
            T tmp = (*itin);
            out_val |= ( tmp << (i * 8) ) & ( 0xFF << (i*8) );
        }

        if( i != -1 )
        {
#ifdef _DEBUG
            assert(false);
#endif
            throw std::runtime_error( "ReadIntFromBytes(): Not enough bytes to read from the source container!" );
        }
    }
    return out_val;
}

/*********************************************************************************************
    ReadIntFromBytes
        Tool to read integer values from a byte container!
            
        #NOTE :The iterator is passed by copy here !! And the incremented iterator is returned!
*********************************************************************************************/
template<class T, class _init> 
    inline _init ReadIntFromBytes( T & dest, _init itin, _init itend, bool basLittleEndian = true ) 
{
    dest = ReadIntFromBytes<typename T, typename _init>( itin, itend, basLittleEndian );
    return itin;
}


/************************************************************************************
    safestrlen
        Count the length of a string, and has a iterator check
        to ensure it won't loop into infinity if it can't find a 0.
************************************************************************************/
template<typename init_t>
    inline size_t safestrlen( init_t beg, init_t pastend )
{
    size_t cntchar = 0;
    for(; beg != pastend && (*beg) != 0; ++cntchar, ++beg );

    if( beg == pastend )
        throw runtime_error("String went past expected end!");

    return cntchar;
}

/************************************************************************************
    FetchString
        Fetchs a null terminated C-String from a file offset.
************************************************************************************/
template<typename _init>
    std::string FetchString( uint32_t fileoffset, _init itfbeg, _init itfend )
{
    auto    itstr = itfbeg;
    std::advance( itstr,  fileoffset );
    size_t  strlength = safestrlen(itstr, itfend);
    string  dest;
    dest.resize(strlength);

    for( size_t cntchar = 0; cntchar < strlength; ++cntchar, ++itstr )
        dest[cntchar] = (*itstr);

    return std::move(dest);
}


/************************************************************************************
    LoadFile
        Load a file into a byte vector for easier parsing.
************************************************************************************/
std::vector<uint8_t> LoadFile( const std::string & fpath )
{
    ifstream fi( fpath, ios::binary );
    if( fi.bad() || !(fi.is_open()) )
        throw std::runtime_error("Couldn't open file " + fpath);

    return std::move( std::vector<uint8_t>( istreambuf_iterator<char>(fi), istreambuf_iterator<char>()) );
}

//============================================================================================================

template<typename _structType, typename _init, typename _outstrm>
    void ParseAndDumpLUT( const uint32_t offset, const size_t nbentries, _init itbeg, _init itend, _outstrm & out, const string & headertext, const uint32_t ptrDiff )
{
    typename _structType::Stats statisticslog;
    auto                        itfbeg        = itbeg; //Save iterator before advancing it
    std::advance( itbeg, offset );

    out << "============================================================\n"
        << headertext <<"\n"
        << "============================================================\n"
        << "\n"
        << "Offset       ";
    _structType::PrintHeader(out);
    out << "\n--------------------------------------------------------------------------------------\n";

    for( size_t cntentries = 0; cntentries < nbentries; ++cntentries )
    {
        _structType curentry;
        itbeg = curentry.Read( itbeg, itend );
        out << "0x" <<setfill('0') <<setw(8) <<right <<uppercase <<hex <<((cntentries * _structType::Size) + offset) <<nouppercase <<" " <<dec;
        curentry.Print( out, itfbeg, itend, ptrDiff );
        statisticslog.LogStats(curentry);
    }
    out <<"\n"
        <<"Stats:\n"
        <<"------------\n"
        <<statisticslog.Print()
        <<"\n";
}




// ----------------------------------------------------------------------------------------
/*
    LevelEntry
        Single entry in the level list
*/
struct LevelEntry
{
    uint32_t ptrstring  = 0;
    int16_t  unk1       = 0;
    int16_t  unk2       = 0;
    int16_t  unk3       = 0;
    int16_t  unk4       = 0;

    static const size_t Size = 12;

    template<typename _init>
        _init Read( _init itbeg, _init itend )
    {
        itbeg = ReadIntFromBytes( ptrstring,    itbeg, itend );
        itbeg = ReadIntFromBytes( unk1,         itbeg, itend );
        itbeg = ReadIntFromBytes( unk2,         itbeg, itend );
        itbeg = ReadIntFromBytes( unk3,         itbeg, itend );
        itbeg = ReadIntFromBytes( unk4,         itbeg, itend );
        return itbeg;
    }

    template<typename _outstrm, typename _init >
        void Print( _outstrm & out, _init itfbeg, _init itfend, const uint32_t ptrDiff )
    {
        string fetchedstr = "NULL";
        if( ptrstring != 0 )
            fetchedstr = FetchString( ptrstring - ptrDiff, itfbeg, itfend );

        out << "-> " <<setfill(' ') <<setw(5) <<unk1
            << ", "  <<setfill(' ') <<setw(5) <<unk2
            << ", "  <<setfill(' ') <<setw(5) <<unk3         
            << ", "  <<setfill(' ') <<setw(5) <<unk4
            << ", \""  <<fetchedstr <<"\""
            <<"\n";
    }

    template<typename _outstrm>
        static void PrintHeader( _outstrm & out )
    {
        out <<"Unk1   "
            //"     5,"
            <<"unk2   "
            //"   342,"
            <<"SomeId "
            //"     0,"
            <<"Unk4   "
            //"   258,"
            <<"Symbol "
            ;
    }

    struct Stats
    {
        void LogStats( const LevelEntry & entry )
        {
            unk1stat.Process(entry.unk1);
            unk2stat.Process(entry.unk2);
            unk3stat.Process(entry.unk3);
            unk4stat.Process(entry.unk4);
        }

        string Print()
        {
            stringstream sstr;
            sstr<<"unk1   :" << unk1stat.Print() <<"\n"
                <<"unk2   :" << unk2stat.Print() <<"\n"
                <<"SomeId :" << unk3stat.Print() <<"\n"
                <<"unk4   :" << unk4stat.Print() <<"\n"
                ;
            return std::move(sstr.str());
        }

        LimitVal<decltype(LevelEntry::unk1)> unk1stat;
        LimitVal<decltype(LevelEntry::unk2)> unk2stat;
        LimitVal<decltype(LevelEntry::unk3)> unk3stat;
        LimitVal<decltype(LevelEntry::unk4)> unk4stat;
    };
};

template<typename _init, typename _outstrm>
    void DumpEventListEoS( _init itbeg, _init itend, _outstrm & out )
{
    //arm9
    //0x000A46EC -> Start of strings
    //0x000A5490 -> Start of LUT. 12 bytes entries. 1 pointer, 4 shorts.
    //0x000A68C4 -> One past end of LUT. 431 entries.
    static const size_t   NbEntries = 431;
    static const uint32_t LUTBeg    = 0xA5490;

    ParseAndDumpLUT<LevelEntry>( LUTBeg, NbEntries, itbeg, itend, out, "Event List Table", Arm9BinLoadOffset );
}


// ----------------------------------------------------------------------------------------
/*
    SpecListEntry
        Single entry in the level list
*/
struct SpecListEntry
{
    int16_t  id         = 0;
    int16_t  unk2       = 0;
    uint32_t ptrstring  = 0;

    static const size_t Size = 8;

    template<typename _init>
        _init Read( _init itbeg, _init itend )
    {
        itbeg = ReadIntFromBytes( id,           itbeg, itend );
        itbeg = ReadIntFromBytes( unk2,         itbeg, itend );
        itbeg = ReadIntFromBytes( ptrstring,    itbeg, itend );
        return itbeg;
    }

    template<typename _outstrm, typename _init >
        void Print( _outstrm & out, _init itfbeg, _init itfend, const uint32_t ptrDiff  )
    {
        string fetchedstr = "NULL";
        if( ptrstring != 0 )
            fetchedstr = FetchString( ptrstring - ptrDiff, itfbeg, itfend );

        out << "-> " <<setfill(' ') <<setw(5) <<id
            << ", "  <<setfill(' ') <<setw(5) <<unk2     
            << ", \""  <<fetchedstr <<"\""
            <<"\n";
    }

    template<typename _outstrm>
        static void PrintHeader( _outstrm & out )
    {
        out <<"Id     "
            //"     5,"
            <<"Unk2   "
            //"   342,"
            <<"Symbol "
            ;
    }

    struct Stats
    {
        void LogStats( const SpecListEntry & entry )
        {
            idstat.Process(entry.id);
            unk2stat.Process(entry.unk2);
        }

        string Print()
        {
            stringstream sstr;
            sstr<<"Id   :" << idstat.Print()   <<"\n"
                <<"unk2 :" << unk2stat.Print() <<"\n"
                ;
            return std::move(sstr.str());
        }

        LimitVal<decltype(SpecListEntry::id)>   idstat;
        LimitVal<decltype(SpecListEntry::unk2)> unk2stat;
    };
};



template<typename _init, typename _outstrm>
    void DumpSpecialListEoS( _init itbeg, _init itend, _outstrm & out )
{
    //overlay_0011
    //0x0003D8AC -> start strings
    //0x000405E8 -> Start LUT. 8 bytes entries, 2 shorts, one pointer.
    //0x00041BD0 -> One past end of LUT. 701 entries.
    static const size_t   NbEntries = 701;
    static const uint32_t LUTBeg    = 0x405E8;

    ParseAndDumpLUT<SpecListEntry>( LUTBeg, NbEntries, itbeg, itend, out, "Special List Table", Overlay_0011LoadOffset );
}

// ----------------------------------------------------------------------------------------
/*
    EventSubFileListEntry
        Single entry in the level list
*/
struct EventSubFileListEntry
{
    int16_t  unk1       = 0;
    int16_t  unk2       = 0;
    uint32_t ptrstring  = 0;
    uint32_t unk3       = 0;

    static const size_t Size = 12;

    template<typename _init>
        _init Read( _init itbeg, _init itend )
    {
        itbeg = ReadIntFromBytes( unk1,         itbeg, itend );
        itbeg = ReadIntFromBytes( unk2,         itbeg, itend );
        itbeg = ReadIntFromBytes( ptrstring,    itbeg, itend );
        itbeg = ReadIntFromBytes( unk3,         itbeg, itend );
        return itbeg;
    }

    template<typename _outstrm, typename _init >
        void Print( _outstrm & out, _init itfbeg, _init itfend, const uint32_t ptrDiff  )
    {
        string fetchedstr = "NULL";
        if( ptrstring != 0 )
            fetchedstr = FetchString( ptrstring - ptrDiff, itfbeg, itfend );

        out << "-> " <<setfill(' ') <<setw(5) <<unk1
            << ", "  <<setfill(' ') <<setw(5) <<unk2
            << ", "  <<setfill(' ') <<setw(8) <<unk3         
            << ", \""  <<fetchedstr <<"\""
            <<"\n";
    }

    template<typename _outstrm>
        static void PrintHeader( _outstrm & out )
    {
        out <<"Unk1   "
            //"     5,"
            <<"Unk2   "
            //"   342,"
            <<"Unk3      "
            //"        0,"
            <<"Symbol "
            ;
    }

    struct Stats
    {
        void LogStats( const EventSubFileListEntry & entry )
        {
            unk1stat.Process(entry.unk1);
            unk2stat.Process(entry.unk2);
            unk3stat.Process(entry.unk3);
        }

        string Print()
        {
            stringstream sstr;
            sstr<<"unk1 :" << unk1stat.Print() <<"\n"
                <<"unk2 :" << unk2stat.Print() <<"\n"
                <<"unk3 :" << unk3stat.Print() <<"\n"
                ;
            return std::move( sstr.str() );
        }

        LimitVal<decltype(EventSubFileListEntry::unk1)> unk1stat;
        LimitVal<decltype(EventSubFileListEntry::unk2)> unk2stat;
        LimitVal<decltype(EventSubFileListEntry::unk3)> unk3stat;
    };
};



template<typename _init, typename _outstrm>
    void DumpEventSubFileListEoS( _init itbeg, _init itend, _outstrm & out )
{
    //overlay_0011
    //0x00041C00 -> Start strings.
    //0x00042C14 -> Start LUT. 12 bytes entries. 2 shorts, 1 pointer, 1 int32
    //0x00044610 -> One past the end of LUT. 555 entries, some were null.
    static const size_t   NbEntries = 555;
    static const uint32_t LUTBeg    = 0x42C14;

    ParseAndDumpLUT<EventSubFileListEntry>( LUTBeg, NbEntries, itbeg, itend, out, "Event Sub File List Table", Overlay_0011LoadOffset );
}
// ----------------------------------------------------------------------------------------
/*
    EntitySymbolListEntry
        Single entry in the level list
*/
struct EntitySymbolListEntry
{
    int16_t  type       = 0;
    int16_t  entityid   = 0;
    uint32_t ptrstring  = 0;
    uint16_t unk3       = 0;
    uint16_t unk4       = 0;

    static const size_t Size = 12;

    template<typename _init>
        _init Read( _init itbeg, _init itend )
    {
        itbeg = ReadIntFromBytes( type,         itbeg, itend );
        itbeg = ReadIntFromBytes( entityid,     itbeg, itend );
        itbeg = ReadIntFromBytes( ptrstring,    itbeg, itend );
        itbeg = ReadIntFromBytes( unk3,         itbeg, itend );
        itbeg = ReadIntFromBytes( unk4,         itbeg, itend );
        return itbeg;
    }

    template<typename _outstrm, typename _init >
        void Print( _outstrm & out, _init itfbeg, _init itfend, const uint32_t ptrDiff  )
    {
        string fetchedstr = "NULL";
        if( ptrstring != 0 )
            fetchedstr = FetchString( ptrstring - ptrDiff, itfbeg, itfend );

        out << "-> " <<setfill(' ') <<setw(5) <<type
            << ", "  <<setfill(' ') <<setw(9) <<entityid
            <<hex <<uppercase
            << ", 0x"  <<setfill('0') <<setw(4) <<(unk3)
            << ", 0x"  <<setfill('0') <<setw(4) <<(unk4)
            <<dec <<nouppercase
            << ", \""  <<fetchedstr <<"\""
            <<"\n";
    }

    template<typename _outstrm>
        static void PrintHeader( _outstrm & out )
    {
        out <<"Type   "
            //"     5,"
            <<"Entity Id "
            //"   342,"
            <<"Unk3   "
            //"     0,"
            <<"Unk4   "
            //"   258,"
            <<"Symbol "
            ;
    }

    // ----------------------------------
    struct Stats
    {
        void LogStats( const EntitySymbolListEntry & entry )
        {
            typestat    .Process(entry.type);
            entityidstat.Process(entry.entityid);
            unk3stat    .Process(entry.unk3);
            unk4stat    .Process(entry.unk4);
        }

        string Print()
        {
            stringstream sstr;
            sstr<<"Type      :" << typestat.Print()     <<"\n"
                <<"Entity ID :" << entityidstat.Print() <<"\n"
                <<"unk3      :" << unk3stat.Print()     <<"\n"
                <<"unk4      :" << unk4stat.Print()     <<"\n"
                ;
            return std::move(sstr.str());
        }

        LimitVal<decltype(EntitySymbolListEntry::type)>     typestat;
        LimitVal<decltype(EntitySymbolListEntry::entityid)> entityidstat;
        LimitVal<decltype(EntitySymbolListEntry::unk3)>     unk3stat;
        LimitVal<decltype(EntitySymbolListEntry::unk4)>     unk4stat;
    };
};



template<typename _init, typename _outstrm>
    void DumpEntitySymbolsEoS( _init itbeg, _init itend, _outstrm & out )
{
    //arm9
    //0x000A6910 -> Start Strings
    //0x000A7FF0 -> Start of LUT. 12 bytes entries. 2 shorts, 1 pointer, 2 shorts.
    //0x000A9208 -> One past end of LUT. 386 entries.
    static const size_t   NbEntries = 386;
    static const uint32_t LUTBeg    = 0xA7FF0;

    ParseAndDumpLUT<EntitySymbolListEntry>( LUTBeg, NbEntries, itbeg, itend, out, "Entity Symbol List Table", Arm9BinLoadOffset );
}



// ----------------------------------------------------------------------------------------
void DumpArm9Stuff( const string & arm9path, const string & targetdir )
{
    vector<uint8_t> fdat( LoadFile(arm9path) );
    auto            itbeg = fdat.begin();
    auto            itend = fdat.end();
    ofstream        out( targetdir + "/" + "arm9.txt" );

    DumpEntitySymbolsEoS( itbeg, itend, out );
    DumpEventListEoS    ( itbeg, itend, out );
}


void DumpOverlay0011Stuff( const string & overlay11path, const string & targetdir )
{
    vector<uint8_t> fdat( LoadFile(overlay11path) );
    auto            itbeg = fdat.begin();
    auto            itend = fdat.end();
    ofstream        out( targetdir + "/" + "overlay_0011.txt" );

    DumpEventSubFileListEoS( itbeg, itend, out );
    DumpSpecialListEoS     ( itbeg, itend, out );
}


//=============================================================================================================

int main( int argc, const char * argv[] )
{
    system("mkdir \"Dumped\"");
    cout <<"Dumping arm9.bin constants..\n";
    DumpArm9Stuff       ( "arm9.bin",         "Dumped" );
    cout <<"Dumping overlay_0011.bin constants..\n";
    DumpOverlay0011Stuff( "overlay_0011.bin", "Dumped" );
    cout <<"Done!\n";
    return 0;
}