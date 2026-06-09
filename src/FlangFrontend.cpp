#include "LegacyFortranAdvisor/FlangFrontend.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cctype>

#ifdef USE_FLANG_PARSER
#include "flang/Parser/parsing.h"
#include "flang/Parser/provenance.h"
#include "llvm/Support/raw_ostream.h"
#endif

namespace LegacyFortranAdvisor {

namespace {

bool isFixedFormComment(const std::string &line) {
  if (line.empty()) return true;
  char c = line[0];
  if (c == 'c' || c == 'C' || c == '*' || c == '!') return true;
  for (char ch : line) {
    if (!std::isspace(static_cast<unsigned char>(ch))) return false;
  }
  return true;
}

std::string stripComment(const std::string &line) {
  std::string result;
  bool inSingleQuote = false;
  bool inDoubleQuote = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '\'' && !inDoubleQuote) {
      inSingleQuote = !inSingleQuote;
    } else if (c == '"' && !inSingleQuote) {
      inDoubleQuote = !inDoubleQuote;
    } else if (c == '!' && !inSingleQuote && !inDoubleQuote) {
      break;
    }
    result.push_back(c);
  }
  while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
    result.pop_back();
  }
  return result;
}

std::string stripAllSpaces(const std::string &str) {
  std::string res;
  res.reserve(str.size());
  for (char c : str) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      res.push_back(static_cast<char>(std::tolower(c)));
    }
  }
  return res;
}

void parseFreeForm(SourceAnalysis &analysis) {
  std::string currentRaw;
  int startLine = 0;
  
  for (std::size_t i = 0; i < analysis.lines.size(); ++i) {
    std::string line = analysis.lines[i];
    std::string trimmed = line;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    
    if (trimmed.empty() || trimmed[0] == '!') {
      continue;
    }
    
    std::string stripped = stripComment(line);
    if (stripped.empty()) {
      continue;
    }
    
    if (currentRaw.empty()) {
      startLine = i + 1;
    }
    
    if (!currentRaw.empty()) {
      std::string nextPart = stripped;
      nextPart.erase(nextPart.begin(), std::find_if(nextPart.begin(), nextPart.end(), [](unsigned char ch) {
          return !std::isspace(ch);
      }));
      if (!nextPart.empty() && nextPart[0] == '&') {
        nextPart = nextPart.substr(1);
      }
      currentRaw += " " + nextPart;
    } else {
      currentRaw = stripped;
    }
    
    std::string checkCont = currentRaw;
    while (!checkCont.empty() && std::isspace(static_cast<unsigned char>(checkCont.back()))) {
      checkCont.pop_back();
    }
    
    if (!checkCont.empty() && checkCont.back() == '&') {
      checkCont.pop_back();
      currentRaw = checkCont;
    } else {
      NormalizedStatement stmt;
      stmt.rawContent = currentRaw;
      stmt.content = stripAllSpaces(currentRaw);
      stmt.startLine = startLine;
      stmt.endLine = i + 1;
      analysis.statements.push_back(stmt);
      currentRaw.clear();
    }
  }
  
  if (!currentRaw.empty()) {
    NormalizedStatement stmt;
    stmt.rawContent = currentRaw;
    stmt.content = stripAllSpaces(currentRaw);
    stmt.startLine = startLine;
    stmt.endLine = analysis.lines.size();
    analysis.statements.push_back(stmt);
  }
}

void parseFixedForm(SourceAnalysis &analysis) {
  std::string currentRaw;
  int startLine = 0;
  
  for (std::size_t i = 0; i < analysis.lines.size(); ++i) {
    std::string line = analysis.lines[i];
    if (isFixedFormComment(line)) {
      continue;
    }
    
    std::string paddedLine = line;
    if (paddedLine.size() < 6) {
      paddedLine.append(6 - paddedLine.size(), ' ');
    }
    
    char col6 = paddedLine[5];
    bool isContinuation = (col6 != ' ' && col6 != '0');
    
    std::string statementPart = paddedLine.substr(6);
    statementPart = stripComment(statementPart);
    
    if (isContinuation && !currentRaw.empty()) {
      currentRaw += statementPart;
    } else {
      if (!currentRaw.empty()) {
        NormalizedStatement stmt;
        stmt.rawContent = currentRaw;
        stmt.content = stripAllSpaces(currentRaw);
        stmt.startLine = startLine;
        stmt.endLine = i;
        analysis.statements.push_back(stmt);
      }
      currentRaw = statementPart;
      startLine = i + 1;
    }
  }
  
  if (!currentRaw.empty()) {
    NormalizedStatement stmt;
    stmt.rawContent = currentRaw;
    stmt.content = stripAllSpaces(currentRaw);
    stmt.startLine = startLine;
    stmt.endLine = analysis.lines.size();
    analysis.statements.push_back(stmt);
  }
}

} // namespace

std::unique_ptr<SourceAnalysis> FlangFrontend::parseFile(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    std::cerr << "[FlangFrontend] Failed to open source file: " << path << "\n";
    return nullptr;
  }

  auto analysis = std::make_unique<SourceAnalysis>();
  analysis->filePath = path;

  std::string line;
  while (std::getline(input, line)) {
    analysis->lines.push_back(line);
  }

  std::filesystem::path sourcePath(path);
  std::string extension = sourcePath.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  analysis->fixedFormHint = (extension == ".f" || extension == ".for" || extension == ".f77");

  if (analysis->fixedFormHint) {
    parseFixedForm(*analysis);
  } else {
    parseFreeForm(*analysis);
  }

#ifdef USE_FLANG_PARSER
  std::cerr << "[FlangFrontend] USE_FLANG_PARSER is defined. Attempting to parse: " << path << "\n";
  try {
    Fortran::parser::Options options;
    Fortran::parser::AllSources allSources;
    Fortran::parser::AllCookedSources allCookedSources{allSources};
    Fortran::parser::Parsing parsing{allCookedSources};
    
    options.isFixedForm = analysis->fixedFormHint;
    
    std::string absolutePath = std::filesystem::absolute(path).string();
    const auto *sourceFile = allSources.Open(absolutePath, llvm::errs());
    if (!sourceFile) {
      std::cerr << "[FlangFrontend] Flang parser could not open file: " << absolutePath << "\n";
    }
    if (sourceFile) {
      parsing.Prescan(absolutePath, options);
      parsing.Parse(llvm::errs());
      bool parseSuccess = !parsing.messages().AnyFatalError();
      if (parseSuccess) {
        std::cerr << "[FlangFrontend] Successfully verified syntax using Flang parser.\n";
      } else {
        std::cerr << "[FlangFrontend] Flang parser warning: syntax errors detected, falling back to robust preprocessor.\n";
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[FlangFrontend] Flang parser exception: " << e.what() << ", falling back to robust preprocessor.\n";
  } catch (...) {
    std::cerr << "[FlangFrontend] Unknown Flang parser exception, falling back to robust preprocessor.\n";
  }
#else
  std::cerr << "[FlangFrontend] USE_FLANG_PARSER is NOT defined.\n";
#endif

  std::cerr << "[FlangFrontend] Parsed " << analysis->lines.size() 
            << " line(s), extracted " << analysis->statements.size() 
            << " normalized statement(s) from " << path << "\n";
  return analysis;
}

} // namespace LegacyFortranAdvisor
