/* -----------------------------------------------------------------------------
 * This file is part of SWIG, which is licensed as a whole under version 3
 * (or any later version) of the GNU General Public License. Some additional
 * terms also apply to certain portions of SWIG. The full details of the SWIG
 * license and copyrights can be found in the LICENSE and COPYRIGHT files
 * included with the SWIG source code as distributed by the SWIG developers
 * and at https://www.swig.org/legal.html.
 *
 * numpydoc.cxx
 *
 * Module to return documentation for nodes formatted for NumPy style
 * NumPy Doc style guide can be found here:
 * https://numpydoc.readthedocs.io/en/latest/format.html
 * ----------------------------------------------------------------------------- */

#include "numpydoc.h"
#include "doxyparser.h"
#include "pydocutils.h"
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

#include "swigmod.h"

// define static tables, they are filled in NumPyDocConverter's constructor
NumPyDocConverter::TagHandlersMap NumPyDocConverter::tagHandlers;
std::map<std::string, std::string> NumPyDocConverter::sectionTitles;

using std::string;


/* static */
NumPyDocConverter::TagHandlersMap::mapped_type NumPyDocConverter::make_handler(tagHandler handler) {
  return make_pair(handler, std::string());
}

/* static */
NumPyDocConverter::TagHandlersMap::mapped_type NumPyDocConverter::make_handler(tagHandler handler, const char *arg) {
  return make_pair(handler, arg);
}

void NumPyDocConverter::fillStaticTables() {
  if (tagHandlers.size())  // fill only once
    return;

  // table of section titles, they are printed only once
  // for each group of specified doxygen commands
  sectionTitles["author"] = "Author: ";
  sectionTitles["authors"] = "Authors: ";
  sectionTitles["copyright"] = "Copyright: ";
  sectionTitles["deprecated"] = "Deprecated: ";
  sectionTitles["example"] = "Example: ";
  sectionTitles["note"] = "Notes: ";
  sectionTitles["remark"] = "Remarks: ";
  sectionTitles["remarks"] = "Remarks: ";
  sectionTitles["warning"] = "Warning: ";
  //  sectionTitles["sa"] = "See also: ";
  //  sectionTitles["see"] = "See also: ";
  sectionTitles["since"] = "Since: ";
  sectionTitles["todo"] = "TODO: ";
  sectionTitles["version"] = "Version: ";

  tagHandlers["a"] = make_handler(&NumPyDocConverter::handleTagWrap, "*");
  tagHandlers["b"] = make_handler(&NumPyDocConverter::handleTagWrap, "**");
  // \c command is translated as single quotes around next word
  tagHandlers["c"] = make_handler(&NumPyDocConverter::handleTagWrap, "``");
  tagHandlers["cite"] = make_handler(&NumPyDocConverter::handleTagWrap, "'");
  tagHandlers["e"] = make_handler(&NumPyDocConverter::handleTagWrap, "*");
  // these commands insert just a single char, some of them need to be escaped
  tagHandlers["$"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["@"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["\\"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["<"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers[">"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["&"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["#"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["%"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["~"] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["\""] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["."] = make_handler(&NumPyDocConverter::handleTagChar);
  tagHandlers["::"] = make_handler(&NumPyDocConverter::handleTagChar);
  // these commands are stripped out, and only their content is printed
  tagHandlers["attention"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["author"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["authors"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["brief"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["bug"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["code"] = make_handler(&NumPyDocConverter::handleCode);
  tagHandlers["copyright"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["date"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["deprecated"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["details"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["em"] = make_handler(&NumPyDocConverter::handleTagWrap, "*");
  tagHandlers["example"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["exception"] = tagHandlers["throw"] = tagHandlers["throws"] = make_handler(&NumPyDocConverter::handleTagException);
  tagHandlers["htmlonly"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["invariant"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["latexonly"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["link"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["manonly"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["note"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["p"] = make_handler(&NumPyDocConverter::handleTagWrap, "``");
  tagHandlers["partofdescription"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["rtfonly"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["remark"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["remarks"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["sa"] = make_handler(&NumPyDocConverter::handleTagMessage, "See also: ");
  tagHandlers["see"] = make_handler(&NumPyDocConverter::handleTagMessage, "See also: ");
  tagHandlers["since"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["short"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["todo"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["version"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["verbatim"] = make_handler(&NumPyDocConverter::handleVerbatimBlock);
  tagHandlers["warning"] = make_handler(&NumPyDocConverter::handleParagraph);
  tagHandlers["xmlonly"] = make_handler(&NumPyDocConverter::handleParagraph);
  // these commands have special handlers
  tagHandlers["arg"] = make_handler(&NumPyDocConverter::handleTagMessage, "* ");
  tagHandlers["cond"] = make_handler(&NumPyDocConverter::handleTagMessage, "Conditional comment: ");
  tagHandlers["else"] = make_handler(&NumPyDocConverter::handleTagIf, "Else: ");
  tagHandlers["elseif"] = make_handler(&NumPyDocConverter::handleTagIf, "Else if: ");
  tagHandlers["endcond"] = make_handler(&NumPyDocConverter::handleTagMessage, "End of conditional comment.");
  tagHandlers["if"] = make_handler(&NumPyDocConverter::handleTagIf, "If: ");
  tagHandlers["ifnot"] = make_handler(&NumPyDocConverter::handleTagIf, "If not: ");
  tagHandlers["image"] = make_handler(&NumPyDocConverter::handleTagImage);
  tagHandlers["li"] = make_handler(&NumPyDocConverter::handleTagMessage, "* ");
  tagHandlers["overload"] = make_handler(&NumPyDocConverter::handleTagMessage,
                                         "This is an overloaded member function, provided for"
                                         " convenience.\nIt differs from the above function only in what"
                                         " argument(s) it accepts.");
  tagHandlers["par"] = make_handler(&NumPyDocConverter::handleTagPar);
  tagHandlers["param"] = tagHandlers["tparam"] = make_handler(&NumPyDocConverter::handleTagParam);
  tagHandlers["ref"] = make_handler(&NumPyDocConverter::handleTagRef);
  tagHandlers["result"] = tagHandlers["return"] = tagHandlers["returns"] = make_handler(&NumPyDocConverter::handleTagReturn);

  // this command just prints its contents
  // (it is internal command of swig's parser, contains plain text)
  tagHandlers["plainstd::string"] = make_handler(&NumPyDocConverter::handlePlainString);
  tagHandlers["plainstd::endl"] = make_handler(&NumPyDocConverter::handleNewLine);
  tagHandlers["n"] = make_handler(&NumPyDocConverter::handleNewLine);

  // \f commands output literal Latex formula, which is still better than nothing.
  tagHandlers["f$"] = tagHandlers["f["] = tagHandlers["f{"] = make_handler(&NumPyDocConverter::handleMath);

  // HTML tags
  tagHandlers["<a"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag_A);
  tagHandlers["<b"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "**");
  tagHandlers["<blockquote"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag_A, "Quote: ");
  tagHandlers["<body"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<br"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "\n");

  // there is no formatting for this tag as it was deprecated in HTML 4.01 and
  // not used in HTML 5
  tagHandlers["<center"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<caption"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<code"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "``");

  tagHandlers["<dl"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<dd"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "    ");
  tagHandlers["<dt"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);

  tagHandlers["<dfn"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<div"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<em"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "**");
  tagHandlers["<form"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<hr"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "--------------------------------------------------------------------\n");
  tagHandlers["<h1"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "# ");
  tagHandlers["<h2"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "## ");
  tagHandlers["<h3"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "### ");
  tagHandlers["<i"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "*");
  tagHandlers["<input"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<img"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "Image:");
  tagHandlers["<li"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "* ");
  tagHandlers["<meta"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<multicol"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<ol"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<p"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, "\n");
  tagHandlers["<pre"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<small"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<span"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "'");
  tagHandlers["<strong"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "**");

  // make a space between text and super/sub script.
  tagHandlers["<sub"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, " ");
  tagHandlers["<sup"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag, " ");

  tagHandlers["<table"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTagNoParam);
  tagHandlers["<td"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag_td);
  tagHandlers["<th"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag_th);
  tagHandlers["<tr"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag_tr);
  tagHandlers["<tt"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<kbd"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<ul"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag);
  tagHandlers["<var"] = make_handler(&NumPyDocConverter::handleDoxyHtmlTag2, "*");

  // HTML entities
  tagHandlers["&copy"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "(C)");
  tagHandlers["&trade"] = make_handler(&NumPyDocConverter::handleHtmlEntity, " TM");
  tagHandlers["&reg"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "(R)");
  tagHandlers["&lt"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "<");
  tagHandlers["&gt"] = make_handler(&NumPyDocConverter::handleHtmlEntity, ">");
  tagHandlers["&amp"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "&");
  tagHandlers["&apos"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "'");
  tagHandlers["&quot"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "\"");
  tagHandlers["&lsquo"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "`");
  tagHandlers["&rsquo"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "'");
  tagHandlers["&ldquo"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "\"");
  tagHandlers["&rdquo"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "\"");
  tagHandlers["&ndash"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "-");
  tagHandlers["&mdash"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "--");
  tagHandlers["&nbsp"] = make_handler(&NumPyDocConverter::handleHtmlEntity, " ");
  tagHandlers["&times"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "x");
  tagHandlers["&minus"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "-");
  tagHandlers["&sdot"] = make_handler(&NumPyDocConverter::handleHtmlEntity, ".");
  tagHandlers["&sim"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "~");
  tagHandlers["&le"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "<=");
  tagHandlers["&ge"] = make_handler(&NumPyDocConverter::handleHtmlEntity, ">=");
  tagHandlers["&larr"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "<--");
  tagHandlers["&rarr"] = make_handler(&NumPyDocConverter::handleHtmlEntity, "-->");
}

NumPyDocConverter::NumPyDocConverter(int flags) : DoxygenTranslator(flags), m_tableLineLen(0), m_prevRowIsTH(false) {
  fillStaticTables();
  printf("NumPyDocConverter\n");
}

// Return the type as it should appear in the output documentation.
static std::string getPyDocType(Node *n, const_String_or_char_ptr lname = "") {
  std::string type;

  String *s = Swig_typemap_lookup("doctype", n, lname, 0);
  if (!s) {
    if (String *t = Getattr(n, "type"))
      s = SwigType_str(t, NULL);
  }

  if (!s)
    return type;

  if (Language::classLookup(s)) {
    // In Python C++ namespaces are flattened, so remove all but last component
    // of the name.
    String *const last = Swig_scopename_last(s);

    // We are not actually sure whether it's a documented class or not, but
    // there doesn't seem to be any harm in making it a reference if it isn't,
    // while there is a lot of benefit in having a hyperlink if it is.
    type = ":py:class:`";
    type += Char(last);
    type += "`";

    Delete(last);
  } else {
    type = Char(s);
  }

  Delete(s);

  return type;
}

std::string NumPyDocConverter::getParamType(std::string param) {
  std::string type;

  ParmList *plist = CopyParmList(Getattr(currentNode, "parms"));
  for (Parm *p = plist; p; p = nextSibling(p)) {
    String *pname = Getattr(p, "name");
    if (pname && Char(pname) == param) {
      type = getPyDocType(p, pname);
      break;
    }
  }
  Delete(plist);
  return type;
}

std::string NumPyDocConverter::getParamValue(std::string param) {
  std::string value;

  ParmList *plist = CopyParmList(Getattr(currentNode, "parms"));
  for (Parm *p = plist; p; p = nextSibling(p)) {
    String *pname = Getattr(p, "name");
    if (pname && Char(pname) == param) {
      String *pval = Getattr(p, "value");
      if (pval)
        value = Char(pval);
      break;
    }
  }
  Delete(plist);
  return value;
}

std::string NumPyDocConverter::translateSubtree(DoxygenEntity &doxygenEntity) {

  std::cout << "translateSubtree '" << doxygenEntity.typeOfEntity << "' '" << doxygenEntity.data << "'" << std::endl;

  std::string translatedComment;

  if (doxygenEntity.isLeaf)
    return translatedComment;

  std::string currentSection;
  std::list<DoxygenEntity>::iterator p = doxygenEntity.entityList.begin();
  while (p != doxygenEntity.entityList.end()) {
    std::map<std::string, std::string>::iterator it;
    it = sectionTitles.find(p->typeOfEntity);
    if (it != sectionTitles.end()) {
      if (it->second != currentSection) {
        currentSection = it->second;
        translatedComment += currentSection;
      }
    }
    translateEntity(*p, translatedComment);
    translateSubtree(*p);
    p++;
  }

  return translatedComment;
}

void NumPyDocConverter::translateEntity(DoxygenEntity &doxyEntity, std::string &translatedComment) {
  // check if we have needed handler and call it
  std::map<std::string, std::pair<tagHandler, std::string> >::iterator it;
  it = tagHandlers.find(getBaseCommand(doxyEntity.typeOfEntity));
  if (it != tagHandlers.end())
    (this->*(it->second.first))(doxyEntity, translatedComment, it->second.second);
}

void NumPyDocConverter::handleParagraph(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  translatedComment += translateSubtree(tag);
}

void NumPyDocConverter::handleVerbatimBlock(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  string verb = translateSubtree(tag);

  eraseLeadingNewLine(verb);

  // Remove the last newline to prevent doubling the newline already present after \endverbatim
  trimWhitespace(verb);  // Needed to catch trailing newline below
  eraseTrailingNewLine(verb);
  translatedComment += verb;
}

void NumPyDocConverter::handleMath(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  IndentGuard indent;

  // Only \f$ is translated to inline formulae, \f[ and \f{ are for the block ones.
  const bool inlineFormula = tag.typeOfEntity == "f$";

  string formulaNL;

  if (inlineFormula) {
    translatedComment += ":math:`";
  } else {
    indent.Init(translatedComment, m_indent);

    trimWhitespace(translatedComment);

    const string formulaIndent = indent.getFirstLineIndent();
    translatedComment += formulaIndent;
    translatedComment += ".. math::\n";

    formulaNL = '\n';
    formulaNL += formulaIndent;
    formulaNL += m_indent;
    translatedComment += formulaNL;
  }

  std::string formula;
  handleTagVerbatim(tag, formula, arg);

  // It is important to ensure that we have no spaces around the inline math
  // contents, so strip them.
  const size_t start = formula.find_first_not_of(" \t\n");
  const size_t end = formula.find_last_not_of(" \t\n");
  if (start != std::string::npos) {
    for (size_t n = start; n <= end; n++) {
      if (formula[n] == '\n') {
        // New lines must be suppressed in inline maths and indented in the block ones.
        if (!inlineFormula)
          translatedComment += formulaNL;
      } else {
        // Just copy everything else.
        translatedComment += formula[n];
      }
    }
  }

  if (inlineFormula) {
    translatedComment += "`";
  }
}

void NumPyDocConverter::handleCode(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  IndentGuard indent(translatedComment, m_indent);

  trimWhitespace(translatedComment);

  // Check for an option given to the code command (e.g. code{.py}),
  // and try to set the code-block language accordingly.
  string option = getCommandOption(tag.typeOfEntity, '{', '}');
  // Set up the language option to the code-block command, which can
  // be any language supported by pygments:
  string codeLanguage;
  if (option == ".py")
    // Other possibilities here are "default" or "python3".  In Sphinx
    // 2.1.2, basic syntax doesn't render quite the same in these as
    // with "python", which for basic keywords seems to provide
    // slightly richer formatting.  Another option would be to leave
    // the language empty, but testing with Sphinx 1.8.5 has produced
    // an error "1 argument required".
    codeLanguage = "python";
  else if (option == ".java")
    codeLanguage = "java";
  else if (option == ".c")
    codeLanguage = "c";
  else
    // If there is not a match, or if no option was given, go out on a
    // limb and assume that the examples in the C or C++ sources use
    // C++.
    codeLanguage = "c++";

  std::string code;
  handleTagVerbatim(tag, code, arg);

  // Try and remove leading newline, which is present for block \code
  // command:
  eraseLeadingNewLine(code);

  // Check for python doctest blocks, and treat them specially:
  bool isDocTestBlock = false;
  size_t startPos;
  // ">>>" would normally appear at the beginning, but doxygen comment
  // style may have space in front, so skip leading whitespace
  if ((startPos = code.find_first_not_of(" \t")) != string::npos && code.substr(startPos, 3) == ">>>")
    isDocTestBlock = true;

  string codeIndent;
  if (!isDocTestBlock) {
    // Use the current indent for the code-block line itself.
    translatedComment += indent.getFirstLineIndent();
    translatedComment += ".. code-block:: " + codeLanguage + "\n\n";

    // Specify the level of extra indentation that will be used for
    // subsequent lines within the code block.  Note that the correct
    // "starting indentation" is already present in the input, so we
    // only need to add the desired code block indentation.
    codeIndent = m_indent;
  }

  translatedComment += codeIndent;
  for (size_t n = 0; n < code.length(); n++) {
    if (code[n] == '\n') {
      // Don't leave trailing white space, this results in PEP8 validation
      // errors in Python code (which are performed by our own unit tests).
      trimWhitespace(translatedComment);
      translatedComment += '\n';

      // Ensure that we indent all the lines by the code indent.
      translatedComment += codeIndent;
    } else {
      // Just copy everything else.
      translatedComment += code[n];
    }
  }

  trimWhitespace(translatedComment);

  // For block commands, the translator adds the newline after
  // \endcode, so try and compensate by removing the last newline from
  // the code text:
  eraseTrailingNewLine(translatedComment);
}

void NumPyDocConverter::handlePlainString(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  trimLeadingWhitespace(tag.data);
  translatedComment += tag.data;
}

void NumPyDocConverter::handleTagVerbatim(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  translatedComment += arg;
  for (DoxygenEntityListCIt it = tag.entityList.begin(); it != tag.entityList.end(); it++) {
    translatedComment += it->data;
  }
}

void NumPyDocConverter::handleTagMessage(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  translatedComment += arg;
  handleParagraph(tag, translatedComment);
}

void NumPyDocConverter::handleTagChar(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  translatedComment += tag.typeOfEntity;
}

void NumPyDocConverter::handleTagIf(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  translatedComment += arg;
  if (tag.entityList.size()) {
    translatedComment += tag.entityList.begin()->data;
    tag.entityList.pop_front();
    translatedComment += " {" + translateSubtree(tag) + "}";
  }
}

void NumPyDocConverter::handleTagPar(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  translatedComment += "Title: ";
  if (tag.entityList.size())
    translatedComment += tag.entityList.begin()->data;
  tag.entityList.pop_front();
  handleParagraph(tag, translatedComment);
}

void NumPyDocConverter::handleTagImage(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  if (tag.entityList.size() < 2)
    return;
  tag.entityList.pop_front();
  translatedComment += "Image: ";
  translatedComment += tag.entityList.begin()->data;
  tag.entityList.pop_front();
  if (tag.entityList.size())
    translatedComment += "(" + tag.entityList.begin()->data + ")";
}

void NumPyDocConverter::handleTagParam(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  if (tag.entityList.size() < 2)
    return;

  IndentGuard indent(translatedComment, m_indent);

  DoxygenEntity paramNameEntity = *tag.entityList.begin();
  tag.entityList.pop_front();

  const std::string &paramName = paramNameEntity.data;

  const std::string paramType = getParamType(paramName);
  const std::string paramValue = getParamValue(paramName);

  // Get command option, e.g. "in", "out", or "in,out"
  string commandOpt = getCommandOption(tag.typeOfEntity, '[', ']');
  if (commandOpt == "in,out")
    commandOpt = "in/out";

  // If provided, append the parameter direction to the type
  // information via a suffix:
  std::string suffix;
  if (commandOpt.size() > 0)
    suffix = ", " + commandOpt;

  // If the parameter has a default value, flag it as optional in the
  // generated type definition.  Particularly helpful when the python
  // call is generated with *args, **kwargs.
  if (paramValue.size() > 0)
    suffix += ", optional";

  translatedComment += paramName;

  if (!paramType.empty()) {
    translatedComment += ": " + paramType + suffix;
  }

  translatedComment += "\n" + m_indent;

  handleParagraph(tag, translatedComment);
}

void NumPyDocConverter::handleTagReturn(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  IndentGuard indent(translatedComment, m_indent);

  const std::string pytype = getPyDocType(currentNode);
  if (!pytype.empty()) {
    translatedComment += ":rtype: ";
    translatedComment += pytype;
    translatedComment += "\n";
    translatedComment += indent.getFirstLineIndent();
  }

  translatedComment += ":return: ";
  handleParagraph(tag, translatedComment);
}

void NumPyDocConverter::handleTagException(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  IndentGuard indent(translatedComment, m_indent);

  translatedComment += ":raises: ";
  handleParagraph(tag, translatedComment);
}

void NumPyDocConverter::handleTagRef(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  if (!tag.entityList.size())
    return;

  string anchor = tag.entityList.begin()->data;
  tag.entityList.pop_front();
  string anchorText = anchor;
  if (!tag.entityList.empty()) {
    anchorText = tag.entityList.begin()->data;
  }
  translatedComment += "'" + anchorText + "'";
}

void NumPyDocConverter::handleTagWrap(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  if (tag.entityList.size()) {  // do not include empty tags
    std::string tagData = translateSubtree(tag);
    // wrap the thing, ignoring whitespace
    size_t wsPos = tagData.find_last_not_of("\n\t ");
    if (wsPos != std::string::npos && wsPos != tagData.size() - 1)
      translatedComment += arg + tagData.substr(0, wsPos + 1) + arg + tagData.substr(wsPos + 1);
    else
      translatedComment += arg + tagData + arg;
  }
}

void NumPyDocConverter::handleDoxyHtmlTag(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  std::string htmlTagArgs = tag.data;
  if (htmlTagArgs == "/") {
    // end html tag, for example "</ul>
    // translatedComment += "</" + arg.substr(1) + ">";
  } else {
    translatedComment += arg + htmlTagArgs;
  }
}

void NumPyDocConverter::handleDoxyHtmlTagNoParam(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  std::string htmlTagArgs = tag.data;
  if (htmlTagArgs == "/") {
    // end html tag, for example "</ul>
  } else {
    translatedComment += arg;
  }
}

void NumPyDocConverter::handleDoxyHtmlTag_A(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  std::string htmlTagArgs = tag.data;
  if (htmlTagArgs == "/") {
    // end html tag, "</a>
    translatedComment += " (" + m_url + ')';
    m_url.clear();
  } else {
    m_url.clear();
    size_t pos = htmlTagArgs.find('=');
    if (pos != string::npos) {
      m_url = htmlTagArgs.substr(pos + 1);
    }
    translatedComment += arg;
  }
}

void NumPyDocConverter::handleDoxyHtmlTag2(DoxygenEntity &tag, std::string &translatedComment, const std::string &arg) {
  std::string htmlTagArgs = tag.data;
  if (htmlTagArgs == "/") {
    // end html tag, for example "</em>
    translatedComment += arg;
  } else {
    translatedComment += arg;
  }
}

void NumPyDocConverter::handleDoxyHtmlTag_tr(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  std::string htmlTagArgs = tag.data;
  size_t nlPos = translatedComment.rfind('\n');
  if (htmlTagArgs == "/") {
    // end tag, </tr> appends vertical table line '|'
    translatedComment += '|';
    if (nlPos != string::npos) {
      size_t startOfTableLinePos = translatedComment.find_first_not_of(" \t", nlPos + 1);
      if (startOfTableLinePos != string::npos) {
        m_tableLineLen = translatedComment.size() - startOfTableLinePos;
      }
    }
  } else {
    if (m_prevRowIsTH) {
      // if previous row contained <th> tag, add horizontal separator
      // but first get leading spaces, because they'll be needed for the next row
      size_t numLeadingSpaces = translatedComment.size() - nlPos - 1;

      translatedComment += string(m_tableLineLen, '-') + '\n';

      if (nlPos != string::npos) {
        translatedComment += string(numLeadingSpaces, ' ');
      }
      m_prevRowIsTH = false;
    }
  }
}

void NumPyDocConverter::handleDoxyHtmlTag_th(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  std::string htmlTagArgs = tag.data;
  if (htmlTagArgs == "/") {
    // end tag, </th> is ignored
  } else {
    translatedComment += '|';
    m_prevRowIsTH = true;
  }
}

void NumPyDocConverter::handleDoxyHtmlTag_td(DoxygenEntity &tag, std::string &translatedComment, const std::string &) {
  std::string htmlTagArgs = tag.data;
  if (htmlTagArgs == "/") {
    // end tag, </td> is ignored
  } else {
    translatedComment += '|';
  }
}

void NumPyDocConverter::handleHtmlEntity(DoxygenEntity &, std::string &translatedComment, const std::string &arg) {
  // html entities
  translatedComment += arg;
}

void NumPyDocConverter::handleNewLine(DoxygenEntity &, std::string &translatedComment, const std::string &) {
  trimWhitespace(translatedComment);

  translatedComment += "\n";
  if (!m_indent.empty())
    translatedComment += m_indent;
}

String *NumPyDocConverter::makeDocumentation(Node *n) {
  String *documentation;
  std::string pyDocString;

  // store the node, we may need it later
  currentNode = n;

  // for overloaded functions we must concat documentation for underlying overloads
  if (Getattr(n, "sym:overloaded")) {
    // rewind to the first overload
    while (Getattr(n, "sym:previousSibling"))
      n = Getattr(n, "sym:previousSibling");

    std::vector<std::string> allDocumentation;

    // minimal indent of any documentation comments, not initialized yet
    size_t minIndent = static_cast<size_t>(-1);

    // for each real method (not a generated overload) append the documentation
    string oneDoc;
    while (n) {
      documentation = getDoxygenComment(n);
      if (!Swig_is_generated_overload(n) && documentation) {
        currentNode = n;
        if (GetFlag(n, "feature:doxygen:notranslate")) {
          String *comment = NewString("");
          Append(comment, documentation);
          Replaceall(comment, "\n *", "\n");
          oneDoc = Char(comment);
          Delete(comment);
        } else {
          std::list<DoxygenEntity> entityList = parser.createTree(n, documentation);
          DoxygenEntity root("root", entityList);

          oneDoc = translateSubtree(root);
        }

        // find the minimal indent of this documentation comment, we need to
        // ensure that the entire comment is indented by it to avoid the leading
        // parts of the other lines being simply discarded later
        const size_t oneIndent = determineIndent(oneDoc);
        if (oneIndent < minIndent)
          minIndent = oneIndent;

        allDocumentation.push_back(oneDoc);
      }
      n = Getattr(n, "sym:nextSibling");
    }

    // construct final documentation string
    if (allDocumentation.size() > 1) {
      string indentStr;
      if (minIndent != static_cast<size_t>(-1))
        indentStr.assign(minIndent, ' ');

      std::ostringstream concatDocString;
      for (size_t realOverloadCount = 0; realOverloadCount < allDocumentation.size(); realOverloadCount++) {
        if (realOverloadCount != 0) {
          // separate it from the preceding one.
          concatDocString << '\n' << indentStr << "|\n\n";
        }

        oneDoc = allDocumentation[realOverloadCount];
        trimWhitespace(oneDoc);
        concatDocString << indentStr << "*Overload " << (realOverloadCount + 1) << ":*\n" << oneDoc;
      }
      pyDocString = concatDocString.str();
    } else if (allDocumentation.size() == 1) {
      pyDocString = *(allDocumentation.begin());
    }
  }
  // for other nodes just process as normal
  else {
    documentation = getDoxygenComment(n);
    if (documentation != NULL) {
      if (GetFlag(n, "feature:doxygen:notranslate")) {
        String *comment = NewString("");
        Append(comment, documentation);
        Replaceall(comment, "\n *", "\n");
        pyDocString = Char(comment);
        Delete(comment);
      } else {
        std::list<DoxygenEntity> entityList = parser.createTree(n, documentation);
        DoxygenEntity root("root", entityList);
        pyDocString = translateSubtree(root);
      }
    }
  }

  // if we got something log the result
  if (!pyDocString.empty()) {

    // remove the last '\n' since additional one is added during writing to file
    eraseTrailingNewLine(pyDocString);

    // ensure that a blank line occurs before code or math blocks
    pyDocString = padCodeAndVerbatimBlocks(pyDocString);

    if (m_flags & debug_translator) {
      std::cout << "\n---RESULT IN PYDOC---" << std::endl;
      std::cout << pyDocString;
      std::cout << std::endl;
    }
  }

  return NewString(pyDocString.c_str());
}
