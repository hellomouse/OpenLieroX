/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////

#ifndef __OLX__XMLUTILS_H__
#define __OLX__XMLUTILS_H__

// Useful functions for XML/HTML parsing
// Created 24/7/08
// Karel Petranek

#include <string>
#include <libxml/xmlmemory.h>
#include "Color.h"

int xmlGetInt(xmlNodePtr node, const std::string& name, int def = 0);
float xmlGetFloat(xmlNodePtr node, const std::string& name, float def = 0);
Color xmlGetColour(xmlNodePtr node, const std::string& name, const Color& def = Color());
std::string xmlGetString(xmlNodePtr node, const std::string& name, const std::string& def = "");
bool xmlGetBool(xmlNodePtr node, const std::string& name, bool def = false);
std::string xmlNodeText(xmlNodePtr node, const std::string& def);
inline bool xmlPropExists(xmlNodePtr node, const std::string& prop) { return xmlGetProp(node, (const xmlChar *)prop.c_str()) != NULL; }
std::string xmlGetBaseURL(xmlNodePtr node);
std::string	xmlEntities(const std::string& text);
void xmlEntityText(std::string& text);

#endif
