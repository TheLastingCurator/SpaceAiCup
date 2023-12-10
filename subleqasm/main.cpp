#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

#define RAM_BITS 14
#define RAM_SIZE (1<<RAM_BITS)
#define RAM_MASK (RAM_SIZE-1)

std::unordered_map<std::string, int64_t> symbol_map;
std::vector<int64_t> symbol_to_addr;

struct Arg {
  bool is_immediate = false;
  int64_t immediate = 0;
  int64_t symbol_id = -1;
};

struct Word {
  int64_t source_line;
  bool is_code;
  Arg arg[3];
};

std::vector<Word> code;

bool isDigit(char ch) {
  return ch >= '0' && ch <= '9';
}

bool isAlpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '_');
}

bool isAlnum(char ch) {
  return isAlpha(ch) || isDigit(ch);
}

bool isSpace(char ch) {
  return ch == ' ' || ch == '\t';
}

bool consumeWhitespace(std::string_view& text) {
  size_t i = 0;
  while (i < text.size() && isSpace(text[i])) {
    ++i;
  }
  text.remove_prefix(i);
  return (i > 0);
}

bool consumeComa(std::string_view& text) {
  if (!text.empty() && text.front() == ',') {
    text.remove_prefix(1);
    return true;
  }
  return false;
}

void trimComment(std::string_view& text) {
  size_t commentStart = text.find(';');
  if (commentStart != std::string_view::npos) {
    text.remove_suffix(text.size() - commentStart);
  }
}

int64_t SymbolToId(const std::string& str) {
  auto it = symbol_map.find(str);
  if (it == symbol_map.end()) {
    int64_t id = static_cast<int64_t>(symbol_to_addr.size());
    symbol_map[str] = id;
    symbol_to_addr.push_back(-1);
    return id;
  }
  return it->second;
}

bool tryParseInteger(std::string_view& text, uint64_t& outValue) {
  std::string_view originalText = text;
  consumeWhitespace(text);
  bool isNegative = false;
  if (!text.empty() && text[0] == '-') {
    isNegative = true;
    text.remove_prefix(1);
  }
  size_t i = 0;
  uint64_t result = 0;
  while (i < text.size() && isDigit(text[i])) {
    uint64_t digit = text[i] - '0';
    if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
      text = originalText;
      return false;
    }
    result = result * 10 + digit;
    ++i;
  }
  if (i == 0) {
    text = originalText;
    return false;
  }
  text.remove_prefix(i);
  outValue = isNegative ? (~result + 1) : result;
  return true;
}

bool tryParseIdentifier(std::string_view& text, std::string& outIdentifier) {
  std::string_view originalText = text;
  consumeWhitespace(text);
  size_t i = 0;
  while (i < text.size() && isAlnum(text[i])) {
    ++i;
  }
  if (i == 0) {
    text = originalText;
    return false;
  }
  outIdentifier = std::string(text.substr(0, i));
  text.remove_prefix(i); // Advance the text after identifier
  return true;
}

bool parseArg(std::string_view& text, Arg& arg, int64_t line_number) {
  uint64_t number;
  std::string identifier;
  if (tryParseInteger(text, number)) {
    arg.is_immediate = true;
    arg.immediate = number;
    return true;
  } else if (tryParseIdentifier(text, identifier)) {
    arg.symbol_id = SymbolToId(identifier);
    return true;
  } else {
    std::cerr << "Error: Expected argument at line " << line_number << std::endl;
    return false;
  }
}


bool parseInstruction(std::string_view& text, int64_t line_number) {
  Word word;
  word.source_line = line_number;
  word.is_code = true;
  bool is_ok = parseArg(text, word.arg[0], line_number);
  is_ok = is_ok && (consumeWhitespace(text) | consumeComa(text) | consumeWhitespace(text));
  is_ok = is_ok && parseArg(text, word.arg[1], line_number);
  is_ok = is_ok && (consumeWhitespace(text) | consumeComa(text) | consumeWhitespace(text));
  is_ok = is_ok && parseArg(text, word.arg[2], line_number);
  if (is_ok) {
    code.push_back(word);
  } else {
    std::cerr << "Error: Can't parse instruction arguments at line " << line_number << std::endl;
  }
  return is_ok;
}

bool tryParseLabel(std::string_view& text, std::string& outLabel) {
  std::string_view originalText = text;
  std::string identifier;
  if (tryParseIdentifier(text, identifier)) {
    consumeWhitespace(text);
    if (!text.empty() && text.front() == ':') {
      text.remove_prefix(1);
      outLabel = std::move(identifier);
      consumeWhitespace(text);
      return true;
    }
  }
  text = originalText;
  return false;
}

bool parseDW(std::string_view& text, int64_t line_number) {
  consumeWhitespace(text);
  while (!text.empty()) {
    consumeWhitespace(text);
    if (text.front() == '\'' || text.front() == '\"') {
      char delimiter = text.front();
      text.remove_prefix(1);
      while (!text.empty() && text.front() != delimiter) {
        if (text.front() == '\n' || text.front() == '\0') {
          std::cerr << "Error: Unterminated string literal at line " << line_number << '\n';
          return false;
        }
        Word word;
        word.source_line = line_number;
        word.is_code = false;
        word.arg[0].is_immediate = true;
        word.arg[0].immediate = static_cast<uint64_t>(text.front());
        code.push_back(word);
        text.remove_prefix(1);
      }
      if (text.empty() || text.front() != delimiter) {
        std::cerr << "Error: Expected closing delimiter at line " << line_number << '\n';
        return false;
      }
      text.remove_prefix(1);
    } else {
      uint64_t value = 0;
      std::string identifier;
      Word word;
      word.source_line = line_number;
      word.is_code = false;
      if (tryParseInteger(text, value)) {
        word.arg[0].is_immediate = true;
        word.arg[0].immediate = value;
        code.push_back(word);
      } else if (tryParseIdentifier(text, identifier)) {
        word.arg[0].is_immediate = false;
        word.arg[0].symbol_id = SymbolToId(identifier);
        code.push_back(word);
      } else {
        std::cerr << "Error: Invalid word value at line " << line_number << '\n';
        return false;
      }
    }
    consumeWhitespace(text);
    consumeComa(text);
  }
  return true;
}

bool parseORG(std::string_view& text, int64_t line_number) {
  std::string_view originalText = text;
  consumeWhitespace(text);
  uint64_t address;
  if (!tryParseInteger(text, address)) {
    std::cerr << "Error: Unable to parse address in ORG directive at line " << line_number << '\n';
    return false;
  }
  if (address < code.size()) {
    std::cerr << "Error: ORG address is less than current code size at line " << line_number << '\n';
    return false;
  }
  Word word;
  word.source_line = line_number;
  word.is_code = false;
  word.arg[0].is_immediate = true;
  word.arg[0].immediate = 0;
  while (code.size() < address) {
    code.push_back(word);
  }
  return true;
}

bool startsWith(const std::string_view& text, const std::string_view& prefix) {
    if (prefix.size() > text.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (text[i] != prefix[i]) return false;
    }
    return true;
}

bool parseLine(std::string_view line, int64_t line_number) {
  trimComment(line);
  consumeWhitespace(line);
  std::string label;
  if (tryParseLabel(line, label)) {
    symbol_to_addr[SymbolToId(label)] = static_cast<int64_t>(code.size());
  }
  consumeWhitespace(line);
  if (startsWith(line, "SUBLEQ")) {
    line.remove_prefix(6);
    consumeWhitespace(line);
    if (!parseInstruction(line, line_number)) {
      return false;
    }
  } else if (startsWith(line, "DW")) {
    line.remove_prefix(2);
    consumeWhitespace(line);
    if (!parseDW(line, line_number)) {
      return false;
    }
  } else if (startsWith(line, "ORG")) {
    line.remove_prefix(3);
    consumeWhitespace(line);
    if (!parseORG(line, line_number)) {
      return false;
    }
  }
  consumeWhitespace(line);
  if (line.size() > 0) {
    std::cerr << "Error: Unexpected tokens at line " << line_number << std::endl;
      return false;
  }
  return true;
}

bool CheckAddr() {
  for (size_t address = 0; address < code.size(); ++address) {
    const auto& word = code[address];
    if (word.is_code) {
      for (size_t i = 0; i < 3; ++i) {
        if (!word.arg[i].is_immediate) {
          if (symbol_to_addr[word.arg[i].symbol_id] < 0) {
            std::cerr << "Error: Undefined symbol at line " << word.source_line << '\n';
            return false;
          }
        }
      }
    } else {
      if (!word.arg[0].is_immediate) {
        if (symbol_to_addr[word.arg[0].symbol_id] < 0) {
          std::cerr << "Error: Undefined symbol at line " << word.source_line << '\n';
          return false;
        }
      }
    }
  }
  return true;
}

void EmitBinaryCode(std::ofstream &out) {
  for (size_t address = 0; address < code.size(); ++address) {
    const auto& word = code[address];
    if (word.is_code) {
      uint64_t a = word.arg[0].is_immediate ?
        word.arg[0].immediate :
        static_cast<uint64_t>(symbol_to_addr[word.arg[0].symbol_id]);
      uint64_t b = word.arg[1].is_immediate ?
        word.arg[1].immediate :
        static_cast<uint64_t>(symbol_to_addr[word.arg[1].symbol_id]);
      uint64_t c = word.arg[2].is_immediate ?
        word.arg[2].immediate :
        static_cast<uint64_t>(symbol_to_addr[word.arg[2].symbol_id]);
      uint64_t instruction = ((c & RAM_MASK) << 42) | ((b & RAM_MASK) << 21) | (a & RAM_MASK);
      out.write((const char*)&instruction, sizeof(instruction));
    } else {
      uint64_t data = word.arg[0].is_immediate ?
        word.arg[0].immediate :
        static_cast<uint64_t>(symbol_to_addr[word.arg[0].symbol_id]);
      out.write((const char*)&data, sizeof(data));
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cout << "Usage: sbuleqasm <input file> <output file>" << std::endl;
    return 1;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::cerr << "Error: Could not open input file." << std::endl;
    return 1;
  }
  std::ofstream out(argv[2], std::ios::binary);
  if (!out) {
    std::cerr << "Error: Could not open output file." << std::endl;
    return 1;
  }

  int64_t line_number = 0;
  while(!in.eof()) {
    ++line_number;
    std::string line;
    std::getline(in, line);
    std::transform(line.begin(), line.end(), line.begin(),
        [](char c){ return ((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c); });
    std::string_view v = line;
    if (!parseLine(v, line_number)) {
      std::cerr << "Error: Could not parse line " << line_number << std::endl;
      return 1;
    }
  }

  if (!CheckAddr()) {
    std::cerr << "Error: Could not emit binary code" << std::endl;
    return 1;
  }
  EmitBinaryCode(out);
  return 0;
}
