// ChatXmlFactory: dispatcher for every custom tag used in chat.xml.
// Plugs into DuiXmlBuilder::CustomFactory so the XML loader produces a
// fully-configured DuiControl tree.
#pragma once

#include "../balloonui/DuiXmlBuilder.h"

namespace newchatdemo {

// Returns a CustomFactory configured for every tag the demo's chat.xml
// uses. Pass the result to DuiXmlBuilder::FromString.
balloonwjui::DuiXmlBuilder::CustomFactory MakeChatFactory();

} // namespace newchatdemo
