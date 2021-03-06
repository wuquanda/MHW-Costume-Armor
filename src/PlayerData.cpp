#include <sstream>
#include "PlayerData.hpp"

/// Begin PlayerData Class Member Functions

PlayerData::PlayerData(byte *CharDataBuffer, bool gender)
{
    for (int i=0;i<5;++i)
        this->_ArmorData[i] = CharDataBuffer[i*4 ];
    this->_empty = false;
}

void PlayerData::setArmorPiece(int num, int value)
{
    if (num > 4 || num < 0)
        return;

    if (value >= _byteLimit || value <= 0 )
        value = _byteLimit;

    this->_ArmorData[num] = value;
}
u_int PlayerData::getArmorPiece(int num) const
{
    if (num > 4 || num < 0)
        num = Armor::HEAD;
    u_int value = this->_ArmorData[num];

    if (value >= _byteLimit)
        value = std::numeric_limits<u_int>::max();
    return value;
}

std::string PlayerData::Print() const
{
    std::stringstream Base;
    Base << "Character Data Info : " << std::endl;
    Base << "Gender : " << (this->_Gender ? "Female" : "Male") << std::endl;
    for(int i=0;i<5;++i)
    {
        Base << std::to_string(i) << ". ";
        Base << Armor::Names[i] <<"\t";
        Base << (this->_ArmorData[i] ==255 ? "None" : std::to_string(this->_ArmorData[i]));
        Base << std::endl;
    }
    return Base.str();
}

std::array<std::string,5> PlayerData::getDataString() const
{
    std::array<std::string,5> Result;
    for(int i=0;i<5;++i)
        Result[i] = (this->_ArmorData[i] == 255 ? "" : std::to_string(this->_ArmorData[i]) );
    return Result;
}

std::ostream &operator<<(std::ostream &out, PlayerData &Play)
{
    out<< Play.Print();
    return out;
}

/// Begin Misc Functions

/// Until now the byte Array to search changes on each MHW Patch.
/// Hopefully we may find a pattern.

// Observed Patterns for byte Array
// Byte Arr: [143,187,66,1] -- Int Value : 21150607 
// Byte Arr: [231,188,66,1] -- Int Value : 21150951 -- Number displayed on MHW window : 163956
// Byte Arr: [174,190,66,1] -- Int Value : 21151406 -- Number displayed on MHW window : 165889
// Byte Arr: [ 47,192,66,1] -- Int Value : 21151791 -- Number displayed on MHW window : 166849

DWORD64 FindDataAddress(Process &Proc, int IntPattern)
{
    byte PatternBuffer[4];
    int LastBits = (0x068C) + 29; // The value of the least significant bits of the address

    DWORD64 BaseAddr = 0;
    byte *ReadBuffer = nullptr;
    MEMORY_BASIC_INFORMATION MemBuffer;

    while (BaseAddr < 0x7fffffffffffLL)
    {
        if (VirtualQueryEx(Proc.getHanlder(), (LPVOID)BaseAddr, &MemBuffer, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
        {
            throw std::system_error(std::error_code(), "VirtualQueryEx Returned Error Code 0");
        }

        BaseAddr += (DWORD64)MemBuffer.RegionSize - 1; // Advance to next Memory Region

        if ( MemBuffer.AllocationProtect == 64 || MemBuffer.AllocationProtect == 4 )
        {
            ReadBuffer = Proc.ReadMemory(MemBuffer.AllocationBase, MemBuffer.RegionSize);
            if (!ReadBuffer)
                continue;

            // Get 12 least significant bits
            DWORD64 index = (DWORD_PTR)MemBuffer.AllocationBase & ( (1 << 12) - 1);
            // Ensure Index + AllocationBase has the least significant bits we're searching for
            if (index < LastBits)
                index = LastBits - index;
            else
                index = ( (1 << 12) - index) + LastBits;

            // On the for loop we only add ones at the 13th bit position
            // To ensure we keep least significant bits value
            for (; index < (MemBuffer.RegionSize - 3); index += (1 << 13) )
            {
                std::copy(ReadBuffer+index, ReadBuffer+(index+4), PatternBuffer);
                if (IntPattern == BytesToInt(PatternBuffer))
                {
                    return (DWORD_PTR)(MemBuffer.AllocationBase) + (DWORD64)index;
                }
            }
            delete[] ReadBuffer;
        }
    }
    return 0;
}