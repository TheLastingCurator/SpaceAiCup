#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>

struct Word {
  int64_t source_line;
  bool is_immediate = false;
  int64_t immediate = 0;
  int64_t symbol_id = -1;
  int64_t offset_bits = 0;
  int64_t size_bits = 52;
};

struct MacroLine {
  std::string text;
  int64_t source_line;

  MacroLine(std::string_view &line, int64_t line_number)
      : text(line)
      , source_line(line_number) {
  }
};

struct Macro {
  std::vector<std::string> args;
  std::vector<MacroLine> lines;
  std::unordered_set<std::string> locals;
};

std::unordered_map<std::string, int64_t> symbol_map; // zero or positive = symbol, negative = macro
std::vector<int64_t> symbol_to_addr;
std::vector<Word> code;
int64_t code_size_bits = 0;
std::vector<Macro> macros;
Macro* macro_being_parsed = nullptr;
int64_t next_macro_substitution_idx = 0;

bool parseLine(std::string_view line, int64_t line_number, std::unordered_map<std::string, Word> &substitutions, bool is_macro); 


void PushCode(Word &word, int64_t size_bits) {
  code.push_back(word);
  code.back().offset_bits = code_size_bits;
  code.back().size_bits = size_bits;
  code_size_bits += size_bits;
}

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

bool consumeComment(std::string_view& text) {
  if (text.empty() || text.front() != ';') {
    return false;
  }
  text = std::string_view();
  return true;
}

int64_t SymbolToId(const std::string& str, std::unordered_map<std::string, Word> &substitutions) {
  auto itSub = substitutions.find(str);
  if (itSub != substitutions.end()) {
    return itSub->second.symbol_id;
  }
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
  if (i < text.size() && isAlpha(text[i])) {
    ++i;
    while (i < text.size() && isAlnum(text[i])) {
      ++i;
    }
  }
  if (i == 0) {
    text = originalText;
    return false;
  }
  outIdentifier = std::string(text.substr(0, i));
  text.remove_prefix(i); // Advance the text after identifier
  return true;
}

bool tryParseArg(std::string_view& text, Word& word, int64_t line_number, std::unordered_map<std::string, Word> &substitutions) {
  word.source_line = line_number;
  uint64_t number;
  std::string identifier;
  if (tryParseInteger(text, number)) {
    word.is_immediate = true;
    word.immediate = number;
    return true;
  } else if (tryParseIdentifier(text, identifier)) {
    word.is_immediate = false;
    word.symbol_id = SymbolToId(identifier, substitutions);
    return true;
  } else {
    return false;
  }
}

bool parseArg(std::string_view& text, Word& word, int64_t line_number, std::unordered_map<std::string, Word> &substitutions) {
  if (tryParseArg(text, word, line_number, substitutions)) {
    return true;
  }
  std::cerr << "Error: Expected argument at line " << line_number << std::endl;
  return false;
}

bool parseSubleqInstruction(std::string_view& text, int64_t line_number, std::unordered_map<std::string, Word> &substitutions) {
  Word word[3];
  bool is_ok = parseArg(text, word[0], line_number, substitutions);
  is_ok = is_ok && (consumeWhitespace(text) | consumeComa(text) | consumeWhitespace(text));
  is_ok = is_ok && parseArg(text, word[1], line_number, substitutions);
  is_ok = is_ok && (consumeWhitespace(text) | consumeComa(text) | consumeWhitespace(text));
  is_ok = is_ok && parseArg(text, word[2], line_number, substitutions);
  if (is_ok) {
    PushCode(word[0], 26);
    PushCode(word[1], 26);
    PushCode(word[2], 26);
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

bool parseDW(std::string_view& text, int64_t line_number, std::unordered_map<std::string, Word> &substitutions) {
  consumeWhitespace(text);
  consumeComment(text);
  while (!text.empty()) {
    if (text.front() == '\'' || text.front() == '\"') {
      char delimiter = text.front();
      text.remove_prefix(1);
      while (!text.empty() && text.front() != delimiter) {
        if (text.front() == '\n' || text.front() == '\0') {
          std::cerr << "Error: Unterminated string literal at line " << line_number << std::endl;
          return false;
        }
        Word word;
        word.source_line = line_number;
        word.is_immediate = true;
        word.immediate = static_cast<uint64_t>(text.front());
        PushCode(word, 52);
        text.remove_prefix(1);
      }
      if (text.empty() || text.front() != delimiter) {
        std::cerr << "Error: Expected closing delimiter at line " << line_number << std::endl;
        return false;
      }
      text.remove_prefix(1);
    } else {
      uint64_t value = 0;
      std::string identifier;
      Word word;
      word.source_line = line_number;
      if (tryParseInteger(text, value)) {
        word.is_immediate = true;
        word.immediate = value;
        PushCode(word, 52);
      } else if (tryParseIdentifier(text, identifier)) {
        word.is_immediate = false;
        word.symbol_id = SymbolToId(identifier, substitutions);
        PushCode(word, 52);
      } else {
        std::cerr << "Error: Invalid word value at line " << line_number << std::endl;
        return false;
      }
    }
    consumeWhitespace(text);
    consumeComa(text);
    consumeWhitespace(text);
    consumeComment(text);
  }
  return true;
}

bool parseORG(std::string_view& text, int64_t line_number) {
  std::string_view originalText = text;
  consumeWhitespace(text);
  uint64_t address;
  if (!tryParseInteger(text, address)) {
    std::cerr << "Error: Unable to parse address in ORG directive at line " << line_number << std::endl;
    return false;
  }
  if (address < code_size_bits) {
    std::cerr << "Error: ORG address (" << address << ") is less than current code size (" << code_size_bits << ") at line " << line_number << std::endl;
    return false;
  }
  /*if (address % 64) {
    std::cerr << "Error: ORG address (" << address << ") is not a multipe of 64 at line " << line_number << std::endl;
    return false;
  }*/
  Word word;
  word.source_line = line_number;
  word.is_immediate = true;
  word.immediate = 0;
  while (code_size_bits < address) {
    if (address - code_size_bits >= 52) {
      PushCode(word, 52);
    } else {
      PushCode(word, address - code_size_bits);
    }
  }
  return true;
}

bool startsWithToken(const std::string_view& text, const std::string_view& prefix) {
    if (prefix.size() > text.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (text[i] != prefix[i]) return false;
    }
    if (prefix.size() == text.size()) {
      return true;
    }
    if (isAlnum(text[prefix.size()])) {
      return false;
    }
    return true;
}

bool parseMacro(std::string_view& text, int64_t line_number) {
  std::string_view originalText = text;
  std::string identifier;
  if (tryParseIdentifier(text, identifier)) {
    if (macro_being_parsed != nullptr) {
      std::cerr << "Error: MACRO definition inside another MACRO definition, line " << line_number << std::endl;
      return false;
    }
    macros.emplace_back();
    macro_being_parsed = &macros.back();
    auto it = symbol_map.find(identifier);
    if (it != symbol_map.end()) {
      std::cerr << "Error: MACRO name " << text << " is already in use, line " << line_number << std::endl;
      return false;
    }
    symbol_map[identifier] = -macros.size();

    if (!consumeWhitespace(text) || text.empty()) {
      // parameterless macro
      return true;
    } else {
      // macro with parameters
      std::string param;
      std::unordered_set<std::string> arg_set;
      while (tryParseIdentifier(text, param)) {
        if (!arg_set.insert(param).second) {
          std::cerr << "Error: MACRO parameter name " << param << " use more than once, line " << line_number << std::endl;
          return false;
        }
        macro_being_parsed->args.push_back(param);
        consumeWhitespace(text);
        consumeComa(text);
        consumeWhitespace(text);
      }
      return true;
    }
  }
  text = originalText;
  return false;
}



bool substituteMacro(std::string_view& text, int64_t line_number, std::string& name, std::unordered_map<std::string, Word> &in_substitutions) {
  int64_t substitution_idx = next_macro_substitution_idx;
  ++next_macro_substitution_idx;
  int64_t id = SymbolToId(name, in_substitutions);
  if (id >= 0) {
    std::cerr << "Error: Undefined macro call at line " << line_number << std::endl;
    return false;
  }
  int64_t macro_id = -1 - id;
  Macro &macro = macros[macro_id];
  std::vector<Word> args;
  if (!consumeWhitespace(text) || text.empty()) {
    // parameterless macro
  } else {
    // macro with parameters
    Word word;
    while (tryParseArg(text, word, line_number, in_substitutions)) {
      args.push_back(word);
      consumeWhitespace(text);
      consumeComa(text);
      consumeWhitespace(text);
    }
  }
  if (args.size() != macro.args.size()) {
    std::cerr << "Error: Macro " << name << " requires " << macro.args.size() << " arguments, provided " << args.size() << " at line " << line_number << std::endl;
    return false;
  }
  std::unordered_map<std::string, Word> substitutions;
  for (size_t i = 0; i < args.size(); ++i) {
    auto itSub = substitutions.find(macro.args[i]); 
    if (itSub != substitutions.end()) {
      std::cerr << "Error: Macro " << name << " argument " << macro.args[i] << " redefinition at line " << line_number << std::endl;
      return false;
    }
    substitutions[macro.args[i]] = args[i];
  }
  for (auto it = macro.locals.begin(); it != macro.locals.end(); ++it) {
    std::stringstream str;
    str << *it << "~" << substitution_idx;
    std::string global = str.str();
    int64_t id = SymbolToId(global, substitutions);
    Word word;
    word.source_line = line_number;
    word.is_immediate = false;
    word.symbol_id = id;
    auto itSub = substitutions.find(*it); 
    if (itSub != substitutions.end()) {
      std::cerr << "Error: Macro " << name << " local " << *it << " redefinition at line " << line_number << std::endl;
      return false;
    }
    substitutions[*it] = word;
  }

  for (size_t i = 0; i < macro.lines.size(); ++i) {
    MacroLine &macro_line = macro.lines[i];
    std::string_view v = macro_line.text;
    if (!parseLine(v, macro_line.source_line, substitutions, true)) {
      std::cerr << "Error: Could not parse line " << macro_line.source_line << std::endl;
      std::cerr << "Error: Could not substitute macro at line " << line_number << std::endl;
      return false;
    }
  }

  return true;
}


bool parseMacroLine(std::string_view line, int64_t line_number) {
  macro_being_parsed->lines.emplace_back(line, line_number);
  consumeWhitespace(line);
  std::string label;
  if (tryParseLabel(line, label)) {
    macro_being_parsed->locals.insert(label);
  }
  consumeWhitespace(line);
  if (startsWithToken(line, "MACRO")) {
    std::cerr << "Error: MACRO definition inside another MACRO definition, line " << line_number << std::endl;
    return false;
  }
  if (startsWithToken(line, "ENDM")) {
    line.remove_prefix(4);
    consumeWhitespace(line);
    macro_being_parsed = nullptr;
    consumeComment(line);
    if (line.size() > 0) {
      std::cerr << "Error: Unexpected tokens at line " << line_number << std::endl;
      return false;
    }
  }
  return true;
}

bool parseLine(std::string_view line, int64_t line_number, std::unordered_map<std::string, Word> &substitutions, bool is_macro) {
  consumeWhitespace(line);
  std::string label;
  if (tryParseLabel(line, label)) {
    symbol_to_addr[SymbolToId(label, substitutions)] = code_size_bits;
  }
  consumeWhitespace(line);
  std::string name;
  if (startsWithToken(line, "SUBLEQ")) {
    line.remove_prefix(6);
    consumeWhitespace(line);
    if (!parseSubleqInstruction(line, line_number, substitutions)) {
      return false;
    }
  } else if (startsWithToken(line, "DW")) {
    line.remove_prefix(2);
    consumeWhitespace(line);
    if (!parseDW(line, line_number, substitutions)) {
      return false;
    }
  } else if (startsWithToken(line, "ORG")) {
    line.remove_prefix(3);
    consumeWhitespace(line);
    if (!parseORG(line, line_number)) {
      return false;
    }
  } else if (startsWithToken(line, "MACRO")) {
    line.remove_prefix(5);
    consumeWhitespace(line);
    if (!parseMacro(line, line_number)) {
      return false;
    }
  } else if (startsWithToken(line, "ENDM")) {
    line.remove_prefix(4);
    consumeWhitespace(line);
    if (!is_macro) {
      return false;
    }
  } else if (tryParseIdentifier(line, name)) {
    if (!substituteMacro(line, line_number, name, substitutions)) {
      return false;
    }
  }
    
  consumeWhitespace(line);
  consumeComment(line);
  if (line.size() > 0) {
    std::cerr << "Error: Unexpected tokens at line " << line_number << std::endl;
      return false;
  }
  return true;
}

bool CheckAddr() {
  for (size_t idx = 0; idx < code.size(); ++idx) {
    const auto& word = code[idx];
    if (!word.is_immediate) {
      if (symbol_to_addr[word.symbol_id] < 0) {
        std::cerr << "Error: Undefined symbol at line " << word.source_line << std::endl;
        return false;
      }
    }
  }
  return true;
}

class BitwiseOutput {
	public:
		explicit BitwiseOutput(std::ofstream &outputStream)
			: out(outputStream), buffer(0), bufferBits(0) {}

		~BitwiseOutput() {
			flushBuffer();
		}

		void write(uint64_t value, size_t bitCount) {
			while (bitCount > 0) {
				size_t bitsToWrite = std::min(bitCount, 8 - bufferBits);
				uint64_t shiftedValue = value >> (bitCount - bitsToWrite);
				buffer |= (shiftedValue & ((1ULL << bitsToWrite) - 1)) << (8 - bufferBits - bitsToWrite);
				bufferBits += bitsToWrite;
				bitCount -= bitsToWrite;

				if (bufferBits == 8) {
					flushBuffer();
				}
			}
		}

		void flushBuffer() {
			if (bufferBits > 0) {
        uint8_t byte = buffer;
				out.write((char*)&byte, 1);
				buffer = 0;
				bufferBits = 0;
			}
		}

	private:
		std::ofstream &out;
		uint64_t buffer;
		size_t bufferBits;
};

void EmitBinaryCode(BitwiseOutput &out) {
  for (size_t idx = 0; idx < code.size(); ++idx) {
    const auto& word = code[idx];
    uint64_t data = word.is_immediate ?
      word.immediate :
      static_cast<uint64_t>(symbol_to_addr[word.symbol_id]);
    out.write(data, word.size_bits);
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
    std::unordered_map<std::string, Word> substitutions;
    if (!macro_being_parsed) {
      if (!parseLine(v, line_number, substitutions, false)) {
        std::cerr << "Error: Could not parse line " << line_number << std::endl;
        return 1;
      }
    } else {
      if (!parseMacroLine(v, line_number)) {
        std::cerr << "Error: Could not parse macro line " << line_number << std::endl;
        return 1;
      }
    }
  }

  if (!CheckAddr()) {
    std::cerr << "Error: Could not emit binary code" << std::endl;
    return 1;
  }
	BitwiseOutput bitwiseOut(out);
  EmitBinaryCode(bitwiseOut);
  bitwiseOut.flushBuffer();
  return 0;
}
