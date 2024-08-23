#include <cassert>
#include "font.h"
#include "include/core/SkFont.h"

uint16_t RiveFont::charToGlyph(SkUnichar u) const
{
    for (size_t i = 0; i < fCMap.size(); ++i)
    {
        if (fCMap[i].fChar == u)
        {
            return fCMap[i].fGlyph;
        }
    }
    return 0;
}

float RiveFont::advance(uint16_t glyph) const
{
    assert(glyph < fGlyphs.size());
    return fGlyphs[glyph].fAdvance;
}

const SkPath* RiveFont::path(uint16_t glyph) const
{
    assert(glyph < fGlyphs.size());
    const SkPath& p = fGlyphs[glyph].fPath;
    return p.isEmpty() ? nullptr : &p;
}

#define kSignature 0x23581321
#define kVersion 1

constexpr uint32_t Tag(unsigned a, unsigned b, unsigned c, unsigned d)
{
    assert((a & 0xFF) == a);
    assert((b & 0xFF) == b);
    assert((c & 0xFF) == c);
    assert((d & 0xFF) == d);
    return (a << 24) | (b << 16) | (c << 8) | d;
}

static inline void tag_to_str(uint32_t tag, char str[5])
{
    str[0] = (tag >> 24) & 0xFF;
    str[1] = (tag >> 16) & 0xFF;
    str[2] = (tag >> 8) & 0xFF;
    str[3] = (tag >> 0) & 0xFF;
    str[4] = 0;
}

constexpr uint32_t kOffsets_TableTag = Tag('p', 'o', 'f', 'f');
constexpr uint32_t kPaths_TableTag = Tag('p', 'a', 't', 'h');
constexpr uint32_t kCMap_TableTag = Tag('c', 'm', 'a', 'p');
constexpr uint32_t kInfo_TableTag = Tag('i', 'n', 'f', 'o');
constexpr uint32_t kAdvances_TableTag = Tag('h', 'a', 'd', 'v');

struct TableDir
{
    uint32_t tag;
    uint32_t offset;
    uint32_t length;
};

struct FontHead
{
    uint32_t signature;
    uint32_t version;
    uint32_t tableCount;
    // TableDir[dirCount]

    const TableDir* dir() const { return (const TableDir*)(this + 1); }

    const void* findTable(uint32_t tag) const
    {
        auto dir = this->dir();
        for (unsigned i = 0; i < this->tableCount; ++i)
        {
            if (dir[i].tag == tag)
            {
                return (char*)this + dir[i].offset;
            }
        }
        return nullptr;
    }
};

struct InfoTable
{
    uint16_t glyphCount;
    uint16_t upem;
};

struct GlyphOffsetTable
{
    //  uint32_t offsets[glyphCount+1]; // relative to the glyphdata table
};

struct GlyphRec
{
    // uint16_t verbCount;
    // uint16_t pointCount; // if verbCount > 0
    // uint8_t  verbs;       // Move, Line, Quad, Cubic, Close
    // pad16[]
    // uint16_t x,y x,y x,y
};

void RiveFont::load(sk_sp<SkTypeface> tf, const char str[], size_t len)
{
    this->clear();

    SkFont font(std::move(tf), 1.0f);

    uint16_t glyphIDs[len];
    int glyphCount = font.textToGlyphs(str, len, SkTextEncoding::kUTF8, glyphIDs, len);
    assert(glyphCount == (int)len);

    struct Rec
    {
        uint16_t charCode;
        uint16_t srcGlyph;
        uint16_t dstGlyph;
    };
    std::vector<Rec> rec;
    uint16_t newDstGlyphID = 1; // leave room for glyphID==0 for missing glyph
    // build vector of unique chars
    for (size_t i = 0; i < len; ++i)
    {
        uint16_t code = str[i];
        auto iter = std::find_if(rec.begin(), rec.end(), [code](const auto& r) {
            return r.charCode == code;
        });
        if (iter == rec.end())
        {
            // gonna add code -- now see if its glyph is unique
            uint16_t srcGlyph = glyphIDs[i];
            auto it2 = std::find_if(rec.begin(), rec.end(), [srcGlyph](const auto& r) {
                return r.srcGlyph == srcGlyph;
            });
            uint16_t dstGlyph;
            if (it2 == rec.end())
            {
                // srcGlyph is unique (or zero)
                dstGlyph = srcGlyph ? newDstGlyphID++ : 0;
            }
            else
            {
                dstGlyph = it2->dstGlyph; // reuse prev dstGlyph
            }
            rec.push_back({code, srcGlyph, dstGlyph});
        }
    }

    std::sort(rec.begin(), rec.end(), [](const Rec& a, const Rec& b) {
        return a.charCode < b.charCode;
    });
    for (const auto& r : rec)
    {
        printf("'%c' [%d] %d -> %d\n", r.charCode, r.charCode, r.srcGlyph, r.dstGlyph);
        fCMap.push_back({r.charCode, r.dstGlyph});
    }

    std::sort(rec.begin(), rec.end(), [](const Rec& a, const Rec& b) {
        return a.dstGlyph < b.dstGlyph;
    });

    font.setLinearMetrics(true);
    auto append_glyph = [&](uint16_t srcGlyph) {
        float width;
        font.getWidths(&srcGlyph, 1, &width);
        SkPath path;
        font.getPath(srcGlyph, &path); // returns false if glyph is bitmap

        fGlyphs.push_back({std::move(path), width});
    };

    append_glyph(0); // missing glyph
    for (int i = 1; i < newDstGlyphID; ++i)
    { // walk through our glyphs
        auto iter =
            std::find_if(rec.begin(), rec.end(), [i](const auto& r) { return r.dstGlyph == i; });
        assert(iter != rec.end());
        append_glyph(iter->srcGlyph);
    }
}

struct ByteBuilder
{
    std::vector<uint8_t> bytes;

    uint32_t length() const { return bytes.size(); }

    uint8_t add8(size_t x)
    {
        assert((x & 0xFF) == x);
        uint8_t b = (uint8_t)x;
        bytes.push_back(b);
        return b;
    }
    int16_t addS16(ssize_t x)
    {
        int16_t s = (int16_t)x;
        assert(s == x);
        this->add(&s, 2);
        return s;
    }
    uint16_t addU16(size_t x)
    {
        assert((x & 0xFFFF) == x);
        uint16_t u = (uint16_t)x;
        this->add(&u, 2);
        return u;
    }
    uint32_t addU32(size_t x)
    {
        assert((x & 0xFFFFFFFF) == x);
        uint32_t u = (uint32_t)x;
        this->add(&u, 4);
        return u;
    }

    void add(const void* src, size_t n)
    {
        size_t size = bytes.size();
        bytes.resize(size + n);
        memcpy(bytes.data() + size, src, n);
    }
    template <typename T> void add(const std::vector<T>& v)
    {
        this->add(v.data(), v.size() * sizeof(T));
    }
    void add(const ByteBuilder& bb) { this->add(bb.bytes.data(), bb.bytes.size()); }
    void add(sk_sp<SkData> data) { this->add(data->data(), data->size()); }

    void padTo16()
    {
        if (bytes.size() & 1)
        {
            this->add8(0);
        }
    }
    void padTo32()
    {
        while (bytes.size() & 3)
        {
            this->add8(0);
        }
    }

    void set32(size_t offset, uint32_t value)
    {
        assert(offset + 4 <= bytes.size());
        assert((offset & 3) == 0);
        memcpy(bytes.data() + offset, &value, 4);
    }

    sk_sp<SkData> detach()
    {
        auto data = SkData::MakeWithCopy(bytes.data(), bytes.size());
        bytes.clear();
        return data;
    }
};

void encode_path(ByteBuilder* bb, const SkPath& path, float scale)
{
    std::vector<uint8_t> varray;
    std::vector<uint16_t> parray;

    auto add_point = [&](SkPoint p) {
        parray.push_back(SkScalarRoundToInt(p.fX * scale));
        parray.push_back(SkScalarRoundToInt(p.fY * scale));
    };

    SkPath::RawIter iter(path);
    for (;;)
    {
        SkPoint pts[4];
        auto verb = iter.next(pts);
        if (verb == SkPath::kDone_Verb)
        {
            break;
        }
        varray.push_back(verb);
        switch ((SkPathVerb)verb)
        {
            case SkPathVerb::kMove:
                add_point(pts[0]);
                break;
            case SkPathVerb::kLine:
                add_point(pts[1]);
                break;
            case SkPathVerb::kQuad:
                add_point(pts[1]);
                add_point(pts[2]);
                break;
            case SkPathVerb::kCubic:
                add_point(pts[1]);
                add_point(pts[2]);
                add_point(pts[3]);
                break;
            case SkPathVerb::kClose:
                break;
            default:
                assert(false); // unsupported
        }
    }
    assert((int)varray.size() == path.countVerbs());
    assert((int)parray.size() == path.countPoints() * 2);

    auto no_useful_verbs = [](const std::vector<uint8_t>& verbs) {
        for (auto v : verbs)
        {
            switch ((SkPathVerb)v)
            {
                case SkPathVerb::kLine:
                case SkPathVerb::kQuad:
                case SkPathVerb::kCubic:
                    return false;
                default:
                    break;
            }
        }
        return true;
    };
    if (no_useful_verbs(varray))
    {
        return; // we signal empty paths with the offset table
    }

    bb->addU16(varray.size());
    assert((parray.size() & 1) == 0);
    bb->addU16(parray.size() / 2); // #points == 2 * #values

    bb->add(varray);
    bb->padTo16();
    bb->add(parray);
}

sk_sp<SkData> RiveFont::encode() const
{
    sk_sp<SkData> infoD, cmapD, offsetsD, pathsD;
    const int upem = 2048;
    const float scale = upem;

    struct DirRec
    {
        uint32_t tag;
        sk_sp<SkData> data;
    };
    std::vector<DirRec> dir;

    {
        InfoTable itable;
        itable.glyphCount = fGlyphs.size();
        itable.upem = upem;
        dir.push_back({kInfo_TableTag, SkData::MakeWithCopy(&itable, sizeof(itable))});
    }

    {
        ByteBuilder cmap;
        // start with #pairs
        cmap.addU16(fCMap.size());
        for (const auto& cm : fCMap)
        {
            cmap.addU16(cm.fChar);
            cmap.addU16(cm.fGlyph);
        }
        dir.push_back({kCMap_TableTag, cmap.detach()});
    }

    // todo: store offset[i] for glyph[i+1] since first offset is always 0
    {
        ByteBuilder paths, advances;
        std::vector<uint32_t> offsets;
        for (const auto& g : fGlyphs)
        {
            offsets.push_back(paths.length());
            encode_path(&paths, g.fPath, scale);
            paths.padTo16();
            advances.addU16(SkScalarRoundToInt(g.fAdvance * scale));
        }
        offsets.push_back(paths.length()); // store N+1 offsets

        dir.push_back(
            {kOffsets_TableTag, SkData::MakeWithCopy(offsets.data(), offsets.size() * 4)});
        dir.push_back({kPaths_TableTag, paths.detach()});
        dir.push_back({kAdvances_TableTag, advances.detach()});
    }

    std::sort(dir.begin(), dir.end(), [](const DirRec& a, const DirRec& b) {
        return a.tag < b.tag;
    });

    ByteBuilder header;
    header.addU32(kSignature);
    header.addU32(kVersion);
    header.addU32(dir.size());
    for (auto& d : dir)
    {
        header.addU32(d.tag);
        header.addU32(0); // offset -- fill in later
        header.addU32(d.data->size());
    }

    size_t offsetToDirEntry = sizeof(FontHead);
    for (auto& d : dir)
    {
        // +4 to skip the tag field of the dir entry
        header.set32(offsetToDirEntry + 4, header.length());
        offsetToDirEntry += sizeof(TableDir);

        header.add(d.data);
        header.padTo32();
    }
    return header.detach();
}

struct Reader
{
    const char* fStart;
    const char* fCurr;
    const char* fStop;

    Reader(const void* data, size_t length)
    {
        fStart = (const char*)data;
        fCurr = fStart;
        fStop = fStart + length;
    }

    bool isAvailable(size_t n) const { return fCurr + n <= fStop; }
    size_t available() const { return fStop - fCurr; }

    template <typename T> const T* skip(size_t size)
    {
        assert(this->isAvailable(size));
        const char* p = fCurr;
        fCurr += size;
        return reinterpret_cast<const T*>(p);
    }

    template <typename T> const T* skip() { return this->skip<T>(sizeof(T)); }

    void skip(size_t size) {}

    uint8_t u8()
    {
        assert(this->isAvailable(1));
        return *fCurr++;
    }

    int16_t s16()
    {
        assert(this->isAvailable(2));
        int16_t s;
        memcpy(&s, fCurr, 2);
        fCurr += 2;
        return s;
    }
    uint16_t u16()
    {
        assert(this->isAvailable(2));
        uint16_t s;
        memcpy(&s, fCurr, 2);
        fCurr += 2;
        return s;
    }
    uint32_t u32()
    {
        assert(this->isAvailable(4));
        uint32_t s;
        memcpy(&s, fCurr, 4);
        fCurr += 4;
        return s;
    }

    void read(void* dst, size_t size)
    {
        assert(this->isAvailable(size));
        memcpy(dst, fCurr, size);
        fCurr += size;
    }

    void skipPad16()
    {
        size_t amount = fCurr - fStart;
        if (amount & 1)
        {
            assert(this->isAvailable(1));
            fCurr += 1;
        }
    }
};

static int compute_point_count(const uint8_t verbs[], int verbCount)
{
    int count = 0;
    for (int i = 0; i < verbCount; ++i)
    {
        switch ((SkPathVerb)verbs[i])
        {
            case SkPathVerb::kMove:
                count += 1;
                break;
            case SkPathVerb::kLine:
                count += 1;
                break;
            case SkPathVerb::kQuad:
                count += 2;
                break;
            case SkPathVerb::kCubic:
                count += 3;
                break;
            case SkPathVerb::kClose:
                count += 0;
                break;
            default:
                assert(false);
                return -1;
        }
    }
    return count;
}

static SkPath decode_path(const void* data, size_t length, float scale)
{
    Reader reader(data, length);

    const int verbCount = reader.u16();
    assert(verbCount > 0);
    const int pointCount = reader.u16();

    auto verbs = reader.skip<uint8_t>(verbCount);
    reader.skipPad16();
    const int computedPointCount = compute_point_count(verbs, verbCount);
    assert(pointCount == computedPointCount);

    auto pts16 = reader.skip<int16_t>(pointCount * 2 * sizeof(int16_t));
    assert(reader.available() == 0);

    SkPoint pts[pointCount];
    for (int i = 0; i < pointCount; ++i)
    {
        pts[i] = {pts16[0] * scale, pts16[1] * scale};
        pts16 += 2;
    }

    return SkPath::Make(pts, pointCount, verbs, verbCount, nullptr, 0, SkPathFillType::kWinding);
}

bool RiveFont::decode(const void* data, size_t length)
{
    Reader reader(data, length);

    auto font = reader.skip<FontHead>();
    assert(font->signature == kSignature);
    assert(font->version == kVersion);
    assert(font->tableCount >= 0);

    assert(reader.isAvailable(font->tableCount * sizeof(TableDir)));

    if (true)
    {
        auto dir = font->dir();
        for (unsigned i = 0; i < font->tableCount; ++i)
        {
            char str[5];
            tag_to_str(dir[i].tag, str);
            printf("[%d] {%0x08 %s %10d} %d %d\n",
                   i,
                   dir[i].tag,
                   str,
                   dir[i].tag,
                   dir[i].offset,
                   dir[i].length);
        }
    }

    auto info = (const InfoTable*)font->findTable(kInfo_TableTag);
    auto cmap = (const uint16_t*)font->findTable(kCMap_TableTag);
    auto paths = (const char*)font->findTable(kPaths_TableTag);
    auto offsets = (const uint32_t*)font->findTable(kOffsets_TableTag);
    auto advances = (const uint16_t*)font->findTable(kAdvances_TableTag);

    const int glyphCount = info->glyphCount;

    this->clear();

    const int pairCount = *cmap++;
    for (int i = 0; i < pairCount; ++i)
    {
        uint16_t c = *cmap++;
        uint16_t g = *cmap++;
        assert(g < glyphCount);

        fCMap.push_back({c, g});
    }

    const float scale = 1.0f / (float)info->upem;

    for (int i = 0; i < glyphCount; ++i)
    {
        float adv = advances[i] * scale;
        uint32_t start = offsets[i];
        uint32_t end = offsets[i + 1];
        assert(start <= end);

        fGlyphs.push_back(
            {(start == end) ? SkPath() : decode_path(paths + start, end - start, scale), adv});
    }
    return true;
}
