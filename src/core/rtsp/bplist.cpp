// src/core/rtsp/bplist.cpp — Binary plist reader/writer.
// Implements bplist00 format per Apple's binary plist spec.
// Format: magic "bplist00" + object table + offset table + trailer.

#include "bplist.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace arak::bplist {

// bplist00 object types
enum ObjType : uint8_t {
    OBJ_NULL     = 0x00,
    OBJ_BOOL_F   = 0x08,
    OBJ_BOOL_T   = 0x09,
    OBJ_INT      = 0x10,
    OBJ_REAL     = 0x20,
    OBJ_DATE     = 0x30,
    OBJ_DATA     = 0x40,
    OBJ_STRING_A = 0x50,
    OBJ_STRING_U = 0x60,
    OBJ_UID      = 0x80,
    OBJ_ARRAY    = 0xA0,
    OBJ_DICT     = 0xD0,
    OBJ_REF      = 0xF0,
};

// Read big-endian integers of various sizes from a buffer.
static uint16_t readBE16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}
static uint32_t readBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
static uint64_t readBE64(const uint8_t* p) {
    return (static_cast<uint64_t>(p[0]) << 56) | (static_cast<uint64_t>(p[1]) << 48) |
           (static_cast<uint64_t>(p[2]) << 40) | (static_cast<uint64_t>(p[3]) << 32) |
           (static_cast<uint64_t>(p[4]) << 24) | (static_cast<uint64_t>(p[5]) << 16) |
           (static_cast<uint64_t>(p[6]) << 8) | p[7];
}

// Read an integer of 'size' bytes (1, 2, 4, or 8) from offset in the buffer.
static uint64_t readIntOfSize(const uint8_t* data, size_t offset, uint8_t size) {
    switch (size) {
    case 1: return data[offset];
    case 2: return readBE16(data + offset);
    case 4: return readBE32(data + offset);
    case 8: return readBE64(data + offset);
    default: return 0;
    }
}

// Write big-endian integer of size bytes.
static void writeBE(uint8_t* p, uint64_t v, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) {
        p[i] = static_cast<uint8_t>(v & 0xFF);
        v >>= 8;
    }
}

// Write extended length (info=0x0F): OBJ_INT type nibble + actual length bytes.
static void writeExtLen(std::vector<uint8_t>& b, uint64_t len) {
    if (len <= 0xFF) {
        b.push_back(OBJ_INT | 0x00);  // 1-byte int
        b.push_back(static_cast<uint8_t>(len));
    } else if (len <= 0xFFFF) {
        b.push_back(OBJ_INT | 0x01);  // 2-byte int
        uint8_t x[2]; writeBE(x, len, 2);
        b.insert(b.end(), x, x + 2);
    } else if (len <= 0xFFFFFFFF) {
        b.push_back(OBJ_INT | 0x02);  // 4-byte int
        uint8_t x[4]; writeBE(x, len, 4);
        b.insert(b.end(), x, x + 4);
    } else {
        b.push_back(OBJ_INT | 0x03);  // 8-byte int
        uint8_t x[8]; writeBE(x, len, 8);
        b.insert(b.end(), x, x + 8);
    }
}

// Forward declarations for mutual recursion.
struct Parser;
static CoreStatus readObject(Parser& p, uint64_t idx, BplistValue& out);
static CoreStatus readRef(Parser& p, uint64_t idx, BplistValue& out);

struct Parser {
    const uint8_t* data;
    size_t size;
    uint8_t objectRefSize;
    uint64_t offsetTableOffset;
    const uint8_t* offsetTable;
};

static CoreStatus readRef(Parser& p, uint64_t idx, BplistValue& out) {
    if (idx >= p.size) return CoreStatus::ProtocolError;
    uint64_t offset = readIntOfSize(p.offsetTable, idx * p.objectRefSize, p.objectRefSize);
    return readObject(p, offset, out);
}

static CoreStatus readObject(Parser& p, uint64_t idx, BplistValue& out) {
    if (idx >= p.size) return CoreStatus::ProtocolError;
    uint8_t typeByte = p.data[idx];
    ObjType type = static_cast<ObjType>(typeByte & 0xF0);
    uint8_t info = typeByte & 0x0F;

    switch (type) {
    case OBJ_NULL:
        // BoolT and BoolF also have type nibble 0x0 — check info first
        if (info == 0x08) { out = BplistValue(false); return CoreStatus::Ok; }
        if (info == 0x09) { out = BplistValue(true); return CoreStatus::Ok; }
        out = BplistValue();
        return CoreStatus::Ok;

    case OBJ_BOOL_F:
        out = BplistValue(false);
        return CoreStatus::Ok;
    case OBJ_BOOL_T:
        out = BplistValue(true);
        return CoreStatus::Ok;

    case OBJ_INT: {
        uint8_t intSize = 1 << info;
        if (idx + 1 + intSize > p.size) return CoreStatus::ProtocolError;
        uint64_t raw = readIntOfSize(p.data, idx + 1, intSize);
        if (intSize == 1) out = BplistValue(static_cast<int64_t>(static_cast<int8_t>(raw)));
        else if (intSize == 2) out = BplistValue(static_cast<int64_t>(static_cast<int16_t>(raw)));
        else if (intSize == 4) out = BplistValue(static_cast<int64_t>(static_cast<int32_t>(raw)));
        else out = BplistValue(static_cast<int64_t>(raw));
        return CoreStatus::Ok;
    }

    case OBJ_REAL: {
        uint8_t realSize = 1 << info;
        if (idx + 1 + realSize > p.size) return CoreStatus::ProtocolError;
        if (realSize == 4) {
            float f;
            std::memcpy(&f, p.data + idx + 1, 4);
            out = BplistValue(static_cast<double>(f));
        } else if (realSize == 8) {
            double d;
            std::memcpy(&d, p.data + idx + 1, 8);
            out = BplistValue(d);
        } else {
            return CoreStatus::ProtocolError;
        }
        return CoreStatus::Ok;
    }

    case OBJ_DATE: {
        if (idx + 9 > p.size) return CoreStatus::ProtocolError;
        double d;
        std::memcpy(&d, p.data + idx + 1, 8);
        out = BplistValue(std::to_string(d));
        return CoreStatus::Ok;
    }

    case OBJ_DATA: {
        uint64_t len = info;
        size_t dataStart = idx + 1;
        if (info == 0x0F) {
            if (idx + 2 >= p.size) return CoreStatus::ProtocolError;
            uint8_t lenType = p.data[idx + 1] & 0xF0;
            uint8_t lenInfo = p.data[idx + 1] & 0x0F;
            if (lenType != OBJ_INT) return CoreStatus::ProtocolError;
            uint8_t lenSize = 1 << lenInfo;
            if (idx + 2 + lenSize > p.size) return CoreStatus::ProtocolError;
            len = readIntOfSize(p.data, idx + 2, lenSize);
            dataStart = idx + 2 + lenSize;
        }
        if (dataStart + len > p.size) return CoreStatus::ProtocolError;
        BplistData bd(p.data + dataStart, p.data + dataStart + len);
        out = BplistValue(std::move(bd));
        return CoreStatus::Ok;
    }

    case OBJ_STRING_A: {
        uint64_t len = info;
        size_t strStart = idx + 1;
        if (info == 0x0F) {
            if (idx + 2 >= p.size) return CoreStatus::ProtocolError;
            uint8_t lenType = p.data[idx + 1] & 0xF0;
            uint8_t lenInfo = p.data[idx + 1] & 0x0F;
            if (lenType != OBJ_INT) return CoreStatus::ProtocolError;
            uint8_t lenSize = 1 << lenInfo;
            if (idx + 2 + lenSize > p.size) return CoreStatus::ProtocolError;
            len = readIntOfSize(p.data, idx + 2, lenSize);
            strStart = idx + 2 + lenSize;
        }
        if (strStart + len > p.size) return CoreStatus::ProtocolError;
        out = BplistValue(std::string(reinterpret_cast<const char*>(p.data + strStart), len));
        return CoreStatus::Ok;
    }

    case OBJ_STRING_U: {
        uint64_t len = info;
        size_t strStart = idx + 1;
        if (info == 0x0F) {
            if (idx + 2 >= p.size) return CoreStatus::ProtocolError;
            uint8_t lenType = p.data[idx + 1] & 0xF0;
            uint8_t lenInfo = p.data[idx + 1] & 0x0F;
            if (lenType != OBJ_INT) return CoreStatus::ProtocolError;
            uint8_t lenSize = 1 << lenInfo;
            if (idx + 2 + lenSize > p.size) return CoreStatus::ProtocolError;
            len = readIntOfSize(p.data, idx + 2, lenSize);
            strStart = idx + 2 + lenSize;
        }
        uint64_t byteLen = len * 2;
        if (strStart + byteLen > p.size) return CoreStatus::ProtocolError;
        std::string utf8;
        utf8.reserve(len);
        for (uint64_t i = 0; i < len; ++i) {
            uint16_t cp = readBE16(p.data + strStart + i * 2);
            if (cp < 0x80) utf8 += static_cast<char>(cp);
            else if (cp < 0x800) {
                utf8 += static_cast<char>(0xC0 | (cp >> 6));
                utf8 += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                utf8 += static_cast<char>(0xE0 | (cp >> 12));
                utf8 += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                utf8 += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        out = BplistValue(std::move(utf8));
        return CoreStatus::Ok;
    }

    case OBJ_UID: {
        uint8_t uidSize = info + 1;
        if (idx + 1 + uidSize > p.size) return CoreStatus::ProtocolError;
        uint64_t uid = readIntOfSize(p.data, idx + 1, uidSize);
        out = BplistValue(static_cast<int64_t>(uid));
        return CoreStatus::Ok;
    }

    case OBJ_ARRAY: {
        uint64_t count = info;
        size_t arrStart = idx + 1;
        if (info == 0x0F) {
            if (idx + 2 >= p.size) return CoreStatus::ProtocolError;
            uint8_t lenType = p.data[idx + 1] & 0xF0;
            uint8_t lenInfo = p.data[idx + 1] & 0x0F;
            if (lenType != OBJ_INT) return CoreStatus::ProtocolError;
            uint8_t lenSize = 1 << lenInfo;
            if (idx + 2 + lenSize > p.size) return CoreStatus::ProtocolError;
            count = readIntOfSize(p.data, idx + 2, lenSize);
            arrStart = idx + 2 + lenSize;
        }
        if (arrStart + count * p.objectRefSize > p.size) return CoreStatus::ProtocolError;
        BplistArray arr;
        arr.reserve(count);
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t refIdx = readIntOfSize(p.data, arrStart + i * p.objectRefSize, p.objectRefSize);
            BplistValue elem;
            CoreStatus st = readRef(p, refIdx, elem);
            if (st != CoreStatus::Ok) return st;
            arr.push_back(std::move(elem));
        }
        out = BplistValue(std::move(arr));
        return CoreStatus::Ok;
    }

    case OBJ_DICT: {
        uint64_t count = info;
        size_t dictStart = idx + 1;
        if (info == 0x0F) {
            if (idx + 2 >= p.size) return CoreStatus::ProtocolError;
            uint8_t lenType = p.data[idx + 1] & 0xF0;
            uint8_t lenInfo = p.data[idx + 1] & 0x0F;
            if (lenType != OBJ_INT) return CoreStatus::ProtocolError;
            uint8_t lenSize = 1 << lenInfo;
            if (idx + 2 + lenSize > p.size) return CoreStatus::ProtocolError;
            count = readIntOfSize(p.data, idx + 2, lenSize);
            dictStart = idx + 2 + lenSize;
        }
        uint64_t refSize = count * p.objectRefSize;
        if (dictStart + refSize * 2 > p.size) return CoreStatus::ProtocolError;
        BplistDict dict;
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t keyRef = readIntOfSize(p.data, dictStart + i * p.objectRefSize, p.objectRefSize);
            uint64_t valRef = readIntOfSize(p.data, dictStart + (count + i) * p.objectRefSize, p.objectRefSize);
            BplistValue keyVal, valVal;
            CoreStatus st = readRef(p, keyRef, keyVal);
            if (st != CoreStatus::Ok) return st;
            st = readRef(p, valRef, valVal);
            if (st != CoreStatus::Ok) return st;
            if (!keyVal.isString()) return CoreStatus::ProtocolError;
            dict[std::move(keyVal.getString())] = std::move(valVal);
        }
        out = BplistValue(std::move(dict));
        return CoreStatus::Ok;
    }

    default:
        return CoreStatus::ProtocolError;
    }
}

CoreStatus decode(const uint8_t* data, size_t size, BplistValue& out) {
    if (!data || size < 48) return CoreStatus::ProtocolError;
    if (std::memcmp(data, "bplist00", 8) != 0) return CoreStatus::ProtocolError;

    const uint8_t* trailer = data + size - 32;
    uint8_t offsetIntSize = trailer[6];
    uint8_t objectRefSize = trailer[7];
    uint64_t numObjects = readBE64(trailer + 8);
    uint64_t topObject = readBE64(trailer + 16);
    uint64_t offsetTableOff = readBE64(trailer + 24);

    if (offsetIntSize == 0 || offsetIntSize > 8) return CoreStatus::ProtocolError;
    if (objectRefSize == 0 || objectRefSize > 8) return CoreStatus::ProtocolError;
    if (offsetTableOff >= size) return CoreStatus::ProtocolError;
    if (topObject >= numObjects) return CoreStatus::ProtocolError;

    Parser parser;
    parser.data = data;
    parser.size = size;
    parser.objectRefSize = objectRefSize;
    parser.offsetTableOffset = offsetTableOff;
    parser.offsetTable = data + offsetTableOff;

    return readRef(parser, topObject, out);
}

// --- Encoder (two-pass: collect indices, then write) ---

struct Encoder {
    struct Obj { enum Kind{Null,Bool,Int,Real,SA,SU,Data,Arr,Dict}; Kind kind;
        bool bv=false; int64_t iv=0; double rv=0; std::string sv; BplistData dv; std::vector<uint64_t> ch; };
    std::vector<Obj> objs; std::vector<size_t> offs;

    uint64_t assign(const BplistValue& v) {
        Obj o;
        if(v.isNull()) o.kind=Obj::Null;
        else if(v.isBool()){o.kind=Obj::Bool;o.bv=v.getBool();}
        else if(v.isInt()){o.kind=Obj::Int;o.iv=v.getInt();}
        else if(v.isReal()){o.kind=Obj::Real;o.rv=v.getReal();}
        else if(v.isString()){const auto& s=v.getString();bool aa=true;for(auto c:s)if(uint8_t(c)>0x7F){aa=false;break;}
            o.kind=aa?Obj::SA:Obj::SU;o.sv=s;}
        else if(v.isData()){o.kind=Obj::Data;o.dv=v.getData();}
        else if(v.isArray()){o.kind=Obj::Arr;for(const auto&e:v.getArray())o.ch.push_back(assign(e));}
        else if(v.isDict()){
            o.kind=Obj::Dict;
            // bplist dict format: [key0,key1,...,keyN,val0,val1,...,valN] (keys first, then values)
            std::vector<uint64_t> keys, vals;
            for(const auto&[k,vv]:v.getDict()){
                keys.push_back(assign(BplistValue(k)));
                vals.push_back(assign(vv));
            }
            o.ch.insert(o.ch.end(), keys.begin(), keys.end());
            o.ch.insert(o.ch.end(), vals.begin(), vals.end());
        }
        // Capture index AFTER children are assigned (children may expand objs)
        uint64_t idx=objs.size();
        objs.push_back(std::move(o)); return idx;
    }

    void writeObj(const Obj& o, std::vector<uint8_t>& b) {
        switch(o.kind){
        case Obj::Null:b.push_back(OBJ_NULL);break;
        case Obj::Bool:b.push_back(o.bv?OBJ_BOOL_T:OBJ_BOOL_F);break;
        case Obj::Int:{int64_t v=o.iv;
            if(v>=-128&&v<=127){b.push_back(OBJ_INT|0x00);b.push_back(uint8_t(v));}
            else if(v>=-32768&&v<=32767){b.push_back(OBJ_INT|0x01);uint8_t x[2];writeBE(x,uint64_t(v),2);b.insert(b.end(),x,x+2);}
            else if(v>=INT32_MIN&&v<=INT32_MAX){b.push_back(OBJ_INT|0x02);uint8_t x[4];writeBE(x,uint64_t(v),4);b.insert(b.end(),x,x+4);}
            else{b.push_back(OBJ_INT|0x03);uint8_t x[8];writeBE(x,uint64_t(v),8);b.insert(b.end(),x,x+8);}
            break;}
        case Obj::Real:{double v=o.rv;float f=float(v);
            if(double(f)==v){b.push_back(OBJ_REAL|0x02);uint8_t x[4];std::memcpy(x,&f,4);b.insert(b.end(),x,x+4);}
            else{b.push_back(OBJ_REAL|0x03);uint8_t x[8];std::memcpy(x,&v,8);b.insert(b.end(),x,x+8);}
            break;}
        case Obj::SA:{const auto&s=o.sv;
            if(s.size()<15){b.push_back(OBJ_STRING_A|uint8_t(s.size()));}
            else{b.push_back(OBJ_STRING_A|0x0F);writeExtLen(b,s.size());}
            b.insert(b.end(),s.begin(),s.end());break;}
        case Obj::SU:{const auto&s=o.sv;uint64_t cl=s.size();
            if(cl<15){b.push_back(OBJ_STRING_U|uint8_t(cl));}
            else{b.push_back(OBJ_STRING_U|0x0F);writeExtLen(b,cl);}
            for(char c:s){b.push_back(0);b.push_back(uint8_t(c));}break;}
        case Obj::Data:{const auto&d=o.dv;
            if(d.size()<15){b.push_back(OBJ_DATA|uint8_t(d.size()));}
            else{b.push_back(OBJ_DATA|0x0F);writeExtLen(b,d.size());}
            b.insert(b.end(),d.begin(),d.end());break;}
        case Obj::Arr:{uint64_t cnt=o.ch.size();
            if(cnt<15){b.push_back(OBJ_ARRAY|uint8_t(cnt));}
            else{b.push_back(OBJ_ARRAY|0x0F);writeExtLen(b,cnt);}
            for(uint64_t i=0;i<cnt;++i){b.push_back(0);b.push_back(0);b.push_back(0);b.push_back(0);}break;}
        case Obj::Dict:{uint64_t cnt=o.ch.size()/2;
            if(cnt<15){b.push_back(OBJ_DICT|uint8_t(cnt));}
            else{b.push_back(OBJ_DICT|0x0F);writeExtLen(b,cnt);}
            for(uint64_t i=0;i<cnt*2;++i){b.push_back(0);b.push_back(0);b.push_back(0);b.push_back(0);}break;}
        }
    }

    CoreStatus encode(const BplistValue& value, std::vector<uint8_t>& out) {
        objs.clear(); offs.clear(); uint64_t top=assign(value);
        out.clear(); out.insert(out.end(),"bplist00","bplist00"+8);
        offs.resize(objs.size()); std::vector<uint8_t> ob;
        for(uint64_t i=0;i<objs.size();++i){offs[i]=out.size();ob.clear();writeObj(objs[i],ob);out.insert(out.end(),ob.begin(),ob.end());}
        // Patch references in arrays/dicts
        for(uint64_t i=0;i<objs.size();++i){const auto&o=objs[i];
            if(o.kind!=Obj::Arr&&o.kind!=Obj::Dict)continue;
            size_t rs=offs[i]; uint8_t info=out[rs]&0x0F; size_t rp=rs+1;
            if(info==0x0F){uint8_t sizeClass=out[rp]&0x0F;rp+=1+(1<<sizeClass);}
            for(uint64_t ci:o.ch){uint8_t b[4];writeBE(b,ci,4);std::copy(b,b+4,out.begin()+rp);rp+=4;}}
        // Offset table
        size_t oto=out.size();
        for(uint64_t i=0;i<objs.size();++i){uint8_t b[4];writeBE(b,offs[i],4);out.insert(out.end(),b,b+4);}
        // Trailer
        uint8_t tr[32]={};tr[6]=4;tr[7]=4;writeBE(tr+8,objs.size(),8);writeBE(tr+16,top,8);writeBE(tr+24,oto,8);
        out.insert(out.end(),tr,tr+32); return CoreStatus::Ok;
    }
};

CoreStatus encode(const BplistValue& value, std::vector<uint8_t>& out) { Encoder e; return e.encode(value,out); }

CoreStatus encodeDict(const BplistDict& dict, std::vector<uint8_t>& out) { return encode(BplistValue(dict),out); }

}  // namespace arak::bplist
