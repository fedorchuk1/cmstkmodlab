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

#ifndef DEFOKEITHLEYMODEL_H
#define DEFOKEITHLEYMODEL_H

#include <KeithleyModel.h>

class DefoKeithleyModel : public KeithleyModel
{
    Q_OBJECT

public:
 
  explicit DefoKeithleyModel(const char* port,
			     int updateInterval = 60,
			     QObject *parent = 0);

protected slots:

  void scanTemperatures();
};

#endif // DEFOKEITHLEYMODEL_H
