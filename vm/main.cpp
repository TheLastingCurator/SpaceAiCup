#include "engine/easy.h"
#include "engine/unicode.h"
using namespace arctic;
using std::string;

const Rgba g_orange{0, 145, 255, 255};
const Rgba g_gray{48, 48, 48, 255};
const Rgba g_black{0, 0, 0, 255};

Vec2Si32 g_screen_size(936, 936);
Vec2Si32 g_screen_pos[2] = {Vec2Si32(12,120), Vec2Si32(972,120)};

#define RAM_BITS 22
#define RAM_SIZE_BITS (1<<RAM_BITS)
#define RAM_MASK_BITS (RAM_SIZE_BITS-1)
#define RAM_SIZE_BYTES (RAM_SIZE_BITS>>3)
#define RAM_MASK_BYTES (RAM_SIZE_BYTES-1)
#define RAM_SIZE_QW (RAM_SIZE_BITS>>6)
#define RAM_MASK_QW (RAM_SIZE_QW-1)

Ui64 ip_register;
Ui64 ram[RAM_SIZE_QW+3];


Font g_font;

inline void Write52(Ui64 * const mem, const Ui64 bitOffset, const Ui64 value) {
  const Ui64 elementIndex = bitOffset >> 6;
  const Ui64 startBit = bitOffset & 63;
  if (startBit > 12) {
    const Ui64 bitsInCurrentElement = 64 - startBit;
    const Ui64 bitsInNextElement = 52 - bitsInCurrentElement;
    const Ui64 currentElementMask = (1ULL << bitsInCurrentElement) - 1;
    const Ui64 nextElementMask = (1ULL << bitsInNextElement) - 1;
    mem[elementIndex] = (mem[elementIndex] & ~(currentElementMask << startBit)) | (value << startBit);
    mem[elementIndex+1] = (mem[elementIndex+1] & ~nextElementMask) | (value >> bitsInCurrentElement);
  } else {
    mem[elementIndex] = (mem[elementIndex] & ~(((1ULL << 52) - 1) << startBit)) | (value << startBit);
  }
}

inline void Xor52(Ui64 * const mem, const Ui64 bitOffset, const Ui64 value) {
  const Ui64 elementIndex = bitOffset >> 6;
  const Ui64 startBit = bitOffset & 63;
  if (startBit > 12) {
    const Ui64 bitsInCurrentElement = 64 - startBit;
    mem[elementIndex] ^= (value << startBit);
    mem[elementIndex+1] ^= (value >> bitsInCurrentElement);
  } else {
    mem[elementIndex] ^= (value << startBit);
  }
}

inline constexpr Ui64 Read52(const Ui64 * const mem, const Ui64 offset) {
  const Ui64 start_index = offset >> 6;
  const Ui64 start_bit = offset & 63;
  const Ui64 value = (mem[start_index] >> start_bit);
  const Ui64 next_value = start_bit ? mem[start_index + 1] << (64-start_bit) : 0;
  return (value | next_value) & 0x000FFFFFFFFFFFFF;
}

inline constexpr Ui64 ReadRambits(const Ui64 * const mem, const Ui64 offset) {
  const Ui64 start_index = offset >> 6;
  const Ui64 start_bit = offset & 63;
  const Ui64 value = (mem[start_index] >> start_bit);
  const Ui64 next_value = start_bit ? mem[start_index + 1] << (64-start_bit) : 0;
  return (value | next_value) & RAM_MASK_BITS;
}

inline Ui64 InterpretOne(Ui64 * const mem, const Ui64 ip) {
  const Ui64 v = Read52(mem, ip);
  const Ui64 a = v & RAM_MASK_BITS;
  const Ui64 b = (v >> 26) & RAM_MASK_BITS;
  const Ui64 va = Read52(mem, a);
  const Ui64 vb = Read52(mem, b);
  const Ui64 res = (va - vb) & 0x000FFFFFFFFFFFFF;
  Xor52(mem, a, res ^ va);
  if (res - 1 < ((1ull<<51) - 1)) {
    return (ip + 3 * 26) & RAM_MASK_BITS;
  } else {
    const Ui64 next_ip = ReadRambits(mem, (ip + 52) );
    return next_ip;
  }
}

void DrawDisplays() {
  Sprite back = GetEngine()->GetBackbuffer();
  DrawRectangle(Vec2Si32(0, 0),
                Vec2Si32(back.Width(), g_screen_pos[0].y),
                g_gray);
  DrawRectangle(Vec2Si32(0, g_screen_pos[0].y),
                Vec2Si32(g_screen_pos[0].x, g_screen_pos[0].y + g_screen_size.y),
                g_gray);
  DrawRectangle(Vec2Si32(g_screen_pos[0].x + g_screen_size.x, g_screen_pos[0].y),
                Vec2Si32(g_screen_pos[1].x, g_screen_pos[0].y + g_screen_size.y),
                g_gray);
  DrawRectangle(Vec2Si32(g_screen_pos[1].x + g_screen_size.x, g_screen_pos[0].y),
                Vec2Si32(back.Width(), g_screen_pos[0].y + g_screen_size.y),
                g_gray);
  DrawRectangle(Vec2Si32(0, g_screen_pos[0].y + g_screen_size.y),
                Vec2Si32(back.Width(), back.Height()),
                g_gray);
}

void DrawScreen(Ui64* mem, Vec2Si32 pos) {
  Sprite back = GetEngine()->GetBackbuffer();
  Rgba* prgba = back.RgbaData();
  Si32 stride = back.StridePixels();
  prgba += pos.x + pos.y * stride;
  Ui64 idx = 0;
  for (Si32 y = 0; y < g_screen_size.y; ++y) {
    Rgba* line = prgba + y * stride;
    for (Si32 x = 0; x < g_screen_size.x; ++x) {
      *line = (mem[idx >> 6] & (1ull << (idx & 63)) ? g_orange : g_black);
      ++line;
      ++idx;
    }
  }
}

void EasyMain() {
  ResizeScreen(1920, 1080);
  ShowFrame();
  g_font.LoadLetterBits(g_tiny_font_letters, 6, 8);

  for (size_t i = 0; i < RAM_SIZE_QW; ++i) {
    ram[i] = 0;//Random64();
  }

  std::vector<Ui8> source = ReadFile("data/rom.dat", true);
  *Log() << "Read " << source.size() << " bytes from data/rom.dat";
  Ui64 to_read = std::min((Ui64)source.size(), (Ui64)RAM_SIZE_BYTES);
  for (Ui64 i = 0; i < to_read; ++i) {
    ram[i >> 3] |= (Ui64(source[i]) << ((i & 7)*8));
  }

  Ui64 ops = 0;
  double start_time = Time();

  while (!IsAnyKeyDownward()) {
    for (Si32 i = 0; i < 125000; ++i) {
      ip_register = InterpretOne(ram, InterpretOne(ram, InterpretOne(ram, InterpretOne(ram,
           InterpretOne(ram, InterpretOne(ram, InterpretOne(ram, InterpretOne(ram, ip_register))))))));
    }
    ops += 8*125000;

    DrawScreen(&ram[0*13689], g_screen_pos[0]);
    DrawScreen(&ram[1*13689], g_screen_pos[1]);
    DrawDisplays();

    double mhz = (ops / 1000000ull) / (Time() - start_time);
    char text[1024];
    snprintf(text, 1024, "MHz: %f", mhz);
    g_font.Draw(GetEngine()->GetBackbuffer(), text,
                100, 100,
                kTextOriginFirstBase,
                kDrawBlendingModeColorize,
                kFilterNearest,
                Rgba(255,255,255,255));

    ShowFrame();
  }
}
