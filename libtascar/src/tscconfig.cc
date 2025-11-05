/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2021 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tscconfig.h"
#include "errorhandling.h"
#include "tictoctimer.h"
#include <atomic>
#include <fstream>
#include <functional>
#include <mutex>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

class xml_init_t {
public:
  xml_init_t() { xercesc::XMLPlatformUtils::Initialize(); };
  ~xml_init_t() { xercesc::XMLPlatformUtils::Terminate(); };
};

static xml_init_t xercesc_init;

static std::string tascar_libdir;

void TASCAR::set_libdir(const std::string& s)
{
  tascar_libdir = s;
}

const std::string& TASCAR::get_libdir()
{
  return tascar_libdir;
}

std::basic_string<XMLCh> str2wstr(const char* text)
{
  XMLCh* resarr(xercesc::XMLString::transcode(text));
  std::basic_string<XMLCh> result(resarr);
  xercesc::XMLString::release(&resarr);
  return result;
}

std::basic_string<XMLCh> str2wstr(const std::string& text)
{
  XMLCh* resarr(xercesc::XMLString::transcode(text.c_str()));
  std::basic_string<XMLCh> result(resarr);
  xercesc::XMLString::release(&resarr);
  return result;
}

std::string wstr2str(const XMLCh* text)
{
  char* resarr(xercesc::XMLString::transcode(text));
  std::string result(resarr);
  xercesc::XMLString::release(&resarr);
  return result;
}

namespace TASCAR {

  class globalconfig_t {
  public:
    globalconfig_t();
    double operator()(const std::string&, double) const;
    std::string operator()(const std::string&, const std::string&) const;
    void forceoverwrite(const std::string&, const std::string&);
    void writeconfig(const std::vector<std::string>& keys);
    ~globalconfig_t();

  private:
    void readconfig(const std::string& fname);
    void readconfig(const std::string& prefix, tsccfg::node_t& e);
    void setxmlconfig(const std::string& key, tsccfg::node_t& e,
                      const std::string& val);
    std::map<std::string, std::string> cfg;
  };

  static std::map<std::string, cfg_node_desc_t> attribute_list;
  static std::vector<std::string> warnings;
  static globalconfig_t config_;
  static std::atomic_size_t maxid(0);
  static std::set<std::string> tuidlist;
  std::mutex tuidlistmtx;

  class console_log_t {
  public:
    class log_entry_t {
    public:
      log_entry_t(double t, const std::string& msg) : t(t), msg(msg){};
      double t = 0.0;
      const std::string msg;
    };
    void log(const std::string& msg)
    {
      std::lock_guard<std::mutex> lk{mtx};
      double t = tictoc.toc();
      logs.push_back(log_entry_t(t, msg));
      if(showlog)
        std::fprintf(stderr, "%8.3f %s\n", t, msg.c_str());
    };
    bool showlog = false;

  private:
    TASCAR::tictoc_t tictoc;
    std::vector<log_entry_t> logs;
    std::mutex mtx;
  };

  static console_log_t log;
} // namespace TASCAR

void TASCAR::console_log(const std::string& msg)
{
  TASCAR::log.log(msg);
}

void TASCAR::console_log_show(bool show)
{
  TASCAR::log.showlog = show;
}

double TASCAR::config(const std::string& v, double d)
{
  return TASCAR::config_(v, d);
}

std::string TASCAR::config(const std::string& v, const std::string& d)
{
  return TASCAR::config_(v, d);
}

void TASCAR::config_save_keys(const std::vector<std::string>& keys)
{
  TASCAR::config_.writeconfig(keys);
}

void TASCAR::config_forceoverwrite(const std::string& v, const std::string& d)
{
  TASCAR::config_.forceoverwrite(v, d);
}

std::map<std::string, TASCAR::cfg_node_desc_t>& TASCAR::get_attribute_list()
{
  return TASCAR::attribute_list;
}

struct elem_cfg_var_desc_t {
  std::string elem;
  std::map<std::string, TASCAR::cfg_var_desc_t> attr;
  std::set<std::string> parents;
  std::string type;
};

void make_common(std::map<std::string, std::set<std::string>>& parentchildren,
                 std::map<std::string, elem_cfg_var_desc_t>& elems,
                 const std::set<std::string>& list,
                 const std::string& commonname,
                 const std::set<std::string>& typeconstraint = {})
{
  if(list.size()) {
    for(auto ch : list)
      parentchildren[commonname].insert(ch);
    // first get a list of common attributes:
    std::set<std::string> common_attributes;
    for(auto attr : elems[(*(list.begin()))].attr)
      common_attributes.insert(attr.first);
    for(auto elem : list) {
      if(typeconstraint.empty() ||
         (typeconstraint.find(elems[elem].type) != typeconstraint.end())) {
        std::set<std::string> attrs;
        for(auto attr : elems[elem].attr)
          attrs.insert(attr.first);
        std::set<std::string> rmattrs;
        for(auto attr : common_attributes)
          if(attrs.find(attr) == attrs.end())
            rmattrs.insert(attr);
        for(auto attr : rmattrs)
          common_attributes.erase(attr);
      }
    }
    // attribute map for common object:
    std::map<std::string, TASCAR::cfg_var_desc_t> commonattr;
    for(auto attr : common_attributes) {
      // fill common attributes from first element in list:
      commonattr[attr] = elems[(*(list.begin()))].attr[attr];
      // remove common attributes from list:
      for(auto elem : list) {
        elems[elem].attr.erase(attr);
        elems[elem].parents.insert(commonname);
      }
    }
    std::set<std::string> parents;
    elems[commonname] = {commonname, commonattr, parents, ""};
  }
}

std::string TASCAR::to_latex(std::string s)
{
  s = TASCAR::strrep(s, "_", "\\_");
  s = TASCAR::strrep(s, "#", "\\#");
  return s;
}

void TASCAR::generate_plugin_documentation_tables(bool latex)
{
  std::map<std::string, std::string> tdesc;
  tdesc["float"] = "single precision floating point number";
  tdesc["double"] = "double precision floating point number";
  tdesc["int32"] = "signed 32 bit integer number";
  tdesc["uint32"] = "unsigned 32 bit integer number";
  tdesc["bool"] = "boolean, \"true\" or \"false\"";
  tdesc["string"] = "character array";
  tdesc["f-weight"] =
      "frequency weighting, one of \"Z\", \"A\", \"C\" or \"bandpass\"";
  tdesc["bits32"] = "list of numbers 0 to 31, or \"all\"";
  tdesc["pos"] =
      "Cartesian coordinates, triplet of doubles separated by spaces";
  tdesc["string array"] = "space separated array of strings";
  std::set<std::string> types;
  std::map<std::string, elem_cfg_var_desc_t> attribute_list;
  std::map<std::string, std::set<std::string>> categories;
  for(auto elem : TASCAR::get_attribute_list()) {
    std::string category(elem.second.category);
    std::string cattype(elem.second.type);
    std::map<std::string, cfg_var_desc_t> previousattr(
        attribute_list[category + cattype].attr);
    for(auto attr : elem.second.vars)
      previousattr[attr.first] = attr.second;
    attribute_list[category + cattype] = {category, previousattr, {}, cattype};
    categories[category].insert(category + cattype);
  }
  for(auto cat : categories) {
    std::cout << "category " << cat.first << std::endl;
    for(auto type : cat.second) {
      std::cout << "  " << type << std::endl;
    }
  }
  std::map<std::string, std::set<std::string>> parentchildren;
  make_common(parentchildren, attribute_list, categories["receiver"],
              "receiver");
  make_common(parentchildren, attribute_list, categories["reverb"], "reverb");
  make_common(parentchildren, attribute_list, categories["sound"], "sound");
  make_common(parentchildren, attribute_list,
              {"receiver", "source", "diffuse", "facegroup", "face",
               "boundingbox", "obstacle", "mask", "reverb"},
              "objects");
  make_common(parentchildren, attribute_list,
              {"receiver", "source", "diffuse", "facegroup", "face", "mask",
               "obstacle", "reverb"},
              "routes");
  make_common(parentchildren, attribute_list, {"receiver", "sound", "diffuse"},
              "ports");
  make_common(parentchildren, attribute_list,
              {"receiverhann", "receiverhoa3d", "receiverhoa2d",
               "receivervbap3d", "receivervbap", "receivernsp", "receiverwfs"},
              "speakerbased");
  for(auto elem : attribute_list) {
    for(auto attr : elem.second.attr)
      types.insert(attr.second.type);
    if(latex && (!elem.second.attr.empty())) {
      std::string fname("tab" + elem.first + ".tex");
      std::cout << "Creating latex table for " + elem.first + " in file " +
                       fname + "."
                << std::endl;
      std::ofstream fh(fname);
      fh << "\\definecolor{shadecolor}{RGB}{255,230,204}\\begin{snugshade}\n{"
            "\\footnotesize\n";
      fh << "\\label{attrtab:" << elem.first << "}\n";
      fh << "Attributes of ";
      if(elem.second.type.empty())
        fh << "element {\\bf " << TASCAR::to_latex(elem.first) + "}";
      else
        fh << TASCAR::to_latex(elem.second.elem) << " element {\\bf "
           << TASCAR::to_latex(elem.second.type) + "}";
      if(!parentchildren[elem.first].empty()) {
        fh << " (";
        size_t k(0);
        for(auto child : parentchildren[elem.first]) {
          if(k)
            fh << " ";
          std::string xchild(child);
          if(attribute_list[child].type.size())
            xchild = attribute_list[child].type;
          fh << "{\\hyperref[attrtab:" << child << "]{"
             << TASCAR::to_latex(xchild) << "}}";
          ++k;
        }
        fh << ")";
      }
      if(!elem.second.parents.empty()) {
        fh << ", inheriting from";
        for(auto parent : elem.second.parents)
          fh << " \\hyperref[attrtab:" << parent << "]{{\\bf "
             << TASCAR::to_latex(parent) << "}}";
      }
      fh << "\\nopagebreak\n\n";
      fh << "\\begin{tabularx}{\\textwidth}{l>{\\raggedright}XX}\n";

      fh << "\\hline\n";
      fh << "name & description (type, unit) & def.\\\\\n\\hline\n";
      for(auto attr : elem.second.attr) {
        //  \indattr{name} & type & def & unit & Name of session (default:
        //  ``tascar'')
        fh << "\\hline\n";
        auto s_info = TASCAR::to_latex(attr.second.info) + " (" +
                      TASCAR::to_latex(attr.second.type);
        if(!attr.second.unit.empty())
          s_info += ", " + TASCAR::to_latex(attr.second.unit);
        s_info += ") ";
        // if(s_info.size() > 40)
        //  s_info = "{\\tiny " + s_info + "}";
        fh << "\\indattr{" << TASCAR::to_latex(attr.first) << "} & " << s_info;
        std::string sdef(TASCAR::to_latex(attr.second.defaultval));
        if(sdef.size() > 24)
          sdef = "{\\tiny " + sdef + "}";
        fh << "& " << sdef << "\\\\" << std::endl;
      }
      fh << "\\hline\n\\end{tabularx}\n";
      fh << "}\n\\end{snugshade}\n";
    }
    std::cout << elem.first << " - " << elem.second.type;
    if(!elem.second.parents.empty()) {
      std::cout << ", inheriting from";
      for(auto parent : elem.second.parents)
        std::cout << " " << parent;
    }
    std::cout << ":\n";
    for(auto attr : elem.second.attr) {
      std::cout << "  " << attr.first << " (" << attr.second.type << ") ["
                << attr.second.defaultval << attr.second.unit << "] "
                << attr.second.info << std::endl;
    }
  }
  std::cout << std::endl << "types:" << std::endl;
  for(auto t : types)
    std::cout << "  " << t << ": " << tdesc[t] << std::endl;
}

std::vector<std::string>& TASCAR::get_warnings()
{
  return TASCAR::warnings;
}

std::string TASCAR::get_tuid()
{
  char c[1024];
  snprintf(c, sizeof(c), "%zx", ++maxid);
  c[sizeof(c) - 1] = 0;
  return c;
}

void TASCAR::validate_tuid(const std::string& id, const tsccfg::node_t& e)
{
  std::lock_guard<std::mutex> lk{TASCAR::tuidlistmtx};
  if(TASCAR::tuidlist.find(id) != TASCAR::tuidlist.end())
    throw TASCAR::ErrMsg("The id \"" + id + "\" is not unique.\n  (" +
                         tsccfg::node_get_path(e) + ")");
  TASCAR::tuidlist.insert(id);
}

const std::vector<tsccfg::node_t>
tsccfg::node_get_children(const tsccfg::node_t& node, const std::string& name)
{
  TASCAR_ASSERT(node);
  std::vector<tsccfg::node_t> children;
  auto nodelist(node->getChildNodes());
  for(size_t k = 0; k < nodelist->getLength(); ++k) {
    auto node(nodelist->item(k));
    if(node->getNodeType() == xercesc::DOMNode::ELEMENT_NODE) {
      tsccfg::node_t sne(dynamic_cast<tsccfg::node_t>(node));
      if(sne)
        if(name.empty() || (name == tsccfg::node_get_name(sne)))
          children.push_back(sne);
    }
  }
  return children;
}

std::vector<tsccfg::node_t> tsccfg::node_get_children(tsccfg::node_t& node,
                                                      const std::string& name)
{
  TASCAR_ASSERT(node);
  std::vector<tsccfg::node_t> children;
  auto nodelist(node->getChildNodes());
  for(size_t k = 0; k < nodelist->getLength(); ++k) {
    auto node(nodelist->item(k));
    if(node->getNodeType() == xercesc::DOMNode::ELEMENT_NODE) {
      tsccfg::node_t sne(dynamic_cast<tsccfg::node_t>(node));
      if(sne)
        if(name.empty() || (name == tsccfg::node_get_name(sne)))
          children.push_back(sne);
    }
  }
  return children;
}

tsccfg::node_t tsccfg::node_add_child(tsccfg::node_t& node,
                                      const std::string& name)
{
  TASCAR_ASSERT(node);
  return dynamic_cast<tsccfg::node_t>(node->appendChild(
      node->getOwnerDocument()->createElement(str2wstr(name).c_str())));
}

void tsccfg::node_remove_child(tsccfg::node_t& parent, tsccfg::node_t child)
{
  parent->removeChild(child);
}

std::string TASCAR::strrep(std::string s, const std::string& pat,
                           const std::string& rep)
{
  std::string out_string("");
  std::string::size_type len = pat.size();
  if(len == 0)
    return s;
  std::string::size_type pos;
  while((pos = s.find(pat)) < s.size()) {
    out_string += s.substr(0, pos);
    out_string += rep;
    s.erase(0, pos + len);
  }
  s = out_string + s;
  return s;
}

void node_register_attr(tsccfg::node_t& e, const std::string& name,
                        const std::string& value, const std::string& unit,
                        const std::string& info, const std::string& type)
{
  TASCAR_ASSERT(e);
  std::string path(tsccfg::node_get_path(e));
  TASCAR::attribute_list[path].category = tsccfg::node_get_name(e);
  TASCAR::attribute_list[path].type =
      tsccfg::node_get_attribute_value(e, "type");
  TASCAR::attribute_list[path].vars[name] = {name, type, value, unit, info};
}

void tsccfg::node_get_and_register_attribute(tsccfg::node_t& e,
                                             const std::string& name,
                                             std::string& value,
                                             const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, value, "", info, "string");
  if(tsccfg::node_has_attribute(e, name))
    value = tsccfg::node_get_attribute_value(e, name);
  else
    tsccfg::node_set_attribute(e, name, value);
}

uint32_t CRC32(const char* data, size_t data_length)
{
  uint32_t crc = 0xFFFFFFFF;
  for(size_t i = 0; i < data_length; ++i) {
    char byte = data[i];
    crc = crc ^ byte;
    size_t j = 8;
    while(j) {
      --j;
      uint32_t mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320 & mask);
    }
  }
  return ~crc;
}

uint32_t TASCAR::xml_element_t::hash(const std::vector<std::string>& attributes,
                                     bool test_children) const
{
  std::string v;
  for(auto& attr : attributes)
    v += tsccfg::node_get_attribute_value(e, attr);
  if(test_children)
    for(auto& sne : tsccfg::node_get_children(e))
      for(auto& attr : attributes)
        v += tsccfg::node_get_attribute_value(sne, attr);
  // std::hash<std::string> hash_fn;
  // return hash_fn(v);
  return CRC32(v.c_str(), v.size());
}

std::string TASCAR::to_string(const TASCAR::pos_t& x)
{
  return TASCAR::to_string(x.x) + " " + TASCAR::to_string(x.y) + " " +
         TASCAR::to_string(x.z);
}

std::string TASCAR::to_string(const TASCAR::posf_t& x)
{
  return TASCAR::to_string(x.x) + " " + TASCAR::to_string(x.y) + " " +
         TASCAR::to_string(x.z);
}

std::string TASCAR::to_string(const std::vector<TASCAR::pos_t>& x)
{
  size_t k(0);
  std::string rv;
  for(auto& p : x) {
    if(k)
      rv += " ";
    rv += TASCAR::to_string(p);
  }
  return rv;
}

std::string TASCAR::to_string(const TASCAR::zyx_euler_t& x)
{
  return TASCAR::to_string(x.z) + " " + TASCAR::to_string(x.y) + " " +
         TASCAR::to_string(x.x);
}

std::string TASCAR::to_string_deg(const TASCAR::zyx_euler_t& x)
{
  return TASCAR::to_string(x.z * RAD2DEG) + " " +
         TASCAR::to_string(x.y * RAD2DEG) + " " +
         TASCAR::to_string(x.x * RAD2DEG);
}

std::string TASCAR::to_string(const TASCAR::levelmeter::weight_t& value)
{
  switch(value) {
  case TASCAR::levelmeter::Z:
    return "Z";
    break;
  case TASCAR::levelmeter::A:
    return "A";
    break;
  case TASCAR::levelmeter::C:
    return "C";
    break;
  case TASCAR::levelmeter::bandpass:
    return "bandpass";
    break;
  }
  return "";
}

std::string TASCAR::to_string(double x, const char* fmt)
{
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, fmt, x);
  return ctmp;
}

std::string TASCAR::to_string(float x, const char* fmt)
{
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, fmt, x);
  return ctmp;
}

std::string TASCAR::to_string(uint32_t x)
{
  return std::to_string(x);
}

std::string TASCAR::to_string(int32_t x)
{
  return std::to_string(x);
}

std::string TASCAR::to_string_bool(bool value)
{
  if(value)
    return "true";
  return "false";
}

std::string TASCAR::to_string(const std::vector<int>& value)
{
  std::stringstream s;
  for(std::vector<int>::const_iterator i_vert = value.begin();
      i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s << " ";
    s << *i_vert;
  }
  return s.str();
}

std::string TASCAR::to_string(const std::vector<uint32_t>& value)
{
  std::stringstream s;
  for(auto i_vert = value.begin(); i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s << " ";
    s << *i_vert;
  }
  return s.str();
}

std::string TASCAR::to_string(const std::vector<double>& value, const char* fmt)
{
  std::string s;
  for(auto v : value)
    s += TASCAR::to_string(v, fmt) + " ";
  if(s.size())
    s.erase(s.size() - 1);
  return s;
}

std::string TASCAR::to_string(const std::vector<float>& value, const char* fmt)
{
  std::string s;
  for(auto v : value)
    s += TASCAR::to_string(v, fmt) + " ";
  if(s.size())
    s.erase(s.size() - 1);
  return s;
}

std::string TASCAR::days_to_string(double x)
{
  int d(floor(x));
  int h(floor(24 * (x - d)));
  char ctmp[1024];
  ctmp[1023] = 0;
  if(d == 1)
    snprintf(ctmp, 1023, "1 day %d hours", h);
  else
    snprintf(ctmp, 1023, "%d days %d hours", d, h);
  return ctmp;
}

std::string TASCAR::to_string_db(double value)
{
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%g", 20.0 * log10(value));
  return ctmp;
}

std::string TASCAR::to_string_dbspl(double value)
{
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%g", 20.0 * log10(value / 2e-5));
  return ctmp;
}

std::string TASCAR::to_string_db(float value)
{
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%g", 20.0f * log10f(value));
  return ctmp;
}

std::string TASCAR::to_string_db(const std::vector<float>& value)
{
  std::vector<float> tmp(value);
  for(auto& x : tmp)
    x = TASCAR::lin2db(x);
  return TASCAR::to_string(tmp);
}

std::string TASCAR::to_string_dbspl(const std::vector<float>& value)
{
  std::vector<float> tmp(value);
  for(auto& x : tmp)
    x = TASCAR::lin2dbspl(x);
  return TASCAR::to_string(tmp);
}

std::string TASCAR::to_string_dbspl(float value)
{
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%g", 20.0f * log10f(value / 2e-5f));
  return ctmp;
}

bool file_exists_ov(const std::string& fname)
{
  if(access(fname.c_str(), F_OK) != -1)
    return true;
  return false;
}

std::string localgetenv(const std::string& env)
{
  if(char* s = getenv(env.c_str()))
    return s;
  return "";
}

TASCAR::globalconfig_t::globalconfig_t()
{
  setlocale(LC_ALL, "C");
  readconfig("/etc/tascar/defaults.xml");
  readconfig("${HOME}/.tascardefaults.xml");
}

TASCAR::globalconfig_t::~globalconfig_t() {}

void TASCAR::globalconfig_t::readconfig(const std::string& fname)
{
  try {
    std::string lfname(TASCAR::env_expand(fname));
    if(file_exists_ov(lfname)) {
      setlocale(LC_ALL, "C");
      xml_doc_t doc(lfname, xml_doc_t::LOAD_FILE);
      readconfig("", doc.root());
    }
  }
  catch(const std::exception& e) {
  }
}

void TASCAR::globalconfig_t::setxmlconfig(const std::string& key,
                                          tsccfg::node_t& e,
                                          const std::string& val)
{
  TASCAR::xml_element_t exml(e);
  if(std::string::npos == key.find(".")) {
    auto subnode = exml.find_or_add_child(key);
    tsccfg::node_set_attribute(subnode, "data", val);
  } else {
    auto dotpos = key.find(".");
    std::string prefix = key.substr(0, dotpos);
    std::string subkey = key.substr(dotpos + 1);
    if(prefix == tsccfg::node_get_name(e))
      setxmlconfig(subkey, e, val);
    else {
      auto subnode = exml.find_or_add_child(prefix);
      setxmlconfig(subkey, subnode, val);
    }
  }
}

void TASCAR::globalconfig_t::writeconfig(const std::vector<std::string>& keys)
{
  std::string lfname(TASCAR::env_expand("${HOME}/.tascardefaults.xml"));
  auto xml_mode = xml_doc_t::LOAD_FILE;
  if(!file_exists_ov(lfname)) {
    lfname = "<tascar/>";
    xml_mode = xml_doc_t::LOAD_STRING;
  }
  xml_doc_t doc(lfname, xml_mode);
  for(auto key : keys) {
    auto e(cfg.find(key));
    if(e != cfg.end()) {
      setxmlconfig(key, doc.root(), cfg[key]);
    }
  }
  // save...
  lfname = TASCAR::env_expand("${HOME}/.tascardefaults.xml");
  try {
    doc.save(lfname);
  }
  catch(...) {
  }
}

void del_whitespace(tsccfg::node_t) {}

void TASCAR::xml_doc_t::save(const std::string& filename)
{
  if(doc) {
    del_whitespace(root());
    auto serial(doc->getImplementation()->createLSSerializer());
    auto config(serial->getDomConfig());
    config->setParameter(str2wstr("format-pretty-print").c_str(), true);
    xercesc::LocalFileFormatTarget target(str2wstr(filename).c_str());
    xercesc::DOMLSOutput* pDomLsOutput(
        doc->getImplementation()->createLSOutput());
    pDomLsOutput->setByteStream(&target);
    serial->write(doc, pDomLsOutput);
    delete pDomLsOutput;
    delete serial;
  }
}

std::string TASCAR::xml_doc_t::save_to_string()
{
  if(doc) {
    del_whitespace(root());
    auto serial(doc->getImplementation()->createLSSerializer());
    auto config(serial->getDomConfig());
    config->setParameter(str2wstr("format-pretty-print").c_str(), true);
    xercesc::MemBufFormatTarget target;
    xercesc::DOMLSOutput* pDomLsOutput(
        doc->getImplementation()->createLSOutput());
    pDomLsOutput->setByteStream(&target);
    serial->write(doc, pDomLsOutput);
    std::string retv((char*)target.getRawBuffer());
    delete pDomLsOutput;
    delete serial;
    return retv;
  }
  return "";
}

void TASCAR::globalconfig_t::forceoverwrite(const std::string& a,
                                            const std::string& b)
{
  cfg[a] = b;
}

void TASCAR::globalconfig_t::readconfig(const std::string& prefix,
                                        tsccfg::node_t& e)
{
  TASCAR_ASSERT(e);
  std::string key(prefix);
  if(prefix.size())
    key += ".";
  key += tsccfg::node_get_name(e);
  TASCAR::xml_element_t xe(e);
  if(xe.has_attribute("data")) {
    cfg[key] = tsccfg::node_get_attribute_value(e, "data");
  }
  for(auto& sn : tsccfg::node_get_children(e)) {
    readconfig(key, sn);
  }
}

double TASCAR::globalconfig_t::operator()(const std::string& key,
                                          double def) const
{
  setlocale(LC_ALL, "C");
  if(localgetenv("TASCARSHOWGLOBAL").size())
    std::cout << key << " (" << def;
  auto e(cfg.find(key));
  if(e != cfg.end()) {
    if(localgetenv("TASCARSHOWGLOBAL").size())
      std::cout << "=>" << e->second.c_str() << ")\n";
    return atof(e->second.c_str());
  }
  if(localgetenv("TASCARSHOWGLOBAL").size())
    std::cout << ")\n";
  return def;
}

std::string TASCAR::globalconfig_t::operator()(const std::string& key,
                                               const std::string& def) const
{
  if(localgetenv("TASCARSHOWGLOBAL").size())
    std::cout << key << " (" << def << ")\n";
  auto e(cfg.find(key));
  if(e != cfg.end())
    return e->second;
  return def;
}

void TASCAR::add_warning(std::string msg, const tsccfg::node_t& e)
{
  add_warning(msg + "\n  (" + tsccfg::node_get_path(e) + ")");
}

void TASCAR::add_warning(std::string msg)
{
  warnings.push_back(msg);
  std::cerr << "Warning: " << msg << std::endl;
}

std::string TASCAR::tscbasename(const std::string& s)
{
  return s.substr(s.rfind("/") + 1);
}

std::string TASCAR::default_string(const std::string& src,
                                   const std::string& def)
{
  if(src.empty())
    return def;
  return src;
}

TASCAR::xml_element_t::xml_element_t(const tsccfg::node_t& src) : e(src)
{
  if(!e)
    throw TASCAR::ErrMsg("Invalid NULL element pointer (xml_element_t).");
}

TASCAR::xml_element_t::xml_element_t() : e(NULL) {}

TASCAR::xml_element_t::~xml_element_t() {}

bool tsccfg::node_has_attribute(const tsccfg::node_t& e,
                                const std::string& name)
{
  TASCAR_ASSERT(e);
  return e->getAttributeNode(str2wstr(name).c_str());
}

std::vector<std::string> TASCAR::xml_element_t::get_attributes() const
{
  std::vector<std::string> r;
  auto attrs(e->getAttributes());
  for(size_t k = 0; k < attrs->getLength(); ++k)
    r.push_back(wstr2str(attrs->item(k)->getNodeName()));
  return r;
}

bool TASCAR::xml_element_t::has_attribute(const std::string& name) const
{
  TASCAR_ASSERT(e);
  return tsccfg::node_has_attribute(e, name);
}

tsccfg::node_t TASCAR::xml_element_t::find_or_add_child(const std::string& name)
{
  TASCAR_ASSERT(e);
  for(auto& ch : tsccfg::node_get_children(e))
    if(tsccfg::node_get_name(ch) == name)
      return ch;
  return add_child(name);
}

tsccfg::node_t TASCAR::xml_element_t::add_child(const std::string& name)
{
  TASCAR_ASSERT(e);
  return tsccfg::node_add_child(e, name);
}

std::string TASCAR::xml_element_t::get_attribute(const std::string& name)
{
  TASCAR_ASSERT(e);
  return tsccfg::node_get_attribute_value(e, name);
}

std::vector<tsccfg::node_t>
TASCAR::xml_element_t::get_children(const std::string& name)
{
  TASCAR_ASSERT(e);
  return tsccfg::node_get_children(e, name);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          std::string& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, value, unit, info, "string");
  if(has_attribute(name))
    value = tsccfg::node_get_attribute_value(e, name);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          double& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info, "double");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name, float& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info, "float");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          uint32_t& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, std::to_string(value), unit, info, "uint32");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          int32_t& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, std::to_string(value), unit, info, "int32");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          uint64_t& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, std::to_string(value), unit, info, "uint64");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          int64_t& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, std::to_string(value), unit, info, "int64");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute_bits(const std::string& name,
                                               uint32_t& value,
                                               const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, to_string_bits(value), "", info, "bits32");
  if(has_attribute(name)) {
    std::string strval;
    strval = tsccfg::node_get_attribute_value(e, name);
    if(strval == "all") {
      value = 0xffffffff;
    } else {
      std::vector<int32_t> bits(TASCAR::str2vecint(strval));
      value = 0;
      for(uint32_t k = 0; k < bits.size(); ++k) {
        if(bits[k] < 32)
          value |= (1 << bits[k]);
      }
    }
  } else {
    set_attribute_bits(name, value);
  }
}

void TASCAR::xml_element_t::get_attribute_bool(const std::string& name,
                                               bool& value,
                                               const std::string& unit,
                                               const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_bool(value), unit, info,
                     "bool");
  if(has_attribute(name))
    get_attribute_value_bool(e, name, value);
  else
    set_attribute_bool(name, value);
}

void TASCAR::xml_element_t::get_attribute_db(const std::string& name,
                                             double& value,
                                             const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_db(value), "dB", info,
                     "double");
  if(has_attribute(name))
    get_attribute_value_db(e, name, value);
  else
    set_attribute_db(name, value);
}

void TASCAR::xml_element_t::get_attribute_dbspl(const std::string& name,
                                                double& value,
                                                const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_dbspl(value), "dB SPL", info,
                     "double");
  if(has_attribute(name))
    get_attribute_value_dbspl(e, name, value);
  else
    set_attribute_dbspl(name, value);
}

void TASCAR::xml_element_t::get_attribute_dbspl(const std::string& name,
                                                float& value,
                                                const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_dbspl(value), "dB SPL", info,
                     "float");
  if(has_attribute(name))
    get_attribute_value_dbspl_float(e, name, value);
  else
    set_attribute_dbspl(name, value);
}

void TASCAR::xml_element_t::get_attribute_dbspl(const std::string& name,
                                                std::vector<float>& value,
                                                const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_dbspl(value), "dB SPL", info,
                     "float array");
  if(has_attribute(name))
    get_attribute_value_dbspl_float_vec(e, name, value);
  else
    set_attribute_dbspl(name, value);
}

void TASCAR::xml_element_t::get_attribute_db(const std::string& name,
                                             float& value,
                                             const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_db(value), "dB", info, "float");
  if(has_attribute(name))
    get_attribute_value_db_float(e, name, value);
  else
    set_attribute_db(name, value);
}

void TASCAR::xml_element_t::get_attribute_deg(const std::string& name,
                                              double& value,
                                              const std::string& info,
                                              const std::string& unit)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value * RAD2DEG), unit, info,
                     "double");
  if(has_attribute(name))
    get_attribute_value_deg(e, name, value);
  else
    set_attribute_deg(name, value);
}

void TASCAR::xml_element_t::get_attribute_deg(const std::string& name,
                                              float& value,
                                              const std::string& info,
                                              const std::string& unit)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value * RAD2DEGf), unit, info,
                     "float");
  if(has_attribute(name))
    get_attribute_value_deg(e, name, value);
  else
    set_attribute_deg(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          TASCAR::pos_t& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info, "pos");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          TASCAR::posf_t& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info, "pos");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          TASCAR::zyx_euler_t& value,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_deg(value), "deg", info,
                     "Euler rot");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          std::vector<TASCAR::pos_t>& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info,
                     "pos array");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          std::vector<std::string>& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::vecstr2str(value), unit, info,
                     "string array");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          std::vector<double>& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info,
                     "double array");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          std::vector<float>& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info,
                     "float array");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute_db(const std::string& name,
                                             std::vector<float>& value,
                                             const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string_db(value), "dB", info,
                     "float array");
  if(has_attribute(name))
    get_attribute_value_db_float_vec(e, name, value);
  else
    set_attribute_db(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          std::vector<int32_t>& value,
                                          const std::string& unit,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), unit, info,
                     "int32 array");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(const std::string& name,
                                          TASCAR::levelmeter::weight_t& value,
                                          const std::string& info)
{
  TASCAR_ASSERT(e);
  node_register_attr(e, name, TASCAR::to_string(value), "", info, "f-weight");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::get_attribute(
    const std::string& name, std::vector<TASCAR::levelmeter::weight_t>& value,
    const std::string& info)
{
  TASCAR_ASSERT(e);
  std::vector<std::string> svalue;
  for(const auto& v : value)
    svalue.push_back(TASCAR::to_string(v));
  node_register_attr(e, name, TASCAR::vecstr2str(svalue), "", info,
                     "f-weight array");
  if(has_attribute(name))
    get_attribute_value(e, name, value);
  else
    set_attribute(name, value);
}

void TASCAR::xml_element_t::set_attribute_bool(const std::string& name,
                                               bool value)
{
  TASCAR_ASSERT(e);
  ::set_attribute_bool(e, name, value);
}

void TASCAR::xml_element_t::set_attribute_db(const std::string& name,
                                             double value)
{
  TASCAR_ASSERT(e);
  ::set_attribute_db(e, name, value);
}

void TASCAR::xml_element_t::set_attribute_db(const std::string& name,
                                             const std::vector<float>& value)
{
  TASCAR_ASSERT(e);
  ::set_attribute_db(e, name, value);
}

void TASCAR::xml_element_t::set_attribute_dbspl(const std::string& name,
                                                const std::vector<float>& value)
{
  TASCAR_ASSERT(e);
  ::set_attribute_value_dbspl(e, name, value);
}

void TASCAR::xml_element_t::set_attribute_dbspl(const std::string& name,
                                                double value)
{
  TASCAR_ASSERT(e);
  ::set_attribute_dbspl(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const std::string& value)
{
  TASCAR_ASSERT(e);
  tsccfg::node_set_attribute(e, name, value);
}

void TASCAR::xml_element_t::set_attribute_deg(const std::string& name,
                                              double value)
{
  TASCAR_ASSERT(e);
  ::set_attribute_double(e, name, value * RAD2DEG);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name, double value)
{
  TASCAR_ASSERT(e);
  set_attribute_double(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          uint32_t value)
{
  TASCAR_ASSERT(e);
  set_attribute_uint32(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          int32_t value)
{
  TASCAR_ASSERT(e);
  set_attribute_int32(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          uint64_t value)
{
  TASCAR_ASSERT(e);
  set_attribute_uint64(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          int64_t value)
{
  TASCAR_ASSERT(e);
  set_attribute_int64(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const TASCAR::pos_t& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const TASCAR::posf_t& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const TASCAR::zyx_euler_t& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(
    const std::string& name, const std::vector<TASCAR::pos_t>& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const std::vector<std::string>& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const std::vector<double>& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const std::vector<float>& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(const std::string& name,
                                          const std::vector<int32_t>& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(
    const std::string& name, const TASCAR::levelmeter::weight_t& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

void TASCAR::xml_element_t::set_attribute(
    const std::string& name,
    const std::vector<TASCAR::levelmeter::weight_t>& value)
{
  TASCAR_ASSERT(e);
  set_attribute_value(e, name, value);
}

std::string TASCAR::env_expand(std::string s)
{
  size_t spos;
  while((spos = s.find("${")) != std::string::npos) {
    size_t epos(s.find("}", spos));
    if(epos == std::string::npos)
      epos = s.size();
    std::string env(s.substr(spos + 2, epos - spos - 2));
    s.replace(spos, epos - spos + 1, localgetenv(env));
  }
  return s;
}

std::string TASCAR::to_string_bits(uint32_t value)
{
  if(value == 0xffffffff)
    return "all";
  std::string sv;
  for(uint32_t b = 0; b < 32; ++b) {
    if(value & (1 << b))
      sv += std::to_string(b) + " ";
  }
  if(!sv.empty())
    sv.erase(sv.size() - 1, 1);
  return sv;
}

void TASCAR::xml_element_t::set_attribute_bits(const std::string& name,
                                               uint32_t value)
{
  TASCAR_ASSERT(e);
  tsccfg::node_set_attribute(e, name, TASCAR::to_string_bits(value));
}

void set_attribute_uint32(tsccfg::node_t& elem, const std::string& name,
                          uint32_t value)
{
  TASCAR_ASSERT(elem);
  tsccfg::node_set_attribute(elem, name, std::to_string(value));
}

void set_attribute_int32(tsccfg::node_t& elem, const std::string& name,
                         int32_t value)
{
  TASCAR_ASSERT(elem);
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%d", value);
  tsccfg::node_set_attribute(elem, name, ctmp);
}

void set_attribute_uint64(tsccfg::node_t& elem, const std::string& name,
                          uint64_t value)
{
  TASCAR_ASSERT(elem);
  tsccfg::node_set_attribute(elem, name, std::to_string(value));
}

void set_attribute_int64(tsccfg::node_t& elem, const std::string& name,
                         int64_t value)
{
  TASCAR_ASSERT(elem);
  tsccfg::node_set_attribute(elem, name, std::to_string(value));
}

void set_attribute_bool(tsccfg::node_t& elem, const std::string& name,
                        bool value)
{
  TASCAR_ASSERT(elem);
  if(value)
    tsccfg::node_set_attribute(elem, name, "true");
  else
    tsccfg::node_set_attribute(elem, name, "false");
}

void set_attribute_double(tsccfg::node_t& elem, const std::string& name,
                          double value)
{
  TASCAR_ASSERT(elem);
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%1.12g", value);
  tsccfg::node_set_attribute(elem, name, ctmp);
}

void set_attribute_db(tsccfg::node_t& elem, const std::string& name,
                      double value)
{
  TASCAR_ASSERT(elem);
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%1.12g", 20.0 * log10(value));
  tsccfg::node_set_attribute(elem, name, ctmp);
}

void set_attribute_db(tsccfg::node_t& elem, const std::string& name,
                      const std::vector<float>& value)
{
  TASCAR_ASSERT(elem);
  std::vector<float> tmp(value);
  for(auto& x : tmp)
    x = TASCAR::lin2db(x);
  tsccfg::node_set_attribute(elem, name, TASCAR::to_string(tmp));
}

void set_attribute_dbspl(tsccfg::node_t& elem, const std::string& name,
                         double value)
{
  TASCAR_ASSERT(elem);
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%1.12g", 20.0 * log10(value / 2e-5));
  tsccfg::node_set_attribute(elem, name, ctmp);
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const TASCAR::pos_t& value)
{
  TASCAR_ASSERT(elem);
  tsccfg::node_set_attribute(elem, name, value.print_cart(" "));
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const TASCAR::posf_t& value)
{
  TASCAR_ASSERT(elem);
  tsccfg::node_set_attribute(elem, name, value.print_cart(" "));
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const TASCAR::zyx_euler_t& value)
{
  TASCAR_ASSERT(elem);
  char ctmp[1024];
  ctmp[1023] = 0;
  snprintf(ctmp, 1023, "%1.12g %1.12g %1.12g", value.z * RAD2DEG,
           value.y * RAD2DEG, value.x * RAD2DEG);
  tsccfg::node_set_attribute(elem, name, ctmp);
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const TASCAR::levelmeter::weight_t& value)
{
  TASCAR_ASSERT(elem);
  tsccfg::node_set_attribute(elem, name, TASCAR::to_string(value));
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const std::vector<TASCAR::levelmeter::weight_t>& value)
{
  TASCAR_ASSERT(elem);
  std::vector<std::string> tmp;
  for(const auto& v : value)
    tmp.push_back(TASCAR::to_string(v));
  tsccfg::node_set_attribute(elem, name, TASCAR::vecstr2str(tmp));
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const std::vector<TASCAR::pos_t>& value)
{
  TASCAR_ASSERT(elem);
  std::string s;
  for(std::vector<TASCAR::pos_t>::const_iterator i_vert = value.begin();
      i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s += " ";
    s += i_vert->print_cart(" ");
  }
  tsccfg::node_set_attribute(elem, name, s);
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const std::vector<std::string>& value)
{
  TASCAR_ASSERT(elem);
  std::string s;
  for(std::vector<std::string>::const_iterator i_vert = value.begin();
      i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s += " ";
    s += *i_vert;
  }
  tsccfg::node_set_attribute(elem, name, s);
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const std::vector<double>& value)
{
  TASCAR_ASSERT(elem);
  std::stringstream s;
  for(std::vector<double>::const_iterator i_vert = value.begin();
      i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s << " ";
    s << *i_vert;
  }
  tsccfg::node_set_attribute(elem, name, s.str());
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const std::vector<float>& value)
{
  TASCAR_ASSERT(elem);
  std::stringstream s;
  for(std::vector<float>::const_iterator i_vert = value.begin();
      i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s << " ";
    s << *i_vert;
  }
  tsccfg::node_set_attribute(elem, name, s.str());
}

void set_attribute_value_dbspl(tsccfg::node_t& elem, const std::string& name,
                               const std::vector<float>& value)
{
  TASCAR_ASSERT(elem);
  std::stringstream s;
  for(auto v : value)
    s << TASCAR::lin2dbspl(v) << " ";
  auto str = s.str();
  if(str.size())
    str.erase(str.size() - 1);
  tsccfg::node_set_attribute(elem, name, str);
}

void set_attribute_value(tsccfg::node_t& elem, const std::string& name,
                         const std::vector<int>& value)
{
  TASCAR_ASSERT(elem);
  std::stringstream s;
  for(std::vector<int>::const_iterator i_vert = value.begin();
      i_vert != value.end(); ++i_vert) {
    if(i_vert != value.begin())
      s << " ";
    s << *i_vert;
  }
  tsccfg::node_set_attribute(elem, name, s.str());
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::string& value)
{
  TASCAR_ASSERT(elem);
  if(tsccfg::node_has_attribute(elem, name))
    value = tsccfg::node_get_attribute_value(elem, name);
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         double& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  double tmpv(strtod(attv.c_str(), &c));
  if(c != attv.c_str())
    value = tmpv;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         float& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  float tmpv(strtof(attv.c_str(), &c));
  if(c != attv.c_str())
    value = tmpv;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         TASCAR::pos_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  TASCAR::pos_t tmpv;
  if(sscanf(attv.c_str(), "%lf%lf%lf", &(tmpv.x), &(tmpv.y), &(tmpv.z)) == 3) {
    value = tmpv;
  }
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         TASCAR::posf_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  TASCAR::posf_t tmpv;
  if(sscanf(attv.c_str(), "%f%f%f", &(tmpv.x), &(tmpv.y), &(tmpv.z)) == 3) {
    value = tmpv;
  }
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         TASCAR::zyx_euler_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  TASCAR::zyx_euler_t tmpv;
  if(sscanf(attv.c_str(), "%lf%lf%lf", &(tmpv.z), &(tmpv.y), &(tmpv.x)) == 3) {
    tmpv.x *= DEG2RAD;
    tmpv.y *= DEG2RAD;
    tmpv.z *= DEG2RAD;
    value = tmpv;
  }
}

std::vector<TASCAR::pos_t> TASCAR::str2vecpos(const std::string& s)
{
  std::vector<TASCAR::pos_t> value;
  if(!s.empty()) {
    std::stringstream ptxt(s);
    while(ptxt.good()) {
      TASCAR::pos_t p;
      ptxt >> p.x;
      if(ptxt.good()) {
        ptxt >> p.y;
        if(ptxt.good()) {
          ptxt >> p.z;
          value.push_back(p);
        }
      }
    }
  }
  return value;
}

std::vector<double> TASCAR::str2vecdouble(const std::string& s)
{
  std::vector<double> value;
  if(!s.empty()) {
    std::stringstream ptxt(s);
    while(ptxt.good()) {
      double p;
      ptxt >> p;
      value.push_back(p);
    }
  }
  return value;
}

std::vector<float> TASCAR::str2vecfloat(const std::string& s)
{
  std::vector<float> value;
  if(!s.empty()) {
    std::stringstream ptxt(s);
    while(ptxt.good()) {
      float p;
      ptxt >> p;
      value.push_back(p);
    }
  }
  return value;
}

std::vector<int32_t> TASCAR::str2vecint(const std::string& s,
                                        const std::string& delim)
{
  std::vector<int32_t> value;
  if(!s.empty()) {
    auto vstr = TASCAR::str2vecstr(s, delim);
    for(auto s : vstr)
      value.push_back(atoi(s.c_str()));
  }
  return value;
}

std::vector<std::string> TASCAR::str2vecstr(const std::string& s,
                                            const std::string& delim)
{
  std::vector<std::string> value;
  std::string tok;
  int mode = 0;
  bool wasquoted = false;
  for(auto c : s) {
    if((mode == 0) && (delim.find(c) != std::string::npos)) {
      //(c == ' ') || (c == '\t'))) {
      if(tok.size() || wasquoted)
        value.push_back(tok);
      tok.clear();
      wasquoted = false;
    } else if((mode == 0) && (c == '\'')) {
      wasquoted = true;
      mode = 1;
    } else if((mode == 1) && (c == '\'')) {
      mode = 0;
    } else if((mode == 0) && (c == '"')) {
      wasquoted = true;
      mode = 2;
    } else if((mode == 2) && (c == '"')) {
      mode = 0;
    } else {
      tok += c;
    }
  }
  if(tok.size())
    value.push_back(tok);
  return value;
}

std::string TASCAR::vecstr2str(const std::vector<std::string>& s,
                               const std::string& delim)
{
  std::string rv;
  for(auto it = s.begin(); it != s.end(); ++it) {
    if(it != s.begin())
      rv += delim;
    if(it->find(' ') != std::string::npos)
      rv += "'" + *it + "'";
    else
      rv += *it;
  }
  return rv;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         TASCAR::levelmeter::weight_t& value)
{
  TASCAR_ASSERT(elem);
  std::string svalue(tsccfg::node_get_attribute_value(elem, name));
  if(svalue.size() == 0)
    return;
  if(svalue == "Z")
    value = TASCAR::levelmeter::Z;
  else if(svalue == "C")
    value = TASCAR::levelmeter::C;
  else if(svalue == "A")
    value = TASCAR::levelmeter::A;
  else if(svalue == "bandpass")
    value = TASCAR::levelmeter::bandpass;
  else
    throw TASCAR::ErrMsg(std::string("Unsupported weight type \"") + svalue +
                         std::string("\" for attribute \"") + name +
                         std::string("\"."));
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::vector<TASCAR::levelmeter::weight_t>& value)
{
  TASCAR_ASSERT(elem);
  std::vector<std::string> svalue(
      TASCAR::str2vecstr(tsccfg::node_get_attribute_value(elem, name)));
  if(svalue.size() == 0)
    return;
  std::vector<TASCAR::levelmeter::weight_t> tmpvalue;
  for(const auto& s : svalue) {
    if(s == "Z")
      tmpvalue.push_back(TASCAR::levelmeter::Z);
    else if(s == "C")
      tmpvalue.push_back(TASCAR::levelmeter::C);
    else if(s == "A")
      tmpvalue.push_back(TASCAR::levelmeter::A);
    else if(s == "bandpass")
      tmpvalue.push_back(TASCAR::levelmeter::bandpass);
    else
      throw TASCAR::ErrMsg(std::string("Unsupported weight type \"") + s +
                           std::string("\" for attribute \"") + name +
                           std::string("\"."));
  }
  value = tmpvalue;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::vector<TASCAR::pos_t>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecpos(tsccfg::node_get_attribute_value(elem, name));
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::vector<std::string>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecstr(tsccfg::node_get_attribute_value(elem, name));
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::vector<double>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecdouble(tsccfg::node_get_attribute_value(elem, name));
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::vector<float>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecfloat(tsccfg::node_get_attribute_value(elem, name));
}

void get_attribute_value_db_float_vec(const tsccfg::node_t& elem,
                                      const std::string& name,
                                      std::vector<float>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecfloat(tsccfg::node_get_attribute_value(elem, name));
  for(auto& x : value)
    x = TASCAR::db2lin(x);
}

void get_attribute_value_dbspl_float_vec(const tsccfg::node_t& elem,
                                         const std::string& name,
                                         std::vector<float>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecfloat(tsccfg::node_get_attribute_value(elem, name));
  for(auto& x : value)
    x = TASCAR::dbspl2lin(x);
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         std::vector<int32_t>& value)
{
  TASCAR_ASSERT(elem);
  value = TASCAR::str2vecint(tsccfg::node_get_attribute_value(elem, name));
}

void get_attribute_value_deg(const tsccfg::node_t& elem,
                             const std::string& name, double& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  double tmpv(strtod(attv.c_str(), &c));
  if(c != attv.c_str())
    value = DEG2RAD * tmpv;
}

void get_attribute_value_deg(const tsccfg::node_t& elem,
                             const std::string& name, float& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  double tmpv(strtod(attv.c_str(), &c));
  if(c != attv.c_str())
    value = DEG2RADf * tmpv;
}

void get_attribute_value_db(const tsccfg::node_t& elem, const std::string& name,
                            double& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  double tmpv(strtod(attv.c_str(), &c));
  if(c != attv.c_str())
    value = pow(10.0, 0.05 * tmpv);
}

void get_attribute_value_dbspl(const tsccfg::node_t& elem,
                               const std::string& name, double& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  double tmpv(strtod(attv.c_str(), &c));
  if(c != attv.c_str())
    value = pow(10.0, 0.05 * tmpv) * 2e-5;
}

void get_attribute_value_dbspl_float(const tsccfg::node_t& elem,
                                     const std::string& name, float& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  float tmpv(strtof(attv.c_str(), &c));
  if(c != attv.c_str())
    value = powf(10.0f, 0.05f * tmpv) * 2e-5f;
}

void get_attribute_value_db_float(const tsccfg::node_t& elem,
                                  const std::string& name, float& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  double tmpv(strtod(attv.c_str(), &c));
  if(c != attv.c_str())
    value = pow(10.0, 0.05 * tmpv);
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         uint32_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  long unsigned int tmpv(strtoul(attv.c_str(), &c, 10));
  if(c != attv.c_str())
    value = tmpv;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         uint64_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  long unsigned int tmpv(strtoul(attv.c_str(), &c, 10));
  if(c != attv.c_str())
    value = tmpv;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         int32_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  long int tmpv(strtol(attv.c_str(), &c, 10));
  if(c != attv.c_str())
    value = tmpv;
}

void get_attribute_value(const tsccfg::node_t& elem, const std::string& name,
                         int64_t& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  char* c;
  long int tmpv(strtol(attv.c_str(), &c, 10));
  if(c != attv.c_str())
    value = tmpv;
}

void get_attribute_value_bool(const tsccfg::node_t& elem,
                              const std::string& name, bool& value)
{
  TASCAR_ASSERT(elem);
  std::string attv(tsccfg::node_get_attribute_value(elem, name));
  if(attv.size())
    value = (attv == "true");
}

std::vector<std::string> TASCAR::xml_element_t::get_unused_attributes() const
{
  TASCAR_ASSERT(e);
  std::vector<std::string> retv;
  std::string path(tsccfg::node_get_path(e));
  for(auto& attrname : get_attributes()) {
    if(attribute_list[path].vars.find(attrname) ==
       attribute_list[path].vars.end())
      retv.push_back(attrname);
  }
  return retv;
}

void TASCAR::xml_element_t::validate_attributes(std::string& msg) const
{
  TASCAR_ASSERT(e);
  std::vector<std::string> unused(get_unused_attributes());
  if(unused.size()) {
    if(!msg.empty())
      msg += "\n";
    std::string path(tsccfg::node_get_path(e));
    msg += "Invalid attributes in element \"" + tsccfg::node_get_name(e) +
           "\" (path " + path + "):";
    for(auto& attr : unused)
      msg += " " + attr;
    msg += " (valid attributes are:";
    for(auto& attr : attribute_list[path].vars)
      msg += " " + attr.first;
    msg += ").";
  }
}

TASCAR::xml_doc_t::xml_doc_t() : doc(NULL)
{
  xercesc::DOMImplementation* impl(
      xercesc::DOMImplementationRegistry::getDOMImplementation(
          str2wstr("XML 1.0").c_str()));
  TASCAR_ASSERT(impl);
  doc = impl->createDocument(0, str2wstr("session").c_str(), NULL);
  root = get_root_node();
}

TASCAR::xml_doc_t::xml_doc_t(const std::string& filename_or_data, load_type_t t)
    : doc(NULL)
{
  std::string msg;
  domp.setValidationScheme(xercesc::XercesDOMParser::Val_Never);
  domp.setDoNamespaces(false);
  domp.setDoSchema(false);
  domp.setLoadExternalDTD(false);
  domp.setErrorHandler(&errh);
  try {
    switch(t) {
    case LOAD_FILE:
      msg = "parsing file \"" + filename_or_data + "\"";
      domp.parse(filename_or_data.c_str());
      break;
    case LOAD_STRING:
      msg = "parsing string of " + std::to_string(filename_or_data.size()) +
            " characters";
      xercesc::MemBufInputSource myxml_buf((XMLByte*)(filename_or_data.c_str()),
                                           filename_or_data.size(),
                                           "xml_doc_t(in memory)");
      domp.parse(myxml_buf);
      break;
    }
  }
  catch(const std::exception& e) {
    throw TASCAR::ErrMsg("While " + msg + ": " + e.what());
  }
  doc = domp.getDocument();
  if(!doc)
    throw TASCAR::ErrMsg("Unable to parse document (" + msg + ").");
  if(!get_root_node())
    throw TASCAR::ErrMsg("The document has no root node (" + msg + ").");
  root = get_root_node();
}

void TASCAR::xml_doc_t::tscerrorhandler_t::warning(
    const xercesc::SAXParseException& exc)
{
  add_warning(std::string("XML parser warning (line ") +
              std::to_string(exc.getLineNumber()) + ", column " +
              std::to_string(exc.getColumnNumber()) +
              "): " + wstr2str(exc.getMessage()));
}

void TASCAR::xml_doc_t::tscerrorhandler_t::error(
    const xercesc::SAXParseException& exc)
{
  throw TASCAR::ErrMsg(std::string("XML parser error (line ") +
                       std::to_string(exc.getLineNumber()) + ", column " +
                       std::to_string(exc.getColumnNumber()) +
                       "): " + wstr2str(exc.getMessage()));
}

void TASCAR::xml_doc_t::tscerrorhandler_t::fatalError(
    const xercesc::SAXParseException& exc)
{
  throw TASCAR::ErrMsg(std::string("XML parser error (line ") +
                       std::to_string(exc.getLineNumber()) + ", column " +
                       std::to_string(exc.getColumnNumber()) +
                       "): " + wstr2str(exc.getMessage()));
}

TASCAR::xml_doc_t::xml_doc_t(const tsccfg::node_t& src) : doc(NULL)
{
  domp.setValidationScheme(xercesc::XercesDOMParser::Val_Never);
  domp.setDoNamespaces(false);
  domp.setDoSchema(false);
  domp.setLoadExternalDTD(false);
  xercesc::DOMImplementation* impl(
      xercesc::DOMImplementationRegistry::getDOMImplementation(
          str2wstr("XML 1.0").c_str()));
  TASCAR_ASSERT(impl);
  doc = impl->createDocument(0, str2wstr("session").c_str(), NULL);
  auto impnode(doc->importNode(src, true));
  doc->replaceChild(impnode, get_root_node());
  root = get_root_node();
}

TASCAR::xml_doc_t::~xml_doc_t()
{
  // cleanup document
}

tsccfg::node_t TASCAR::xml_doc_t::get_root_node()
{
  TASCAR_ASSERT(doc);
  return doc->getDocumentElement();
}

std::string TASCAR::xml_element_t::get_element_name() const
{
  TASCAR_ASSERT(e);
  return tsccfg::node_get_name(e);
}

std::string tsccfg::node_get_text(tsccfg::node_t& n, const std::string& child)
{
  TASCAR_ASSERT(n);
  if(!n)
    return "";
  if(child.empty())
    return wstr2str(n->getTextContent());
  std::string retval;
  for(auto& ch : tsccfg::node_get_children(n, child))
    retval += tsccfg::node_get_text(ch);
  return retval;
}

void tsccfg::node_set_text(tsccfg::node_t& node, const std::string& text)
{
  node->setTextContent(str2wstr(text).c_str());
}

void tsccfg::node_import_node(tsccfg::node_t& node, const tsccfg::node_t& src)
{
  auto impnode(node->getOwnerDocument()->importNode(src, true));
  node->appendChild(impnode);
}

void tsccfg::node_import_node_before(tsccfg::node_t& node,
                                     const tsccfg::node_t& src,
                                     const tsccfg::node_t& before)
{
  auto impnode(node->getOwnerDocument()->importNode(src, true));
  node->insertBefore(impnode, before);
}

std::string tsccfg::node_get_attribute_value(const tsccfg::node_t& node,
                                             const std::string& name)
{
  TASCAR_ASSERT(node);
  return wstr2str(node->getAttribute(str2wstr(name).c_str()));
}

std::string tsccfg::node_get_name(const tsccfg::node_t& node)
{
  TASCAR_ASSERT(node);
  return wstr2str(node->getTagName());
}

void tsccfg::node_set_name(const tsccfg::node_t& node, const std::string& name)
{
  TASCAR_ASSERT(node);
  node->getOwnerDocument()->renameNode(node, NULL, str2wstr(name).c_str());
}

std::string tsccfg::node_get_path(const tsccfg::node_t& node)
{
  TASCAR_ASSERT(node);
  std::string thisname(tsccfg::node_get_name(node));
  xercesc::DOMNode* sib(node);
  size_t sib_prev(0);
  while((sib = sib->getPreviousSibling())) {
    tsccfg::node_t sibe(dynamic_cast<tsccfg::node_t>(sib));
    if(sibe && (tsccfg::node_get_name(sibe) == thisname))
      sib_prev++;
  }
  sib = node;
  size_t sib_next(0);
  while((sib = sib->getNextSibling())) {
    tsccfg::node_t sibe(dynamic_cast<tsccfg::node_t>(sib));
    if(sibe && (tsccfg::node_get_name(sibe) == thisname))
      sib_next++;
  }
  std::string nodepath(std::string("/") + thisname);
  if(sib_prev + sib_next != 0)
    nodepath += "[" + std::to_string(sib_prev + 1) + "]";
  tsccfg::node_t parent(dynamic_cast<tsccfg::node_t>(node->getParentNode()));
  if(parent)
    nodepath = tsccfg::node_get_path(parent) + nodepath;
  return nodepath;
}

void tsccfg::node_set_attribute(tsccfg::node_t& node, const std::string& name,
                                const std::string& value)
{
  TASCAR_ASSERT(node);
  node->setAttribute(str2wstr(name).c_str(), str2wstr(value).c_str());
}

float TASCAR::db2lin(const float& x)
{
  return powf(10.0f, 0.05f * x);
}

double TASCAR::db2lin(const double& x)
{
  return pow(10.0, 0.05 * x);
}

float TASCAR::lin2db(const float& x)
{
  return 20.0f * log10f(x);
}

double TASCAR::lin2db(const double& x)
{
  return 20.0 * log10(x);
}

float TASCAR::dbspl2lin(const float& x)
{
  return 2e-5f * powf(10.0f, 0.05f * x);
}

float TASCAR::lin2dbspl(const float& x)
{
  return 20.0f * log10f(x / 2e-5f);
}

TASCAR::cfg_node_desc_t::cfg_node_desc_t() {}
TASCAR::cfg_node_desc_t::~cfg_node_desc_t() {}

void donothing() {}

std::function<void()> mainloopupdate_fun = donothing;

void TASCAR::mainloopupdate()
{
  mainloopupdate_fun();
}

void TASCAR::set_mainloopupdate(std::function<void()> fun)
{
  mainloopupdate_fun = fun;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
