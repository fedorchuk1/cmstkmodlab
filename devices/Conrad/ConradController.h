/////////////////////////////////////////////////////////////////////////////////
//                                                                             //
//               Copyright (C) 2011-2017 - The DESY CMS Group                  //
//                           All rights reserved                               //
//                                                                             //
//      The CMStkModLab source code is licensed under the GNU GPL v3.0.        //
//      You have the right to modify and/or redistribute this source code      //
//      under the terms specified in the license, which may be found online    //
//      at http://www.gnu.org/licenses or at License.txt.                      //
//                                                                             //
/////////////////////////////////////////////////////////////////////////////////

#ifndef ConradController_h
#define ConradController_h

#include <vector>

#include "VConradController.h"

class ConradCommunication;

/// \brief Class handling readout of Conrad multimeter
class ConradController : public VConradController {

public:

  ConradController( const ioport_t );
  ~ConradController();

  bool initialize();

  std::vector<bool> queryStatus() const;
  bool setChannel(unsigned channel, bool value) const;
  bool setSingleChannel(unsigned channel, bool value) const;

private:
  bool queryRawStatus(unsigned char& status) const;

  ConradCommunication* m_communication;
};

#endif
