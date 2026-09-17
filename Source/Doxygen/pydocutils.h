/* -----------------------------------------------------------------------------
 * This file is part of SWIG, which is licensed as a whole under version 3
 * (or any later version) of the GNU General Public License. Some additional
 * terms also apply to certain portions of SWIG. The full details of the SWIG
 * license and copyrights can be found in the LICENSE and COPYRIGHT files
 * included with the SWIG source code as distributed by the SWIG developers
 * and at https://www.swig.org/legal.html.
 *
 * pydocutils.h
 *
 * Module to return documentation for nodes formatted for NumPy style
 * NumPy Doc style guide can be found here:
 * https://numpydoc.readthedocs.io/en/latest/format.html
 * ----------------------------------------------------------------------------- */

#include "doxyparser.h"
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

#include "swigmod.h"

using std::string;

// Helper class increasing the provided indent string in its ctor and decreasing
// it in its dtor.
class IndentGuard {
public:
  // One indent level.
  static const char *Level() {
    return "    ";
  }
  // Default ctor doesn't do anything and prevents the dtor from doing anything
  // too and should only be used when the guard needs to be initialized
  // conditionally as Init() can then be called after checking some condition.
  // Otherwise, prefer to use the non default ctor below.
  IndentGuard() {
    m_initialized = false;
  }

  // Ctor takes the output to determine the current indent and to remove the
  // extra indent added to it in the dtor and the variable containing the indent
  // to use, which must be used after every new line by the code actually
  // updating the output.
  IndentGuard(string &output, string &indent) {
    Init(output, indent);
  }

  // Really initializes the object created using the default ctor.
  void Init(string &output, string &indent) {
    m_output = &output;
    m_indent = &indent;

    const string::size_type lastNonSpace = m_output->find_last_not_of(' ');
    if (lastNonSpace == string::npos) {
      m_firstLineIndent = m_output->length();
    } else if ((*m_output)[lastNonSpace] == '\n') {
      m_firstLineIndent = m_output->length() - (lastNonSpace + 1);
    } else {
      m_firstLineIndent = 0;
    }

    // Notice that the indent doesn't include the first line indent because it's
    // implicit, i.e. it is present in the input and so is copied into the
    // output anyhow.
    *m_indent = Level();

    m_initialized = true;
  }

  // Get the indent for the first line of the paragraph, which is smaller than
  // the indent for the subsequent lines.
  string getFirstLineIndent() const {
    return string(m_firstLineIndent, ' ');
  }

  ~IndentGuard() {
    if (!m_initialized)
      return;

    m_indent->clear();

    // Get rid of possible remaining extra indent, e.g. if there were any trailing
    // new lines: we shouldn't add the extra indent level to whatever follows
    // this paragraph.
    static const size_t lenIndentLevel = strlen(Level());
    if (m_output->length() > lenIndentLevel) {
      const size_t start = m_output->length() - lenIndentLevel;
      if (m_output->compare(start, string::npos, Level()) == 0)
        m_output->erase(start);
    }
  }

private:
  string *m_output;
  string *m_indent;
  string::size_type m_firstLineIndent;
  bool m_initialized;

  IndentGuard(const IndentGuard &);
  IndentGuard &operator=(const IndentGuard &);
};

// Return the indent of the given multiline string, i.e. the maximal number of
// spaces present in the beginning of all its non-empty lines.
static size_t determineIndent(const string &s) {
  size_t minIndent = static_cast<size_t>(-1);

  for (size_t lineStart = 0; lineStart < s.length();) {
    const size_t lineEnd = s.find('\n', lineStart);
    const size_t firstNonSpace = s.find_first_not_of(' ', lineStart);

    // If inequality doesn't hold, it means that this line contains only spaces
    // (notice that this works whether lineEnd is valid or string::npos), in
    // which case it doesn't matter when determining the indent.
    if (firstNonSpace < lineEnd) {
      // Here we can be sure firstNonSpace != string::npos.
      const size_t lineIndent = firstNonSpace - lineStart;
      if (lineIndent < minIndent)
        minIndent = lineIndent;
    }

    if (lineEnd == string::npos)
      break;

    lineStart = lineEnd + 1;
  }

  return minIndent;
}

static void trimLeadingWhitespace(string &s) {
  const string::size_type firstNonSpace = s.find_first_not_of(' ');
  if (firstNonSpace == string::npos)
    s.clear();
  else
    s.erase(0, firstNonSpace);
}

static void trimWhitespace(string &s) {
  const string::size_type lastNonSpace = s.find_last_not_of(' ');
  if (lastNonSpace == string::npos)
    s.clear();
  else
    s.erase(lastNonSpace + 1);
}

// Erase the first character in the string if it is a newline
static void eraseLeadingNewLine(string &s) {
  if (!s.empty() && s[0] == '\n')
    s.erase(s.begin());
}

// Erase the last character in the string if it is a newline
static void eraseTrailingNewLine(string &s) {
  if (!s.empty() && s[s.size() - 1] == '\n')
    s.erase(s.size() - 1);
}

// Check the generated docstring line by line and make sure that any
// code and verbatim blocks have an empty line preceding them, which
// is necessary for Sphinx.  Additionally, this strips any empty lines
// appearing at the beginning of the docstring.
static string padCodeAndVerbatimBlocks(const string &docString) {
  std::string result;

  std::istringstream iss(docString);

  // Initialize to false because there is no previous line yet
  bool lastLineWasNonBlank = false;

  for (string line; std::getline(iss, line); result += line) {
    if (!result.empty()) {
      // Terminate the previous line
      result += '\n';
    }

    const size_t pos = line.find_first_not_of(" \t");
    if (pos == string::npos) {
      lastLineWasNonBlank = false;
    } else {
      if (lastLineWasNonBlank && (line.compare(pos, 13, ".. code-block") == 0 || line.compare(pos, 7, ".. math") == 0 || line.compare(pos, 3, ">>>") == 0)) {
        // Must separate code or math blocks from the previous line
        result += '\n';
      }
      lastLineWasNonBlank = true;
    }
  }
  return result;
}

// Helper function to extract the option value from a command,
// e.g. param[in] -> in
static std::string getCommandOption(const std::string &command, char openChar, char closeChar) {
  string option;

  size_t opt_begin, opt_end;
  opt_begin = command.find(openChar);
  opt_end = command.find(closeChar);
  if (opt_begin != string::npos && opt_end != string::npos)
    option = command.substr(opt_begin + 1, opt_end - opt_begin - 1);

  return option;
}
