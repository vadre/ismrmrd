#ifndef ICP_USERAPPBASE_H
#define ICP_USERAPPBASE_H

#include "icp.h"
#include "icpSession.h"


class icpUserAppBase
{
  public:
  virtual      ~icpUserAppBase() = default;
               //icpUserAppBase() : _session (NULL) {}
  //virtual      ~icpUserAppBase() { if (_session) _session = NULL; }

  virtual void saveSessionPointer (icpSession* session) = 0;
  virtual void handleHandshake    (HANDSHAKE msg)       = 0;
  virtual void handleCommand      (COMMAND msg)         = 0;

  virtual void handleErrorNotification (ERRNOTE msg)
    { std::cout << __func__ << " - Not immplemented\n"; }
  virtual void handleIsmrmrdHeader     (XMLHEAD msg)
    { std::cout << __func__ << " - Not immplemented\n"; }
  virtual void handleImage             (ENTITY* ent, STYPE stype)
    { std::cout << __func__ << " - Not immplemented\n"; }
  virtual void handleAcquisition       (ENTITY* ent, STYPE stype)
    { std::cout << __func__ << " - Not immplemented\n"; }

  //TODO:
  virtual void handleWaveform (ENTITY* ent, STYPE stype)
    { std::cout << __func__ << " - Not immplemented\n"; }
  virtual void handleBlub     (ENTITY* ent, STYPE stype)
    { std::cout << __func__ << " - Not immplemented\n"; }
 };

#endif
