#include "session_reader.h"
#include <string.h>
#ifdef WIN32
#define PATH_MAX MAX_PATH
#else
#include <libgen.h>
#endif
#include "errorhandling.h"
#include <stdlib.h>
#include <unistd.h>

#include "cwalk.h"

TASCAR::tsc_reader_t::tsc_reader_t()
    : xml_element_t(doc->get_root_node()),
      licensed_component_t(typeid(*this).name()), file_name("")
{
  // avoid problems with number format in xml file:
  setlocale(LC_ALL, "C");
  char c_respath[PATH_MAX];
  session_path = getcwd(c_respath, PATH_MAX);
  if(get_element_name() != "session")
    throw TASCAR::ErrMsg("Invalid root node name. Expected \"session\", got " +
                         get_element_name() + ".");
}

void add_includes(xmlpp::Element* e, const std::string& parentdoc,
                  licensehandler_t* lh)
{
  xmlpp::Node::NodeList subnodes = e->get_children();
  for(xmlpp::Node::NodeList::iterator sn = subnodes.begin();
      sn != subnodes.end(); ++sn) {
    xmlpp::Element* sne(dynamic_cast<xmlpp::Element*>(*sn));
    if(sne) {
      if(sne->get_name() == "include") {
        std::string idocname(
            TASCAR::env_expand(sne->get_attribute_value("name")));
        if((!idocname.empty()) && (idocname != parentdoc)) {
          TASCAR::xml_doc_t idoc(idocname, TASCAR::xml_doc_t::LOAD_FILE);
          if(idoc.doc->get_root_node()->get_name() != e->get_name()) {
            throw TASCAR::ErrMsg("Invalid root node \"" +
                                 idoc.doc->get_root_node()->get_name() +
                                 "\" in include file \"" + idocname +
                                 "\".\nexpected \"" + e->get_name() + "\".");
          }
          std::string sublicense;
          std::string subattribution;
          get_license_info(idoc.doc->get_root_node(), "", sublicense,
                           subattribution);
          lh->add_license(sublicense, subattribution,
                          TASCAR::tscbasename(idocname));
          add_includes(idoc.doc->get_root_node(), idocname, lh);
          xmlpp::Node::NodeList isubnodes =
              idoc.doc->get_root_node()->get_children();
          for(xmlpp::Node::NodeList::iterator isn = isubnodes.begin();
              isn != isubnodes.end(); ++isn) {
            xmlpp::Element* isne(dynamic_cast<xmlpp::Element*>(*isn));
            if(isne) {
              e->import_node(isne);
            }
          }
        }
      } else {
        add_includes(sne, parentdoc, lh);
      }
    }
  }
}

TASCAR::tsc_reader_t::tsc_reader_t(const std::string& filename_or_data,
                                   load_type_t t, const std::string& path)
    : xml_doc_t(filename_or_data, t), xml_element_t(doc->get_root_node()),
      licensed_component_t(typeid(*this).name()),
      file_name(((t == LOAD_FILE) ? filename_or_data : ""))
{
  switch(t) {
  case LOAD_FILE:
    file_name = TASCAR::env_expand(filename_or_data);
    break;
  case LOAD_STRING:
    file_name = "(loaded from string)";
    break;
  }
  // avoid problems with number format in xml file:
  setlocale(LC_ALL, "C");
  if(path.size()) {
    std::vector<char> c_fname(path.size() + 1); // g++ extension was used to have variable size array, non-portable
    char c_respath[PATH_MAX];
    memcpy(c_fname.data(), path.c_str(), path.size() + 1);

	size_t dirend;
    cwk_path_get_dirname(c_fname.data(), &dirend);
    if(dirend != 0) {
      c_fname[dirend] = '\0'; // Use only the dir portion of the filename
      cwk_path_normalize(c_fname.data(), c_respath, PATH_MAX);
      session_path = c_respath;
    } else {
      add_warning("No path found");
    }
    
    if(chdir(session_path.c_str()) != 0)
      add_warning("Unable to change directory.");
  } else {
    char c_respath[PATH_MAX];
    session_path = getcwd(c_respath, PATH_MAX);
  }
  if(get_element_name() != "session")
    throw TASCAR::ErrMsg("Invalid root node name. Expected \"session\", got " +
                         get_element_name() + ".");
  // add session-includes:
  add_includes(e, "", this);
}

void TASCAR::tsc_reader_t::read_xml()
{
  GET_ATTRIBUTE(license);
  GET_ATTRIBUTE(attribution);
  add_license(license, attribution, "session file");
  xmlpp::Node::NodeList subnodes = e->get_children();
  for(xmlpp::Node::NodeList::iterator sn = subnodes.begin();
      sn != subnodes.end(); ++sn) {
    xmlpp::Element* sne(dynamic_cast<xmlpp::Element*>(*sn));
    if(sne) {
      if(sne->get_name() == "scene")
        add_scene(sne);
      else if(sne->get_name() == "range")
        add_range(sne);
      else if(sne->get_name() == "connect")
        add_connection(sne);
      else if(sne->get_name() == "modules") {
        xmlpp::Node::NodeList lsubnodes = sne->get_children();
        for(xmlpp::Node::NodeList::iterator lsn = lsubnodes.begin();
            lsn != lsubnodes.end(); ++lsn) {
          xmlpp::Element* lsne(dynamic_cast<xmlpp::Element*>(*lsn));
          if(lsne)
            add_module(lsne);
        }
      } else if(sne->get_name() == "license") {
        TASCAR::xml_element_t lic(sne);
        std::string license;
        std::string attribution;
        std::string name;
        lic.GET_ATTRIBUTE(license);
        lic.GET_ATTRIBUTE(attribution);
        lic.GET_ATTRIBUTE(name);
        add_license(license, attribution, name);
      } else if(sne->get_name() == "author") {
        TASCAR::xml_element_t lic(sne);
        std::string of;
        std::string name;
        lic.GET_ATTRIBUTE(name);
        lic.GET_ATTRIBUTE(of);
        add_author(name, of);
      } else if(sne->get_name() == "bibitem") {
        xmlpp::NodeSet stxt(sne->find("text()"));
        for(auto it = stxt.begin(); it != stxt.end(); ++it) {
          xmlpp::TextNode* txt(dynamic_cast<xmlpp::TextNode*>(*it));
          if(txt)
            add_bibitem(txt->get_content());
        }
      } else if((sne->get_name() != "include") &&
                (sne->get_name() != "mainwindow") &&
                (sne->get_name() != "description"))
        add_warning("Invalid element: " + sne->get_name(), sne);
      if(sne->get_name() == "module")
        add_module(sne);
    }
  }
}

const std::string& TASCAR::tsc_reader_t::get_session_path() const
{
  return session_path;
}

const std::string& TASCAR::tsc_reader_t::get_file_name() const
{
  return file_name;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
